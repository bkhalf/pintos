/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/kernel/list.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      struct list *temp = &sema->waiters;
      /*ordered according to ticks*/
      list_insert_ordered (temp, &thread_current ()->elem, &list_max_priority_comp, NULL);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
    sema->value++;
    if (!list_empty (&sema->waiters)) {
      list_sort(&(sema->waiters), &list_max_priority_comp, NULL);
      thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                  struct thread, elem));
    }
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
  lock->max_priority = 0;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  struct thread *current = thread_current ();
  if(!thread_mlfqs){
    current->waiting_on_lock = lock;
    /*if a higher priority thread enters --> change lock's max_priority and the holder's if exists*/
    if(current->priority > lock->max_priority){
        lock->max_priority = current ->priority;
        if(lock->holder!=NULL && !thread_mlfqs){
          lock->holder->priority = lock->max_priority;
          /*in case this holder is waiting somewhere*/
          notify_waiting_on_lock(lock->holder);
        }
    }
    /*if a lower priority thread enters --> just wait*/
    /*also it will be blocked, no need for lock notification here*/
    else{
        /*do nothing*/
    }
  }

  sema_down (&lock->semaphore);
  enum intr_level old_level = intr_disable ();
  struct thread* th = thread_current();
  if(!thread_mlfqs){
    lock->max_priority = th->priority;
    th->waiting_on_lock = NULL;
    /*add this lock to current thread aquired locks list*/
    struct list *temp = &th->locks;
    list_push_front(temp, &lock->lock_elem); 
  }
  lock->holder = th;
  intr_set_level (old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
  enum intr_level old_level = intr_disable ();

    /*releasing*/
  lock->holder = NULL;
  sema_up (&lock->semaphore);
  /*removing lock from current thread's locks_list*/
  if (!thread_mlfqs){
    struct thread *current = thread_current();
    list_remove(&lock->lock_elem);
  /*if it was donnated, get the next max priority of other aquired locks*/
  /*indeed this is in case this is the maximum donnation to enable change*/
    if(!list_empty(&current->locks)){
        /*get max priority of all max_priority of locks in current thread*/
        struct list *temp = &current->locks;
        int max = 0;
        for (struct list_elem *e = list_begin (temp); e != list_end (temp);
        e = list_next (e)){
        /*x = max priority of ith lock*/
        int x =list_entry (e, struct lock, lock_elem)->max_priority;
        if(max < x){
            max = x;
        }
      }
      current->priority = max;
      thread_yield();
    }
    /*in case of no other aquired locks, restore its original priorirty "no donnation"*/
    else{
      current->priority = current->original_priority;
      thread_yield();
    }
  }

  intr_set_level (old_level);
}
/*updates priority of hold thread by a lock if their priority is less than lock's*/
void
notify_holder(struct lock *lock){
  if(lock->holder!= NULL && lock->holder->priority < lock->max_priority){
        lock->holder->priority = lock->max_priority;
        /*in case this holder is waiting somewhere*/
        notify_waiting_on_lock(lock->holder);
      }
}
/*if thread t wait on another lock then update this lock's max_priority*/
/*NOTE THAT YOU MUST SORT waitings sema_up in case a thread is increased but not as much to change priority*/
/*and this case too*/
void
notify_waiting_on_lock(struct thread *t){
  if(t->waiting_on_lock!=NULL && t->waiting_on_lock->max_priority < t->priority){
      t->waiting_on_lock->max_priority = t->priority;
      notify_holder(t->waiting_on_lock);
  }
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}
/*Not clean.. to be edited later*/
/*of threads*/
int get_lock_max_priority(struct semaphore * sema){
  /*when no waiting threads ----> set lock max_priority to zero*/
  if(list_empty(&sema->waiters)){
    return 0;
  }
  return list_entry (list_front (&sema->waiters),
                                struct thread, elem)->priority;
 }


/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  waiter.semaphore.priority = thread_current()->priority;
  list_insert_ordered (&cond->waiters, &waiter.elem, list_sema_priority_comp, NULL);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
void
thread_sleep (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());
  old_level = intr_disable ();
  struct list *temp = &sema->waiters;
  list_insert_ordered (temp, &thread_current ()->elem, list_less_comp, NULL);
  thread_block();
  intr_set_level (old_level);
}
struct list_elem*
thread_awake (struct semaphore *sema, struct list_elem *e) 
{
    struct list_elem *x = list_remove(e);
    if(thread_mlfqs)
      thread_unblock_advanced(list_entry (e, struct thread, elem));
    else
      thread_unblock (list_entry (e, struct thread, elem));
    return x;
}

