#include <memory.h>

static void *pf = NULL;
static void *pf_start = NULL;

void* new_page(size_t nr_page) {
  uintptr_t size = nr_page * PGSIZE;
  void *ret = pf;
  void *next = (void *)((uintptr_t)pf + size);

  if (nr_page == 0 || next > heap.end) {
    return NULL;
  }

  pf = next;
  memset(ret, 0, size);
  return ret;
}

#ifdef HAS_VME
static void* pg_alloc(int n) {
  return NULL;
}
#endif

void free_page(void *p) {
  panic("not implement yet");
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
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
