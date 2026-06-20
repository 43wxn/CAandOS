#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <common.h>

#ifndef PGSIZE
#define PGSIZE 4096
#endif

#define PG_ALIGN __attribute((aligned(PGSIZE)))

void* new_page(size_t);
void free_page(void *);
void get_memory_info(size_t *total, size_t *used, size_t *free);

/*
 * set_initial_brk — called by the ELF loader after a user program is loaded.
 * It tells the memory manager where the user program's BSS ends, so that
 * subsequent sbrk/brk calls from user space stay below the kernel heap region.
 */
void set_initial_brk(uintptr_t brk);

/*
 * mm_brk — user-space brk syscall handler.
 * Returns 0 on success, -1 if the request would overlap kernel allocations.
 */
int  mm_brk(uintptr_t brk);

#endif
