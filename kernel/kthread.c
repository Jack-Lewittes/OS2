#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

//Task 2.2

extern struct proc proc[NPROC];
int nexttid = 1;
struct spinlock tid_lock;

void kthreadinit(struct proc *p)
{

  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {

    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
    kt->state = UNUSED;
    kt->proc = p;
    // for each thread, initialize the kt lock
    initlock(&kt->lock, "kthread_lock");


  }
    initlock(&p->tid_lock, "tid_lock");
    p->next_tid = 1;

}

struct kthread *mykthread()
{
  //return &myproc()->kthread[0];

  struct cpu *c = &cpus[cpuid()];
  return c->thread;
}

int alloctid(struct proc *p)
{
  acquire(&p->tid_lock);
  int tid = p->next_tid++;
  release(&p->tid_lock);

  return tid;
}

static struct kthread* allockthread(struct proc *p){
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
  return 0;

}

static void freekthread(struct kthread *kt)
{
  if(kt->trapframe)
    kfree((void*)kt->trapframe);
  kt->trapframe = 0;
  //set context to zeros
  memset(&kt->context, 0, sizeof(kt->context));
  kt->tid = 0;
  kt->proc = 0;
  kt->kstack = 0;
  kt->chan = 0;
  kt->killed = 0;
  kt->exit_status = 0;
  kt->state = UNUSED;
  //release(&kt->lock);
}



//Given Functions below

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}


// TODO: delte this after you are done with task 2.2
void allocproc_help_function(struct proc *p) {
  p->kthread->trapframe = get_kthread_trapframe(p, p->kthread);

  p->context.sp = p->kthread->kstack + PGSIZE;
}

