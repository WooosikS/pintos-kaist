/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "../include/userprog/process.h"
#include "../include/lib/round.h"
#include "../include/threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}


/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	 struct file_page *file_page UNUSED = &page->file;

	if(page == NULL){
			return false;
	}

	struct file_info *aux = (struct file_info*)page->uninit.aux;

	struct file *file = aux->file;
	off_t offset = aux->ofs;
	size_t page_read_bytes = aux->read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	file_seek(file, offset);

	if(file_read(file, kva, page_read_bytes) != (int)page_read_bytes){
			return false;
	}

	memset(kva + page_read_bytes, 0, page_zero_bytes);

	return true;
}


/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current();
	if (page == NULL) {
		return false;
	}
	// 수정된 페이지를 파일에 업데이트하고 dirty_bit을 0으로 만든다.
	struct file_info *file_info = (struct file_info *)page->uninit.aux;
	if (pml4_is_dirty(curr->pml4, page->va)) {
		// file_write_at (struct file *file, const void *buffer, off_t size, off_t file_ofs) 
		file_write_at(file_info->file, page->va, file_info->read_bytes, file_info->ofs);
		// pml4_is_dirty (uint64_t *pml4, const void *vpage)
		pml4_set_dirty(curr->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}


/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}


/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {

	uint32_t read_bytes = file_length(file) < length ? file_length(file) : length;
	uint32_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
	uint64_t mmap_addr = (uint64_t)addr;
	struct file *opened_file = file_reopen(file);

	if (opened_file == NULL)
		return NULL;
	// 파일을 페이지 단위로 잘라서 해당파일의 정보를 구조체에 넣는다.
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct file_info *file_info = (struct file_info *)malloc(sizeof(struct file_info));
		file_info->file = opened_file;
		file_info->read_bytes = page_read_bytes;
		file_info->ofs = offset;

		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_segment, file_info)) {
			file_close(opened_file);
			return NULL;
		}
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return mmap_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *curr = thread_current();
	 
	while(true) {
		// 매핑 해제 => persent bit을 0으로 만든다.
		struct page *current_page = spt_find_page(&curr->spt, addr);
		if (current_page == NULL){
			return;
		}

		// 수정된 페이지를 파일에 업데이트하고 dirty_bit을 0으로 만든다.
		struct file_info *file_info = (struct file_info *)current_page->uninit.aux;
		if (pml4_is_dirty(curr->pml4, current_page->va)) {

			file_write_at(file_info->file, addr, file_info->read_bytes, file_info->ofs);
			pml4_set_dirty(curr->pml4, current_page->va, 0);
		}

		// present bit을 0으로 만든다.
		pml4_clear_page(curr->pml4, current_page->va);
		addr += PGSIZE;
	}
}
