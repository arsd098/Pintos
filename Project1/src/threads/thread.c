#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Added Headerfile */
/* For 4BSD scheduler */
#include "threads/fixed_point.h"  /* For fixed point calculation */

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Added variables */
/* For Alarm clock */
/* Store the smallest wakeup value among threads in sleep list */
static int64_t next_wakeup = INT64_MAX;  
/* List to manage THREAD_BLOCKED threads */
static struct list sleep_list;
/* For 4BSD scheduler */
int load_avg;          /* Declaer load_avg */

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&sleep_list);     // Initialize sleep_list

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);
  // For 4BSD scheduler
  load_avg = LOAD_AVG_DEFAULT;  

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  /* For priority scheduling */
  /* Compare the priority of create thread with current thread */
  max_compare_priority();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  // list_push_back (&ready_list, &t->elem);
  /* For priority scheduling */ 
  /* Put the therad in ready_list using priority method */
  list_insert_ordered(&ready_list, &t->elem, compare_priority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    // list_push_back (&ready_list, &cur->elem);
    /* For priority scheduling */
    /* Put the therad in ready_list using priority method */
    list_insert_ordered(&ready_list, &cur->elem, compare_priority, NULL);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{ 
  /* For 4BSD scheduler */
  if(!thread_mlfqs)             /* When not an advance scheduler */
  {
    thread_current()->priority = new_priority;
    /* For priority donation */
    /* Store input value in init_priority */
    thread_current()->init_priority = new_priority;
    /* Execute recover_priority */
    recover_priority();

    /* For priority scheduling */
    /* Compare the priority of current thread 
       with other threads in ready_list  */
    max_compare_priority();
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  enum intr_level old_level;
  struct thread *t = thread_current();
  old_level = intr_disable();
  /* Limit the nice value to a range */
  int new_nice = nice;
  if(new_nice>20)
    new_nice = 20;
  else if(new_nice<-20)
    new_nice = -20;
  /* Change the thread's nice value with input */
  t->nice = new_nice;
  /* Calculate bsd_priority*/
  bsd_priority(t);
  /* Compare the priority of current thread 
       with other threads in ready_list  */
  max_compare_priority();

  intr_set_level (old_level);
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  enum intr_level old_level;
  int result_load_avg;
  old_level = intr_disable();
  /* Calculate and make fixed pioner to 100*load_avg */
  result_load_avg = mult_mixed(load_avg,100);
  /* Round the calculated value */
  result_load_avg = fp_to_int_round(result_load_avg);

  intr_set_level (old_level);
  /* Return the calculated load_avg */
  return result_load_avg;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  enum intr_level old_level;
  int result_recent_cpu;
  old_level = intr_disable();
  /* Calculate and make fixed pioner to 100*recent_cpu */
  result_recent_cpu = mult_mixed(thread_current()->recent_cpu,100);
  /* Round the calculated value */
  result_recent_cpu = fp_to_int_round(result_recent_cpu);

  intr_set_level (old_level);
  /* Return the calculated recent_cpu */
  return result_recent_cpu;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);

  /* Added initialize of value */
  /* For priority-donation */
  t->init_priority = priority;   /* Initialize the init_priority */
  lock_init (&t->wait_lock);     /* Initialize the wait_lock */
  list_init (&t->donation);      /* Initialize the donation list */
  /* For 4BSD scheduler */
  t->nice = NICE_DEFAULT;        /* Initialize the nice value */
  t->recent_cpu = RECENT_CPU_DEFAULT;  /* Initialize the recnet cpu value */
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Added Function */
/* For Alarm clock */
/* Make running thread to THREAD_BLOCKED state 
   and add it to the sleep list */
void
thread_sleep(int64_t ticks)
{
  /* Disables interrupts */
  enum intr_level old_level;    
  old_level = intr_disable();      
  /* Stores running thread to *t variable */
  struct thread *t = thread_current();
  /* When running thread is not idle_thread */
  if(t != idle_thread)    
  {
    /* Save input argument to wakeup variable of thread */
    t->wakeup = ticks;
    /* Update next_wakup value */
    update_wakeup(t->wakeup);
    /* Add running thread to the sleep list */
    list_push_back(&sleep_list,&t->elem);
    /* Make running thread to THREAD_BLOCKED state and schedule */
    thread_block();
  }
  /* Returns the previous interrput status */
  intr_set_level(old_level);       
}
/* Compare the existing next_wakeup value with the input value
   and store the smaller value in the next_wakeup variable */
void
update_wakeup(int64_t ticks)
{
  if(next_wakeup > ticks)
    next_wakeup = ticks;
}
/* Return next_wakeup value */
int64_t
return_wakeup(void)
{
  return next_wakeup;
}
/* Wake up a thread that is asleep in the sleep list */
void
thread_awake(int64_t ticks)
{
  struct list_elem *wake_e;
  /* Search sleep list for find thread to wake up */
  for(wake_e = list_begin(&sleep_list); wake_e != list_end(&sleep_list);)
  {
    struct thread *t = list_entry(wake_e, struct thread, elem);
    /* When it's time to wake up the thread */
    if (ticks >= t->wakeup)
    {
      /* Remove the thread from sleep list */
      wake_e = list_remove(&t->elem);
      /* Make thread to THREAD_READY state and schedule */
      thread_unblock(t);
    }
    else
    {
      wake_e = list_next(wake_e);
      /* Update value of next_wakeup variable */
      update_wakeup(t->wakeup);
    }
  }
}

/* For priority scheduling */
/* Compare the priority between two threads */
bool 
compare_priority(const struct list_elem *a,
                 const struct list_elem *b, void* aux UNUSED)
{
  /* Return the two threads as struct thread */
  struct thread *t_a = list_entry(a, struct thread, elem);
  struct thread *t_b = list_entry(b, struct thread, elem);
  /* Compare the priority */
  if (t_a->priority > t_b->priority)
    return true;
  else return false;
}
/* Compare the priority of the current thread with the highest 
   priority thread in ready_list and scheduling */
void
max_compare_priority(void)
{ 
  /* If the ready_list is empty, do nothing */
  if( list_empty(&ready_list) )
    return;
  /* Return the highest priority threads as struct thread */
  struct list_elem *front_e = list_front(&ready_list);
  struct thread *front_t = list_entry(front_e, struct thread, elem);
  /* If the priority of current thread is lower than highest thread */
  if(front_t->priority > thread_current()->priority)
    /* Give up CPU occupancy and scheduling */
    thread_yield();
}

/* For priority-donation */
/* Compare the priority of the donation thread and holder thread 
   and perform multiple & nested priority donation */
void
priority_donate(void)
{
  /* Return 2 thread for compare and 1 lock */
  struct thread *t = thread_current();
  struct lock *wait_l = t->wait_lock;
  struct thread *holder_t = wait_l->holder;
  /* Search the donation list of holder */
  while(true)
  {
    /* Compare the priority of the donation thread and holder thread */
    if(t->priority > holder_t->priority)
      /* Execute priority donation */
      holder_t->priority = t->priority;
    /* Change wait_l to holder's wait_lock */
    wait_l = holder_t->wait_lock;
    /* If nested priority donation case exists*/
    if(wait_l != NULL)
    {
      /* Change donation thread to holder thread */
      t = holder_t;
      /* Change holder thread to next holder thread */
      holder_t = wait_l->holder;
    }
    /* Escape condtion of while */
    else return;
  }
}
/* When the lock is released, remove the thread from donation list */
void
remove_lock(struct lock *lock)
{
  /* Return thread and list_elem for search */
  struct thread *holder_t = lock->holder;
  struct list_elem *e;
  /* Search the donation list of holder */
  for(e=list_begin(&holder_t->donation); e!=list_end(&holder_t->donation);
      e=list_next(e))
  { 
    struct thread *remove_t = list_entry(e, struct thread, elem_donation);
    /* When the wait_lock of some thread is released lock */
    if(remove_t->wait_lock == lock)
    {
      /* remove the thread from donation list */
      struct list_elem *pre = list_prev(e);
      list_remove(&remove_t->elem_donation);
      e = pre;
    }
  }
}
/* Change the priority of the current thread to initial priority
   and perform priority donation again */
void
recover_priority(void)
{
  struct thread *t = thread_current();
  /* Return list_elem of the first thread in donation list */
  struct list_elem *e = list_begin(&t->donation);
  /* Change the priority of the current thread to initial priority */
  t->priority = t->init_priority;
  /* When the donation thread exists */
  if(e!=NULL)
  {
    struct thread *donate_t = list_entry(e, struct thread, elem_donation);
    /* Compare the priority of current thread with donation thread */
    if(donate_t->priority > t->priority)
      /* Execute priority donation */
      t->priority = donate_t->priority;
  }
}

/* For 4BSD scheduler */
/* Calculate the priority of 4BSD scheduler case */
void
bsd_priority (struct thread *t)
{
  int cal_priority;
  int factor1, factor2;
  /* When the current thread is not idle_thread */
  if(t!=idle_thread)
  { 
    /* Calculate and make fixed pioner to recent_cpu/2 */
    factor1 = div_mixed(t->recent_cpu,4);
    /* Calculate and make fixed pioner to nice*2 */
    factor2 = int_to_fp(t->nice*2);
    /* Make fixed pionter to PRI_MAX */
    cal_priority = int_to_fp(PRI_MAX);
    /* PRI_MAX - recent_cpu/4 */
    cal_priority = sub_fp(cal_priority, factor1);
    /* PRI_MAX - recent_cpu/4 - nice*2 */
    cal_priority = sub_fp(cal_priority, factor2);
    /* Round the calculated value */
    cal_priority = fp_to_int_round(cal_priority);
    /* Update the priority of current thread with calculated value */
    t->priority = cal_priority;
  }
}
/* Calculate the recent cpu of 4BSD scheduler case */
void
bsd_recent_cpu (struct thread *t)
{
  int cal_recent_cpu;
  int factor1, factor2, decay;
  /* When the current thread is not idle_thread */
  if(t!=idle_thread)
  {
    /* Calculate and make fixed pioner to 2*load_avg */
    factor1 = mult_mixed(load_avg, 2);
    /* Calculate and make fixed pioner to 2*loag_avg + 1 */
    factor2 = add_mixed(factor1, 1);
    /* Calculate and make fixed pioner to decay */
    decay = div_fp(factor1, factor2);
    /* decay*recent_cpu */
    cal_recent_cpu = mult_fp(decay, t->recent_cpu);
    /* decay*recent_cpu + nice */
    cal_recent_cpu = add_mixed(cal_recent_cpu, t->nice);
    /* Update the recent cpu of current thread with calculated value */
    t->recent_cpu = cal_recent_cpu;
  }
}
/* Calculate the load average of 4BSD scheduler case */
void
bsd_load_avg (void)
{
  int cal_load_avg;
  int factor1, factor2;
  /* Count the number of threads in ready_list */
  int num_ready_thread = list_size(&ready_list);
  /* When the current thread is not idle_thread */
  if(thread_current()!=idle_thread)
    /* Since list_size functino return 1 less than the actual value */
    num_ready_thread++;
  /* Calculate and make fixed pioner to 59*load_avg */
  factor1 = mult_mixed(load_avg,59);
  /* Calculate and make fixed pioner to (59/60)*load_avg */
  factor1 = div_mixed(factor1, 60);
  /* Make fixed pioner to num_ready_thread */
  factor2 = int_to_fp(num_ready_thread);
  /* Calculate and make fixed pioner to num_ready_thread/60 */
  factor2 = div_mixed(factor2, 60);
  /* (59/60)*load_avg + num_ready_thread/60 */
  cal_load_avg = add_fp(factor1, factor2);
  /* Update the load_avg with calculated value */
  load_avg = cal_load_avg;   
}
/* Increse the current thread's recent cpu value by 1 */
void
bsd_increment (void)
{
  struct thread *t = thread_current();
  /* When the current thread is not idle_thread */
  if(t!=idle_thread)
    /* Increse the recent cpu value by 1 */
    t->recent_cpu = add_mixed(t->recent_cpu, 1);
}
/* Recalculate the recent cpu and priority of all threads */
void
bsd_recalc (void)
{
  struct list_elem *e;
  /* Search all threads in all list */
  for (e = list_begin(&all_list); e != list_end(&all_list);
       e = list_next(e))
  {
    struct thread *t = list_entry(e, struct thread, allelem);
    /* Recalculate the recent cpu */
    bsd_recent_cpu(t);
    /*Recalculate the priority */
    bsd_priority(t);
  }
}
/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
