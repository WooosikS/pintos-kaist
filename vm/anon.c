/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

#include "threads/vaddr.h"
#include "bitmap.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

struct bitmap *swap_table;
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	size_t swap_size = disk_size(swap_disk);
	swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping */
// 유저프로그램이 실행될때 page fault 발생할때 실행
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// Union 영역 0으로 초기화
	struct uninit_page *uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));

	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_sec = -1;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	int page_no = anon_page->swap_sec;

	// bitmap_test (const struct bitmap *b, size_t idx)
	// 스왑슬롯 사용여부
	if (bitmap_test(swap_table, page_no) == false) {
		return false;
	}
	
	// 디스크에 있는데이터를 램으로
	for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
		// disk_read (struct disk *d, disk_sector_t sec_no, void *buffer)
		disk_read(swap_disk, page_no * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
	}

	// 스왑슬롯 false로 만들기
	bitmap_set(swap_table, page_no, false);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	
	// false값을 가지는 비트를 찾는다.
	// bitmap_scan (const struct bitmap *b, size_t start, size_t cnt, bool value)
	int page_no = bitmap_scan(swap_table, 0, 1, false);
	if (page_no == BITMAP_ERROR) {
		return false;
	}

	// 8번 반복해서 페이지를 디스크에 저장
	for (int i=0; i<SECTORS_PER_PAGE; ++i) {
		// disk_write (struct disk *d, disk_sector_t sec_no, const void *buffer)
		disk_write(swap_disk, page_no * SECTORS_PER_PAGE + i, page->va + DISK_SECTOR_SIZE * i);
	}

	// swap slot의 비트를 true로
	// bitmap_set (struct bitmap *b, size_t idx, bool value) 
	bitmap_set(swap_table, page_no, true);
	  // 해당 페이지의 PTE에서 present bit을 0으로 바꿔준다.
	pml4_clear_page(thread_current()->pml4, page->va);

	anon_page->swap_sec = page_no;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
