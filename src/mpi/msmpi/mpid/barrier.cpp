// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

//
// Inverse BCast To Rank0
//
// Because eventual destination is rank0, tests against relative
// rank are avoided.  Higher ranks exit early; rank0 exits last.
//
static MPI_RESULT DeclareBarrierEntryToRank0(_In_ const MPID_Comm *comm_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    int rank = comm_ptr->rank;
    int size = comm_ptr->remote_size;
    //
    // If not a power of 2, take care of all above the binomial tree
    //
    if (IsPowerOf2(size) == false)
    {
        int pof2Floor = PowerOf2Floor(size);

        //
        // If above binomial tree, notify rank below and exit
        //
        if (rank - pof2Floor >= 0)
        {
            return MPIC_Send(
                nullptr,
                0,
                g_hBuiltinTypes.MPI_Datatype_null,
                rank - pof2Floor,
                MPIR_BARRIER_TAG,
                comm_ptr
                );
        }

        //
        // If there is a rank above that needs to send, receive it
        //
        if (rank + pof2Floor < size)
        {
            mpi_errno = MPIC_Recv(
                nullptr,
                0,
                g_hBuiltinTypes.MPI_Datatype_null,
                rank + pof2Floor,
                MPIR_BARRIER_TAG,
                comm_ptr,
                MPI_STATUS_IGNORE
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
        //
        // Only need to work with binomial tree after this
        //
        size = pof2Floor;
    }

    //
    // Set size to midpoint of binomial tree
    // ranks below the midpoint receive from ranks above the midpoint
    //
    size >>= 1;
    while (rank < size)
    {
        //
        // Receive from rank above midpoint
        //
        mpi_errno = MPIC_Recv(
            nullptr,
            0,
            g_hBuiltinTypes.MPI_Datatype_null,
            rank + size,
            MPIR_BARRIER_TAG,
            comm_ptr,
            MPI_STATUS_IGNORE
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        //
        // Reset midpoint for next round
        //
        size >>= 1;
    }
    //
    // Send to parent
    //
    if (rank != 0)
    {
        return MPIC_Send(
            nullptr,
            0,
            g_hBuiltinTypes.MPI_Datatype_null,
            rank - size,
            MPIR_BARRIER_TAG,
            comm_ptr
            );
    }
    return MPI_SUCCESS;
}


static MPI_RESULT ReleaseAllFromBarrier(_In_ const MPID_Comm *comm_ptr)
{
    int pof2;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    int size = comm_ptr->remote_size;
    int rank = comm_ptr->rank;

    if (rank != 0)
    {
        pof2 = PowerOf2Floor(rank);
        //
        // Receive from rank below pof2
        //
        mpi_errno = MPIC_Recv(
            nullptr,
            0,
            g_hBuiltinTypes.MPI_Datatype_null,
            rank - pof2,
            MPIR_BARRIER_TAG,
            comm_ptr,
            MPI_STATUS_IGNORE
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        //
        // Shift pof2 left, causing rank to be below it
        pof2 <<= 1;
    }
    else
    {
        pof2 = 1;
    }

    //
    // Send to rank above current pof2
    // provided it is still less than size
    //
    while (rank + pof2 < size)
    {
        mpi_errno = MPIC_Send(
            nullptr,
            0,
            g_hBuiltinTypes.MPI_Datatype_null,
            rank + pof2,
            MPIR_BARRIER_TAG,
            comm_ptr
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        //
        // Bring all ranks that have received underneath the umbrella
        //
        pof2 <<= 1;
    }

    return MPI_SUCCESS;
}

/* This is the default implementation of the barrier operation.  The
   algorithm is:

   Algorithm: MPI_Barrier

   We use the dissemination algorithm described in:
   Debra Hensgen, Raphael Finkel, and Udi Manbet, "Two Algorithms for
   Barrier Synchronization," International Journal of Parallel
   Programming, 17(1):1-17, 1988.

   It uses ceiling(lgp) steps. In step k, 0 <= k <= (ceiling(lgp)-1),
   process i sends to process (i + 2^k) % p and receives from process
   (i - 2^k + p) % p.

   Possible improvements:
   End Algorithm: MPI_Barrier

   This is an intracommunicator barrier only!
*/


static MPI_RESULT MPIR_Barrier_intra_flat(_In_ const MPID_Comm *comm_ptr )
{
    int size, rank, src, dst, mask;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    rank = comm_ptr->rank;
    size = comm_ptr->remote_size;

    mask = 0x1;
    while (mask < size)
    {
        dst = rank + mask;
        if (dst >= size)
        {
            dst -= size;
        }
        src = rank - mask;
        if (src < 0)
        {
            src += size;
        }

        mpi_errno = MPIC_Sendrecv(NULL, 0, g_hBuiltinTypes.MPI_Byte, dst,
                                  MPIR_BARRIER_TAG, NULL, 0, g_hBuiltinTypes.MPI_Byte,
                                  src, MPIR_BARRIER_TAG, comm_ptr,
                                  MPI_STATUS_IGNORE);
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }

        mask <<= 1;
    }

    return MPI_SUCCESS;
}

_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Barrier_inter(
_In_ const MPID_Comm *comm_ptr
)
{
    int rank, root;

    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    rank = comm_ptr->rank;

    /* do a barrier on the local intracommunicator */
    MPI_RESULT mpi_errno = MPIR_Barrier_intra(comm_ptr->inter.local_comm);
    ON_ERROR_FAIL(mpi_errno);

    /* rank 0 on each group does an intercommunicator broadcast to the
       remote group to indicate that all processes in the local group
       have reached the barrier. We do a 1-byte bcast because a 0-byte
       bcast will just return without doing anything. */

    /* first broadcast from left to right group, then from right to
       left group */
    if (comm_ptr->inter.is_low_group)
    {
        /* bcast to right*/
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Bcast_inter(&mpi_errno, 1, g_hBuiltinTypes.MPI_Byte, root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* receive bcast from right */
        root = 0;
        mpi_errno = MPIR_Bcast_inter(&mpi_errno, 1, g_hBuiltinTypes.MPI_Byte, root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* receive bcast from left */
        root = 0;
        mpi_errno = MPIR_Bcast_inter(&mpi_errno, 1, g_hBuiltinTypes.MPI_Byte, root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* bcast to left */
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Bcast_inter(&mpi_errno, 1, g_hBuiltinTypes.MPI_Byte, root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }

fn_fail:
    return mpi_errno;
}

static MPI_RESULT MPIR_Barrier_intra_HA(_In_ const MPID_Comm *comm_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    /* do the intranode barrier on all nodes */
    if (comm_ptr->intra.local_subcomm != nullptr)
    {
        mpi_errno = DeclareBarrierEntryToRank0(comm_ptr->intra.local_subcomm);
        ON_ERROR_FAIL(mpi_errno);
    }

    /* do the barrier across roots of all nodes */
    if (comm_ptr->intra.leaders_subcomm != nullptr)
    {
        mpi_errno = MPIR_Barrier_intra_flat(comm_ptr->intra.leaders_subcomm);
        ON_ERROR_FAIL(mpi_errno);
    }

    /* release the local processes on each node with a 1-byte broadcast
    (0-byte broadcast just returns without doing anything) */
    if (comm_ptr->intra.local_subcomm != nullptr)
    {
        mpi_errno = ReleaseAllFromBarrier(comm_ptr->intra.local_subcomm);
        ON_ERROR_FAIL(mpi_errno);
    }
fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Barrier_intra(
    _In_ const MPID_Comm* comm_ptr
    )
{
    MPIU_Assert(comm_ptr->comm_kind != MPID_INTERCOMM);

    if (Mpi.SwitchoverSettings.SmpBarrierEnabled &&
        comm_ptr->IsNodeAware())
    {
        return MPIR_Barrier_intra_HA( comm_ptr );
    }
    else
    {
        return MPIR_Barrier_intra_flat( comm_ptr );
    }
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
IbarrierBuildTaskList(
    _Inout_ MPID_Request* pReq,
    _In_ MPID_Comm *pComm
    )
{
    int size = pComm->remote_size;

    ULONG msb;
    _BitScanReverse(&msb, size);
    if (!IsPowerOf2(size))
    {
        msb++;
    }

    //
    // On each iteration, there is one send and one receive
    // so number of tasks equals twice number of iterations.
    //
    ULONG nTasks = msb * 2;

    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    int src;
    int dst;
    int rank = pComm->rank;
    int mask = 0x1;
    MPI_RESULT mpi_errno;

    MPIU_Assert( pReq->nbc.nTasks == 0 );

    while (mask < size)
    {
        dst = (rank + mask) % size;
        src = (rank - mask + size) % size;

        //
        // Initiate receive first to reduce the chance of late receives.
        //
        mpi_errno = pReq->nbc.tasks[pReq->nbc.nTasks].InitIrecv(
            nullptr,
            0,
            g_hBuiltinTypes.MPI_Byte,
            src
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        MPIU_Assert( pReq->nbc.nTasks < nTasks );

        //
        // Next task is send which will be initiated with this receive.
        //
        pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnInit = pReq->nbc.nTasks + 1;

        //
        // Only proceed to the next receive/send pair when this receive is done.
        //
        pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnComplete = pReq->nbc.nTasks + 2;
        pReq->nbc.nTasks++;

        mpi_errno = pReq->nbc.tasks[pReq->nbc.nTasks].InitIsend(
            nullptr,
            0,
            g_hBuiltinTypes.MPI_Datatype_null,
            dst,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnInit = NBC_TASK_NONE;
        pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnComplete = NBC_TASK_NONE;
        pReq->nbc.nTasks++;

        mask <<= 1;
    }

    MPIU_Assert( pReq->nbc.nTasks == nTasks );

    //
    // Fix up the last receive's m_iNextOnComplete.
    //
    pReq->nbc.tasks[pReq->nbc.nTasks - 2].m_iNextOnComplete = NBC_TASK_NONE;

    pReq->nbc.tag = pReq->comm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_BARRIER_TAG ) );

    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IbarrierBuildIntra(
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    pRequest->comm = const_cast<MPID_Comm*>( pComm );
    pRequest->comm->AddRef();

    if( pComm->remote_size == 1 )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
    }
    else
    {
        MPI_RESULT mpi_errno = IbarrierBuildTaskList( pRequest.get(), pComm );
        if( mpi_errno != MPI_SUCCESS )
        {
            pRequest->cancel_nbc_tasklist();
            return mpi_errno;
        }
    }

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibarrier_intra(
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IbarrierBuildIntra( pComm, &pRequest );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    mpi_errno = pRequest->execute();
    if( mpi_errno != MPI_SUCCESS )
    {
        pRequest->Release();
    }
    else
    {
        *ppRequest = pRequest;
    }

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
static
MPI_RESULT
IbarrierBuildInter(
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // Intercommunicator non-blocking barrier.
    //
    MPIU_Assert( pComm->comm_kind == MPID_INTERCOMM );
    MPIU_Assert( pComm->inter.local_comm != NULL );

    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_BARRIER_TAG ) );

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    pRequest->comm = pComm;
    pComm->AddRef();

    //
    // Three tasks required for intercomm non-blocking barrier.
    // 1. Non-blocking intracomm barrier on local intracommunicator.
    // 2. Non-blocking intercomm bcast from rank 0 to the remote group to
    //    indicate that all processes in the local group have reached the
    //    barrier.
    // 3. Non-blocking intercomm bcast receive from the remote group.
    //
    // Note: Task 1 and 3 can be initiated at the same time. Task 2 needs
    //       to wait till task 1 completes. In the implementation below,
    //       the order of task 2 and 3 depends on which group this process
    //       belongs to (local vs remote).
    //
    pRequest->nbc.tag = tag;
    pRequest->nbc.tasks = new NbcTask[3];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    int root1;
    int root2;
    if( pComm->inter.is_low_group )
    {
        root1 = ( pComm->rank == 0 )? MPI_ROOT : MPI_PROC_NULL;
        root2 = 0;
    }
    else
    {
        root1 = 0;
        root2 = ( pComm->rank == 0 )? MPI_ROOT : MPI_PROC_NULL;
    }

    MPI_RESULT mpi_errno = pRequest->nbc.tasks[0].InitIbarrierIntra( pComm->inter.local_comm );
    if( mpi_errno != MPI_SUCCESS )
    {
        pRequest->cancel_nbc_tasklist();
        return mpi_errno;
    }

    pRequest->nbc.tasks[0].m_iNextOnInit = ( root1 != 0 )? 2 : 1;
    pRequest->nbc.tasks[0].m_iNextOnComplete = ( root1 != 0 )? 1 : 2;
    pRequest->nbc.nTasks++;

    //
    // Initiate 0-byte intercomm ibcast by using MPI_Datatype_null which
    // has 0 size. The actual count we pass in cannot be 0, otherwise,
    // it will be treated as a noop request.
    //
    mpi_errno = pRequest->nbc.tasks[1].InitIbcastInter(
        nullptr,
        1,
        g_hBuiltinTypes.MPI_Datatype_null,
        root1,
        pComm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        pRequest->cancel_nbc_tasklist();
        return mpi_errno;
    }

    pRequest->nbc.tasks[1].m_iNextOnInit = NBC_TASK_NONE;
    pRequest->nbc.tasks[1].m_iNextOnComplete = NBC_TASK_NONE;
    pRequest->nbc.nTasks++;

    //
    // Initiate 0-byte intercomm ibcast by using MPI_Datatype_null which
    // has 0 size. The actual count we pass in cannot be 0, otherwise,
    // it will be treated as a noop request.
    //
    mpi_errno = pRequest->nbc.tasks[2].InitIbcastInter(
        nullptr,
        1,
        g_hBuiltinTypes.MPI_Datatype_null,
        root2,
        pComm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        pRequest->cancel_nbc_tasklist();
        return mpi_errno;
    }

    pRequest->nbc.tasks[2].m_iNextOnInit = NBC_TASK_NONE;
    pRequest->nbc.tasks[2].m_iNextOnComplete = NBC_TASK_NONE;
    pRequest->nbc.nTasks++;

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibarrier_inter(
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IbarrierBuildInter( pComm, &pRequest );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    mpi_errno = pRequest->execute_nbc_tasklist();
    if( mpi_errno != MPI_SUCCESS )
    {
        pRequest->Release();
    }
    else
    {
        *ppRequest = pRequest;
    }

    return mpi_errno;
}
