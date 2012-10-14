#include "sched.h"
#include "instr.h"
#include "../deps/libev/ev.h"

#if S_DEBUG
void SSchedDump(SSched* s) {
  printf("[sched %p] run queue:", s);
  STask* t = s->runq.head;
  size_t count = 0;
  while (t != 0) {
    printf(
      "%s[task %p]%s",
      (count++ % 4 == 0) ? "\n  " : "",
      t,
      (t->next) ? " -> " : ""
    );
    t = t->next;
  }
  printf("\n");
}
#else
#define SSchedDump(x) ((void)0)
#endif

SSched* SSchedCreate() {
  SSched* s = (SSched*)malloc(sizeof(SSched));
  s->runq = S_RUNQ_INIT;
  return s;
}

void SSchedDestroy(SSched* s) {
  STask* t;
  SRunQPopEachHead(&s->runq, t) {
    STaskDestroy(t);
  }
  free((void*)s);
}

// For the sake of readability and structure, the `SSchedExec` function is
// defined in a separate file
#include "sched_exec.h"

void SSchedRun(SVM* vm, SSched* s) {
  STask* t;
  SRunQ* runq = &s->runq;

  struct ev_loop* S_UNUSED evloop = ev_loop_new(EVFLAG_AUTO | EVFLAG_NOENV);

  SSchedDump(s);

  while ((t = runq->head) != 0) {
    //printf("[sched] resuming <STask %p>\n", t);
    #if S_VM_DEBUG_LOG
    printf(
        "[vm] ______________ __________ ____ _______ ____ ______________\n"
        "[vm] Task           PC         Op#  Op      Values\n"
    );// [vm] 0x7fd431c03930 1          [10] LT      ABC:   0, 255,   0
    #endif

    int ts = SSchedExec(vm, s, t);

    switch (ts) {
      case STaskStatusError:
      // task ended from an error. Remove from scheduler.
      printf("Program error\n");
      SRunQPopHead(runq);
      STaskDestroy(t);
      break;

    case STaskStatusEnd:
      // task ended. Remove from scheduler.
      SRunQPopHead(runq);
      STaskDestroy(t);
      break;

    case STaskStatusYield:
      SRunQPopHeadPushTail(runq);
      break;

    case STaskStatusWait:
      SRunQPopHead(runq);
      // But don't destroy as the task will be scheduled back in when whatever
      // it's waiting for has arrived
      S_NOT_IMPLEMENTED;
      break;

    default:
      S_UNREACHABLE;
    }

    ev_run(evloop, EVRUN_NOWAIT);
    SLogD("evloop refs: %d", ev_refcount(evloop));
  } // while there are queued tasks

  ev_loop_destroy(evloop);
}
