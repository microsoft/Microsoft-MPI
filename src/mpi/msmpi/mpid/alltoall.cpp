// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* This is the default implementation of alltoall. The algorithm is:

   Algorithm: MPI_Alltoall

   We use four algorithms for alltoall. For short messages and
   (comm_size >= 8), we use the algorithm by Jehoshua Bruck et al,
   IEEE TPDS, Nov. 1997. It is a store-and-forward algorithm that
   takes lgp steps. Because of the extra communication, the bandwidth
   requirement is (n/2).lgp.beta.

   Cost = lgp.alpha + (n/2).lgp.beta

   where n is the total amount of data a process needs to send to all
   other processes.

   For medium size messages and (short messages for comm_size < 8), we
   use an algorithm that posts all irecvs and isends and then does a
   waitall. We scatter the order of sources and destinations among the
   processes, so that all processes don't try to send/recv to/from the
   same process at the same time.

   For long messages and power-of-two number of processes, we use a
   pairwise exchange algorithm, which takes p-1 steps. We
   calculate the pairs by using an exclusive-or algorithm:
           for (i=1; i<comm_size; i++)
               dest = rank ^ i;
   This algorithm doesn't work if the number of processes is not a power of
   two. For a non-power-of-two number of processes, we use an
   algorithm in which, in step i, each process  receives from (rank-i)
   and sends to (rank+i).

   Cost = (p-1).alpha + n.beta

   where n is the total amount of data a process needs to send to all
   other processes.

   Possible improvements:

   End Algorithm: MPI_Alltoall
*/
static
MPI_RESULT
IalltoallBuildMpiInPlaceTasks(
    _Inout_          MPID_Request* pReq,
    _In_             void*         recvbuf,
    _In_range_(>, 0) int           recvcount,
    _In_             size_t        cbBuffer,
    _In_             MPI_Count     extent,
    _In_             TypeHandle    recvtype,
    _In_             unsigned      dst,
    _In_             MPID_Comm*    pComm,
    _Outptr_         NbcTask**     ppTask
    )
{
    NbcTask* pTask = *ppTask;

    MPI_RESULT mpi_errno = pTask->InitIrecv(
        pReq->nbc.tmpBuf,
        cbBuffer,
        g_hBuiltinTypes.MPI_Byte,
        dst
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }
    pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
    pTask->m_iNextOnComplete = NBC_TASK_NONE;
    ++pTask;

    mpi_errno = pTask->InitIsend(
        static_cast<BYTE*>( recvbuf ) + extent * dst * recvcount,
        recvcount,
        recvtype,
        dst,
        pComm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    pTask->InitLocalCopy(
        pReq->nbc.tmpBuf,
        static_cast<BYTE*>( recvbuf ) + extent * dst * recvcount,
        static_cast<int>( cbBuffer ),
        recvcount,
        g_hBuiltinTypes.MPI_Byte,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    *ppTask = pTask;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallBuildMpiInPlaceTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_      void*            recvbuf,
    _In_range_(>, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
    )
{
    //
    // When user calls ialltoall with MPI_IN_PLACE, they either don't have
    // enough memory or they want to save the memory for something else.
    // We receive the data from rankX into a scratch buffer, send our data from
    // recvbuf to rankX and when the send finishes, we local copy the received
    // data from the scratch bufer to recvbuf. And then we reuse the scratch
    // buffer for the next round. This minimizes the memory usage and since
    // all the ranks send / receive in the same order, there won't be deadlocks.
    //
    MPI_RESULT mpi_errno;
    unsigned rank = pComm->rank;
    unsigned size = pComm->remote_size;

    pReq->nbc.tasks = new NbcTask[( size - 1 ) * 3];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    size_t cbBuffer = static_cast<size_t>( recvcount * recvtype.GetSize() );
    pReq->nbc.tmpBuf = new BYTE[cbBuffer];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Separate into two loops at rank so that we don't need to make sure
    // it's not the current rank at every iteration.
    //
    MPI_Count extent = recvtype.GetExtent();
    unsigned i;
    for( i = 0; i < rank; i++ )
    {
        mpi_errno = IalltoallBuildMpiInPlaceTasks(
            pReq,
            recvbuf,
            recvcount,
            cbBuffer,
            extent,
            recvtype,
            i,
            pComm,
            &pTask
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    for( ++i; i < size; i++ )
    {
        mpi_errno = IalltoallBuildMpiInPlaceTasks(
            pReq,
            recvbuf,
            recvcount,
            cbBuffer,
            extent,
            recvtype,
            i,
            pComm,
            &pTask
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
     }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallBuildBruckTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_      void*            recvbuf,
    _In_range_(>, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
    )
{
    //
    // Use the indexing algorithm by Jehoshua Bruck et al,
    // IEEE TPDS, Nov. 97
    //
    unsigned size = pComm->remote_size;
    ULONG iter;
    _BitScanReverse(&iter, size);
    if( !IsPowerOf2( size ) )
    {
        iter++;
    }

    unsigned taskCount = iter * 3 + size + 4;

    unsigned rank = pComm->rank;
    if( rank == 0 )
    {
        //
        // Only need 1 local copy (instead of 2) for phase 1 if it's rank 0.
        //
        taskCount--;
    }

    pReq->nbc.tasks = new NbcTask[taskCount];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    MPI_Count sendTypeExtent = sendtype.GetExtent();
    MPI_Count recvTypeExtent = recvtype.GetExtent();

    //
    // Do phase 1 of the algorithim. Shift the data blocks on process i
    // upwards by a distance of i blocks. Store the result in recvbuf.
    //
    pTask->InitLocalCopy(
        static_cast<const BYTE*>( sendbuf ) + sendTypeExtent * rank * sendcount,
        recvbuf,
        ( size - rank ) * sendcount,
        ( size - rank ) * recvcount,
        sendtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    if( rank != 0 )
    {
        pTask->InitLocalCopy(
            sendbuf,
            static_cast<BYTE*>( recvbuf ) + recvTypeExtent * ( size - rank ) * recvcount,
            rank * sendcount,
            rank * recvcount,
            sendtype,
            recvtype
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;
    }

    //
    // Do phase 2 of the algorithm. It takes ceiling(lg(size)) steps. In each
    // step i, each process sends to rank + 2^i and receives from rank - 2^i,
    // and exchanges all data blocks whose ith bit is 1.
    //
    MPI_Count recvTypeTrueLb;
    MPI_Count recvTypeTrueExtent = recvtype.GetTrueExtentAndLowerBound( &recvTypeTrueLb );
    MPI_Count recvTypeSize = recvtype.GetSize();
    if( recvTypeTrueExtent < recvTypeExtent )
    {
        recvTypeTrueExtent = recvTypeExtent;
    }
    MPI_Count recvBufExtent = recvTypeTrueExtent * recvcount * size;
    MPI_Count packSize = recvTypeSize * recvcount * size;
    if( recvBufExtent > INT_MAX || packSize > INT_MAX )
    {
        return MPIU_ERR_NOMEM();
    }
    if( packSize < recvBufExtent )
    {
        packSize = recvBufExtent;
    }

    pReq->nbc.tmpBuf = new BYTE[static_cast<size_t>(packSize)];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    StackGuardArray<int> displs = new int[size];
    if( displs == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    unsigned pof2 = 1;
    unsigned dst;
    unsigned src;
    unsigned count;
    unsigned block;
    MPI_Datatype tmpType;
    while( pof2 < size )
    {
        //
        // Need to exchange all data blocks whose ith bit is 1, create an
        // indexed datatype for this purpose.
        //
        count = 0;
        block = pof2;
        while( block < size )
        {
            if( block & pof2 )
            {
                displs[count++] = block * recvcount;
                block++;
            }
            else
            {
                block |= pof2;
            }
        }

        MPI_RESULT mpi_errno = NMPI_Type_create_indexed_block(
            count,
            recvcount,
            displs,
            recvtype.GetMpiHandle(),
            &tmpType
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        mpi_errno = NMPI_Type_commit( &tmpType );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        TypeHandle tmpTypeHandle = TypeHandle::Create( tmpType );

        mpi_errno = pTask->InitPack(
            pReq->nbc.tmpBuf,
            recvbuf,
            1,
            tmpTypeHandle
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        dst = pComm->RankAdd( pof2 );
        src = pComm->RankSub( pof2 );

        mpi_errno = pTask->InitIrecv(
            recvbuf,
            1,
            tmpTypeHandle,
            src,
            true
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        mpi_errno = pTask->InitIsend(
            pReq->nbc.tmpBuf,
            fixme_cast<size_t>( tmpTypeHandle.GetSize() ),
            g_hBuiltinTypes.MPI_Packed,
            dst,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        pof2 *= 2;
    }

    //
    // Do phase 3 of the algorithm. Rotate blocks in recvbuf upwards by
    // rank + 1 blocks into scratch buffer. Blocks in the scratch buffer are in
    // reverse order. Finally reorder them into recvbuf.
    //

    //
    // Adjust for potential negative lower bound in datatype.
    //
    BYTE* tmpBuf = pReq->nbc.tmpBuf - recvTypeTrueLb;

    pTask->InitLocalCopy(
        static_cast<BYTE*>( recvbuf ) + recvTypeExtent * ( rank + 1 ) * recvcount,
        tmpBuf,
        ( size - rank - 1 ) * recvcount,
        ( size - rank - 1 ) * recvcount,
        recvtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    pTask->InitLocalCopy(
        recvbuf,
        tmpBuf + recvTypeExtent * ( size - rank - 1 ) * recvcount,
        ( rank + 1 ) * recvcount,
        ( rank + 1 ) * recvcount,
        recvtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    for( unsigned i = 0; i < size; i++ )
    {
        pTask->InitLocalCopy(
            tmpBuf + recvTypeExtent * i * recvcount,
            static_cast<BYTE*>( recvbuf ) + recvTypeExtent * ( size - i - 1 ) * recvcount,
            recvcount,
            recvcount,
            recvtype,
            recvtype
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallBuildMediumMsgTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_      void*            recvbuf,
    _In_range_(>, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
    )
{
    //
    // Medium message size. Local copy rank's own data from sendbuf to recvbuf.
    // Post recvs for the rest size - 1 ranks and send to the rest size - 1
    // ranks in parallel.
    //
    unsigned rank = pComm->rank;
    unsigned size = pComm->remote_size;
    pReq->nbc.tasks = new NbcTask[1 + ( size - 1 ) * 2];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    MPI_Count sendTypeExtent = sendtype.GetExtent();
    MPI_Count recvTypeExtent = recvtype.GetExtent();

    pTask->InitLocalCopy(
        static_cast<const BYTE*>( sendbuf ) + sendTypeExtent * rank * sendcount,
        static_cast<BYTE*>( recvbuf ) + recvTypeExtent * rank * recvcount,
        sendcount,
        recvcount,
        sendtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    //
    // Issue all the recvs in parallel.
    //
    unsigned i;
    unsigned dst;
    MPI_RESULT mpi_errno;
    for( i = 1; i < size; i++ )
    {
        dst = pComm->RankAdd( i );

        mpi_errno = pTask->InitIrecv(
            static_cast<BYTE*>( recvbuf ) + recvTypeExtent * dst * recvcount,
            recvcount,
            recvtype,
            dst
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;
    }

    //
    // Then issue all the sends in parallel.
    //
    for( i = 1; i < size; i++ )
    {
        dst = pComm->RankAdd( i );

        mpi_errno = pTask->InitIsend(
            static_cast<const BYTE*>( sendbuf ) + sendTypeExtent * dst * sendcount,
            sendcount,
            sendtype,
            dst,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallBuildLongMsgTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_      void*            recvbuf,
    _In_range_(>, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
    )
{
    //
    // Long message size. If size is pof2, do a pairwise exchange using
    // exclusive-or to create pairs. Else send to rank + i, receive from rank - i.
    //
    unsigned rank = pComm->rank;
    unsigned size = pComm->remote_size;
    pReq->nbc.tasks = new NbcTask[1 + ( size - 1 ) * 2];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    MPI_Count sendTypeExtent = sendtype.GetExtent();
    MPI_Count recvTypeExtent = recvtype.GetExtent();

    pTask->InitLocalCopy(
        static_cast<const BYTE*>( sendbuf ) + sendTypeExtent * rank * sendcount,
        static_cast<BYTE*>( recvbuf ) + recvTypeExtent * rank * recvcount,
        sendcount,
        recvcount,
        sendtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    unsigned int MPIR_alltoall_pair_algo_msg_threshold = pComm->SwitchPoints()->MPIR_alltoall_pair_algo_msg_threshold;
    unsigned int MPIR_alltoall_pair_algo_procs_threshold = pComm->SwitchPoints()->MPIR_alltoall_pair_algo_procs_threshold;

    bool simplePairLogic = (size >= MPIR_alltoall_pair_algo_procs_threshold) && 
        (sendcount*sendTypeExtent >= static_cast<int>(MPIR_alltoall_pair_algo_msg_threshold));
    bool isPof2 = IsPowerOf2(size);

    unsigned int chunk = Mpi.SwitchoverSettings.RequestGroupSize;

    unsigned int src;
    unsigned int dst;
    MPI_RESULT mpi_errno;
    for( unsigned int i = 1; i < size; i++ )
    {
        if( isPof2 && !simplePairLogic)
        {
            src = dst = rank ^ i;
        }
        else
        {
            src = pComm->RankSub( i );
            dst = pComm->RankAdd( i );
        }

        mpi_errno = pTask->InitIrecv(
            static_cast<BYTE*>( recvbuf ) + recvTypeExtent * src * recvcount,
            recvcount,
            recvtype,
            src
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        mpi_errno = pTask->InitIsend(
            static_cast<const BYTE*>( sendbuf ) + sendTypeExtent * dst * sendcount,
            sendcount,
            sendtype,
            dst,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        if (i % chunk == 0)
        {
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        }
        else
        {
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
        }
        ++pTask;
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallBuildTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_      void*            recvbuf,
    _In_range_(>, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      unsigned int     tag
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag( tag );

    MPI_Count cbBuffer = sendtype.GetSize() * sendcount;
    int MPIR_alltoall_short_msg = pComm->SwitchPoints()->MPIR_alltoall_short_msg;
    int MPIR_alltoall_medium_msg = pComm->SwitchPoints()->MPIR_alltoall_medium_msg;
    MPI_RESULT mpi_errno;

    if( sendbuf == MPI_IN_PLACE )
    {
        //
        // Handle MPI_IN_PLACE case.
        //
        mpi_errno = IalltoallBuildMpiInPlaceTaskList(
            pReq,
            recvbuf,
            recvcount,
            recvtype,
            pComm
            );
    }
    else if (cbBuffer <= MPIR_alltoall_short_msg && 
        cbBuffer*pComm->remote_size / 2 <= MSMPI_MAX_TRANSFER_SIZE &&
        pComm->remote_size >= 8)
    {
        //
        // Short message and more than 8 ranks,
        // and we can guarantee that the aggregated message size is less than max message possible,
        // use Bruck algorithm.
        //
        mpi_errno = IalltoallBuildBruckTaskList(
            pReq,
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            pComm
            );
    }
    else if( cbBuffer <= MPIR_alltoall_medium_msg )
    {
        //
        // Medium message.
        //
        mpi_errno = IalltoallBuildMediumMsgTaskList(
            pReq,
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            pComm
            );
    }
    else
    {
        //
        // Long message.
        //
        mpi_errno = IalltoallBuildLongMsgTaskList(
            pReq,
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            pComm
            );
    }

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallBuildIntra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _When_(pComm->remote_size == 1 && sendbuf != MPI_IN_PLACE, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      unsigned int     tag,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    pRequest->comm = pComm;
    pRequest->comm->AddRef();

    MPI_RESULT mpi_errno;

    if( ( sendbuf != MPI_IN_PLACE && sendcount == 0 ) || recvcount == 0 )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
    }
    else if( pComm->remote_size == 1 )
    {
        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MPIR_Localcopy(
                sendbuf,
                sendcount,
                sendtype,
                recvbuf,
                recvcount,
                recvtype
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }

        pRequest->kind = MPID_REQUEST_NOOP;
    }
    else
    {
        mpi_errno = IalltoallBuildTaskList(
            pRequest.get(),
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            pComm,
            tag
            );
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
MPIR_Ialltoall_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      unsigned int     tag,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IalltoallBuildIntra(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcount,
        recvtype,
        pComm,
        tag,
        &pRequest
        );
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
IalltoallBuildInter(
    _In_opt_    const void*      sendbuf,
    _In_range_(>=, 0)
                int              sendcount,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_range_(>=, 0)
                int              recvcount,
    _In_        TypeHandle       recvtype,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    //
    // Intercommunicator ialltoall. We use a pairwise exchange algorithm similar
    // to the one used in intracommunicator ialltoall for long messages. Since
    // the local and remote groups can be of different sizes, we first compute
    // the max of local group size and remote group size.
    // At step i, 0 <= i < maxSize, each process receives from
    // src = (rank - i + maxSize) % maxSize if src < remoteSize, and sends to
    // dst = (rank + i) % maxSize if dst < remoteSize.
    //
    // For example, local group has 3 ranks (L0, L1 and L2) and remote group has
    // 4 ranks (R0, R1, R2, R3), maxSize is 4 here. The following is the
    // send/recv sequences for each step:
    // Step 1: L0<=>R0, L1<=>R1, L2<=>R2, NOOP<=>R3
    // Step 2: R1=>L2=>R3=>L0=>R1, NOOP=>R0=>L1=>R2=>NOOP
    // Step 3: L0<=>R2, L1<=>R3, L2<=>R0, NOOP<=>R1
    // Step 4: R1=>L0=>R3=>L2=>R1, NOOP=>R2=>L1=>R0=>NOOP
    //
    MPIU_Assert( pComm->comm_kind == MPID_INTERCOMM );
    MPIU_Assert( pComm->inter.local_comm != nullptr );

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pComm->AddRef();
    pRequest->nbc.tag = pComm->GetNextNBCTag(tag);

    unsigned remoteSize = pComm->remote_size;
    unsigned maxSize = ( static_cast<unsigned>( pComm->inter.local_size ) > remoteSize ) ?
        pComm->inter.local_size : remoteSize;

    pRequest->nbc.tasks = new NbcTask[maxSize * 2];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pRequest->nbc.tasks[0];

    MPI_Count sendTypeExtent = sendtype.GetExtent();
    MPI_Count recvTypeExtent = recvtype.GetExtent();

    MPI_RESULT mpi_errno;
    unsigned src = pComm->rank;
    unsigned dst = src;
    for( unsigned i = 0; i < maxSize; i++ )
    {
        if( src < remoteSize && recvbuf != nullptr && recvcount > 0 )
        {
            mpi_errno = pTask->InitIrecv(
                static_cast<BYTE*>( recvbuf ) + recvTypeExtent * src * recvcount,
                recvcount,
                recvtype,
                src
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            //
            // Create a noop task for the out of bound case.
            //
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = ++pRequest->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        if( dst < remoteSize && sendbuf != nullptr && sendcount > 0 )
        {
            mpi_errno = pTask->InitIsend(
                static_cast<const BYTE*>( sendbuf ) + sendTypeExtent * dst * sendcount,
                sendcount,
                sendtype,
                dst,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            //
            // Create a noop task for the out of bound case.
            //
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
        ++pTask;

        if( src == 0 )
        {
            src = maxSize;
        }
        --src;
        ++dst;
        if( dst == maxSize )
        {
            dst = 0;
        }
    }

    pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


static
MPI_RESULT
IalltoallvBuildMpiInPlaceTasks(
    _Inout_          MPID_Request* pReq,
    _In_             void*         recvbuf,
    _In_opt_         unsigned int  recvcnt,
    _In_opt_         unsigned int  rdispls,
    _In_             MPI_Count     extent,
    _In_             TypeHandle    recvtype,
    _In_             unsigned      dst,
    _In_             MPID_Comm*    pComm,
    _Outptr_         NbcTask**     ppTask
)
{
    NbcTask* pTask = *ppTask;

    MPI_RESULT mpi_errno = pTask->InitIrecv(
        pReq->nbc.tmpBuf,
        recvcnt,
        recvtype,
        dst
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }
    pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
    pTask->m_iNextOnComplete = NBC_TASK_NONE;
    ++pTask;

    mpi_errno = pTask->InitIsend(
        static_cast<BYTE*>(recvbuf)+rdispls * extent,
        recvcnt,
        recvtype,
        dst,
        pComm
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    pTask->InitLocalCopy(
        pReq->nbc.tmpBuf,
        static_cast<BYTE*>(recvbuf)+rdispls * extent,
        recvcnt,
        recvcnt,
        recvtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    *ppTask = pTask;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallvBuildMpiInPlaceTaskList(
    _Inout_  MPID_Request*    pReq,
    _In_     void*            recvbuf,
    _In_     const int*       recvcnts,
    _In_     const int*       rdispls,
    _In_     TypeHandle       recvtype,
    _In_     MPI_Count        maxMessageSize,
    _In_     MPID_Comm*       pComm,
    _In_     unsigned int     tag
    )
{
    //
    // When user calls ialltoallv with MPI_IN_PLACE, they either don't have
    // enough memory or they want to save the memory for something else.
    // We allocate the scratch buffer big enough to store the longest message.
    // We receive the data from rankX into a scratch buffer, send our data from
    // recvbuf to rankX and when the send finishes, we local copy the received
    // data from the scratch bufer to recvbuf. And then we reuse the scratch
    // buffer for the next round. This minimizes the memory usage and since
    // all the ranks send / receive in the same order, there won't be deadlocks.
    //
    MPI_RESULT mpi_errno;
    unsigned rank = pComm->rank;
    unsigned size = pComm->remote_size;

    pReq->nbc.tag = pComm->GetNextNBCTag(tag);

    pReq->nbc.tasks = new NbcTask[(size - 1) * 3];
    if (pReq->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    size_t cbBuffer = static_cast<size_t>(maxMessageSize);
    pReq->nbc.tmpBuf = new BYTE[cbBuffer];
    if (pReq->nbc.tmpBuf == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Separate into two loops at rank so that we don't need to make sure
    // it's not the current rank at every iteration.
    //
    MPI_Count extent = recvtype.GetExtent();
    unsigned i;
    for (i = 0; i < rank; i++)
    {
        mpi_errno = IalltoallvBuildMpiInPlaceTasks(
            pReq,
            recvbuf,
            recvcnts[i],
            rdispls[i],
            extent,
            recvtype,
            i,
            pComm,
            &pTask
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
    }

    for (++i; i < size; i++)
    {
        mpi_errno = IalltoallvBuildMpiInPlaceTasks(
            pReq,
            recvbuf,
            recvcnts[i],
            rdispls[i],
            extent,
            recvtype,
            i,
            pComm,
            &pTask
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
IalltoallvBuildInter(
    _In_opt_    const void*      sendbuf,
    _In_opt_    const int*       sendcnts,
    _In_opt_    const int*       sdispls,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_opt_    const int*       recvcnts,
    _In_opt_    const int*       rdispls,
    _In_        TypeHandle       recvtype,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    //
    // Intercommunicator ialltoallv. We use a pairwise exchange algorithm similar
    // to the one used in intracommunicator ialltoallv. Since
    // the local and remote groups can be of different sizes, we first compute
    // the max of local group size and remote group size.
    // At step i, 0 <= i < maxSize, each process receives from
    // src = (rank - i + maxSize) % maxSize if src < remoteSize, and sends to
    // dst = (rank + i) % maxSize if dst < remoteSize.
    //
    MPIU_Assert(pComm->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(pComm->inter.local_comm != nullptr);

    StackGuardRef<MPID_Request> pRequest(MPID_Request_create(MPID_REQUEST_NBC));
    if (pRequest == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pComm->AddRef();

    //
    // All processes must get the tag so that we remain in sync.
    //
    pRequest->nbc.tag = pComm->GetNextNBCTag(tag);

    unsigned remoteSize = pComm->remote_size;
    unsigned maxSize = (static_cast<unsigned>(pComm->inter.local_size) > remoteSize) ?
        pComm->inter.local_size : remoteSize;

    pRequest->nbc.tasks = new NbcTask[maxSize * 2];
    if (pRequest->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pRequest->nbc.tasks[0];

    MPI_Count sendTypeExtent = sendtype.GetExtent();
    MPI_Count recvTypeExtent = recvtype.GetExtent();

    MPI_RESULT mpi_errno;
    unsigned src = pComm->rank;
    unsigned dst = src;
    for (unsigned i = 0; i < maxSize; i++)
    {
        if (src < remoteSize && recvbuf != nullptr && recvcnts[src] > 0)
        {
            mpi_errno = pTask->InitIrecv(
                static_cast<BYTE*>(recvbuf)+rdispls[src] * recvTypeExtent,
                recvcnts[src],
                recvtype,
                src
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
        else
        {
            //
            // Create a noop task for the out of bound case.
            //
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = ++pRequest->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        if (dst < remoteSize && sendbuf != nullptr && sendcnts[dst] > 0)
        {
            mpi_errno = pTask->InitIsend(
                static_cast<const BYTE*>( sendbuf ) + sdispls[dst] * sendTypeExtent,
                sendcnts[dst],
                sendtype,
                dst,
                pComm
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
        else
        {
            //
            // Create a noop task for the out of bound case.
            //
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
        ++pTask;

        if (src == 0)
        {
            src = maxSize;
        }
        --src;
        ++dst;
        if (dst == maxSize)
        {
            dst = 0;
        }
    }

    pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoall_inter(
    _In_opt_    const void*      sendbuf,
    _In_range_(>=, 0)
                int              sendcount,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_range_(>=, 0)
                int              recvcount,
    _In_        TypeHandle       recvtype,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IalltoallBuildInter(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcount,
        recvtype,
        pComm,
        tag,
        &pRequest
        );
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


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallvBuildTaskList(
    _Inout_     MPID_Request*    pReq,
    _In_opt_    const void*      sendbuf,
    _In_        const int*       sendcnts,
    _In_        const int*       sdispls,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_        const int*       recvcnts,
    _In_        const int*       rdispls,
    _In_        TypeHandle       recvtype,
    _In_        MPI_Count        maxMessageSize,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag(tag);
    //
    // If size is pof2, do a pairwise exchange using
    // exclusive-or to create pairs. Else send to rank + i, receive from rank - i.
    //
    unsigned rank = pComm->rank;
    unsigned size = pComm->remote_size;
    pReq->nbc.tasks = new NbcTask[1 + (size - 1) * 2];
    if (pReq->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    int MPIR_alltoall_pair_algo_msg_threshold = pComm->SwitchPoints()->MPIR_alltoall_pair_algo_msg_threshold;
    unsigned int MPIR_alltoall_pair_algo_procs_threshold = pComm->SwitchPoints()->MPIR_alltoall_pair_algo_procs_threshold;

    bool simplePairLogic = (size >= MPIR_alltoall_pair_algo_procs_threshold) && 
        (maxMessageSize >= MPIR_alltoall_pair_algo_msg_threshold);

    MPI_Count sendTypeExtent = sendtype.GetExtent();
    MPI_Count recvTypeExtent = recvtype.GetExtent();

    pTask->InitLocalCopy(
        static_cast<const BYTE*>(sendbuf) +sdispls[rank] * sendTypeExtent,
        static_cast<BYTE*>(recvbuf)+rdispls[rank] * recvTypeExtent,
        sendcnts[rank],
        recvcnts[rank],
        sendtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    unsigned int chunk = Mpi.SwitchoverSettings.RequestGroupSize;

    bool isPof2 = IsPowerOf2(size);
    unsigned int src;
    unsigned int dst;
    for (unsigned int i = 1; i < size; i++)
    {
        if (isPof2 && !simplePairLogic)
        {
            src = dst = rank ^ i;
        }
        else
        {
            src = pComm->RankSub(i);
            dst = pComm->RankAdd(i);
        }

        MPI_RESULT mpi_errno = pTask->InitIrecv(
            static_cast<BYTE*>(recvbuf)+rdispls[src] * recvTypeExtent,
            recvcnts[src],
            recvtype,
            src
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        mpi_errno = pTask->InitIsend(
            static_cast<const BYTE*>( sendbuf ) + sdispls[dst] * sendTypeExtent,
            sendcnts[dst],
            sendtype,
            dst,
            pComm
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }

        if (i % chunk == 0)
        {
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        }
        else
        {
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
        }
        ++pTask;
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallvBuildBruckTaskList(
    _Inout_     MPID_Request*    pReq,
    _In_opt_    const void*      sendbuf,
    _In_        const int*       sendcnts,
    _In_        const int*       sdispls,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_        const int*       recvcnts,
    _In_        const int*       rdispls,
    _In_        TypeHandle       recvtype,
    _In_        MPI_Count        maxMessageSize,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag(tag);

    unsigned int rank = pComm->rank;
    unsigned int size = pComm->remote_size;

    ULONG numIters;
    _BitScanReverse(&numIters, size);
    if (!IsPowerOf2(size))
    {
        numIters++;
    }

    pReq->nbc.tasks = new NbcTask[2*size + 3*numIters];
    if (pReq->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    MPI_Count sendTypeExtent = sendtype.GetExtent();
    MPI_Count recvTypeExtent = recvtype.GetExtent();

    //
    // Allocate scrap buffer to hold temporary blocks on the process
    // and receive the packed blocks from the other processes
    //
    size_t cbBuffer = static_cast<size_t>(maxMessageSize * (size + (size / 2)));

    pReq->nbc.tmpBuf = new BYTE[cbBuffer];
    if (pReq->nbc.tmpBuf == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Assign some helpful pointers inside pReq->nbc.tmpBuf
    // tempbuf consists of size sections, maxMessageSize bytes each. 
    // tempbuf is used to store individual blocks that are residing in this process on this stage.
    // stageSendBuf is used to store packed message for sending
    //
    BYTE* tempbuf = pReq->nbc.tmpBuf;
    BYTE* stageSendBuf = pReq->nbc.tmpBuf + maxMessageSize*size;

    unsigned dst;
    unsigned src;

    //
    // Do phase 1 of the algorithim. Shift the data blocks on process i
    // upwards by a distance of i blocks. Store the result in individual 
    // sections of tempbuf.
    //
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    for (unsigned int i = 0; i < size; i++)
    {
        dst = (i - rank + size) % size;
        pTask->InitLocalCopy(
            static_cast<const BYTE*>(sendbuf) + sdispls[i] * sendTypeExtent,
            tempbuf + dst*maxMessageSize,
            sendcnts[i],
            sendcnts[i]*fixme_cast<unsigned int>(sendtype.GetSize()),
            sendtype,
            g_hBuiltinTypes.MPI_Byte
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;
    }


    //
    // Do phase 2 of the algorithm. It takes ceiling(lg(size)) steps. In each
    // step i, each process sends to (rank + 2^i) and receives from (rank - 2^i),
    // and exchanges all data blocks whose ith bit is 1.
    //
    MPI_Datatype tmpType;
    unsigned pof2 = 1;

    StackGuardArray<int> displs = new int[size];
    if (displs == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    while (pof2 < size)
    {
        dst = pComm->RankAdd(pof2);
        src = pComm->RankSub(pof2);

        unsigned int count = 0;
        unsigned int block = pof2;
        while (block < size)
        {
            if (block & pof2)
            {
                if (maxMessageSize * block > INT_MAX)
                {
                    return MPIU_ERR_CREATE(MPI_ERR_COUNT, "**intoverflow %l", maxMessageSize * block);
                }
                displs[count++] = fixme_cast<int>( maxMessageSize * block );
                block++;
            }
            else
            {
                block |= pof2;
            }
        }

        //
        // Bruck algorithm might be used only if messages are small.
        // Using static cast is guaranteed to not lose the data.
        //
        mpi_errno = NMPI_Type_create_indexed_block(
            count,
            static_cast<int>(maxMessageSize),
            displs,
            MPI_BYTE,
            &tmpType
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        mpi_errno = NMPI_Type_commit(&tmpType);
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        TypeHandle tmpTypeHandle = TypeHandle::Create(tmpType);

        mpi_errno = pTask->InitPack(
            stageSendBuf,
            tempbuf,
            1,
            tmpTypeHandle
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        mpi_errno = pTask->InitIrecv(
            tempbuf,
            1,
            tmpTypeHandle,
            src,
            true
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        mpi_errno = pTask->InitIsend(
            stageSendBuf,
            fixme_cast<size_t>(tmpTypeHandle.GetSize()),
            g_hBuiltinTypes.MPI_Packed,
            dst,
            pComm
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        pof2 *= 2;
    }

    //
    // Do phase 3 of the algorithm. Local reverse and shift towards index (p-1)
    // by (i+1) indexes. R[j] is moved to R[(p-j+i)%p]
    // Copy the blocks from tempbuf to recvbuf in described order.
    //
    for (unsigned int i = 0; i < size; i++)
    {
        dst = (size - i + rank) % size;

        pTask->InitLocalCopy(
            tempbuf + i*maxMessageSize,
            static_cast<BYTE*>(recvbuf)+rdispls[dst] * recvTypeExtent,
            recvcnts[dst] * fixme_cast<unsigned int>(recvtype.GetSize()),
            recvcnts[dst],
            g_hBuiltinTypes.MPI_Byte,
            recvtype
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IalltoallvBuildIntra(
    _In_opt_    const void*      sendbuf,
    _In_        const int*       sendcnts,
    _In_        const int*       sdispls,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_        const int*       recvcnts,
    _In_        const int*       rdispls,
    _In_        TypeHandle       recvtype,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    MPIU_Assert(pComm->comm_kind != MPID_INTERCOMM);

    StackGuardRef<MPID_Request> pRequest(MPID_Request_create(MPID_REQUEST_NBC));
    if (pRequest == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    pRequest->comm = pComm;
    pRequest->comm->AddRef();

    MPI_RESULT mpi_errno;

    if (sendbuf == MPI_IN_PLACE)
    {
        if (pComm->remote_size > 1)
        {
            //
            // Handle MPI_IN_PLACE case.
            //
            MPI_Count maxMessageSize = MaxElement(recvcnts, pComm->remote_size) * recvtype.GetExtent();

            mpi_errno = IalltoallvBuildMpiInPlaceTaskList(
                pRequest.get(),
                recvbuf,
                recvcnts,
                rdispls,
                recvtype,
                maxMessageSize,
                pComm,
                tag
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                pRequest->cancel_nbc_tasklist();
                return mpi_errno;
            }
        }
        else
            pRequest->kind = MPID_REQUEST_NOOP;
    }
    else if (pComm->remote_size == 1)
    {
        MPI_Count sendExtent = sendtype.GetExtent();
        MPI_Count recvExtent = recvtype.GetExtent();
        mpi_errno = MPIR_Localcopy(
            static_cast<const BYTE*>(sendbuf) + sdispls[0] * sendExtent,
            sendcnts[0],
            sendtype,
            static_cast<BYTE*>(recvbuf) + rdispls[0] * recvExtent,
            recvcnts[0],
            recvtype
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pRequest->kind = MPID_REQUEST_NOOP;
    }
    else
    {
        int          comm_size = pComm->remote_size;
        bool         useBruck = false;
        MPI_Count     globalMaxMessageSize = 0;
        int MPIR_alltoallv_short_msg = pComm->SwitchPoints()->MPIR_alltoallv_short_msg;

        //
        // make a decision about which algorithm to use, 
        // Bruck algorithm shows better results for short messages and bigger number of processes
        //
        if (comm_size >= 8)
        {
            MPI_Count localMaxMessageSize = MaxElement(sendcnts, comm_size) * sendtype.GetSize();

            mpi_errno = MPIR_Allreduce_intra(
                reinterpret_cast<BYTE*>(&localMaxMessageSize),
                reinterpret_cast<BYTE*>(&globalMaxMessageSize),
                1,
                g_hBuiltinTypes.MPI_Long_long,
                OpPool::Lookup(MPI_MAX),
                pComm
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }

            useBruck = (globalMaxMessageSize <= MPIR_alltoallv_short_msg) &&
                (globalMaxMessageSize * comm_size / 2 <= MSMPI_MAX_TRANSFER_SIZE);
        }

        if (useBruck)
        {
            mpi_errno = IalltoallvBuildBruckTaskList(
                pRequest.get(),
                sendbuf,
                sendcnts,
                sdispls,
                sendtype,
                recvbuf,
                recvcnts,
                rdispls,
                recvtype,
                globalMaxMessageSize,
                pComm,
                tag
                );
        }
        else
        {
            mpi_errno = IalltoallvBuildTaskList(
                pRequest.get(),
                sendbuf,
                sendcnts,
                sdispls,
                sendtype,
                recvbuf,
                recvcnts,
                rdispls,
                recvtype,
                globalMaxMessageSize,
                pComm,
                tag
                );
        }
        if (mpi_errno != MPI_SUCCESS)
        {
            pRequest->cancel_nbc_tasklist();
            return mpi_errno;
        }
    }

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


/* This is the default implementation of alltoallv. The algorithm is:

   Algorithm: MPI_Alltoallv

   For small number of processes or big message sizes we simply use
   the "middle of the road" isend/irecv algorithm that works
   reasonably well in all cases.

   We post all irecvs and isends and then do a waitall. We scatter the
   order of sources and destinations among the processes, so that all
   processes don't try to send/recv to/from the same process at the
   same time.

   For big number of processes and small message sizes we use the 
   modification of Bruck algorithm. Since each process sends/receives 
   different amounts of data to every other process, we don't know 
   the total message size for allprocesses without additional 
   communication. Therefore we perform allreduce operation to find 
   out the maximum message size. After that we allocate big enough
   scrap buffer for intermediate communication and proceed with Bruck
   algorithm.

   Possible improvements:

   End Algorithm: MPI_Alltoallv
*/
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallv_intra(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        TypeHandle         sendtype,
    _Inout_opt_ void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        TypeHandle         recvtype,
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IalltoallvBuildIntra(
        sendbuf,
        sendcnts,
        sdispls,
        sendtype,
        recvbuf,
        recvcnts,
        rdispls,
        recvtype,
        const_cast<MPID_Comm*>(comm_ptr),
        tag,
        &pRequest
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    mpi_errno = pRequest->execute();
    if (mpi_errno != MPI_SUCCESS)
    {
        pRequest->Release();
    }
    else
    {
        *ppRequest = pRequest;
    }

    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallv_inter(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        TypeHandle         sendtype,
    _Inout_opt_ void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        TypeHandle         recvtype,
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IalltoallvBuildInter(
        sendbuf,
        sendcnts,
        sdispls,
        sendtype,
        recvbuf,
        recvcnts,
        rdispls,
        recvtype,
        const_cast<MPID_Comm*>(comm_ptr),
        tag,
        &pRequest
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    mpi_errno = pRequest->execute();
    if (mpi_errno != MPI_SUCCESS)
    {
        pRequest->Release();
    }
    else
    {
        *ppRequest = pRequest;
    }

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallwBuildTaskList(
    _Inout_     MPID_Request*    pReq,
    _In_opt_    const void*      sendbuf,
    _In_        const int        sendcnts[],
    _In_        const int        sdispls[],
    _In_        const TypeHandle sendtypes[],
    _Inout_opt_ void*            recvbuf,
    _In_        const int        recvcnts[],
    _In_        const int        rdispls[],
    _In_        const TypeHandle recvtypes[],
    _In_        MPI_Count        maxMessageSize,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag(tag);

    unsigned rank = pComm->rank;
    unsigned int size = pComm->remote_size;
    unsigned int src, dst;
    bool isPof2 = IsPowerOf2( size );

    int MPIR_alltoall_pair_algo_msg_threshold = pComm->SwitchPoints()->MPIR_alltoall_pair_algo_msg_threshold;
    unsigned int MPIR_alltoall_pair_algo_procs_threshold = pComm->SwitchPoints()->MPIR_alltoall_pair_algo_procs_threshold;
    
    bool simplePairLogic = (size >= MPIR_alltoall_pair_algo_procs_threshold) && 
        (maxMessageSize >= MPIR_alltoall_pair_algo_msg_threshold);

    MPI_RESULT mpi_errno = MPI_SUCCESS;

    pReq->nbc.tasks = new NbcTask[2*(size-1) + 1];
    
    if (pReq->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    pTask->InitLocalCopy(
        static_cast<const BYTE*>(sendbuf) + sdispls[rank],
        static_cast<BYTE*>(recvbuf)+rdispls[rank],
        sendcnts[rank],
        recvcnts[rank],
        sendtypes[rank],
        recvtypes[rank]
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    unsigned int chunk = Mpi.SwitchoverSettings.RequestGroupSize;

    for (unsigned int i = 1; i<size; i++)
    {
        if (isPof2 && !simplePairLogic)
        {
            src = dst = rank ^ i;
        }
        else
        {
            src = pComm->RankSub(i);
            dst = pComm->RankAdd(i);
        }

        mpi_errno = pTask->InitIrecv(
                static_cast<BYTE*>(recvbuf)+rdispls[src],
                recvcnts[src],
                recvtypes[src],
                src
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            mpi_errno = pTask->InitIsend(
                static_cast<const BYTE*>( sendbuf ) + sdispls[dst],
                sendcnts[dst],
                sendtypes[dst],
                dst,
                pComm
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
            if (i % chunk == 0)
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;

            }
            else
            {
                pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
            }
            ++pTask;
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


static
MPI_RESULT
IalltoallwBuildMpiInPlaceTasks(
    _Inout_          MPID_Request* pReq,
    _In_             void*         recvbuf,
    _In_opt_         unsigned int  recvcnt,
    _In_opt_         unsigned int  rdispls,
    _In_             TypeHandle    recvtype,
    _In_             unsigned      dst,
    _In_             MPID_Comm*    pComm,
    _Outptr_         NbcTask**     ppTask
    )
{
    NbcTask* pTask = *ppTask;

    MPI_RESULT mpi_errno = pTask->InitIrecv(
        pReq->nbc.tmpBuf,
        recvcnt,
        recvtype,
        dst
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }
    pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
    pTask->m_iNextOnComplete = NBC_TASK_NONE;
    ++pTask;

    mpi_errno = pTask->InitIsend(
        static_cast<BYTE*>(recvbuf) + rdispls,
        recvcnt,
        recvtype,
        dst,
        pComm
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    pTask->InitLocalCopy(
        pReq->nbc.tmpBuf,
        static_cast<BYTE*>(recvbuf)+rdispls,
        recvcnt,
        recvcnt,
        recvtype,
        recvtype
        );
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
    ++pTask;

    *ppTask = pTask;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallwBuildMpiInPlaceTaskList(
    _Inout_  MPID_Request*    pReq,
    _In_     void*            recvbuf,
    _In_     const int        recvcnts[],
    _In_     const int        rdispls[],
    _In_     const TypeHandle recvtypes[],
    _In_     MPI_Count        maxMessageSize,
    _In_     MPID_Comm*       pComm,
    _In_     unsigned int     tag
    )
{
    //
    // When user calls ialltoallw with MPI_IN_PLACE, they either don't have
    // enough memory or they want to save the memory for something else.
    // We allocate the scratch buffer big enough to store the longest message.
    // We receive the data from rankX into a scratch buffer, send our data from
    // recvbuf to rankX and when the send finishes, we local copy the received
    // data from the scratch bufer to recvbuf. And then we reuse the scratch
    // buffer for the next round. This minimizes the memory usage and since
    // all the ranks send / receive in the same order, there won't be deadlocks.
    //
    MPI_RESULT mpi_errno;
    unsigned rank = pComm->rank;
    unsigned size = pComm->remote_size;

    pReq->nbc.tag = pComm->GetNextNBCTag(tag);

    pReq->nbc.tasks = new NbcTask[(size - 1) * 3];
    if (pReq->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    size_t cbBuffer = static_cast<size_t>(maxMessageSize);
    pReq->nbc.tmpBuf = new BYTE[cbBuffer];
    if (pReq->nbc.tmpBuf == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Separate into two loops at rank so that we don't need to make sure
    // it's not the current rank at every iteration.
    //
    unsigned i;
    for (i = 0; i < rank; i++)
    {
        mpi_errno = IalltoallwBuildMpiInPlaceTasks(
            pReq,
            recvbuf,
            recvcnts[i],
            rdispls[i],
            recvtypes[i],
            i,
            pComm,
            &pTask
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
    }

    for (++i; i < size; i++)
    {
        mpi_errno = IalltoallwBuildMpiInPlaceTasks(
            pReq,
            recvbuf,
            recvcnts[i],
            rdispls[i],
            recvtypes[i],
            i,
            pComm,
            &pTask
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IalltoallwBuildBruckTaskList(
    _Inout_     MPID_Request*    pReq,
    _In_opt_    const void*      sendbuf,
    _In_        const int        sendcnts[],
    _In_        const int        sdispls[],
    _In_        const TypeHandle sendtypes[],
    _Inout_opt_ void*            recvbuf,
    _In_        const int        recvcnts[],
    _In_        const int        rdispls[],
    _In_        const TypeHandle recvtypes[],
    _In_        MPI_Count        maxMessageSize,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag(tag);

    unsigned int rank = pComm->rank;
    unsigned int size = pComm->remote_size;

    ULONG numIters;
    _BitScanReverse(&numIters, size);
    if (!IsPowerOf2(size))
    {
        numIters++;
    }

    pReq->nbc.tasks = new NbcTask[2 * size + 3 * numIters];
    if (pReq->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    //
    // Allocate scrap buffer to hold temporary blocks on the process
    // and receive the packed blocks from the other processes
    //
    size_t cbBuffer = static_cast<size_t>(maxMessageSize * (size + (size / 2)));

    pReq->nbc.tmpBuf = new BYTE[cbBuffer];
    if (pReq->nbc.tmpBuf == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Assign some helpful pointers inside pReq->nbc.tmpBuf
    // tempbuf consists of size sections, maxMessageSize bytes each. 
    // tempbuf is used to store individual blocks that are residing in this process on this stage.
    // stageSendBuf is used to store packed message for sending
    //
    BYTE* tempbuf = pReq->nbc.tmpBuf;
    BYTE* stageSendBuf = pReq->nbc.tmpBuf + maxMessageSize*size;

    unsigned dst;
    unsigned src;

    //
    // Do phase 1 of the algorithim. Shift the data blocks on process i
    // upwards by a distance of i blocks. Store the result in individual 
    // sections of tempbuf.
    //
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    for (unsigned int i = 0; i < size; i++)
    {
        dst = (i - rank + size) % size;
        pTask->InitLocalCopy(
            static_cast<const BYTE*>(sendbuf) + sdispls[i],
            tempbuf + dst*maxMessageSize,
            sendcnts[i],
            sendcnts[i] * fixme_cast<unsigned int>(sendtypes[i].GetSize()),
            sendtypes[i],
            g_hBuiltinTypes.MPI_Byte
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;
    }


    //
    // Do phase 2 of the algorithm. It takes ceiling(lg(size)) steps. In each
    // step i, each process sends to (rank + 2^i) and receives from (rank - 2^i),
    // and exchanges all data blocks whose ith bit is 1.
    //
    MPI_Datatype tmpType;
    unsigned pof2 = 1;

    StackGuardArray<int> displs = new int[size];
    if (displs == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    while (pof2 < size)
    {
        dst = pComm->RankAdd(pof2);
        src = pComm->RankSub(pof2);

        unsigned int count = 0;
        unsigned int block = pof2;
        while (block < size)
        {
            if (block & pof2)
            {
                if (maxMessageSize * block > INT_MAX)
                {
                    return MPIU_ERR_CREATE(MPI_ERR_COUNT, "**intoverflow %l", maxMessageSize * block);
                }
                displs[count++] = fixme_cast<int>(maxMessageSize * block);
                block++;
            }
            else
            {
                block |= pof2;
            }
        }

        mpi_errno = NMPI_Type_create_indexed_block(
            count,
            fixme_cast<int>(maxMessageSize),
            displs,
            MPI_BYTE,
            &tmpType
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        mpi_errno = NMPI_Type_commit(&tmpType);
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        TypeHandle tmpTypeHandle = TypeHandle::Create(tmpType);

        mpi_errno = pTask->InitPack(
            stageSendBuf,
            tempbuf,
            1,
            tmpTypeHandle
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        mpi_errno = pTask->InitIrecv(
            tempbuf,
            1,
            tmpTypeHandle,
            src,
            true
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        mpi_errno = pTask->InitIsend(
            stageSendBuf,
            fixme_cast<size_t>(tmpTypeHandle.GetSize()),
            g_hBuiltinTypes.MPI_Packed,
            dst,
            pComm
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        pof2 *= 2;
    }

    //
    // Do phase 3 of the algorithm. Local reverse and shift towards index (p-1)
    // by (i+1) indexes. R[j] is moved to R[(p-j+i)%p]
    // Copy the blocks from tempbuf to recvbuf in described order.
    //
    for (unsigned int i = 0; i < size; i++)
    {
        dst = (size - i + rank) % size;

        pTask->InitLocalCopy(
            tempbuf + i*maxMessageSize,
            static_cast<BYTE*>(recvbuf)+rdispls[dst],
            recvcnts[dst] * fixme_cast<unsigned int>(recvtypes[dst].GetSize()),
            recvcnts[dst],
            g_hBuiltinTypes.MPI_Byte,
            recvtypes[dst]
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;
    }

    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IalltoallwBuildIntra(
    _In_opt_    const void*      sendbuf,
    _In_        const int        sendcnts[],
    _In_        const int        sdispls[],
    _In_        const TypeHandle sendtypes[],
    _Inout_opt_ void*            recvbuf,
    _In_        const int        recvcnts[],
    _In_        const int        rdispls[],
    _In_        const TypeHandle recvtypes[],
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    MPIU_Assert(pComm->comm_kind != MPID_INTERCOMM);

    StackGuardRef<MPID_Request> pRequest(MPID_Request_create(MPID_REQUEST_NBC));
    if (pRequest == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    pRequest->comm = pComm;
    pRequest->comm->AddRef();

    MPI_RESULT mpi_errno;

    MPI_Count maxMessageSize = 0;
    MPI_Count messageSize;

    for (int i = 0; i < pComm->remote_size; ++i)
    {
        messageSize = recvcnts[i] * recvtypes[i].GetExtent();
        if (messageSize > maxMessageSize)
        {
            maxMessageSize = messageSize;
        }
    }

    if (sendbuf == MPI_IN_PLACE)
    {
        if (pComm->remote_size > 1)
        {
            //
            // Handle MPI_IN_PLACE case.
            //
            mpi_errno = IalltoallwBuildMpiInPlaceTaskList(
                pRequest.get(),
                recvbuf,
                recvcnts,
                rdispls,
                recvtypes,
                maxMessageSize,
                pComm,
                tag
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                pRequest->cancel_nbc_tasklist();
                return mpi_errno;
            }
        }
        else
            pRequest->kind = MPID_REQUEST_NOOP;
    }
    else if (pComm->remote_size == 1)
    {
        if (sendcnts[0] > 0 && recvcnts[0] > 0)
        {
            mpi_errno = MPIR_Localcopy(
                static_cast<const BYTE*>(sendbuf) + sdispls[0],
                sendcnts[0],
                sendtypes[0],
                static_cast<BYTE*>(recvbuf) + rdispls[0],
                recvcnts[0],
                recvtypes[0]
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
        pRequest->kind = MPID_REQUEST_NOOP;
    }
    else
    {
        int          comm_size = pComm->remote_size;
        bool         useBruck = false;
        MPI_Count     globalMaxMessageSize = 0;
        int MPIR_alltoallv_short_msg = pComm->SwitchPoints()->MPIR_alltoallv_short_msg;

        //
        // make a decision about which algorithm to use, 
        // Bruck algorithm shows better results for short messages and  bigger number of processes
        //
        if (comm_size >= 8)
        {
            mpi_errno = MPIR_Allreduce_intra(
                reinterpret_cast<BYTE*>(&maxMessageSize),
                reinterpret_cast<BYTE*>(&globalMaxMessageSize),
                1,
                g_hBuiltinTypes.MPI_Long_long,
                OpPool::Lookup(MPI_MAX),
                pComm
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }

            useBruck = (globalMaxMessageSize <= MPIR_alltoallv_short_msg) &&
                (globalMaxMessageSize * comm_size / 2 <= MSMPI_MAX_TRANSFER_SIZE);
        }

        if (useBruck)
        {
            mpi_errno = IalltoallwBuildBruckTaskList(
                pRequest.get(),
                sendbuf,
                sendcnts,
                sdispls,
                sendtypes,
                recvbuf,
                recvcnts,
                rdispls,
                recvtypes,
                globalMaxMessageSize,
                pComm,
                tag
                );
        }
        else
        {
            mpi_errno = IalltoallwBuildTaskList(
                pRequest.get(),
                sendbuf,
                sendcnts,
                sdispls,
                sendtypes,
                recvbuf,
                recvcnts,
                rdispls,
                recvtypes,
                globalMaxMessageSize,
                pComm,
                tag
                );
        }

        if (mpi_errno != MPI_SUCCESS)
        {
            pRequest->cancel_nbc_tasklist();
            return mpi_errno;
        }
    }

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
IalltoallwBuildInter(
    _In_opt_    const void*      sendbuf,
    _In_opt_    const int        sendcnts[],
    _In_opt_    const int        sdispls[],
    _In_        const TypeHandle sendtypes[],
    _Inout_opt_ void*            recvbuf,
    _In_opt_    const int        recvcnts[],
    _In_opt_    const int        rdispls[],
    _In_        const TypeHandle recvtypes[],
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    //
    // Intercommunicator ialltoallw. We use a pairwise exchange algorithm similar
    // to the one used in intercommunicator ialltoallv. Since
    // the local and remote groups can be of different sizes, we first compute
    // the max of local group size and remote group size.
    // At step i, 0 <= i < maxSize, each process receives from
    // src = (rank - i + maxSize) % maxSize if src < remoteSize, and sends to
    // dst = (rank + i) % maxSize if dst < remoteSize.
    //
    MPIU_Assert(pComm->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(pComm->inter.local_comm != nullptr);

    StackGuardRef<MPID_Request> pRequest(MPID_Request_create(MPID_REQUEST_NBC));
    if (pRequest == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pComm->AddRef();

    //
    // All processes must get the tag so that we remain in sync.
    //
    pRequest->nbc.tag = pComm->GetNextNBCTag(tag);

    unsigned remoteSize = pComm->remote_size;
    unsigned maxSize = (static_cast<unsigned>(pComm->inter.local_size) > remoteSize) ?
        pComm->inter.local_size : remoteSize;

    pRequest->nbc.tasks = new NbcTask[maxSize * 2];
    if (pRequest->nbc.tasks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pRequest->nbc.tasks[0];


    MPI_RESULT mpi_errno;
    unsigned src = pComm->rank;
    unsigned dst = src;
    for (unsigned i = 0; i < maxSize; i++)
    {
        if (src < remoteSize && recvbuf != nullptr && recvcnts[src] > 0)
        {
            mpi_errno = pTask->InitIrecv(
                static_cast<BYTE*>(recvbuf)+rdispls[src],
                recvcnts[src],
                recvtypes[src],
                src
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
        else
        {
            //
            // Create a noop task for the out of bound case.
            //
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = ++pRequest->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        if (dst < remoteSize && sendbuf != nullptr && sendcnts[dst] > 0)
        {
            mpi_errno = pTask->InitIsend(
                static_cast<const BYTE*>( sendbuf ) + sdispls[dst],
                sendcnts[dst],
                sendtypes[dst],
                dst,
                pComm
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
        else
        {
            //
            // Create a noop task for the out of bound case.
            //
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
        ++pTask;

        if (src == 0)
        {
            src = maxSize;
        }
        --src;
        ++dst;
        if (dst == maxSize)
        {
            dst = 0;
        }
    }

    pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallw_intra(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        const TypeHandle   sendtypes[],
    _Out_opt_   void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        const TypeHandle   recvtypes[],
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IalltoallwBuildIntra(
        sendbuf,
        sendcnts,
        sdispls,
        sendtypes,
        recvbuf,
        recvcnts,
        rdispls,
        recvtypes,
        const_cast<MPID_Comm*>(comm_ptr),
        tag,
        &pRequest
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    mpi_errno = pRequest->execute();
    if (mpi_errno != MPI_SUCCESS)
    {
        pRequest->Release();
    }
    else
    {
        *ppRequest = pRequest;
    }

    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallw_inter(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        const TypeHandle   sendtypes[],
    _Out_opt_   void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        const TypeHandle   recvtypes[],
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IalltoallwBuildInter(
        sendbuf,
        sendcnts,
        sdispls,
        sendtypes,
        recvbuf,
        recvcnts,
        rdispls,
        recvtypes,
        const_cast<MPID_Comm*>(comm_ptr),
        tag,
        &pRequest
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    mpi_errno = pRequest->execute();
    if (mpi_errno != MPI_SUCCESS)
    {
        pRequest->Release();
    }
    else
    {
        *ppRequest = pRequest;
    }

    return mpi_errno;
}
