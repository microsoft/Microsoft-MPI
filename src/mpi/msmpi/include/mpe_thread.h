// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 *  Only edit the mpe_thread.h.in version of this file:
 *  @configure_input@
 */

#if !defined(MPE_THREAD_H_INCLUDED)
#define MPE_THREAD_H_INCLUDED

#if !defined(TRUE)
#define TRUE 1
#endif
#if !defined(FALSE)
#define FALSE 0
#endif


/*
 * Implementation specific type definitions
 */

/* For "historical" reasons windows.h includes winsock.h 
 * internally and WIN32_MEAN_AND_LEAN is to be defined
 * if we plan to include winsock2.h later on -- in the 
 * include hierarchy -- to prevent type redefinition 
 * errors...
 */

typedef HANDLE MPE_Thread_mutex_t;
typedef DWORD MPE_Thread_id_t;
typedef DWORD MPE_Thread_tls_t;

typedef struct MPE_Thread_cond_fifo_t
{
    HANDLE event;
    struct MPE_Thread_cond_fifo_t *next;
} MPE_Thread_cond_fifo_t;
typedef struct MPE_Thread_cond_t
{
    MPE_Thread_tls_t tls;
    MPE_Thread_mutex_t fifo_mutex;
    MPE_Thread_cond_fifo_t *fifo_head, *fifo_tail;
} MPE_Thread_cond_t;



/*
 * Threads
 */

typedef void (* MPE_Thread_func_t)(void * data);

/*@
  MPE_Thread_create - create a new thread

  Input Parameters:
+ func - function to run in new thread
- data - data to be passed to thread function

  Output Parameters:
+ id - identifier for the new thread
- err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred

  Notes:
  The thread is created in a detach state, meaning that is may not be waited upon.  If another thread needs to wait for this
  thread to complete, the threads must provide their own synchronization mechanism.
@*/
void MPE_Thread_create(MPE_Thread_func_t func, void * data, MPE_Thread_id_t * id, int * err);

/*@
  MPE_Thread_exit - exit from the current thread
@*/
void MPE_Thread_exit(void);

/*@
  MPE_Thread_self - get the identifier of the current thread

  Output Parameter:
. id - identifier of current thread
@*/
void MPE_Thread_self(MPE_Thread_id_t * id);

/*@
  MPE_Thread_same - compare two threads identifiers to see if refer to the same thread

  Input Parameters:
+ id1 - first identifier
- id2 - second identifier
  
  Output Parameter:
. same - TRUE if the two threads identifiers refer to the same thread; FALSE otherwise
@*/
void MPE_Thread_same(const MPE_Thread_id_t * id1, const MPE_Thread_id_t * id2, int * same);

/*@
  MPE_Thread_yield - voluntarily relinquish the CPU, giving other threads an opportunity to run
@*/
void MPE_Thread_yield(void);


/*
 *    Mutexes
 */

/*@
  MPE_Thread_mutex_create - create a new mutex
  
  Output Parameters:
+ mutex - mutex
- err - error code (non-zero indicates an error has occurred)
@*/
void MPE_Thread_mutex_create(MPE_Thread_mutex_t * mutex, int * err);

/*@
  MPE_Thread_mutex_destroy - destroy an existing mutex
  
  Input Parameter:
. mutex - mutex

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_mutex_destroy(MPE_Thread_mutex_t * mutex, int * err);

/*@
  MPE_Thread_lock - acquire a mutex
  
  Input Parameter:
. mutex - mutex

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_mutex_lock(MPE_Thread_mutex_t * mutex, int * err);

/*@
  MPE_Thread_unlock - release a mutex
  
  Input Parameter:
. mutex - mutex

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_mutex_unlock(MPE_Thread_mutex_t * mutex, int * err);

/*@
  MPE_Thread_mutex_trylock - try to acquire a mutex, but return even if unsuccessful
  
  Input Parameter:
. mutex - mutex

  Output Parameters:
+ flag - flag
- err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_mutex_trylock(MPE_Thread_mutex_t * mutex, int * flag, int * err);


/*
 * Condition Variables
 */

/*@
  MPE_Thread_cond_create - create a new condition variable
  
  Output Parameters:
+ cond - condition variable
- err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_cond_create(MPE_Thread_cond_t * cond, int * err);

/*@
  MPE_Thread_cond_destroy - destroy an existinga condition variable
  
  Input Parameter:
. cond - condition variable

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_cond_destroy(MPE_Thread_cond_t * cond, int * err);

/*@
  MPE_Thread_cond_wait - wait (block) on a condition variable
  
  Input Parameters:
+ cond - condition variable
- mutex - mutex

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred

  Notes:
  This function may return even though another thread has not requested that a thread be released.  Therefore, the calling
  program must wrap the function in a while loop that verifies program state has changed in a way that warrants letting the
  thread proceed.
@*/
void MPE_Thread_cond_wait(MPE_Thread_cond_t * cond, MPE_Thread_mutex_t * mutex, int * err);

/*@
  MPE_Thread_cond_broadcast - release all threads currently waiting on a condition variable
  
  Input Parameter:
. cond - condition variable

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_cond_broadcast(MPE_Thread_cond_t * cond, int * err);

/*@
  MPE_Thread_cond_signal - release one thread currently waitng on a condition variable
  
  Input Parameter:
. cond - condition variable

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_cond_signal(MPE_Thread_cond_t * cond, int * err);


/*
 * Thread Local Storage
 */
typedef void (*MPE_Thread_tls_exit_func_t)(void * value);


/*@
  MPE_Thread_tls_create - create a thread local storage space

  Input Parameter:
. exit_func - function to be called when the thread exists; may be NULL is a callback is not desired
  
  Output Parameters:
+ tls - new thread local storage space
- err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_tls_create(MPE_Thread_tls_exit_func_t exit_func, MPE_Thread_tls_t * tls, int * err);

/*@
  MPE_Thread_tls_destroy - destroy a thread local storage space
  
  Input Parameter:
. tls - thread local storage space to be destroyed

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
  
  Notes:
  The destroy function associated with the thread local storage will not called after the space has been destroyed.
@*/
void MPE_Thread_tls_destroy(const MPE_Thread_tls_t * tls, int * err);

/*@
  MPE_Thread_tls_set - associate a value with the current thread in the thread local storage space
  
  Input Parameters:
+ tls - thread local storage space
- value - value to associate with current thread

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_tls_set(const MPE_Thread_tls_t * tls, void * value, int * err);

/*@
  MPE_Thread_tls_get - obtain the value associated with the current thread from the thread local storage space
  
  Input Parameter:
. tls - thread local storage space

  Output Parameters:
+ value - value associated with current thread
- err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPE_Thread_tls_get(const MPE_Thread_tls_t * tls, void ** value, int * err);


/*
 * Error values
 */
#define MPE_THREAD_SUCCESS MPE_THREAD_ERR_SUCCESS
#define MPE_THREAD_ERR_SUCCESS 0
/* FIXME: Define other error codes.  For now, any non-zero value is an error. */


/*
 * Implementation specific function definitions (usually in the form of macros)
 */

/*
 * Threads
 * no inline functions
 */


/*
 * Mutexes
 * no inline functions
 */


/*
 * Condition Variables
 * no inline functions
 */


/*
 * Thread Local Storage
 * no inline functions
 */




#endif /* !defined(MPE_THREAD_H_INCLUDED) */
