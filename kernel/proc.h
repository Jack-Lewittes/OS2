


//TASK 2.2 
/*
cpu's run threads -> only trap threads can be these states.  
If a cpu runs process, its running a specific thread, so it can only be running or runnable (thread).

Used == RUNNING or RUNNABLE  
Running a process == running a thread within.  Therefore threadstate is expanded, and procstate only needs
these three states.

*/

enum procstate {P_UNUSED, P_USED, P_ZOMBIE };


// Per-process state (PCB)
struct proc {
  struct spinlock lock;        // lock for public properties

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  struct kthread kthread[NKT];        // kthread group table
  struct trapframe *base_trapframes;  // data page for trampolines

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  
  //TASK 2.1
  struct spinlock tid_lock;    // Lock for thread ID allocation
  int next_tid;                // Next available thread ID

};
