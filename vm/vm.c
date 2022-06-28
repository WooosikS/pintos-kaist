/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

#include <hash.h>
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */

struct list frame_table;
struct list_elem *start;


void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	start = list_begin(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	// vm_alloc_page_with_initializer() 안의 spt_find_page()에서 해당 페이지가 없으면 malloc으로 해당 Page만큼 크기 할당 받는다.
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* --------------- Project 3 --------------- */
		bool (*initializer)(struct page *, enum vm_type, void *);
		
		switch (VM_TYPE(type)) {
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}

		struct page *new_page = malloc(sizeof(struct page));
		uninit_new (new_page, upage, init, type, aux, initializer);

		new_page->writable = writable;
		// new_page->page_cnt = -1;

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, new_page);
		/* ----------------------------------------- */		
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* --------------- Project 3 --------------- */
	struct page *page = (struct page*)malloc(sizeof(struct page));
	page->va = pg_round_down(va);

	struct hash_elem *e;
	e = hash_find(&spt->spt_table, &page->hash_elem);
	
	free(page);
	
	if (e == NULL) {
		return NULL;
	}

	return hash_entry(e, struct page, hash_elem);
	/* ----------------------------------------- */

	// return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *e = hash_find(&spt->spt_table, &page->hash_elem);
	
	if (e != NULL) {
		return succ;
	}

	hash_insert(&spt->spt_table, &page->hash_elem);
	
	return succ = true;

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->spt_table, &page->hash_elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	struct thread *curr = thread_current();
	struct list_elem *e = start;

	for (start = e; start != list_end(&frame_table); start = list_next(start)) {
		victim = list_entry(start, struct frame, frame_elem);
		// pml4_is_accessed (uint64_t *pml4, const void *vpage)
		// 1인경우
		if (pml4_is_accessed(curr->pml4, victim->page->va)){
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		} else {
			return victim;
		}
	}

	for (start = list_begin(&frame_table); start != e; start = list_next(start)) {
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va)) {
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		} else {
			return victim;
		}
	}
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */
	// struct frame *frame = NULL;
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER); //USER POOL에서 커널 가상 주소 공간으로 1page 할당

	/* if 프레임이 꽉 차서 할당받을 수 없다면 페이지 교체 실시
	   else 성공했다면 frame 구조체 커널 주소 멤버에 위에서 할당받은 메모리 커널 주소 넣기 */
	if (frame->kva == NULL)
	{
		frame = vm_evict_frame();
		frame->page = NULL; 

		return frame;
	}
	list_push_back (&frame_table, &frame->frame_elem);
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	// stack에 해당하는 anon페이지를 uninit으로 만들고 SPT에 넣어준다.
	// 물리 메모리와 매핑
	if (vm_alloc_page(VM_ANON | VM_STACK, addr, 1)) {
		vm_claim_page(addr);
		thread_current()->stack_bottom -= PGSIZE;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	
	// 유저가상 메모리 여야함
	if (is_kernel_vaddr(addr)) {
		return false;
	}

	// 유저 스택 포인터를 가져와야함
	void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
	if (not_present) {
		// 페이지 못 불러온 경우
		if (!vm_claim_page(addr)) {
			// 유저스택내에 존재하는지 체크
			if (rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK) {
				vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
				return true;
			}
			return false;
		}
		// 페이지 불러온경우
		else {
			return true;
		}
	} 
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* va가 속한 페이지를 불러옴*/
bool vm_claim_page (void *va UNUSED) {
	ASSERT(is_user_vaddr(va)) 

	struct page * page = spt_find_page (&thread_current()->spt, va);
	if (page == NULL){
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* 인자로 받은 Page와 새 Frame을 서로 연결 */
/* pml4_set_page() 함수로 프로세스의 pml4페이지 가상주소와 프레임 물리주소 매핑한 결과를 저장 */
static bool vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// page와 frame에 저장된 실제 physical memory 주소 (kernel vaddr) 관계를 page table에 등록
	
	if (install_page(page->va, frame->kva, page->writable)) {
		return swap_in(page, frame->kva);
	}

	return false;
}

unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	
	return hash_bytes (&p->va, sizeof p->va);
}

bool page_less (const struct hash_elem *a_,
           		const struct hash_elem *b_, void *aux UNUSED) {
	const struct page *a = hash_entry (a_, struct page, hash_elem);
  	const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

void *page_destory(struct hash_elem *h_elem, void *aux UNUSED){
	struct page *p = hash_entry(h_elem, struct page, hash_elem);
	vm_dealloc_page(p);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init (&spt->spt_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {

	struct hash_iterator i;
	struct hash *child = &src->spt_table;
	hash_first(&i, child);
	while(hash_next(&i)) {
		struct page	*page_entry = hash_entry (hash_cur (&i), struct page, hash_elem);

		enum vm_type type = page_entry->operations->type;
		void *upage = page_entry->va;
		bool writable = page_entry->writable;
		void *aux = page_entry->uninit.aux;
		struct file_info *file_info;
		struct page *new_page;

		switch(VM_TYPE(type)){

			case VM_UNINIT :
				file_info = (struct file_info *)malloc(sizeof(struct file_info));

				memcpy(file_info, (struct file_info*)page_entry->uninit.aux, sizeof(struct file_info));
				vm_alloc_page_with_initializer(VM_ANON, upage, writable, page_entry->uninit.init, file_info);
				break;

			case VM_ANON :

				if(!(vm_alloc_page(type, upage, writable))){
					return false;
				}

				new_page = spt_find_page(dst, upage);
				if(!vm_claim_page(upage)){
					return false;

				}
				memcpy(new_page->frame->kva, page_entry->frame->kva, PGSIZE);
				break;

			case VM_FILE :
				if(!(vm_alloc_page(type, upage, writable))){
					return false;
				}

				new_page = spt_find_page(dst, upage);
				if(!vm_claim_page(upage)){
					return false;
				}
				memcpy(new_page->frame->kva, page_entry->frame->kva, PGSIZE);
				break;
		}			
	}

	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	struct hash_iterator i;
	if (spt->spt_table.buckets != NULL)
	{
		hash_first(&i, &spt->spt_table);
		while(hash_next(&i))
		{
			struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);
			if (page->operations->type == VM_FILE)
			{
				do_munmap(page->va);
			}
		}
	}
	hash_destroy(&spt->spt_table, page_destory);
}