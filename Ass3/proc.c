#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change isUsed to EMBRYO and initialize
// isUsed required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;


  if(p->pid>2 && createSwapFile(p)==-1)
    panic("could not create swap");

  for(int i =0;i<MAX_PSYC_PAGES;i++){
    p->ramStrct[i].isUsed=0;
    p->fileStrct[i].isUsed=0;
    }
  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
    p->loadOrderCounter = 0;
    p->timesFaulted = 0;
    p->PgfltsCounter = 0;

    return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->isUsed lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);

}

int
pmalloc_check(void* addr, int set_pmalloc_or_protect_page_or_check_if_protected){
  struct proc* curproc = myproc();
  int ans;
  switch (set_pmalloc_or_protect_page_or_check_if_protected){
      case 0: {
          ans = set_pmalloc_bit(curproc->pgdir,addr);
          //if(ans == -1)
              //cprintf("set_pmalloc_bit failed\n");
          return ans;
      }
      case 1:{
          ans = ppage(curproc->pgdir, addr);
          //if(ans == -1)
              //cprintf("ppage failed\n");
          return ans;
      }
      case 2:{
          return check_if_protected(curproc->pgdir,addr);
      }
      case 3:{
          turnoff_protected_bit(curproc->pgdir,addr);
          return 1;
      }
      case 4:{
          struct proc* p = myproc();
          if(p->sz % 8){
              //cprintf("i was called!!!\n");
              growproc(8 -(p->sz % 8));
            }
          return 1;
      }
      default:
          panic("can't get here!\n");
  }
  // check within pte that it was allocated using pmalloc.
  // check that is begins at the start of the page.
  // if not return -1, else continue.
  
}
// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set isUsed of returned proc to RUNNABLE.
int
fork(void)
{
  int pid;
  struct proc *np;
  struct proc *curproc = myproc();
  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  // Copy process isUsed from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
    np->sz = curproc->sz;
    int i;
    if (curproc->pid > 2){
        deepCopySwap(curproc,np);
        np->loadOrderCounter = curproc->loadOrderCounter;
        for (i = 0; i < MAX_PSYC_PAGES; i++){
            np->ramStrct[i] = curproc->ramStrct[i];
            //if(curproc->ramStrct[i].isUsed)
              //cprintf("is protected? %d\n",check_if_protected(curproc->ramStrct[i].pgdir,(void*)curproc->ramStrct[i].virAddrs));
            np->ramStrct[i].pgdir = np->pgdir;
        }
        for (i = 0; i < MAX_TOTAL_PAGES-MAX_PSYC_PAGES; i++){
            np->fileStrct[i] = curproc->fileStrct[i];
            //if(curproc->fileStrct[i].isUsed)
              //cprintf("is protected? %d\n",check_if_protected(curproc->fileStrct[i].pgdir,(void*)curproc->fileStrct[i].virAddrs));
            np->fileStrct[i].pgdir = np->pgdir;
        }
    }



  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

    for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  np->timesFaulted = 0;
  np->PgfltsCounter = 0;
  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie isUsed
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");
  if (curproc->pid > 2)
      removeSwapFile(curproc);
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;

  #if TRUE
    if(curproc->pid>2)
      procdump();
  #endif

  sched();
  panic("zombie exit");
}
void
cleanPageStructs(struct proc* p){
    int index;
    for (index = 0; index < MAX_PSYC_PAGES; index++){
        p->ramStrct[index].isUsed=0;
        p->fileStrct[index].isUsed=0;
      }
}
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        //cprintf("Reached here\n");
        // Found one.
        p->state = UNUSED;
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        if(p->pid>2)
          cleanPageStructs(p);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        //added
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->isUsed before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->isUsed. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->isUsed and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
int getPagedOutAmout(struct proc* p){

    int i;
    int amout = 0;

    for (i=0;i < MAX_PSYC_PAGES; i++){
        if (p->fileStrct[i].isUsed== 1)
            amout++;
    }
    return amout;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void){
    static char *states[] = {
            [UNUSED]    "unused",
            [EMBRYO]    "embryo",
            [SLEEPING]  "sleep ",
            [RUNNABLE]  "runble",
            [RUNNING]   "run   ",
            [ZOMBIE]    "zombie"
    };
    struct proc *p;
    char *state;
    uint pc[10];
    int allocatedPages;
    int pagedOutAmount;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == UNUSED)
            continue;
        if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        int ppages = 0;
        int indx=0;
        for (indx=0;indx<MAX_PSYC_PAGES;indx++){
            if (check_if_protected(p->pgdir, (const void *) p->ramStrct[indx].virAddrs)){
                if(p->ramStrct[indx].isUsed)
                      ppages++;

              }
        }
        for (indx=0;indx<MAX_TOTAL_PAGES-MAX_PSYC_PAGES;indx++){
            if (check_if_protected(p->pgdir, (const void *) p->fileStrct[indx].virAddrs)){
                if(p->fileStrct[indx].isUsed)
                      ppages++;
                }
        }
        allocatedPages = PGROUNDUP(p->sz)/PGSIZE;
        pagedOutAmount = getPagedOutAmout(p);

        cprintf("%d %s %d %d %d %d %d %s", p->pid, state, allocatedPages,
                pagedOutAmount,ppages,p->timesFaulted,  p->PgfltsCounter ,p->name);
        if(p->state == SLEEPING){
            getcallerpcs((uint*)p->context->ebp+2, pc);
            for(indx=0; indx<10 && pc[indx] != 0; indx++)
                cprintf(" %p", pc[indx]);
        }

        cprintf("\n");
    }
    cprintf("%d/%d free pages in the system\n",getFreePages(),getTotalPages());


}
void deepCopySwap(struct proc* source, struct proc* dest){
    if (source->pid < 3)
        return;
    char buff[PGSIZE];
    int i;
    for (i = 0; i < MAX_TOTAL_PAGES-MAX_PSYC_PAGES; i++){
        if (source->fileStrct[i].isUsed){
            if (readFromSwapFile(source, buff, PGSIZE*i, PGSIZE) != PGSIZE)
                panic("CopySwapFile error");
            if (writeToSwapFile(dest, buff, PGSIZE*i, PGSIZE) != PGSIZE)
                panic("CopySwapFile error");
        }
    }
}
