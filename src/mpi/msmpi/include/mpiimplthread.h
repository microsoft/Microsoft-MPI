// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#if !defined(MPIIMPLTHREAD_H_INCLUDED)
#define MPIIMPLTHREAD_H_INCLUDED

/* Rather than embed a conditional test in the MPICH2 code, we define a 
   single value on which we can test */
#if (MPICH_THREAD_LEVEL == MPI_THREAD_MULTIPLE)
#define MPICH_IS_THREADED 1
#endif

#if (MPICH_THREAD_LEVEL >= MPI_THREAD_SERIALIZED)    
#include "mpid_thread.h"
#endif

typedef struct MPICH_ThreadInfo_t
{
    int               thread_provided;  /* Provided level of thread support */
    /* This is a special case for is_thread_main, which must be
       implemented even if MPICH2 itself is single threaded.  */
#if (MPICH_THREAD_LEVEL >= MPI_THREAD_SERIALIZED)    
    MPID_Thread_tls_t thread_storage;   /* Id for perthread data */
    MPID_Thread_id_t  master_thread;    /* Thread that started MPI */
#endif
} MPICH_ThreadInfo_t;
extern MPICH_ThreadInfo_t MPIR_ThreadInfo;

/*
 * Get a pointer to the thread's private data
 * Also define a macro to release any storage that may be allocated
 * by Malloc to ensure that memory leak tools don't report this when
 * there is no real leak.
 */
#ifndef MPICH_IS_THREADED

#define MPIR_GetPerThread() \
    (&MPIR_Thread)

#else

/* Define a macro to acquire or create the thread private storage */
static inline MPICH_PerThread_t* MPIR_GetOrInitThreadPriv(void)
{
    MPICH_PerThread_t* pt;
    MPID_Thread_tls_get(&MPIR_ThreadInfo.thread_storage, &pt);
    if (pt == NULL)
    {
        pt = (MPICH_PerThread_t *) MPIU_Calloc(1, sizeof(MPICH_PerThread_t));
        MPID_Thread_tls_set(&MPIR_ThreadInfo.thread_storage, pt);
    }

    return pt;
}

#define MPIR_GetPerThread() \
    MPIR_GetOrInitThreadPriv()

#endif /* MPICH_IS_THREADED */

#endif /* !defined(MPIIMPLTHREAD_H_INCLUDED) */
