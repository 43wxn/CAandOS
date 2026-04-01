#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "proc.h"
#include "syscall.h"

void do_syscall(Context *c);

static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_YIELD:
      return schedule(c);
    case EVENT_SYSCALL:
      do_syscall(c);
      c->mepc += 4;
      return c;
    case EVENT_IRQ_TIMER:
      return c;
    default:
      panic("Unhandled event ID = %d", e.event);
  }
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}