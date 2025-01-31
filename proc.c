#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

struct {
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void trapret(void);

int
cpuid() {
  return 0;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  return &cpus[0];
}

// Read proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c = mycpu();
  return c->proc;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  if((p->offset = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  p->sz = PGSIZE - KSTACKSIZE;

  sp = (char*)(p->offset + PGSIZE);

  // Allocate kernel stack.
  p->kstack = sp - KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)trapret;

  return p;
}

// Set up first process.
void
pinit(int pol)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;

  memmove(p->offset, _binary_initcode_start, (int)_binary_initcode_size);
  memset(p->tf, 0, sizeof(*p->tf));

  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;

  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE - KSTACKSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
  p->policy = pol;
  p->state = RUNNABLE;
}

// process scheduler.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();
//   c->proc = 0;
  
//   for(;;){
//     // Enable interrupts on this processor.
//     sti();

//     // Loop over process table looking for process to run.
//     for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//       if(p->state != RUNNABLE)
//         continue;

//       // Switch to chosen process. 
//       c->proc = p;
//       p->state = RUNNING;

//       switchuvm(p);
//       swtch(&(c->scheduler), p->context);
//     }
//   }
// }

void scheduler(void) {
  struct proc *p, *p1, *p2;
  struct cpu *c = mycpu();
  c->proc = 0;
  int num_foreground_tasks = 0, num_background_tasks = 0;
  int f_cnt = 0, b_cnt = 0;
  int x = 0, y = 0;

  for (;;) {

    sti();
    num_foreground_tasks = 0;
    num_background_tasks = 0;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state != RUNNABLE) {
        continue;
      }

      if (p->policy == 0) {
        num_foreground_tasks++;
      } else if (p->policy == 1) {
        num_background_tasks++;
      }
    }

    if (num_foreground_tasks == 0 && num_background_tasks == 0) {
      continue;
    }

    if (x == NPROC) {
      x = 0;
    }
    if (y == NPROC) {
      y = 0;
    }

    if (num_foreground_tasks > 0 && f_cnt < 9) {
      while (x < NPROC && f_cnt < 9) {
        p1 = &ptable.proc[x];
        x++;
        if (p1->state != RUNNABLE) {
          continue;
        }

        if (p1->policy == 0 && f_cnt < 9) {
          c->proc = p1;
          p1->state = RUNNING;
          b_cnt = 0;
          f_cnt++;
          switchuvm(p1);
          swtch(&(c->scheduler), p1->context);

          if (num_background_tasks == 0) {
            f_cnt = 0;
          }
          if (f_cnt == 9) {
            break;
          }

        }
      }

    } else if (num_background_tasks > 0 && b_cnt < 1) {
      while (y < NPROC && b_cnt < 1) {
        p2 = &ptable.proc[y];
        y++;
        if (p2->state != RUNNABLE) {
          continue;
        }

        if (p2->policy == 1 && b_cnt < 1) {
          c->proc = p2;
          p2->state = RUNNING;
          f_cnt = 0;
          b_cnt++;
          switchuvm(p2);
          swtch(&(c->scheduler), p2->context);

          if (num_foreground_tasks == 0) {
            b_cnt = 0;
          } 
          if (b_cnt == 1) {
            break;
          }
        }

      }
    }
  }

}

// void scheduler(void) {
//   struct proc *p;
//   struct cpu *c = mycpu();
//   c->proc = 0;
//   int num_foreground_tasks = 0, num_background_tasks = 0;

//   struct proc *p1, *p2;
//   int i=0, j=0, cp=0;

//   for (;;) {
//     sti();
//     num_foreground_tasks = 0;
//     num_background_tasks = 0;

//     for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
//       if (p->state != RUNNABLE) {
//         continue;
//       }

//       if (p->policy == 0) {
//         num_foreground_tasks++;
//       } else if (p->policy == 1) {
//         num_background_tasks++;
//       }
//     }

//     if(num_foreground_tasks == 0 && num_background_tasks == 0) {
//       continue;
//     }
//     if(num_foreground_tasks == 0) {
//       cp = 9;
//     }
//     else if(num_background_tasks == 0) {
//       cp = 0;
//     }

//     if(i == NPROC) {
//       i=0;
//     }
//     if(j == NPROC){
//       j=0;
//     }

//     if(num_foreground_tasks>0 && cp<9) {
//       while(i < NPROC) {
//         p1 = &ptable.proc[i];
//         if(p1->state != RUNNABLE) {
//           i++;
//           continue;
//         }

//         if(p1->policy == 0 && cp<9) {
//           c->proc = p1;
//           p1->state = RUNNING;

//           switchuvm(p1);
//           swtch(&(c->scheduler), p1->context);
//           cp++;
//           if(num_background_tasks==0) {
//             cp = 0;
//           }
//           if(cp == 9) {
//             i++;
//             break;
//           }
//         }
//         i++;
//       }
        
//     } else if (num_background_tasks>0 && cp==9) {
//       while(j < NPROC) {
//         p2 = &ptable.proc[j];
//         if(p2->state != RUNNABLE) {
//           j++;
//           continue;
//         }

//         if(p2->policy == 1) {
//           c->proc = p2;
//           p2->state = RUNNING;

//           switchuvm(p2);
//           swtch(&(c->scheduler), p2->context);
//           if(num_foreground_tasks==0) {
//             cp = 9;
//           } else {
//             cp = 0;
//             break;
//           }
//         }
//         j++;
//       }
//     }

//   }

// }

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct cpu* c = mycpu();
  struct proc *p = myproc();

  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = c->intena;
  swtch(&p->context, c->scheduler);
  c->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  myproc()->state = RUNNABLE;
  sched();
}

void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  };
  struct proc *p;
  char *state;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    cprintf("\n");
  }
}