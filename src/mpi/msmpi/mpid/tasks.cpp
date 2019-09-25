// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"

MPI_RESULT
NbcTask::InitIsend(
    _In_opt_ const void* buffer,
    _In_ size_t count,
    _In_ TypeHandle datatype,
    _In_ int destRank,
    _In_ MPID_Comm* pComm,
    _In_ bool isHelperDatatype
    )
{
    if( isHelperDatatype )
    {
        MPIU_Assert( datatype.IsPredefined() == false );
        m_helperDatatype = datatype.GetMpiHandle();
    }

    MPID_Request* pSendReq = MPIDI_Request_create_sreq(
        MPIDI_REQUEST_TYPE_SEND,
        buffer,
        fixme_cast<int>( count ),
        datatype,
        destRank,
        0,  // Tag to be filled in later. :S
        pComm
        );
    if( pSendReq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    InitAsync( NBC_TASK_KIND_ISEND, pSendReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIrecv(
    _In_opt_ void* buffer,
    _In_ size_t count,
    _In_ TypeHandle datatype,
    _In_ int srcRank,
    _In_ bool isHelperDatatype
    )
{
    if( isHelperDatatype )
    {
        MPIU_Assert( datatype.IsPredefined() == false );
        m_helperDatatype = datatype.GetMpiHandle();
    }

    m_kind = NBC_TASK_KIND_IRECV;
    m_recv.pBuffer = buffer;
    m_recv.count = count;
    m_recv.datatype = datatype;
    m_recv.srcRank = srcRank;
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIbarrierIntra(
    _In_ MPID_Comm* pComm
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPID_Request* pIbarrierReq;
    MPI_RESULT mpi_errno = IbarrierBuildIntra(
        pComm,
        &pIbarrierReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intracomm barriers could be noop or an actual NBC request.
    // We use the generic ASYNC_REQUEST kind here to ensure that the execute
    // method is called, rather than one of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pIbarrierReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIbcastIntra(
    _In_ void* buffer,
    _In_ size_t count,
    _In_ TypeHandle datatype,
    _In_ int root,
    _In_ MPID_Comm* pComm,
    _In_ bool isHelperDatatype
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    if( isHelperDatatype )
    {
        MPIU_Assert( datatype.IsPredefined() == false );
        m_helperDatatype = datatype.GetMpiHandle();
    }

    MPID_Request* pIbcastReq;
    MPI_RESULT mpi_errno = IbcastBuildIntra(
        buffer,
        fixme_cast<int>( count ),
        datatype,
        root,
        pComm,
        &pIbcastReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intracomm bcasts could be noop or an actual NBC request.
    // We use the generic ASYNC_REQUEST kind here to ensure that the execute
    // method is called, rather than one of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pIbcastReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIbcastInter(
    _In_opt_ void* buffer,
    _In_     size_t count,
    _In_     TypeHandle datatype,
    _In_     int root,
    _In_     MPID_Comm* pComm
    )
{
    MPIU_Assert( pComm->comm_kind == MPID_INTERCOMM );
    MPID_Request* pIbcastReq;
    MPI_RESULT mpi_errno = IbcastBuildInter(
        buffer,
        fixme_cast<int>( count ),
        datatype,
        root,
        pComm,
        &pIbcastReq
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intercomm bcasts could be various types of requests - noop, send, or an actual NBC
    // request.  We use the generic ASYNC_REQUEST kind here to ensure that the execute
    // method is called, rather than one of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pIbcastReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIreduceIntra(
    _In_opt_  const void*      sendbuf,
    _When_(root == pComm->rank, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              size_t           count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_range_(0, pComm->remote_size - 1)
              int              root,
    _In_      MPID_Comm*       pComm
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPID_Request* pReq;
    MPI_RESULT mpi_errno = IreduceBuildIntra(
        sendbuf,
        recvbuf,
        fixme_cast<int>( count ),
        datatype,
        pOp,
        root,
        pComm,
        &pReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intracomm reductions could be noop or an actual NBC request.
    // We use the generic ASYNC_REQUEST kind here to ensure that the execute
    // method is called, rather than one of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIreduceInter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _When_(root != MPI_PROC_NULL, _In_)
              MPID_Op*         pOp,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm
    )
{
    MPIU_Assert( pComm->comm_kind == MPID_INTERCOMM );

    MPID_Request* pReq;
    MPI_RESULT mpi_errno = IreduceBuildInter(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
        root,
        pComm,
        &pReq
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intercomm reductions could be various types of requests - noop, receive,
    // or an actual NBC request. We use the generic ASYNC_REQUEST kind here to
    // ensure that the execute method is called, rather than one of the more
    // specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIgatherIntra(
    _mpi_when_not_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_not_root_(pComm, root, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(pComm, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(pComm, root, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _In_range_(0, pComm->remote_size - 1)
              int              root,
    _In_      MPID_Comm*       pComm
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPID_Request* pReq;
    MPI_RESULT mpi_errno = IgatherBuildIntra(
        sendbuf,
        sendcnt,
        sendtype,
        recvbuf,
        recvcnt,
        recvtype,
        root,
        pComm,
        &pReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intracomm gathers could be noop or an actual NBC request.
    // We use the generic ASYNC_REQUEST kind here to ensure that the execute
    // method is called, rather than one of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIgathervBoth(
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(sendbuf) != MPI_IN_PLACE && sendcnt >= 0, _In_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && sendcnt >= 0, _In_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && sendcnt >= 0, _In_opt_)
              const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(pComm, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        recvcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm
    )
{
    MPID_Request* pReq;
    MPI_RESULT mpi_errno = IgathervBuildBoth(
        sendbuf,
        sendcnt,
        sendtype,
        recvbuf,
        recvcnts,
        displs,
        recvtype,
        root,
        pComm,
        &pReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intracomm/intercomm allgathervs could be various types of requests -
    // noop, send or an actual NBC request. We use the generic ASYNC_REQUEST
    // kind here to ensure that the execute method is called, rather than one
    // of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIscatterIntra(
    _When_(root == pComm->rank, _In_opt_)
              const void*      sendbuf,
    _When_(root == pComm->rank, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _When_(((pComm->rank == root && sendcnt != 0) || (pComm->rank != root && recvcnt != 0)) || _Old_(recvbuf) != MPI_IN_PLACE, _Out_opt_)
              void*            recvbuf,
    _When_(root != pComm->rank || recvbuf != MPI_IN_PLACE, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _In_range_(0, pComm->remote_size - 1)
              int              root,
    _In_      MPID_Comm*       pComm,
    _In_      unsigned int     tag
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPID_Request* pReq;
    MPI_RESULT mpi_errno = IscatterBuildIntra(
        sendbuf,
        sendcnt,
        sendtype,
        recvbuf,
        recvcnt,
        recvtype,
        root,
        pComm,
        tag,
        &pReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intracomm scatters could be noop or an actual NBC request.
    // We use the generic ASYNC_REQUEST kind here to ensure that the execute
    // method is called, rather than one of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitIscattervBoth(
    _mpi_when_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        sendcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       sendtype,
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(recvbuf) != MPI_IN_PLACE && recvcnt >= 0, _Out_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && recvcnt >= 0, _Out_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && recvcnt >= 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm
    )
{
    MPID_Request* pReq;
    MPI_RESULT mpi_errno = IscattervBuildBoth(
        sendbuf,
        sendcnts,
        displs,
        sendtype,
        recvbuf,
        recvcnt,
        recvtype,
        root,
        pComm,
        &pReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Intracomm / intercomm scattervs could be various types of requests -
    // noop, receive or an actual NBC request. We use the generic ASYNC_REQUEST
    // kind here to ensure that the execute method is called, rather than one
    // of the more specialized ones.
    //
    InitAsync( NBC_TASK_KIND_ASYNC_REQUEST, pReq );
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitPack(
    _In_ BYTE* dest,
    _In_ void* src,
    _In_ MPI_Count count,
    _In_ TypeHandle datatype
    )
{
    m_kind = NBC_TASK_KIND_PACK;
    m_pack.pPackBuffer = dest;

    m_pack.pSeg = MPID_Segment_alloc();
    if( m_pack.pSeg == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_RESULT mpi_errno = MPID_Segment_init(
        src,
        fixme_cast<int>( count ),  // TODO: remove the cast after we properly support MPI_Count in the packing functions
        datatype.GetMpiHandle(),
        m_pack.pSeg,
        0
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        MPID_Segment_free( m_pack.pSeg );
        m_pack.pSeg = nullptr;
    }

    return mpi_errno;
}


MPI_RESULT
NbcTask::InitUnpack(
    _In_ BYTE* src,
    _In_ void* dest,
    _In_ MPI_Count count,
    _In_ TypeHandle datatype
    )
{
    m_kind = NBC_TASK_KIND_UNPACK;
    m_pack.pPackBuffer = src;
    m_pack.pSeg = MPID_Segment_alloc();
    if( m_pack.pSeg == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_RESULT mpi_errno = MPID_Segment_init(
        dest,
        fixme_cast<int>( count ),  // TODO: remove the cast after we properly support MPI_Count in the packing functions
        datatype.GetMpiHandle(),
        m_pack.pSeg,
        0
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        MPID_Segment_free( m_pack.pSeg );
        m_pack.pSeg = nullptr;
    }

    return mpi_errno;
}


MPI_RESULT
NbcTask::InitReduce(
    _Inout_ void* pRecvBuf,
    _Inout_opt_ void* pReduceBuf,
    _In_ int count,
    _In_ TypeHandle type,
    _In_ MPID_Op* pOp,
    _In_ MPI_Datatype hType,
    _In_ bool rightOrder
    )
{
    m_kind = NBC_TASK_KIND_REDUCE;
    m_reduce.pRecvBuf = pRecvBuf;
    m_reduce.pReduceBuf = pReduceBuf;
    m_reduce.count = count;
    m_reduce.type = type;
    m_reduce.pOp = pOp;
    pOp->AddRef();
    m_reduce.hType = hType;
    m_reduce.rightOrder = rightOrder;
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::InitScan(
    _Inout_ void* pRecvBuf,
    _Inout_ void* pReduceBuf,
    _Inout_ void* pTmpBuf,
    _In_ int count,
    _In_ TypeHandle type,
    _In_ MPID_Op* pOp,
    _In_ MPI_Datatype hType,
    _In_ bool rightOrder,
    _In_ bool copyOnly
    )
{
    m_kind = NBC_TASK_KIND_SCAN;
    m_scan.pRecvBuf = pRecvBuf;
    m_scan.pReduceBuf = pReduceBuf;
    m_scan.pTmpBuf = pTmpBuf;
    m_scan.count = count;
    m_scan.type = type;
    m_scan.pOp = pOp;
    pOp->AddRef();
    m_scan.hType = hType;
    m_scan.rightOrder = rightOrder;
    m_scan.copyOnly = copyOnly;
    return MPI_SUCCESS;
}


void
NbcTask::InitLocalCopy(
    _In_opt_  const void* pSrcBuf,
    _In_      void* pDestBuf,
    _In_      size_t srcCount,
    _In_      size_t destCount,
    _In_      TypeHandle srcType,
    _In_      TypeHandle destType
    )
{
    m_kind = NBC_TASK_KIND_LOCAL_COPY;
    m_localCopy.pSrcBuf = pSrcBuf;
    m_localCopy.pDestBuf = pDestBuf;
    m_localCopy.srcCount = srcCount;
    m_localCopy.destCount = destCount;
    m_localCopy.srcType = srcType;
    m_localCopy.destType = destType;
}


MPI_RESULT
NbcTask::ExecuteIrecv(
    _In_ MPID_Comm* pComm,
    _In_ int tag
    )
{
    //
    // Once executed, the parameters are discarded and
    // the request is tracked in the m_async struct.
    //
    return MPID_Recv(
        m_recv.pBuffer,
        fixme_cast<int>( m_recv.count ),
        m_recv.datatype,
        m_recv.srcRank,
        tag,
        pComm,
        &m_async.pReq
        );
}


MPI_RESULT
NbcTask::ExecutePack()
{
    //
    // NOTE: the use of buffer values and positions in MPI_Pack and in
    // MPID_Segment_pack are quite different.  See code or docs or something.
    //
    MPI_Aint last = SEGMENT_IGNORE_LAST;

    MPID_Segment_pack(
        m_pack.pSeg,
        0,
        &last,
        reinterpret_cast<void *>( m_pack.pPackBuffer )
        );
    MPID_Segment_free( m_pack.pSeg );
    m_state = NBC_TASK_STATE_COMPLETED;

    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::ExecuteUnpack()
{
    //
    // NOTE: the use of buffer values and positions in MPI_Unpack and in
    // MPID_Segment_unpack are quite different.  See code or docs or something.
    //
    MPI_Aint last = SEGMENT_IGNORE_LAST;

    MPID_Segment_unpack(
        m_pack.pSeg,
        0,
        &last,
        reinterpret_cast<void *>( m_pack.pPackBuffer )
        );
    MPID_Segment_free( m_pack.pSeg );
    m_state = NBC_TASK_STATE_COMPLETED;

    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::ExecuteReduce()
{
    m_state = NBC_TASK_STATE_COMPLETED;

    if( !m_reduce.rightOrder && m_reduce.pOp->IsCommutative() == false )
    {
        MPID_Uop_call(
            m_reduce.pOp,
            m_reduce.pReduceBuf,
            m_reduce.pRecvBuf,
            &m_reduce.count,
            &m_reduce.hType
            );

        m_reduce.pOp->Release();
        return MPIR_Localcopy( m_reduce.pRecvBuf, m_reduce.count, m_reduce.type,
            m_reduce.pReduceBuf, m_reduce.count, m_reduce.type );
    }

    MPID_Uop_call(
        m_reduce.pOp,
        m_reduce.pRecvBuf,
        m_reduce.pReduceBuf,
        &m_reduce.count,
        &m_reduce.hType
        );

    m_reduce.pOp->Release();
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::ExecuteScan()
{
    m_state = NBC_TASK_STATE_COMPLETED;

    if( m_scan.rightOrder )
    {
        //
        // srcRank > dstRank
        //
        MPID_Uop_call(
            m_scan.pOp,
            m_scan.pTmpBuf,
            m_scan.pReduceBuf,
            &m_scan.count,
            &m_scan.hType );

        if( m_scan.copyOnly )
        {
            //
            // This step is executed in the first step of an exclusive scan
            //
            m_scan.pOp->Release();
            return MPIR_Localcopy(
                m_scan.pTmpBuf,
                m_scan.count,
                m_scan.type,
                m_scan.pRecvBuf,
                m_scan.count,
                m_scan.type );
        }
        else
        {
            MPID_Uop_call(
                m_scan.pOp,
                m_scan.pTmpBuf,
                m_scan.pRecvBuf,
                &m_scan.count,
                &m_scan.hType );
        }
    }
    else
    {
        if( m_scan.pOp->IsCommutative() == true )
        {
            MPID_Uop_call(
                m_scan.pOp,
                m_scan.pTmpBuf,
                m_scan.pReduceBuf,
                &m_scan.count,
                &m_scan.hType );
        }
        else
        {
            MPID_Uop_call(
                m_scan.pOp,
                m_scan.pReduceBuf,
                m_scan.pTmpBuf,
                &m_scan.count,
                &m_scan.hType );

            m_scan.pOp->Release();
            return MPIR_Localcopy(
                m_scan.pTmpBuf,
                m_scan.count,
                m_scan.type,
                m_scan.pReduceBuf,
                m_scan.count,
                m_scan.type );
        }
    }
    m_scan.pOp->Release();
    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::ExecuteLocalCopy()
{
    m_state = NBC_TASK_STATE_COMPLETED;

    MPIU_Assert(m_localCopy.srcCount <= INT_MAX);
    MPIU_Assert(m_localCopy.destCount <= INT_MAX);

    return MPIR_Localcopy(
        m_localCopy.pSrcBuf,
        fixme_cast<int>(m_localCopy.srcCount),
        m_localCopy.srcType,
        m_localCopy.pDestBuf,
        fixme_cast<int>(m_localCopy.destCount),
        m_localCopy.destType
        );
}


MPI_RESULT
NbcTask::Execute(
    _In_ const MPID_Comm* pComm,
    _In_ const unsigned tag,
    _Out_ ULONG* pNextInit
)
{
    MPI_RESULT mpi_errno;

    m_state = NBC_TASK_STATE_STARTED;

    switch( m_kind )
    {
    case NBC_TASK_KIND_ISEND:
        mpi_errno = ExecuteIsend(
            tag | pComm->comm_kind
            );
        break;

    case NBC_TASK_KIND_IRECV:
        mpi_errno = ExecuteIrecv(
            const_cast<MPID_Comm *>( pComm ),
            tag | pComm->comm_kind
            );
        break;

    case NBC_TASK_KIND_NBC:
        mpi_errno = m_async.pReq->execute_nbc_tasklist();
        break;

    case NBC_TASK_KIND_ASYNC_REQUEST:
        mpi_errno = m_async.pReq->execute();
        break;

    case NBC_TASK_KIND_PACK:
        mpi_errno = ExecutePack();
        break;

    case NBC_TASK_KIND_UNPACK:
        mpi_errno = ExecuteUnpack();
        break;

    case NBC_TASK_KIND_REDUCE:
        mpi_errno = ExecuteReduce();
        break;

    case NBC_TASK_KIND_SCAN:
        mpi_errno = ExecuteScan();
        break;

    case NBC_TASK_KIND_LOCAL_COPY:
        mpi_errno = ExecuteLocalCopy();
        break;

    case NBC_TASK_KIND_NOOP:
        mpi_errno = ExecuteNoOp();
        break;

    default:
        //
        // TODO: update the following assertion when adding new task kinds.
        //
        MPIU_Assert(
            m_kind == NBC_TASK_KIND_PACK ||
            m_kind == NBC_TASK_KIND_UNPACK ||
            m_kind == NBC_TASK_KIND_ISEND ||
            m_kind == NBC_TASK_KIND_IRECV ||
            m_kind == NBC_TASK_KIND_NBC ||
            m_kind == NBC_TASK_KIND_ASYNC_REQUEST ||
            m_kind == NBC_TASK_KIND_REDUCE ||
            m_kind == NBC_TASK_KIND_SCAN ||
            m_kind == NBC_TASK_KIND_LOCAL_COPY ||
            m_kind == NBC_TASK_KIND_NOOP
            );
        m_state = NBC_TASK_STATE_FAILED;
        return MPI_ERR_INTERN;
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        m_state = NBC_TASK_STATE_FAILED;
        return mpi_errno;
    }

    if( m_state == NBC_TASK_STATE_COMPLETED )
    {
        //
        // Tasks that execute synchronously should return
        // the next thing that needs to run after them.
        // Since we can only return one index, we assert
        // that nothing was queued to be initialized after
        // this task is initialized.
        //
        MPIU_Assert( m_iNextOnInit == NBC_TASK_NONE );
        *pNextInit = m_iNextOnComplete;
    }
    else
    {
        *pNextInit = m_iNextOnInit;
    }

    return MPI_SUCCESS;
}


MPI_RESULT
NbcTask::TestProgress(
    _Out_ ULONG* pNextOnComplete
)
{
    MPI_RESULT mpi_errno;

    switch( m_state )
    {
    case NBC_TASK_STATE_NOTSTARTED:
        return MPI_ERR_PENDING;

    case NBC_TASK_STATE_STARTED:
        if( m_kind != NBC_TASK_KIND_NOOP )
        {
            if( m_async.pReq->test_complete() == false )
            {
                return MPI_ERR_PENDING;
            }

            mpi_errno = m_async.pReq->status.MPI_ERROR;
            m_async.pReq->Release();

            if( mpi_errno != MPI_SUCCESS )
            {
                m_state = NBC_TASK_STATE_FAILED;
                return mpi_errno;
            }
        }

        m_state = NBC_TASK_STATE_COMPLETED;
        *pNextOnComplete = m_iNextOnComplete;
        return MPI_SUCCESS;

    case NBC_TASK_STATE_COMPLETED:
        //
        // Do not re-execute on-complete task on already completed task.
        //
        *pNextOnComplete = NBC_TASK_NONE;
        return MPI_SUCCESS;

    case NBC_TASK_STATE_FAILED:
    default:
        MPIU_Assert(
            m_state == NBC_TASK_STATE_NOTSTARTED ||
            m_state == NBC_TASK_STATE_STARTED ||
            m_state == NBC_TASK_STATE_COMPLETED ||
            m_state == NBC_TASK_STATE_FAILED
            );
        //
        // Shouldn't come here at all, something is wrong
        //
        return MPI_ERR_INTERN;
    }
}


void
NbcTask::Cancel()
{
    //
    // Convert task kind to request kind.
    //
    NBC_TASK_KIND taskKind = m_kind;
    if( taskKind == NBC_TASK_KIND_ASYNC_REQUEST )
    {
        switch( m_async.pReq->kind )
        {
        case MPID_REQUEST_SEND:
            taskKind = NBC_TASK_KIND_ISEND;
            break;

        case MPID_REQUEST_RECV:
            taskKind = NBC_TASK_KIND_IRECV;
            break;

        case MPID_REQUEST_NBC:
            taskKind = NBC_TASK_KIND_NBC;
            break;

        default:
            //
            // The rest of the code below will
            // treat NBC_TASK_KIND_ASYNC_REQUEST
            // as a noop - simply release the request.
            //
            break;
        }
    }

    switch( m_state )
    {
    case NBC_TASK_STATE_NOTSTARTED:
        switch( taskKind )
        {
        case NBC_TASK_KIND_NBC:
            m_async.pReq->cancel_nbc_tasklist();
            __fallthrough;
        case NBC_TASK_KIND_ISEND:
        case NBC_TASK_KIND_ASYNC_REQUEST:
            m_async.pReq->Release();
            break;

        case NBC_TASK_KIND_PACK:
        case NBC_TASK_KIND_UNPACK:
            MPID_Segment_free( m_pack.pSeg );
            break;
        }
        break;

    case NBC_TASK_STATE_STARTED:
        if( taskKind != NBC_TASK_KIND_NOOP )
        {
            m_async.pReq->cancel();
            m_async.pReq->Release();
        }
        break;

    case NBC_TASK_STATE_COMPLETED:
    case NBC_TASK_STATE_FAILED:
        return;
    }

    //
    // Set state to failed to prevent re-entrance for some reason.
    //
    m_state = NBC_TASK_STATE_FAILED;
}


void
NbcTask::Print()
{
    size_t count = SIZE_MAX;

    NBC_TASK_KIND taskKind = m_kind;
    if( taskKind == NBC_TASK_KIND_ASYNC_REQUEST )
    {
        switch( m_async.pReq->kind )
        {
        case MPID_REQUEST_SEND:
            taskKind = NBC_TASK_KIND_ISEND;
            break;

        case MPID_REQUEST_RECV:
            taskKind = NBC_TASK_KIND_IRECV;
            break;

        case MPID_REQUEST_NBC:
            taskKind = NBC_TASK_KIND_NBC;
            break;

        default:
            //
            // The rest of the code below will
            // treat NBC_TASK_KIND_ASYNC_REQUEST
            // as a noop.
            //
            break;
        }
    }

    const char* szTaskKind;
    switch( taskKind )
    {
    case NBC_TASK_KIND_PACK:
        szTaskKind = "pack";
        break;
    case NBC_TASK_KIND_UNPACK:
        szTaskKind = "unpack";
        break;
    case NBC_TASK_KIND_ISEND:
        szTaskKind = "send";
        count = m_async.pReq->dev.user_count;
        break;
    case NBC_TASK_KIND_IRECV:
        szTaskKind = "recv";
        count = m_recv.count;
        break;
    case NBC_TASK_KIND_NBC:
        szTaskKind = "nbc";
        break;
    case NBC_TASK_KIND_REDUCE:
        szTaskKind = "reduce";
        break;
    case NBC_TASK_KIND_SCAN:
        szTaskKind = "scan";
        break;
    case NBC_TASK_KIND_LOCAL_COPY:
        szTaskKind = "localcopy";
        count = m_localCopy.srcCount;
        break;
    case NBC_TASK_KIND_ASYNC_REQUEST:
    case NBC_TASK_KIND_NOOP:
        //
        // We should have converted valid types above, so this is a noop.
        //
        szTaskKind = "noop";
        break;
    default:
        szTaskKind = "invalid";
        break;
    }

    int target;
    switch( taskKind )
    {
    case NBC_TASK_KIND_IRECV:
        target = m_recv.srcRank;
        break;

    case NBC_TASK_KIND_ISEND:
        target = m_async.pReq->dev.match.rank;
        break;

    default:
        target = 0;
    }

    printf(
        "%s %d %u %u %Iu",
        szTaskKind,
        target,
        m_iNextOnInit,
        m_iNextOnComplete,
        count
        );

    if( taskKind == NBC_TASK_KIND_NBC )
    {
        printf( "\nbegin nested NBC operation:\n" );
        m_async.pReq->print_nbc_tasklist();
        printf( "end nested NBC operation" );
    }
}

