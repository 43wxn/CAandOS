#ifndef __PROC_H__
#define __PROC_H__

#include <common.h>

typedef struct PCB {
  Context *cp;
  Area stack;
} PCB;

extern PCB *current;

void switch_boot_pcb();
void init_proc();
Context* schedule(Context *prev);
void naive_uload(PCB *pcb, const char *filename);

#endif