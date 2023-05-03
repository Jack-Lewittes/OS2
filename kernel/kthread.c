#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "kthread.h"
#include "proc.h"
#include "defs.h"

//Task 2.2

extern struct proc proc[NPROC];
int nexttid = 1;
struct spinlock tid_lock;

void kthreadinit(struct proc *p)
{
  // printf("kthreadinit\n");
  initlock(&p->tid_lock, "tid_lock");

  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    // for each thread, initialize the kt lock
    initlock(&kt->lock, "kthread_lock");

    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
    kt->state = UNUSED;
    kt->proc = p;
  }
    p->next_tid = 1;

    
}

struct kthread *mykthread()
{
  
    push_off();
    struct cpu *c = mycpu();
    struct kthread *kt = c->thread;
    pop_off();
    return kt;

}

int alloctid(struct proc *p)
{
  // printf("alloctid\n");
  acquire(&p->tid_lock);
  int tid = p->next_tid++;
  release(&p->tid_lock);
  printf("alloctid: tid± %d\n", tid);
  return tid;
}

struct kthread* allockthread(struct proc *p){
  printf("allockthread\n");

  struct kthread *kt;
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);
    if (kt->state == UNUSED)
    {
      kt->state = USED;
      kt->tid = alloctid(p);
      //assign trapframe to the kthread
      kt->trapframe = get_kthread_trapframe(p, kt);
      //init context to zeros
      memset(&kt->context, 0, sizeof(kt->context));
      //change ra register in context to forkret address
      kt->context.ra = (uint64)forkret;
      //change ‘sp’ register in context to the top of the stack
      kt->context.sp = kt->kstack + PGSIZE;
      return kt;
    }
    else{
      release(&kt->lock);
    }
  }
  printf("allockthread: kt.tid: %d\n", kt->tid);
  return 0;

}

void freekthread(struct kthread *kt)
{
  // kt->kstack = 0;
  kt->trapframe = 0;
  kt->tid = 0;
  kt->proc = 0;
  kt->chan = 0;
  kt->killed = 0;
  kt->exit_status = 0;
  kt->state = UNUSED;
  memset(&kt->context, 0, sizeof(kt->context));
}


//Given Functions below

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  // printf("get_kthread_trapframe\n");
  return p->base_trapframes + ((int)(kt - p->kthread));
}


// TODO: delte this after you are done with task 2.2

// void allocproc_help_function(struct proc *p) {
//   p->kthread->trapframe = get_kthread_trapframe(p, p->kthread);

//   p->context.sp = p->kthread->kstack + PGSIZE;
// }

