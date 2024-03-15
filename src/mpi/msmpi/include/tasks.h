// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef TASKS_H
#define TASKS_H


#define NBC_TASK_NONE ULONG_MAX

//
// Non-blocking collectives consist of task lists that represent the operation being executed.
//
class NbcTask
{
public:
    enum NBC_TASK_STATE
    {
        NBC_TASK_STATE_NOTSTARTED,
        NBC_TASK_STATE_STARTED,
        NBC_TASK_STATE_COMPLETED,
        NBC_TASK_STATE_FAILED
    } m_state;

    ULONG m_iNextOnInit;
    ULONG m_iNextOnComplete;


private:
    enum NBC_TASK_KIND
    {
        NBC_TASK_KIND_ISEND,
        NBC_TASK_KIND_IRECV,
        NBC_TASK_KIND_NBC,
        NBC_TASK_KIND_ASYNC_REQUEST,
        NBC_TASK_KIND_PACK,
        NBC_TASK_KIND_UNPACK,
        NBC_TASK_KIND_REDUCE,
        NBC_TASK_KIND_LOCAL_COPY,
        NBC_TASK_KIND_SCAN,
        NBC_TASK_KIND_NOOP
    } m_kind;

    union
    {
        struct NBC_TASK_ASYNC
        {
            MPID_Request* pReq;
        } m_async;

        struct NBC_TASK_RECV_PARAMS
        {
            void* pBuffer;
            size_t count;
            TypeHandle datatype;
            int srcRank;
        } m_recv;

        struct NBC_TASK_PACK
        {
            MPID_Segment* pSeg;
            BYTE* pPackBuffer;
        } m_pack;

        struct NBC_TASK_REDUCE
        {
            void* pRecvBuf;
            void* pReduceBuf;
            TypeHandle type;
            MPID_Op* pOp;
            MPI_Datatype hType;
            int count;
            bool rightOrder;
        } m_reduce;

        struct NBC_TASK_LOCAL_COPY
        {
            const void* pSrcBuf;
            void* pDestBuf;
            size_t srcCount;
            size_t destCount;
            TypeHandle srcType;
            TypeHandle destType;
        } m_localCopy;

        struct NBC_TASK_SCAN : NBC_TASK_REDUCE
        {
            void* pTmpBuf;
            bool  copyOnly;
        } m_scan;
    };

    //
    // In some algorithm, e.g. igather, we create custom datatype to help
    // send / recv scattered data. Store the custom datatype in this member
    // so that it can be properly de-allocated later.
    //
    MPI_Datatype m_helperDatatype;


public:
    NbcTask():
        m_state( NBC_TASK_STATE_NOTSTARTED ),
        m_helperDatatype( MPI_DATATYPE_NULL )
    {}


    ~NbcTask()
    {
        //
        // Only free the data type if it's not freed yet. The data type
        // could have been free by other NbcTask object already.
        //
        if( m_helperDatatype != MPI_DATATYPE_NULL  &&
            TypePool::Lookup( m_helperDatatype ) != nullptr )
        {
            NMPI_Type_free( &m_helperDatatype );
        }
    }


    MPI_RESULT
    InitIsend(
        _In_opt_ const void* buffer,
        _In_ size_t count,
        _In_ TypeHandle datatype,
        _In_ int destRank,
        _In_ MPID_Comm* pComm,
        _In_ bool isHelperDatatype = false
        );


    MPI_RESULT
    InitIrecv(
        _In_opt_ void* buffer,
        _In_ size_t count,
        _In_ TypeHandle datatype,
        _In_ int srcRank,
        _In_ bool isHelperDatatype = false
        );


    MPI_RESULT
    InitIbarrierIntra(
        _In_ MPID_Comm* pComm
        );


    MPI_RESULT
    InitIbcastIntra(
        _In_opt_ void* buffer,
        _In_ size_t count,
        _In_ TypeHandle datatype,
        _In_ int root,
        _In_ MPID_Comm* pComm,
        _In_ bool isHelperDatatype = false
        );


    MPI_RESULT
    InitIbcastInter(
        _In_opt_ void* buffer,
        _In_ size_t count,
        _In_ TypeHandle datatype,
        _In_ int root,
        _In_ MPID_Comm* pComm
        );


    MPI_RESULT
    InitIreduceIntra(
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
        );


    MPI_RESULT
    InitIreduceInter(
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
        );


    MPI_RESULT
    InitIgatherIntra(
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
        );


    MPI_RESULT
    InitIgathervBoth(
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
        );


    MPI_RESULT
    InitIscatterIntra(
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
        );


    MPI_RESULT
    InitIscattervBoth(
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
        );


    MPI_RESULT
    InitPack(
        _In_ BYTE* dest,
        _In_ void* src,
        _In_ MPI_Count count,
        _In_ TypeHandle datatype
        );


    MPI_RESULT
    InitUnpack(
        _In_ BYTE* src,
        _In_ void* dest,
        _In_ MPI_Count count,
        _In_ TypeHandle datatype
        );


    MPI_RESULT
    InitReduce(
        _Inout_ void* pRecvBuf,
        _Inout_opt_ void* pReduceBuf,
        _In_ int count,
        _In_ TypeHandle type,
        _In_ MPID_Op* pOp,
        _In_ MPI_Datatype hType,
        _In_ bool rightOrder
        );


    MPI_RESULT
    InitScan(
        _Inout_ void* pRecvBuf,
        _Inout_ void* pReduceBuf,
        _Inout_ void* pTmpBuf,
        _In_ int count,
        _In_ TypeHandle type,
        _In_ MPID_Op* pOp,
        _In_ MPI_Datatype hType,
        _In_ bool rightOrder,
        _In_ bool copyOnly = false
        );


    void
    InitLocalCopy(
        _In_opt_  const void* pSrcBuf,
        _Out_opt_ void* pDestBuf,
        _In_      size_t srcCount,
        _In_      size_t destCount,
        _In_      TypeHandle srcType,
        _In_      TypeHandle destType
        );


    inline
    void
    InitNoOp()
    {
        m_kind = NBC_TASK_KIND_NOOP;
    }


    //
    // Execute the current task, return the iNextOnInit index
    // so that the caller can init the next one.
    // NBC_TASK_NONE indicates no task left to be initiated.
    //
    MPI_RESULT
    Execute(
        _In_ const MPID_Comm* pComm,
        _In_ const unsigned tag,
        _Out_ ULONG* pNextInit
        );


    MPI_RESULT
    TestProgress(
        _Out_ ULONG* pNextOnComplete
        );


    void
    Cancel();


    void
    Print();

private:
    inline
    void
    InitAsync(
        _In_ NBC_TASK_KIND taskKind,
        _In_ MPID_Request* pReq
        )
    {
        //
        // Make sure when task kind is NBC, request kind is NBC too.
        //
        MPIU_Assert( taskKind != NBC_TASK_KIND_NBC || pReq->kind == MPID_REQUEST_NBC );

        m_kind = taskKind;
        m_async.pReq = pReq;
    }


    inline
    MPI_RESULT
    ExecuteIsend(
        _In_ int tag
        )
    {
        m_async.pReq->dev.match.tag = tag;
        return m_async.pReq->execute_send();
    }


    inline
    MPI_RESULT
    ExecuteNoOp()
    {
        return MPI_SUCCESS;
    }


    MPI_RESULT
    ExecuteIrecv(
        _In_ MPID_Comm* pComm,
        _In_ int tag
        );


    MPI_RESULT
    ExecutePack();


    MPI_RESULT
    ExecuteUnpack();


    MPI_RESULT
    ExecuteReduce();


    MPI_RESULT
    ExecuteScan();


    MPI_RESULT
    ExecuteLocalCopy();
};


//
// Applies the provided functor (or lambda) to each child node of a binomial
// tree rooted at the calling node from highest to lowest ranked child.  The
// child nodes are powers-of-two away from the calling node.
//
// This is useful when data flows from the root to the children, and is optimal
// for balanced trees where the highest-ranked child has the deepest subtree
// (true for power-of-two sized trees).
//
template<typename F>
MPI_RESULT
BinomialChildBuilderDescending(
    _In_ F functor,
    _In_ unsigned nChildren
)
{
    while( nChildren != 0 )
    {
        //
        // The children are all powers of 2 ranks away from us.  We start with
        // the highest-ranked child (the one furthest away) under the assumption
        // that this will be the deepest subtree.  This may not always be the case.
        //
        unsigned offset = 1 << ( --nChildren );

        MPI_RESULT mpi_errno = functor( offset );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    return MPI_SUCCESS;
}


//
// Applies the provided functor (or lambda) to each child node of a binomial
// tree rooted at the calling node from lowest to highest ranked child.  The
// child nodes are powers-of-two away from the calling node.
//
// This is useful when data flows from the children to the root, and is optimal
// for balanced trees where the highest-ranked child has the deepest subtree
// (true for power-of-two sized trees).
//
template<typename F>
MPI_RESULT
BinomialChildBuilderAscending(
    _In_ F functor,
    _In_ unsigned nChildren
)
{
    unsigned shift = 0;
    while( shift != nChildren )
    {
        //
        // The children are all powers of 2 ranks away from us.  We start with
        // the lowest-ranked child (the closest one).
        //
        unsigned offset = 1 << ( shift++ );

        MPI_RESULT mpi_errno = functor( offset );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    return MPI_SUCCESS;
}


//
// Applies the provided functor (or lambda) to the parent of the calling node in
// a binomial tree.  The parent is determined by clearing the least significant
// bit of the calling node's rank in the tree.  That is, rank 4 (0b100)'s parent
// is rank 0; rank 7 (0b111)'s parent is rank 6 (0b110), etc.
//
//  Note that the process that is the tree root is handled automatically
//  by this code, since it has no bits set.
//
template<typename F>
MPI_RESULT
BinomialParentBuilder(
    _In_ F functor,
    _In_ int rank,
    _In_ int commSize,
    _In_ int relativeRank
)
{
    //
    // Find the least significant bit set to calculate the offset from our rank
    // from which to receive.
    //
    ULONG lsb;
    if( _BitScanForward( &lsb, relativeRank ) != 0 )
    {
        int targetRank = rank - (1 << lsb);
        if( targetRank < 0 )
        {
            targetRank += commSize;
        }

        MPI_RESULT mpi_errno = functor( targetRank );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    return MPI_SUCCESS;
}

#endif // TASKS_H

