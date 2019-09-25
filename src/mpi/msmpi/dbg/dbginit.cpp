// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"
#include "dbgtypes.h"

/*
 * This file contains information and routines used to simplify the interface
 * to a debugger.  This follows the description in "A Standard Interface
 * for Debugger Access to Message Queue Information in MPI", by Jim Cownie
 * and William Gropp.
 *
 * This file should be compiled with debug information (-g)
 */

/* The following is used to tell a debugger the location of the shared
   library that the debugger can load in order to access information about
   the parallel program, such as message queues */

extern "C"
{
__declspec(dllexport) char MPIR_dll_name[] = "msmpi.dll";
}

/* Other symbols:
 * MPIR_i_am_starter - Indicates that this process is not an MPI process
 *   (for example, the forker mpiexec?)
 * MPIR_acquired_pre_main -
 * MPIR_partial_attach_ok -
*/


/* ------------------------------------------------------------------------- */
/*
 * Manage the send queue.
 *
 * The send queue is needed only by the debugger.  The communication
 * device has a separate notion of send queue, which are the operations
 * that it needs to complete, independent of whether the user has called
 * MPI_Wait/Test/etc on the request.
 *
 * This implementation uses a doubly linked list of user-visible requests
 * (more specifically, requests created with MPI_Isend, MPI_Issend, or
 * MPI_Irsend). The head's back link points to null. The tail's forward link
 * points to null
 *
 * The sendq is cleaned up at DLL_PROCESS_DETACH
 *
 * FIXME: We need to add MPI_Ibsend and the persistent send requests to
 * the known send requests.
 * FIXME: We should exploit this to allow Finalize to report on
 * send requests that were never completed.
 */

extern "C"
{
MPIR_Sendq* MPIR_Sendq_head = nullptr;
}

//
// Keep a pool of previous sendq elements to speed allocation of queue.
// The pool is a doubly linked list but is treated like a singly linked list.
// The head's back link points to itself, and the tail's forward link points
// to null.
//
// The pool is cleaned up at DLL_PROCESS_DETACH
//

MPIR_Sendq* MPIR_Sendq_pool = nullptr;

MPI_RWLOCK  MPIR_Sendq_lock = MPI_RWLOCK_INIT;

void MPIR_Sendq_remember( MPID_Request *req,
                          int rank, int tag, const MPI_CONTEXT_ID& context_id )
{
    MPIR_Sendq *p;

    MpiRwLockAcquireExclusive(&MPIR_Sendq_lock);

    if( MPIR_Sendq_pool != nullptr )
    {
        p = MPIR_Sendq_pool;
        MPIR_Sendq_pool = reinterpret_cast<MPIR_Sendq*>(p->ListEntry.Flink);
    }
    else
    {
        p = MPIU_Malloc_obj(MPIR_Sendq);
        if( p == nullptr )
        {
            /* Just ignore it */
            req->sendq_req_ptr = nullptr;
            goto fn_exit;
        }
    }
    p->sreq       = req;
    p->tag        = tag;
    p->rank       = rank;
    p->context_id = context_id;

    p->ListEntry.Flink = reinterpret_cast<PLIST_ENTRY>( MPIR_Sendq_head );
    p->ListEntry.Blink = nullptr;
    if( MPIR_Sendq_head != nullptr )
    {
        MPIR_Sendq_head->ListEntry.Blink = reinterpret_cast<PLIST_ENTRY>( p );
    }

    MPIR_Sendq_head = p;

    //
    // Set the back-link for req so that we can do O(1) removal in MPIR_Sendq_forget
    //
    req->sendq_req_ptr = p;
fn_exit:
    MpiRwLockReleaseExclusive(&MPIR_Sendq_lock);
}


void MPIR_Sendq_forget( MPID_Request *req )
{
    MpiRwLockAcquireExclusive(&MPIR_Sendq_lock);

    MPIR_Sendq* p = req->sendq_req_ptr;

    if( p == nullptr )
    {
        goto fn_exit;
    }
    req->sendq_req_ptr = nullptr;

    if( p == MPIR_Sendq_head )
    {
        MPIR_Sendq_head =
            reinterpret_cast<MPIR_Sendq*>( p->ListEntry.Flink );
        if( MPIR_Sendq_head != nullptr )
        {
            MPIR_Sendq_head->ListEntry.Blink = nullptr;
        }
    }
    else
    {
        PLIST_ENTRY nextEntry = p->ListEntry.Flink;
        PLIST_ENTRY prevEntry = p->ListEntry.Blink;

        prevEntry->Flink = nextEntry;
        if( nextEntry != nullptr )
        {
            nextEntry->Blink = prevEntry;
        }
    }

    //
    // Return p to the pool
    //
    p->ListEntry.Flink = reinterpret_cast<PLIST_ENTRY>( MPIR_Sendq_pool );
    p->ListEntry.Blink = nullptr;
    MPIR_Sendq_pool = p;

fn_exit:
    MpiRwLockReleaseExclusive(&MPIR_Sendq_lock);
}

/* Manage the known communicators */
/* Provide a list of all active communicators.  This is used only by the
   debugger message queue interface */
extern "C"
{
MPIR_Comm_list MPIR_All_communicators = { 0, nullptr };
}

MPI_RWLOCK     MPIR_All_communicators_lock = MPI_RWLOCK_INIT;


void MPIR_CommL_remember( MPID_Comm *comm_ptr )
{
    MPIU_Assert(comm_ptr->comm_next == nullptr);

    MpiRwLockAcquireExclusive(&MPIR_All_communicators_lock);

    if (comm_ptr == MPIR_All_communicators.head)
    {
        MPIU_Internal_error_printf( "Internal error: communicator is already on the list\n" );
        goto fn_exit;
    }
    comm_ptr->comm_next = MPIR_All_communicators.head;
    MPIR_All_communicators.head = comm_ptr;
    MPIR_All_communicators.sequence_number++;

fn_exit:
    MpiRwLockReleaseExclusive(&MPIR_All_communicators_lock);
}


void MPIR_CommL_forget( MPID_Comm *comm_ptr )
{
    MPID_Comm *p, *prev;

    MpiRwLockAcquireExclusive(&MPIR_All_communicators_lock);

    p = MPIR_All_communicators.head;
    prev = 0;
    while (p)
    {
        if (p == comm_ptr)
        {
            if (prev)
            {
                prev->comm_next = p->comm_next;
            }
            else
            {
                MPIR_All_communicators.head = p->comm_next;
            }
            break;
        }
        prev = p;
        p = p->comm_next;
    }
    /* Record a change to the list */
    MPIR_All_communicators.sequence_number++;

    MpiRwLockReleaseExclusive(&MPIR_All_communicators_lock);

}
