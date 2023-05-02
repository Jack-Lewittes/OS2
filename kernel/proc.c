#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "kthread.h"
#include "proc.h"
#include "defs.h"



//COPIED VERSION

struct cpu cpus[NCPU];
struct proc proc[NPROC];
struct proc *initproc;
int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      uint64 va = KSTACK((int) ((p - proc) * NKT + (kt - p->kthread)));
      kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      kthreadinit(p);
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  struct kthread *kt = mykthread();
  struct proc *p = kt != 0 ? kt->proc : 0;
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == P_UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = P_USED;
  // TASK 2.2
  acquire(&p->tid_lock);
  p->next_tid = 1;
  release(&p->tid_lock);
  //

    // Allocate a trapframe page.
  if((p->base_trapframes = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  //TASK 2.2
  // alloc_kthread calls memset, sets ra, sp
  if(!allockthread(p)){
    release(&p->lock);
    return 0;
  }

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->base_trapframes)
    kfree((void*)p->base_trapframes);
  p->base_trapframes = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  // TASK 2.2 free all thread in proc
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = P_UNUSED;
  // free all thread in proc
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    if (kt->state != UNUSED) {
      acquire(&kt->lock);
      freekthread(kt);
      release(&kt->lock);
    }
  }
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
    if(mappages(pagetable, TRAPFRAME(0), PGSIZE,
              (uint64)(p->base_trapframes), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME(0), 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->kthread[0].trapframe->epc = 0;      // user program counter
  p->kthread[0].trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
   // set up first user thread 
  p->kthread[0].state = RUNNABLE;
  // release the lock to let scheduler to schedule this thread
  release(&p->kthread[0].lock);

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  printf("fork\n");
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct kthread *kt = mykthread();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    // printf("in uvmcopy with p; %d np; %d myproc; %d\n", p->pid, np->pid, myproc()->pid);
    release(&np->kthread[0].lock); //new
    // printf("after release of kthread.pid : %d of proc.id : %d\n",np->kthread[0].tid, np->pid); 
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->kthread[0].trapframe) = *(kt->trapframe);

  // Cause fork to return 0 in the child.
  np->kthread[0].trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  np->kthread[0].proc = np;
  pid = np->pid;

  release(&np->kthread[0].lock);
  // printf("fork: %d -> %d\n", p->pid, np->pid);
  release(&np->lock);
  // printf("fork: %d -> %d\n", p->pid, np->pid);

  acquire(&wait_lock);
  // printf("acquire wait_lock with pid: %d\n", myproc()->pid);
  np->parent = p;

  release(&wait_lock);
  //maintain order of process creation, so that the first thread is the one that is scheduled first
  acquire(&np->lock);
  // printf("Acquire np->lock with pid: %d\n", myproc()->pid);
  acquire(&np->kthread[0].lock);

  np->kthread[0].state = RUNNABLE;
  // printf("kthread runnable with pid: %d\n", np->kthread[0].tid);
  release(&np->kthread[0].lock);
  release(&np->lock);
  // printf("release np->lock with pid: %d\n", myproc()->pid);
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f); 
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = P_ZOMBIE;

  release(&p->lock);

  //Task 2.2 - exit all threads
  for(int i = 0; i < NKT; i++){
    //if unused, then that thread was never initialized, so we don't need to change its state
    acquire(&p->kthread[i].lock);
    p->kthread[i].exit_status = status;
    p->kthread[i].state = ZOMBIE;
    if (&p->kthread[i] != mykthread()) {
      release(&p->kthread[i].lock);
    }
  }

  //Taske 2.3
  struct kthread *kt = mykthread();

  for(int i = 0; i < NKT; i++){
      if(p->kthread[i].tid != kt->tid && p->kthread[i].state !=UNUSED){
        acquire(&(p->kthread[i].lock));
        p->kthread[i].killed = 1;
        release(&(p->kthread[i].lock));
        kthread_join(p->kthread[i].tid,0);
      }
    }

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == P_ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p) || kthread_killed(mykthread())){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->thread = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
       acquire(&p->lock);
      if (p->state != P_USED) {
        release(&p->lock);
        continue;
      }
      release(&p->lock);
      for (struct kthread *kt = p->kthread; kt< &p->kthread[NKT]; kt++) {
        acquire(&kt->lock);
        if(kt->state == RUNNABLE) {
          // Switch to chosen process.  It is the process's job
          // to release its lock and then reacquire it
          // before jumping back to us.
          kt->state = RUNNING;
          c->thread = kt;
          swtch(&c->context, &kt->context);

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->thread = 0;
        }
        release(&kt->lock);
      }
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct kthread *kt = mykthread();

  if(!holding(&kt->lock))
    panic("sched kthread->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(kt->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&kt->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct kthread *kt = mykthread();
  acquire(&kt->lock);
  kt->state = RUNNABLE;
  sched();
  release(&kt->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&mykthread()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct kthread *kt = mykthread();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&kt->lock);
  release(lk);

  // Go to sleep (parent process)
  kt->chan = chan;
  kt->state = SLEEPING;

  sched();

  // Tidy up.
  kt->chan = 0;
  // maintain order of acquire and release.
  release(&kt->lock);
  // Reacquire original lock.
  acquire(lk);
}


// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    for (struct kthread *kernal_thread = p->kthread; kernal_thread < &p->kthread[NKT] ; kernal_thread++) { 
      // go over all the threads in the process 
      if(kernal_thread != mykthread()){ //similar to original where it was if(p != myproc())
        acquire(&kernal_thread->lock);
        if(kernal_thread->state == SLEEPING && kernal_thread->chan == chan) {
          kernal_thread->state = RUNNABLE;
        }
        release(&kernal_thread->lock);
    }
    }
  }
}



// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      // Wake kthreads sleeping
      for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++){
        acquire(&kt->lock);
        kt->killed = 1;
        if (kt->state == SLEEPING)
          kt->state = RUNNABLE;
        release(&kt->lock);
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

//TASK 2.3
int
kthread_killed(struct kthread *kt)
{
  int k; 
  acquire(&kt->lock);
  k = kt->killed;
  release(&kt->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == P_UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

//Task 2.3

int kthread_create(void *(*start_func)(), void *stack, uint stack_size) {
  struct proc *curproc = myproc(); // get current process
  struct kthread *kt = allockthread(curproc); 
  // allocate a kthread (lock released only if no new thread is available)
  if (!kt) {
    //printf("kthread_create: !kt\n");
    return -1; // no available kthread found
  }

  // initialize kthread fields
  kt->killed = 0;
  kt->exit_status = 0;
  kt->chan = 0;

  // allocate kernel stack for the kthread
  kt->kstack = (uint64)stack + stack_size;
  
  // set user-space 'epc' register to point to start_func
  kt->trapframe->epc = (uint64)start_func;

  // set user-space 'sp' register to the top of the stack
  kt->trapframe->sp = (uint64)stack + stack_size;

  // set thread state to RUNNABLE and release lock
  
  // acquire(&kt->lock);
  kt->state = RUNNABLE;
  release(&kt->lock);

  return kt->tid; // return thread ID
}


int
kthread_id() {
  struct kthread *kt = mykthread();
  if (!kt) {
    return -1;
  }
  return kt->tid;  // return the thread's tid
}

int
kthread_kill(int ktid) {
    struct proc *p = myproc();
    struct kthread *kt;
    int found = 0;

    acquire(&p->lock);
    for (kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
        acquire(&kt->lock);
        if (kt->state == UNUSED) {
            release(&kt->lock);
            continue;
        }
        if (kt->tid == ktid) {
            found = 1;
            kt->killed = 1;
            if (kt->state == SLEEPING) {
                kt->state = RUNNABLE;
            }
            release(&kt->lock);
            break;
        }
        release(&kt->lock);
    }
    release(&p->lock);

    if (!found) {
        return -1;
    }
    return 0;
}


void
kthread_exit(int status)
{
  struct kthread *t = mykthread();
  struct proc *p = t->proc;

  acquire(&t->lock);

  t->exit_status = status;
  t->state = ZOMBIE;

  //release(&t->lock);

  acquire(&p->lock);
  // Check if all threads in the process have exited
  int all_threads_exited = 1;
  for (int i = 0; i < NKT; i++) {
    if (p->kthread[i].state != UNUSED && p->kthread[i].state != ZOMBIE) {
      all_threads_exited = 0;
      break;
    }
  }
  if (all_threads_exited) {
    p->xstate = status;
    p->state = P_ZOMBIE;
    // Notify parent if it's waiting for the process
    wakeup(p->parent);
  }
  release(&p->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("kthread_exit: scheduler returned");
}

int
kthread_join(int ktid, int *status){
  struct proc *p = myproc();
  struct kthread *kt;

  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    if(kt->tid == ktid){
      break;
    }
  }
  acquire(&p->lock);
  for(;;){
    acquire(&kt->lock);
    if(kt->state == ZOMBIE){
      if(status != 0 && copyout(p->pagetable,(uint64)status, (char *)&kt->exit_status, sizeof(kt->exit_status)) < 0){
        release(&kt->lock);
        release(&p->lock);
        return -1;
      }
      freekthread(kt);
      release(&kt->lock);
      release(&p->lock);
      return 0;
    }
    release(&kt->lock);

    sleep(&kt->lock, &p->lock);
  }
}