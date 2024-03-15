// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#if !defined(MPID_THREAD_H_INCLUDED)
#define MPID_THREAD_H_INCLUDED


/*
 * Implementation specific type definitions
 */

#include "mpe_thread.h"

typedef MPE_Thread_mutex_t MPID_Thread_mutex_t;
typedef MPE_Thread_cond_t  MPID_Thread_cond_t;
typedef MPE_Thread_id_t    MPID_Thread_id_t;
typedef MPE_Thread_tls_t   MPID_Thread_tls_t;



/*
 * Threads
 */

typedef void (* MPID_Thread_func_t)(void * data);

/*@
  MPID_Thread_create - create a new thread

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
void MPID_Thread_create(MPID_Thread_func_t func, void * data, MPID_Thread_id_t * id, int * err);

/*@
  MPID_Thread_exit - exit from the current thread
@*/
void MPID_Thread_exit(void);

/*@
  MPID_Thread_self - get the identifier of the current thread

  Output Parameter:
. id - identifier of current thread
@*/
void MPID_Thread_self(MPID_Thread_id_t * id);

/*@
  MPID_Thread_same - compare two threads identifiers to see if refer to the same thread

  Input Parameters:
+ id1 - first identifier
- id2 - second identifier
  
  Output Parameter:
. same - TRUE if the two threads identifiers refer to the same thread; FALSE otherwise
@*/
void MPID_Thread_same(MPID_Thread_id_t * id1, MPID_Thread_id_t * id2, int * same);

/*@
  MPID_Thread_yield - voluntarily relinquish the CPU, giving other threads an opportunity to run
@*/
void MPID_Thread_yield(void);


/*
 *    Mutexes
 */

/*@
  MPID_Thread_mutex_create - create a new mutex
  
  Output Parameters:
+ mutex - mutex
- err - error code (non-zero indicates an error has occurred)
@*/
void MPID_Thread_mutex_create(MPID_Thread_mutex_t * mutex, int * err);

/*@
  MPID_Thread_mutex_destroy - destroy an existing mutex
  
  Input Parameter:
. mutex - mutex

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPID_Thread_mutex_destroy(MPID_Thread_mutex_t * mutex, int * err);

/*@
  MPID_Thread_lock - acquire a mutex
  
  Input Parameter:
. mutex - mutex
@*/
void MPID_Thread_mutex_lock(MPID_Thread_mutex_t * mutex);

/*@
  MPID_Thread_unlock - release a mutex
  
  Input Parameter:
. mutex - mutex
@*/
void MPID_Thread_mutex_unlock(MPID_Thread_mutex_t * mutex);

/*@
  MPID_Thread_mutex_trylock - try to acquire a mutex, but return even if unsuccessful
  
  Input Parameter:
. mutex - mutex

  Output Parameter:
. flag - flag
@*/
void MPID_Thread_mutex_trylock(MPID_Thread_mutex_t * mutex, int * flag);


/*
 * Condition Variables
 */

/*@
  MPID_Thread_cond_create - create a new condition variable
  
  Output Parameters:
+ cond - condition variable
- err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
void MPID_Thread_cond_create(MPID_Thread_cond_t * cond, int * err);

/*@
  MPID_Thread_cond_destroy - destroy an existinga condition variable
  
  Input Parameter:
. cond - condition variable

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero 
        for success, non-zero if a failure occurred
@*/
void MPID_Thread_cond_destroy(MPID_Thread_cond_t * cond, int * err);

/*@
  MPID_Thread_cond_wait - wait (block) on a condition variable
  
  Input Parameters:
+ cond - condition variable
- mutex - mutex

  Notes:
  This function may return even though another thread has not requested that a 
  thread be released.  Therefore, the calling
  program must wrap the function in a while loop that verifies program state 
  has changed in a way that warrants letting the
  thread proceed.
@*/
void MPID_Thread_cond_wait(MPID_Thread_cond_t * cond, MPID_Thread_mutex_t * mutex);

/*@
  MPID_Thread_cond_broadcast - release all threads currently waiting on a condition variable
  
  Input Parameter:
. cond - condition variable
@*/
void MPID_Thread_cond_broadcast(MPID_Thread_cond_t * cond);

/*@
  MPID_Thread_cond_signal - release one thread currently waitng on a condition variable
  
  Input Parameter:
. cond - condition variable
@*/
void MPID_Thread_cond_signal(MPID_Thread_cond_t * cond);


/*
 * Thread Local Storage
 */
typedef void (*MPID_Thread_tls_exit_func_t)(void * value);


/*@
  MPID_Thread_tls_create - create a thread local storage space

  Input Parameter:
. exit_func - function to be called when the thread exists; may be NULL if a 
  callback is not desired
  
  Output Parameters:
+ tls - new thread local storage space
- err - location to store the error code; pointer may be NULL; error is zero 
        for success, non-zero if a failure occurred
@*/
void MPID_Thread_tls_create(MPID_Thread_tls_exit_func_t exit_func, MPID_Thread_tls_t * tls, int * err);

/*@
  MPID_Thread_tls_destroy - destroy a thread local storage space
  
  Input Parameter:
. tls - thread local storage space to be destroyed

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero 
        for success, non-zero if a failure occurred
  
  Notes:
  The destroy function associated with the thread local storage will not 
  called after the space has been destroyed.
@*/
void MPID_Thread_tls_destroy(MPID_Thread_tls_t * tls, int * err);

/*@
  MPID_Thread_tls_set - associate a value with the current thread in the 
  thread local storage space
  
  Input Parameters:
+ tls - thread local storage space
- value - value to associate with current thread
@*/
void MPID_Thread_tls_set(MPID_Thread_tls_t * tls, void * value);

/*@
  MPID_Thread_tls_get - obtain the value associated with the current thread 
  from the thread local storage space
  
  Input Parameter:
. tls - thread local storage space

  Output Parameter:
. value - value associated with current thread
@*/
void MPID_Thread_tls_get(MPID_Thread_tls_t * tls, void ** value);


/*
 * Error values
 */
#define MPID_THREAD_SUCCESS MPID_THREAD_ERR_SUCCESS
#define MPID_THREAD_ERR_SUCCESS 0
/* FIXME: Define other error codes.  For now, any non-zero value is an error. */


/*
 * Implementation specific function definitions (usually in the form of macros)
 */

/*
 * Threads
 */

#define MPID_Thread_create(func_, data_, id_, err_)     \
{                                                       \
    MPE_Thread_create((func_), (data_), (id_), (err_)); \
}

#define MPID_Thread_exit()                      \
{                                               \
    MPE_Thread_exit();                          \
}

#define MPID_Thread_self(id_)                   \
{                                               \
    MPE_Thread_self(id_);                       \
}

#define MPID_Thread_same(id1_, id2_, same_)     \
{                                               \
    MPE_Thread_same((id1_), (id2_), (same_));   \
}

#define MPID_Thread_yield()                     \
{                                               \
    MPE_Thread_yield();                         \
}


/*
 *    Mutexes
 */

#define MPID_Thread_mutex_create(mutex_, err_)  \
{                                               \
    MPE_Thread_mutex_create((mutex_), (err_));  \
}

#define MPID_Thread_mutex_destroy(mutex_, err_) \
{                                               \
    MPE_Thread_mutex_destroy((mutex_), (err_)); \
}

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_mutex_lock(mutex_)          \
{                                               \
    MPE_Thread_mutex_lock((mutex_), NULL);      \
}
#else
#define MPID_Thread_mutex_lock(mutex_)          \
{                                               \
    int err_;                                   \
    MPE_Thread_mutex_lock((mutex_), &err_);     \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);    \
}
#endif

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_mutex_unlock(mutex_)        \
{                                               \
    MPE_Thread_mutex_unlock((mutex_), NULL);    \
}
#else
#define MPID_Thread_mutex_unlock(mutex_)        \
{                                               \
    int err_;                                   \
    MPE_Thread_mutex_unlock((mutex_), &err_);   \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);    \
}
#endif

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_mutex_trylock(mutex_, flag_)        \
{                                                       \
    MPE_Thread_mutex_trylock((mutex_), (flag_), NULL);  \
}
#else
#define MPID_Thread_mutex_trylock(mutex_, flag_)                \
{                                                               \
    int err_;                                                   \
    MPE_Thread_mutex_trylock((mutex_), (flag_), &err_); \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);                    \
}
#endif


/*
 * Condition Variables
 */

#define MPID_Thread_cond_create(cond_, err_)    \
{                                               \
    MPE_Thread_cond_create((cond_), (err_));    \
}

#define MPID_Thread_cond_destroy(cond_, err_)   \
{                                               \
    MPE_Thread_cond_destroy((cond_), (err_));   \
}

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_cond_wait(cond_, mutex_)            \
{                                                       \
    MPE_Thread_cond_wait((cond_), (mutex_), NULL);      \
}
#else
#define MPID_Thread_cond_wait(cond_, mutex_)            \
{                                                       \
    int err_;                                           \
    MPE_Thread_cond_wait((cond_), (mutex_), &err_);     \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);            \
}
#endif

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_cond_broadcast(cond_)       \
{                                               \
    MPE_Thread_cond_broadcast(cond_, NULL);     \
}
#else
#define MPID_Thread_cond_broadcast(cond_)       \
{                                               \
    int err_;                                   \
    MPE_Thread_cond_broadcast((cond_), &err_);  \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);    \
}
#endif

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_cond_signal(cond_)          \
{                                               \
    MPE_Thread_cond_signal(cond_, NULL);        \
}
#else
#define MPID_Thread_cond_signal(cond_)          \
{                                               \
    int err_;                                   \
    MPE_Thread_cond_signal((cond_), &err_);     \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);    \
}
#endif


/*
 * Thread Local Storage
 */

#define MPID_Thread_tls_create(exit_func_, tls_, err_)          \
{                                                               \
    MPE_Thread_tls_create((exit_func_), (tls_), (err_));        \
}

#define MPID_Thread_tls_destroy(tls_, err_)     \
{                                               \
    MPE_Thread_tls_destroy((tls_), (err_));     \
}

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_tls_set(tls_, value_)       \
{                                               \
    MPE_Thread_tls_set((tls_), (value_), NULL); \
}
#else
#define MPID_Thread_tls_set(tls_, value_)               \
{                                                       \
    int err_;                                           \
    MPE_Thread_tls_set((tls_), (value_), &err_);        \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);            \
}
#endif

#if !defined(MPID_THREAD_DEBUG)
#define MPID_Thread_tls_get(tls_, value_)       \
{                                               \
    MPE_Thread_tls_get((tls_), (value_), NULL); \
}
#else
#define MPID_Thread_tls_get(tls_, value_)               \
{                                                       \
    int err_;                                           \
                                                        \
    MPE_Thread_tls_get((tls_), (value_), &err_);        \
    MPIU_Assert(err_ == MPE_THREAD_SUCCESS);            \
}
#endif





#endif /* !defined(MPID_THREAD_H_INCLUDED) */
