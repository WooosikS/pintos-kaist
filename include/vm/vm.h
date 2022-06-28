#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

#include <hash.h> 
#include "threads/vaddr.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. 
	 저장소 정보에 대한 보조 비트 플래그 마커. 값이 int에 맞을 때까지 마커를 더 추가할 수 있습니다.
	 */
	VM_STACK = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "hash.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. 
 * 
 * "페이지"의 표현입니다. 이것은 일종의 부모 클래스이며,
 * 4개의 자식 클래스(uninit_page, file_page, non_page, page 캐시)를 가지고 있다(프로젝트 4).
 * 이 구조물의 미리 정의된 부재를 제거/수정하지 마십시오
 * 
 * */


// 유저 가상 메모리에 만든 페이지를 관리하기위해 커널 주소 영역에 선언한 구조체
// 유저 가상 메모리내 페이지의 실제 주소는 page->va에 있다.
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */


	/* Your implementation */
	uint8_t type;
	bool is_loaded;
	struct hash_elem hash_elem;
	bool writable;

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};


/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
	struct list_elem frame_elem;
};


struct list frame_table;

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
/* 페이지 작업을 위한 함수 테이블입니다.
	 이것은 C에서 "인터페이스"를 구현하는 한 가지 방법이다.
	 구조체의 구성원에 "메서드" 표를 넣고 필요할 때마다 호출합니다. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};


#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
/* - 페이지 폴트시 커널이 supplemental_page_table에서 오류가 발생한
		 가상 페이지를 조회하여 어떤 데이터가 있어야 하는지 확인
	 - 프로세스가 종료될 때 커널이 추가 페이지 테이블을 참조하여 어떤 리소스를 free시킬 것인지 결정
*/
struct supplemental_page_table {
	struct hash spt_table;
};


#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
bool insert_page(struct hash *pages, struct page *p);
bool delete_page(struct hash *pages, struct page *p);

#endif  /* VM_VM_H */
