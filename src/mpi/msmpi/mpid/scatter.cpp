// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* This is the default implementation of scatter. The algorithm is:

   Algorithm: MPI_Scatter

   We use a binomial tree algorithm for both short and
   long messages. At nodes other than leaf nodes we need to allocate
   a temporary buffer to store the incoming message. If the root is
   not rank 0, we reorder the sendbuf in order of relative ranks by
   copying it into a temporary buffer, so that all the sends from the
   root are contiguous and in the right order. In the heterogeneous
   case, we first pack the buffer by using MPI_Pack and then do the
   scatter.

   Cost = lgp.alpha + n.((p-1)/p).beta
   where n is the total size of the data to be scattered from the root.

   Improvements:
   1. A new switch over point (MPICH_DEFAULT_SCATTER_VSMALL_MSG) is introduced
      to control whether we use scratch buffer or data type for data transfers:
      a. Root case:
         i.   Root == 0 or data size >= MPICH_DEFAULT_SCATTER_VSMALL_MSG or
              data size < MPICH_DEFAULT_SCATTER_VSMALL_MSG but no data rotation
              happens for any of its subtree, no scratch buffer is created.
              Sendbuf is used directly in data transfer. At the end, the root's
              own data is copied into recvbuf if it's not in place.
         ii.  Root != 0, data size < MPICH_DEFAULT_SCATTER_VSMALL_MSG and data
              rotation happens for one of its subtrees, a scratch buffer of
              length numranks - 1 (doesn't include the root's own data) is
              created to hold the rotated sendbuf order by relative rank. The
              scratch buffer is used in data transfers. At the end, the root's
              own data is copied from sendbuf to recvbuf if it's not in place.
      b. Non-root case:
         i.   If it's leaf, no scratch buffer is created. Data is received into
              the recvbuf directly.
         ii.  If it's not leaf and data size >= MPICH_DEFAULT_SCATTER_VSMALL_MSG,
              a scratch buffer with length treesize - 1 (doesn't include its own
              data) is created. Data of its tree and itself is received into the
              recvbuf and the scratch buffer in one receive with the help of a
              struct data type.
         iii. If it's not leaf and data size < MPICH_DEFAULT_SCATTER_VSMALL_MSG,
              a scratch buffer with length treesize (include its own data) is
              created. Data of its tree and itself is received into the scratch
              buffer. At the end, its own data is copied into recvbuf from the
              scratch buffer.
   2. Allocates exact amount of buffer it needs depends on the logic described
      in 1.
   3. For root case, only allocates the scratch buffer on root if needed depends
      on the logic described in 1.
   4. For non-root and large buffer case, receives its own data directly into
      recvbuf without the intermediate copy.

   End Algorithm: MPI_Scatter
*/
static inline
unsigned
IscatterTaskCount(
    _In_range_( <, pComm->remote_size ) unsigned root,
    _In_range_( <, pComm->remote_size ) unsigned relativeRank,
    _In_range_( <, pComm->remote_size ) unsigned childCount,
    _In_range_( >, 0 ) size_t cbBuffer,
    _In_ bool isInPlace,
    _In_ MPID_Comm* pComm
    )
{
    unsigned size = pComm->remote_size;

    MPIU_Assert( root < size );
    MPIU_Assert( cbBuffer > 0 );

    unsigned rank = pComm->rank;
    //
    // Send to all the children.
    //
    unsigned taskCount = childCount;
    unsigned MPIR_scatter_vsmall_msg = pComm->SwitchPoints()->MPIR_scatter_vsmall_msg;

    if( rank == root )
    {
        if( !isInPlace )
        {
            //
            // Root, not in place, needs one task to copy root's send buffer
            // to its receive buffer.
            //
            ++taskCount;
        }

        if( root != 0 && cbBuffer < MPIR_scatter_vsmall_msg )
        {
            unsigned treeSize;
            unsigned childRank;
            unsigned childRelativeRank;

            while( childCount != 0 )
            {
                childRelativeRank = relativeRank + ( 1 << ( --childCount ) );
                childRank = pComm->RankAdd( root, childRelativeRank );
                treeSize = TreeSize( childRelativeRank, size );
                if( childRank + treeSize > size )
                {
                    //
                    // Small message size and data rotation happens, needs two
                    // tasks to copy from the send buffer to the scratch buffer
                    // to rotate the data into the right order.
                    //
                    taskCount += 2;
                    break;
                }
            }
        }
    }
    else
    {
        //
        // Not root, needs one task to receive from the parent.
        //
        ++taskCount;

        if( childCount > 0 && cbBuffer < MPIR_scatter_vsmall_msg )
        {
            //
            // Not leaf, small message size, needs one task to copy the scratch
            // buffer to its receive buffer.
            //
            ++taskCount;
        }
    }

    return taskCount;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IscatterBuildTasklist(
    _Inout_  MPID_Request*   pReq,
    _When_(root == pComm->rank, _In_opt_)
             const void*     sendbuf,
    _When_(root == pComm->rank, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(root != pComm->rank || _Old_(recvbuf) != MPI_IN_PLACE, _Out_opt_)
             void*           recvbuf,
    _When_(root != pComm->rank || recvbuf != MPI_IN_PLACE, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _In_range_(0, pComm->remote_size - 1)
             unsigned        root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    tag
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag(tag);

    unsigned size = pComm->remote_size;
    unsigned MPIR_scatter_vsmall_msg = pComm->SwitchPoints()->MPIR_scatter_vsmall_msg;
    MPI_Count extent;
    size_t cbBuffer;
    MPI_RESULT mpi_errno;

    unsigned rank = pComm->rank;
    unsigned relativeRank = pComm->RankSub( root );
    unsigned childCount = ChildCount( relativeRank, size );
    unsigned treeSize = TreeSize( relativeRank, size );
    unsigned numTmpBufBlocks = treeSize - 1;

    if( rank == root )
    {
        extent = sendtype.GetExtent();
        MPI_Count sendTypeSize = sendtype.GetSize();
        cbBuffer = fixme_cast<size_t>( sendTypeSize * sendcnt );
    }
    else
    {
        //
        // Set extent to 0 to avoid the compiler potentially-uninitialized-local-
        // variable-error. It will not be used if rank != root. By not setting
        // extent to 0 where it's declared, we save one assignment for the
        // rank == root path.
        //
        extent = 0;
        MPI_Count recvTypeSize = recvtype.GetSize();
        cbBuffer = fixme_cast<size_t>( recvTypeSize * recvcnt );

        if( numTmpBufBlocks > 0 && cbBuffer < MPIR_scatter_vsmall_msg )
        {
            //
            // Receive data of the subtree including self data into the scratch
            // buffer for very small message size.
            //
            ++numTmpBufBlocks;
        }
    }

    bool isInPlace = ( recvbuf == MPI_IN_PLACE );
    unsigned nTasks = IscatterTaskCount(
        root,
        relativeRank,
        childCount,
        cbBuffer,
        isInPlace,
        pComm
        );
    if( rank == root && nTasks <= childCount + 1 )
    {
        //
        // Root, large message or no data rotation happens, don't need the
        // scratch buffer.
        //
        numTmpBufBlocks = 0;
    }
    else if( numTmpBufBlocks > 0 )
    {
        pReq->nbc.tmpBuf = new BYTE[numTmpBufBlocks * cbBuffer];
        if( pReq->nbc.tmpBuf == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
    }

    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        //
        // No need to free pReq->nbc.tmpBuf here because the failure here will
        // trigger the request object to clean up automatically because it's
        // created via the StackGuardRef class.
        //
        return MPIU_ERR_NOMEM();
    }
    ULONG iCurrTask = pReq->nbc.nTasks;
    ULONG* piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnComplete;

    if( rank == root && numTmpBufBlocks > 0 )
    {
        //
        // Copy from the send buffer to the scratch buffer to rotate the data
        // into the right order.
        // rank + 1 is safe here because if rank == size - 1, then
        // numTmpBufBlocks should have been 0.
        //
        MPIU_Assert( rank < size - 1 );

        pReq->nbc.tasks[iCurrTask].InitLocalCopy(
            static_cast<const BYTE*>( sendbuf ) + extent * sendcnt * ( rank + 1 ),
            pReq->nbc.tmpBuf,
            static_cast<size_t>(sendcnt) * ( size - rank - 1 ),
            cbBuffer * ( size - rank - 1 ),
            sendtype,
            g_hBuiltinTypes.MPI_Byte
            );
        ++pReq->nbc.nTasks;
        pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
        pReq->nbc.tasks[iCurrTask].m_iNextOnComplete = pReq->nbc.nTasks;
        ++iCurrTask;

        pReq->nbc.tasks[iCurrTask].InitLocalCopy(
            sendbuf,
            pReq->nbc.tmpBuf + cbBuffer * ( size - rank - 1 ),
            static_cast<size_t>(sendcnt) * rank,
            cbBuffer * rank,
            sendtype,
            g_hBuiltinTypes.MPI_Byte
            );
        ++pReq->nbc.nTasks;
        pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnComplete;
        ++iCurrTask;
    }

    mpi_errno = BinomialParentBuilder(
        [&]( _In_range_( >= , 0 ) int targetRank )
        {
            BYTE* pRecv;
            size_t count;
            TypeHandle datatype = g_hBuiltinTypes.MPI_Byte;
            bool isHelperDatatype = false;

            if( treeSize == 1 )
            {
                //
                // Leaf, receive directly into recvbuf.
                //
                pRecv = static_cast<BYTE*>( recvbuf );
                count = recvcnt;
                datatype = recvtype;
            }
            else if( cbBuffer < MPIR_scatter_vsmall_msg )
            {
                //
                // Not leaf, very small message size, receive into the scratch buffer.
                //
                pRecv = pReq->nbc.tmpBuf;
                count = cbBuffer * treeSize;
            }
            else
            {
                //
                // Not leaf, large message size, receive rank's own data and
                // data of its children into its recvbuf and the scratch buffer
                // directly with the help of a struct data type.
                //
                pRecv = static_cast<BYTE*>( MPI_BOTTOM );
                count = 1;

                int blocks[2];
                MPI_Aint displs[2];
                MPI_Datatype tmpType;
                MPI_Datatype tmpTypes[2];

                blocks[0] = recvcnt;
                displs[0] = reinterpret_cast<MPI_Aint>( recvbuf );
                tmpTypes[0] = recvtype.GetMpiHandle();
                blocks[1] = fixme_cast<int>( cbBuffer * ( treeSize - 1 ) );
                displs[1] = reinterpret_cast<MPI_Aint>( pReq->nbc.tmpBuf );
                tmpTypes[1] = MPI_BYTE;
                NMPI_Type_create_struct(
                    2,
                    blocks,
                    displs,
                    tmpTypes,
                    &tmpType
                    );
                NMPI_Type_commit( &tmpType );
                datatype = TypeHandle::Create( tmpType );
                isHelperDatatype = true;
            }

            mpi_errno = pReq->nbc.tasks[iCurrTask].InitIrecv(
                pRecv,
                count,
                datatype,
                targetRank,
                isHelperDatatype
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
            piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnComplete;
            ++pReq->nbc.nTasks;
            ++iCurrTask;

            return MPI_SUCCESS;
        },
        rank,
        size,
        relativeRank
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    mpi_errno = BinomialChildBuilderDescending(
        [&]( _In_range_( >=, 1 ) unsigned offset )
        {
            *piNext = iCurrTask;

            unsigned relativeTarget = relativeRank + offset;
            unsigned targetRank = pComm->RankAdd( relativeTarget, root );
            unsigned childTreeSize = TreeSize( relativeTarget, size );
            const BYTE* pSend;
            size_t count;
            TypeHandle datatype = g_hBuiltinTypes.MPI_Byte;
            bool isHelperDatatype = false;

            if( rank == root )
            {
                if( numTmpBufBlocks > 0 )
                {
                    //
                    // Root, send from the scratch buffer which contains rotated
                    // sendbuf order by relative rank without the root's data.
                    //
                    pSend = pReq->nbc.tmpBuf + cbBuffer * ( relativeTarget - 1 );
                    count = cbBuffer * childTreeSize;
                }
                else
                {
                    if( targetRank + childTreeSize <= size )
                    {
                        //
                        // No scratch buffer is allocated.
                        // Small message size with no data rotation or
                        // large message size but the child's subtree doesn't
                        // cross the boundary, send from the sendbuf directly.
                        //
                        pSend = static_cast<const BYTE*>( sendbuf ) + extent * sendcnt * targetRank;
                        count = sendcnt * childTreeSize;
                        datatype = sendtype;
                    }
                    else
                    {
                        //
                        // Large message size and the child's subtree crosses the
                        // boundary, send from the sendbuf with the help of an
                        // indexed data type.
                        //
                        pSend = static_cast<const BYTE*>( sendbuf );
                        count = 1;

                        int blocks[2];
                        int displs[2];
                        MPI_Datatype tmpType;

                        blocks[0] = ( size - targetRank ) * sendcnt;
                        displs[0] = targetRank * sendcnt;
                        blocks[1] = ( targetRank + childTreeSize - size ) * sendcnt;
                        displs[1] = 0;
                        NMPI_Type_indexed(
                            2,
                            blocks,
                            displs,
                            sendtype.GetMpiHandle(),
                            &tmpType
                            );
                        NMPI_Type_commit( &tmpType );
                        datatype = TypeHandle::Create( tmpType );
                        isHelperDatatype = true;
                    }
                }
            }
            else
            {
                //
                // Not root, use the scratch buffer.
                //
                if( cbBuffer < MPIR_scatter_vsmall_msg )
                {
                    //
                    // Small message size, the rank's own data is received in
                    // the scratch buffer as well.
                    //
                    pSend = pReq->nbc.tmpBuf + cbBuffer * offset;
                }
                else
                {
                    //
                    // Large message size, the rank's own data is received in
                    // its recvbuf, not in the scratch buffer.
                    //
                    pSend = pReq->nbc.tmpBuf + cbBuffer * ( offset - 1 );
                }

                count = cbBuffer * childTreeSize;
            }

            mpi_errno = pReq->nbc.tasks[iCurrTask].InitIsend(
                pSend,
                count,
                datatype,
                fixme_cast<int>( targetRank ),
                pComm,
                isHelperDatatype
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pReq->nbc.tasks[iCurrTask].m_iNextOnComplete = NBC_TASK_NONE;
            piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnInit;
            ++pReq->nbc.nTasks;
            ++iCurrTask;

            return MPI_SUCCESS;
        },
        childCount
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( rank == root )
    {
        if( !isInPlace )
        {
            //
            // Root, not in place, take care of root's recvbuf here.
            //
            pReq->nbc.tasks[iCurrTask].InitLocalCopy(
                static_cast<const BYTE*>( sendbuf ) + extent * sendcnt * rank,
                recvbuf,
                sendcnt,
                recvcnt,
                sendtype,
                recvtype
                );
            *piNext = iCurrTask;
            pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
            piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnComplete;
            ++pReq->nbc.nTasks;
            ++iCurrTask;
        }
    }
    else if( childCount > 0 && cbBuffer < MPIR_scatter_vsmall_msg )
    {
        //
        // Not root and not leaf, small message size, copy its data from scratch
        // buffer to recvbuf.
        //
        pReq->nbc.tasks[iCurrTask].InitLocalCopy(
            pReq->nbc.tmpBuf,
            recvbuf,
            cbBuffer,
            recvcnt,
            g_hBuiltinTypes.MPI_Byte,
            recvtype
            );
        *piNext = iCurrTask;
        pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnComplete;
        ++pReq->nbc.nTasks;
    }

    *piNext = NBC_TASK_NONE;
    MPIU_Assert( pReq->nbc.nTasks == nTasks );

    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IscatterBuildIntra(
    _When_(root == pComm->rank, _In_opt_)
             const void*     sendbuf,
    _When_(root == pComm->rank, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(((pComm->rank == root && sendcnt != 0) || (pComm->rank != root && recvcnt != 0)) || _Old_(recvbuf) != MPI_IN_PLACE, _Out_opt_)
             void*           recvbuf,
    _When_(root != pComm->rank || recvbuf != MPI_IN_PLACE, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _In_range_(0, pComm->remote_size - 1)
             int             root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    tag,
    _Outptr_ MPID_Request**  ppRequest
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

    if( ( pComm->rank == root && sendcnt == 0 ) ||
         ( pComm->rank != root && recvcnt == 0 ) )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        //
        // fix OACR warning 6101 for recvbuf
        //
        OACR_USE_PTR(recvbuf);
    }
    else if( pComm->remote_size == 1 )
    {
        if( recvbuf != MPI_IN_PLACE )
        {
            pRequest->nbc.tag = tag;
            pRequest->nbc.nTasks = 1;
            pRequest->nbc.tasks = new NbcTask[1];
            if (pRequest->nbc.tasks == nullptr)
            {
                return MPIU_ERR_NOMEM();
            }
            pRequest->nbc.tasks[0].InitLocalCopy(
                sendbuf,
                recvbuf,
                sendcnt,
                recvcnt,
                sendtype,
                recvtype
            );
            pRequest->nbc.tasks[0].m_iNextOnInit = NBC_TASK_NONE;
            pRequest->nbc.tasks[0].m_iNextOnComplete = NBC_TASK_NONE;
        }
    }
    else
    {
        mpi_errno = IscatterBuildTasklist(
            pRequest.get(),
            sendbuf,
            sendcnt,
            sendtype,
            recvbuf,
            recvcnt,
            recvtype,
            fixme_cast<unsigned>( root ),
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
MPIR_Iscatter_intra(
    _When_(root == pComm->rank, _In_opt_)
             const void*     sendbuf,
    _When_(root == pComm->rank, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(root != pComm->rank || _Old_(recvbuf) != MPI_IN_PLACE, _Out_opt_)
             void*           recvbuf,
    _When_(root != pComm->rank || recvbuf != MPI_IN_PLACE, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _In_range_(0, pComm->remote_size - 1)
             int             root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    tag,
    _Outptr_ MPID_Request**  ppRequest
    )
{
    MPID_Request* pRequest;
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
IscatterBuildInter(
    _When_(root == MPI_ROOT, _In_opt_)
             const void*     sendbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(root >= 0 && recvcnt > 0, _Out_opt_)
             void*           recvbuf,
    _When_(root >= 0, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _mpi_coll_rank_(root)
             int             root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    inTag,
    _Outptr_ MPID_Request**  ppRequest
    )
{
    //
    // Intercommunicator non-blocking scatter.
    // For short messages, root sends to rank 0 in remote group.
    // Rank 0 does a local intracommunicator scatter.
    // For long messages, we use linear scatter.
    //
    MPIU_Assert(pComm->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(pComm->inter.local_comm != nullptr);

    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag(inTag);

    unsigned remoteSize = pComm->remote_size;
    unsigned localSize = pComm->inter.local_size;
    size_t cbBuffer;
    MPI_Count extent;

    if( root == MPI_ROOT )
    {
        MPI_Count sendTypeSize = sendtype.GetSize();
        cbBuffer = fixme_cast<size_t>( sendTypeSize * sendcnt * remoteSize );
    }
    else
    {
        MPI_Count recvTypeSize = recvtype.GetSize();
        cbBuffer = fixme_cast<size_t>( recvTypeSize * recvcnt * localSize );
    }

    if( cbBuffer < pComm->SwitchPoints()->MPIR_scatter_short_msg )
    {
        if( root == MPI_ROOT && sendcnt > 0 )
        {
            MPID_Request* sreq = MPIDI_Request_create_sreq(
                MPIDI_REQUEST_TYPE_SEND,
                sendbuf,
                sendcnt * remoteSize,
                sendtype,
                0,
                tag | pComm->comm_kind,
                pComm
                );
            if( sreq == nullptr )
            {
                return MPIU_ERR_NOMEM();
            }

            *ppRequest = sreq;
            return MPI_SUCCESS;
        }

        if( root >= 0 && pComm->inter.local_comm->rank != 0 && recvcnt > 0 )
        {
            //
            // Non-root rank in the remote group, just do iscatter.
            //
            return IscatterBuildIntra(
                sendbuf,
                sendcnt,
                sendtype,
                recvbuf,
                recvcnt,
                recvtype,
                0,
                pComm->inter.local_comm,
                tag,
                ppRequest
                );
        }
    }
    else
    {
        if( root >= 0 && recvcnt > 0 )
        {
            //
            // All ranks in the remote group receive from root.
            //
            return MPID_Recv(
                recvbuf,
                recvcnt,
                recvtype,
                root,
                tag | pComm->comm_kind,
                pComm,
                ppRequest
                );
        }
    }

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    pRequest->comm = pComm;
    pComm->AddRef();
    pRequest->nbc.tag = tag;

    if( root == MPI_PROC_NULL ||
        ( root == MPI_ROOT && sendcnt == 0 ) ||
        ( root >= 0 && recvcnt == 0 ) )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    MPI_RESULT mpi_errno;

    if( cbBuffer < pComm->SwitchPoints()->MPIR_scatter_short_msg )
    {
        pRequest->nbc.tasks = new NbcTask[2];
        if( pRequest->nbc.tasks == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        //
        // Rank 0 of the remote group needs to allocate temp buffer to receive
        // from root.
        //
        MPI_Count trueLb;
        MPI_Count trueExtent = recvtype.GetTrueExtentAndLowerBound( &trueLb );
        extent = recvtype.GetExtent();
        size_t tmpBufSize;
        if( extent >= trueExtent )
        {
            tmpBufSize = fixme_cast<size_t>(extent * recvcnt * localSize);
        }
        else
        {
            tmpBufSize = fixme_cast<size_t>(trueExtent * recvcnt * localSize);
        }

        pRequest->nbc.tmpBuf = new BYTE[tmpBufSize];
        if( pRequest->nbc.tmpBuf == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        //
        // The datatype provided by the user may include padding (adjusted lower bound).
        // We allocated a temporary buffer for the contents of the datatype only (extent),
        // excluding padding before the data.  The dataloop logic will take the padding
        // into account so we need to shift the pointer accordingly so that we end up
        // transferring only the valid data.
        //
        void* tmpBuf = pRequest->nbc.tmpBuf - trueLb;

        //
        // Rank 0 in the remote group receives from root.
        //
        mpi_errno = pRequest->nbc.tasks[0].InitIrecv(
            tmpBuf,
            static_cast<size_t>(recvcnt) * localSize,
            recvtype,
            root
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pRequest->nbc.tasks[0].m_iNextOnInit = NBC_TASK_NONE;
        pRequest->nbc.tasks[0].m_iNextOnComplete = 1;
        pRequest->nbc.nTasks++;

        //
        // Rank 0 in the remote group, do intracomm iscatter.
        //
        mpi_errno = pRequest->nbc.tasks[1].InitIscatterIntra(
            tmpBuf,
            recvcnt,
            recvtype,
            recvbuf,
            recvcnt,
            recvtype,
            0,
            pComm->inter.local_comm,
            inTag
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            pRequest->cancel_nbc_tasklist();
            return mpi_errno;
        }
        pRequest->nbc.tasks[1].m_iNextOnInit = NBC_TASK_NONE;
        pRequest->nbc.tasks[1].m_iNextOnComplete = NBC_TASK_NONE;
        pRequest->nbc.nTasks++;
    }
    else
    {
        //
        // Long messages - use linear scatter.
        // Only MPI_ROOT can get here.
        //
        // OACR failed to figure this out and threw C6101 warning.
        // Force it to understand here.
        //
        __analysis_assume( root == MPI_ROOT );

        pRequest->nbc.tasks = new NbcTask[remoteSize];
        if( pRequest->nbc.tasks == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        extent = sendtype.GetExtent() * sendcnt;

        for( unsigned i = 0; i < remoteSize; i++ )
        {
            mpi_errno = pRequest->nbc.tasks[i].InitIsend(
                static_cast<const BYTE*>( sendbuf ) + extent * i,
                sendcnt,
                sendtype,
                i,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                pRequest->cancel_nbc_tasklist();
                return mpi_errno;
            }
            //
            // Initiate all the sends in parallel.
            //
            pRequest->nbc.tasks[i].m_iNextOnInit = i + 1;
            pRequest->nbc.tasks[i].m_iNextOnComplete = NBC_TASK_NONE;
            pRequest->nbc.nTasks++;
        }

        //
        // Fix the last task's m_iNextOnInit.
        //
        pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
    }

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Iscatter_inter(
    _When_(root == MPI_ROOT, _In_opt_)
             const void*     sendbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(root >= 0, _Out_opt_)
             void*           recvbuf,
    _When_(root >= 0, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _mpi_coll_rank_(root)
             int             root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    tag,
    _Outptr_ MPID_Request**  ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IscatterBuildInter(
        sendbuf,
        sendcnt,
        sendtype,
        recvbuf,
        recvcnt,
        recvtype,
        root,
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


/* This is the default implementation of scatterv. The algorithm is:

   Algorithm: MPI_Scatterv

   Since the array of sendcounts is valid only on the root, we cannot
   do a tree algorithm without first communicating the sendcounts to
   other processes. Therefore, we simply use a linear algorithm for the
   scatter, which takes (p-1) steps versus lgp steps for the tree
   algorithm. The bandwidth requirement is the same for both algorithms.

   Cost = (p-1).alpha + n.((p-1)/p).beta

   Possible improvements:

   End Algorithm: MPI_Scatterv
*/
MPI_RESULT
MPIR_Scatterv(
    _mpi_when_root_(comm_ptr, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_root_(comm_ptr, root, _In_)
              const int        sendcnts[],
    _mpi_when_root_(comm_ptr, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       sendtype,
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root == comm_ptr->rank && _Old_(recvbuf) != MPI_IN_PLACE && recvcnt > 0, _Out_opt_)
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root != comm_ptr->rank && recvcnt > 0, _Out_opt_)
    _When_(comm_ptr->comm_kind == MPID_INTERCOMM && root > 0 && recvcnt > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int rank;
    MPI_Count extent;
    int      i;

    rank = comm_ptr->rank;
    //
    // fix OACR warning 6101 for recvbuf
    //
    OACR_USE_PTR(recvbuf);

    /* If I'm the root, then scatter */
    if ((comm_ptr->comm_kind != MPID_INTERCOMM) && (rank == root))
    {
        /* intracomm root */
        int comm_size;

        comm_size = comm_ptr->remote_size;
        extent = sendtype.GetExtent();

        /* We could use Isend here, but since the receivers need to execute
           a simple Recv, it may not make much difference in performance,
           and using the blocking version is simpler */
        for ( i=0; i<root; i++ )
        {
            if (sendcnts[i] > 0)
            {
                mpi_errno = MPIC_Send(
                                static_cast<const BYTE *>(sendbuf) + extent * displs[i],
                                sendcnts[i],
                                sendtype,
                                i,
                                MPIR_SCATTERV_TAG,
                                comm_ptr
                                );
                ON_ERROR_FAIL(mpi_errno);
            }
        }
        if (recvbuf != MPI_IN_PLACE)
        {
            if (sendcnts[rank] > 0)
            {
                mpi_errno = MPIR_Localcopy(
                                static_cast<const BYTE *>(sendbuf) + extent * displs[rank],
                                sendcnts[rank],
                                sendtype,
                                recvbuf,
                                recvcnt,
                                recvtype
                                );
                ON_ERROR_FAIL(mpi_errno);
            }
        }
        for ( i=root+1; i<comm_size; i++ )
        {
            if (sendcnts[i] > 0)
            {
                mpi_errno = MPIC_Send(
                                static_cast<const BYTE *>(sendbuf) + extent * displs[i],
                                sendcnts[i],
                                sendtype,
                                i,
                                MPIR_SCATTERV_TAG,
                                comm_ptr
                                );
                ON_ERROR_FAIL(mpi_errno);
            }
        }
    }
    else if ((comm_ptr->comm_kind == MPID_INTERCOMM) && (root == MPI_ROOT))
    {
        /* intercommunicator root */
        int remote_comm_size;

        remote_comm_size = comm_ptr->remote_size;
        extent = sendtype.GetExtent();

        for (i=0; i<remote_comm_size; i++)
        {
            if (sendcnts[i] > 0)
            {
                mpi_errno = MPIC_Send(
                                static_cast<const BYTE *>(sendbuf) + extent * displs[i],
                                sendcnts[i],
                                sendtype,
                                i,
                                MPIR_SCATTERV_TAG,
                                comm_ptr
                                );
                ON_ERROR_FAIL(mpi_errno);
            }
        }
    }
    else if (root != MPI_PROC_NULL)
    {
        /* non-root nodes, and in the intercomm. case, non-root nodes on remote side */
        if (recvcnt > 0)
        {
            mpi_errno = MPIC_Recv(
                            recvbuf,
                            recvcnt,
                            recvtype,
                            root,
                            MPIR_SCATTERV_TAG,
                            comm_ptr,
                            MPI_STATUS_IGNORE
                            );
        }
    }

fn_fail:
    return (mpi_errno);
}


static inline
MPI_RESULT
InitIscattervSendTask(
    _In_opt_ const BYTE*   sendbuf,
    _In_     const int     sendcnts[],
    _In_     const int     displs[],
    _In_     unsigned      dst,
    _In_     MPI_Count     extent,
    _In_     TypeHandle    sendtype,
    _In_     MPID_Comm*    pComm,
    _In_     NbcTask**     ppCurTask,
    _In_     MPID_Request* pRequest
    )
{
    if( sendcnts[dst] > 0 )
    {
        MPI_RESULT mpi_errno = (*ppCurTask)->InitIsend(
            sendbuf + extent * displs[dst],
            sendcnts[dst],
            sendtype,
            dst,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            pRequest->cancel_nbc_tasklist();
            return mpi_errno;
        }
        //
        // Initiate all the sends in parallel.
        //
        (*ppCurTask)->m_iNextOnInit = ++pRequest->nbc.nTasks;
        (*ppCurTask)->m_iNextOnComplete = NBC_TASK_NONE;
        (*ppCurTask)++;
    }

    return MPI_SUCCESS;
}


MPI_RESULT
IscattervBuildBoth(
    _mpi_when_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        sendcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       sendtype,
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(recvbuf) != MPI_IN_PLACE && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && recvcnt > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_SCATTERV_TAG ) );

    if( ( ( pComm->comm_kind != MPID_INTERCOMM && pComm->rank != root ) ||
        ( pComm->comm_kind == MPID_INTERCOMM && root >= 0 ) ) &&
        recvcnt > 0 )
    {
        //
        // Intracomm non-root nodes or intercomm all nodes on the remote side
        // with the condition of recvcnt > 0.
        //
        return MPID_Recv(
            recvbuf,
            recvcnt,
            recvtype,
            root,
            tag | pComm->comm_kind,
            pComm,
            ppRequest
            );
    }

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pComm->AddRef();
    pRequest->nbc.tag = tag;

    if( root == MPI_PROC_NULL ||
        ( root != MPI_ROOT && recvcnt == 0 && ( pComm->comm_kind == MPID_INTERCOMM || pComm->rank != root ) ) )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    int size = pComm->remote_size;
    int i;
    unsigned taskCount = 0;
    MPI_Count extent = sendtype.GetExtent();
    MPI_RESULT mpi_errno;

    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        //
        // Intracomm and the current rank must be root if it reaches here.
        //
        MPIU_Assert( pComm->rank == root );

        if( recvbuf != MPI_IN_PLACE && recvcnt > 0 )
        {
            //
            // Need one task to local copy root's own data.
            //
            taskCount++;
        }

        //
        // Separate into two loops at root so that we don't need to make sure
        // it's not root at every iteration.
        //
        for( i = 0; i < root; i++ )
        {
            if( sendcnts[i] > 0 )
            {
                taskCount++;
            }
        }
        for( ++i; i < size; i++ )
        {
            if( sendcnts[i] > 0 )
            {
                taskCount++;
            }
        }

        if( taskCount == 0 )
        {
            pRequest->kind = MPID_REQUEST_NOOP;
            *ppRequest = pRequest.detach();
            return MPI_SUCCESS;
        }

        pRequest->nbc.tasks = new NbcTask[taskCount];
        if( pRequest->nbc.tasks == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
        NbcTask* curTask = pRequest->nbc.tasks;

        if( recvbuf != MPI_IN_PLACE && recvcnt > 0 )
        {
            //
            // Taking care of root's own data.
            //
            curTask->InitLocalCopy(
                static_cast<const BYTE*>( sendbuf ) + extent * displs[root],
                recvbuf,
                sendcnts[root],
                recvcnt,
                sendtype,
                recvtype
                );
            curTask->m_iNextOnInit = NBC_TASK_NONE;
            curTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
            curTask++;
        }

        //
        // Separate into two loops at root so that we don't need to make sure
        // it's not root at every iteration.
        //
        for( i = 0; i < root; i++ )
        {
            mpi_errno = InitIscattervSendTask(
                static_cast<const BYTE*>( sendbuf ),
                sendcnts,
                displs,
                i,
                extent,
                sendtype,
                pComm,
                &curTask,
                pRequest.get()
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        for( ++i; i < size; i++ )
        {
            mpi_errno = InitIscattervSendTask(
                static_cast<const BYTE*>( sendbuf ),
                sendcnts,
                displs,
                i,
                extent,
                sendtype,
                pComm,
                &curTask,
                pRequest.get()
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }

        //
        // Fix the last task's m_iNextOnInit. Also need to fix its
        // m_iNextOnComplete for the single rank non-MPI_IN_PLACE case
        // where local copy is the only task in the task list.
        //
        pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
        pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    }
    else
    {
        //
        // Intercomm and the current must be root if it reaches here.
        //
        MPIU_Assert( root == MPI_ROOT );

        for( i = 0; i < size; i++ )
        {
            if( sendcnts[i] > 0 )
            {
                taskCount++;
            }
        }

        if( taskCount == 0 )
        {
            pRequest->kind = MPID_REQUEST_NOOP;
            *ppRequest = pRequest.detach();
            return MPI_SUCCESS;
        }

        pRequest->nbc.tasks = new NbcTask[taskCount];
        if( pRequest->nbc.tasks == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
        NbcTask* curTask = pRequest->nbc.tasks;

        for( i = 0; i < size; i++ )
        {
            mpi_errno = InitIscattervSendTask(
                static_cast<const BYTE*>( sendbuf ),
                sendcnts,
                displs,
                i,
                extent,
                sendtype,
                pComm,
                &curTask,
                pRequest.get()
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }

        //
        // Fix the last task's m_iNextOnInit.
        //
        pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
    }

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


MPI_RESULT
MPIR_Iscatterv(
    _mpi_when_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        sendcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       sendtype,
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(recvbuf) != MPI_IN_PLACE && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && recvcnt > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
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

