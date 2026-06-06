#include <memory.h>

static void *pf = NULL;
static void *pf_start = NULL;

#define MAX_PAGE_ALLOCS 1024

typedef struct {
  void *base;
  size_t size;
} PageAlloc;

static PageAlloc page_allocs[MAX_PAGE_ALLOCS];
static int page_alloc_top = 0;

void* new_page(size_t nr_page) {
  if (nr_page == 0) {
    return NULL;
  }

  uintptr_t size = nr_page * PGSIZE;
  uintptr_t cur = (uintptr_t)pf;
  uintptr_t next = cur + size;
  void *ret = pf;

  if (next < cur || next > (uintptr_t)heap.end) {
    return NULL;
  }

  if (page_alloc_top >= MAX_PAGE_ALLOCS) {
    Log("new_page: allocation record table is full");
    return NULL;
  }

  pf = (void *)next;
  page_allocs[page_alloc_top++] = (PageAlloc) {
    .base = ret,
    .size = size,
  };
  memset(ret, 0, size);
  return ret;
}

#ifdef HAS_VME
static void* pg_alloc(int n) {
  return NULL;
}
#endif

void free_page(void *p) {
  if (p == NULL) return;

  /*
   * This is still a bump allocator.  It can reclaim only the most recent
   * allocation, which is enough for simple RAMFS deletion/reuse demos and
   * avoids pretending that we have a full page free list.
   */
  if (page_alloc_top > 0) {
    PageAlloc *last = &page_allocs[page_alloc_top - 1];
    if (last->base == p) {
      memset(last->base, 0, last->size);
      pf = last->base;
      page_alloc_top--;
      return;
    }
  }

  Log("free_page: non-LIFO block %p ignored by bump allocator", p);
}

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) {
  return 0;
}

void get_memory_info(size_t *total, size_t *used, size_t *free) {
  size_t total_bytes = (uintptr_t)heap.end - (uintptr_t)pf_start;
  size_t used_bytes = (uintptr_t)pf - (uintptr_t)pf_start;

  if (total != NULL) *total = total_bytes;
  if (used != NULL) *used = used_bytes;
  if (free != NULL) *free = total_bytes - used_bytes;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  pf_start = pf;
  page_alloc_top = 0;
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
