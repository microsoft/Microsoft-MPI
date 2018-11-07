// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* This is the default implementation of gather. The algorithm is:

   Algorithm: MPI_Gather

   We use a binomial tree algorithm for both short and
   long messages. At nodes other than leaf nodes we need to allocate
   a temporary buffer to store the incoming message. If the root is
   not rank 0, we receive data in a temporary buffer on the root and
   then reorder it into the right order. In the heterogeneous case
   we first pack the buffers by using MPI_Pack and then do the gather.

   Cost = lgp.alpha + n.((p-1)/p).beta
   where n is the total size of the data gathered at the root.

   Possible improvements:

   End Algorithm: MPI_Gather
*/
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Gather_intra(
    _mpi_when_not_root_(comm_ptr, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_not_root_(comm_ptr, root, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(comm_ptr, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(comm_ptr, root, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    )
{
    int        comm_size, rank;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int relative_rank, mask, src, dst, relative_src;
    MPI_Count curr_cnt=0, nbytes, sendtype_size, recvtype_size;
    int recvblks;
    MPI_Count tmp_buf_size;
    int missing;
    StackGuardArray<BYTE> auto_buf;
    BYTE* tmp_buf=NULL;
    MPI_Status status;
    MPI_Count   extent=0;            /* Datatype extent */
    int blocks[2];
    int displs[2];
    MPI_Aint struct_displs[2];
    MPI_Datatype types[2], tmp_type;
    int copy_offset = 0, copy_blks = 0;
    int MPIR_gather_vsmall_msg = comm_ptr->SwitchPoints()->MPIR_gather_vsmall_msg;

    MPIU_Assert(comm_ptr->comm_kind != MPID_INTERCOMM);

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    if ( ((rank == root) && (recvcnt == 0)) ||
         ((rank != root) && (sendcnt == 0)) )
    {
        return MPI_SUCCESS;
    }

    /* Use binomial tree algorithm. */

    relative_rank = (rank >= root) ? rank - root : rank - root + comm_size;

    if (rank == root)
    {
        extent = recvtype.GetExtent();
    }

    /* communicator is homogeneous. no need to pack buffer. */

    if (rank == root)
    {
        recvtype_size = recvtype.GetSize();
        nbytes = recvtype_size * recvcnt;
    }
    else
    {
        sendtype_size = sendtype.GetSize();
        nbytes = sendtype_size * sendcnt;
    }

    /* Find the number of missing nodes in my sub-tree compared to
     * a balanced tree */
    mask = 1;
    while( mask < comm_size )
    {
        mask <<= 1;
    }
    --mask;

    while (relative_rank & mask)
    {
        mask >>= 1;
    }

    missing = (relative_rank | mask) - comm_size + 1;

    if (missing < 0)
    {
        missing = 0;
    }

    tmp_buf_size = (mask - missing);

    /* If the message is smaller than the threshold, we will copy
     * our message in there too */
    if (nbytes < MPIR_gather_vsmall_msg)
    {
        tmp_buf_size++;
    }

    tmp_buf_size *= nbytes;

    /* For zero-ranked root, we don't need any temporary buffer */
    if ((rank == root) && (!root || (nbytes >= MPIR_gather_vsmall_msg)))
    {
        tmp_buf_size = 0;
    }

    if (tmp_buf_size)
    {
        auto_buf = new BYTE[static_cast<size_t>(tmp_buf_size)];
        if( auto_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        tmp_buf = auto_buf;
    }

    if (rank == root)
    {
        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MPIR_Localcopy(sendbuf, sendcnt, sendtype,
                                       (static_cast<char *>(recvbuf) + extent * recvcnt * rank), recvcnt, recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }
    }
    else if (tmp_buf_size && (nbytes < MPIR_gather_vsmall_msg))
    {
        /* copy from sendbuf into tmp_buf */
        mpi_errno = MPIR_Localcopy(sendbuf, sendcnt, sendtype,
                                   tmp_buf, fixme_cast<int>(nbytes), g_hBuiltinTypes.MPI_Byte);
        ON_ERROR_FAIL(mpi_errno);
    }
    curr_cnt = nbytes;

    mask = 0x1;
    while (mask < comm_size)
    {
        if ((mask & relative_rank) == 0)
        {
            src = relative_rank | mask;
            if (src < comm_size)
            {
                src = (src + root) % comm_size;

                if (rank == root)
                {
                    recvblks = mask;
                    if ((2 * recvblks) > comm_size)
                        recvblks = comm_size - recvblks;

                    if ((rank + mask + recvblks == comm_size) ||
                        (((rank + mask) % comm_size) <
                         ((rank + mask + recvblks) % comm_size)))
                    {
                        /* If the data contiguously fits into the
                         * receive buffer, place it directly. This
                         * should cover the case where the root is
                         * rank 0. */
                        mpi_errno = MPIC_Recv((static_cast<char *>(recvbuf) +
                                               extent*((rank + mask) % comm_size)*recvcnt),
                                              recvblks * recvcnt, recvtype, src,
                                              MPIR_GATHER_TAG, comm_ptr,
                                              &status);
                    }
                    else if (nbytes < MPIR_gather_vsmall_msg)
                    {
                        mpi_errno = MPIC_Recv(tmp_buf, fixme_cast<int>(recvblks * nbytes), g_hBuiltinTypes.MPI_Byte,
                                              src, MPIR_GATHER_TAG, comm_ptr, &status);
                        copy_offset = rank + mask;
                        copy_blks = recvblks;
                    }
                    else
                    {
                        blocks[0] = recvcnt * (comm_size - root - mask);
                        displs[0] = recvcnt * (root + mask);
                        blocks[1] = (recvcnt * recvblks) - blocks[0];
                        displs[1] = 0;

                        NMPI_Type_indexed(2, blocks, displs, recvtype.GetMpiHandle(), &tmp_type);
                        NMPI_Type_commit(&tmp_type);

                        mpi_errno = MPIC_Recv(
                            recvbuf,
                            1,
                            TypeHandle::Create( tmp_type ),
                            src,
                            MPIR_GATHER_TAG,
                            comm_ptr,
                            &status
                            );

                        NMPI_Type_free(&tmp_type);
                    }
                }
                else /* Intermediate nodes store in temporary buffer */
                {
                    MPI_Count offset;

                    /* Estimate the amount of data that is going to come in */
                    recvblks = mask;
                    relative_src = ((src - root) < 0) ? (src - root + comm_size) : (src - root);
                    if (relative_src + mask > comm_size)
                        recvblks -= (relative_src + mask - comm_size);

                    if (nbytes < MPIR_gather_vsmall_msg)
                        offset = mask * nbytes;
                    else
                        offset = (mask - 1) * nbytes;

                    mpi_errno = CheckIntRange(recvblks * nbytes);
                    if (mpi_errno != MPI_SUCCESS)
                    {
                        return mpi_errno;
                    }
                    mpi_errno = MPIC_Recv((tmp_buf + offset),
                                          fixme_cast<int>(recvblks * nbytes), g_hBuiltinTypes.MPI_Byte, src,
                                          MPIR_GATHER_TAG, comm_ptr,
                                          &status);
                    curr_cnt += (recvblks * nbytes);
                }
                ON_ERROR_FAIL(mpi_errno);
            }
        }
        else
        {
            dst = relative_rank ^ mask;
            dst = (dst + root) % comm_size;

            if (!tmp_buf_size)
            {
                /* leaf nodes send directly from sendbuf */
                mpi_errno = MPIC_Send(sendbuf, sendcnt, sendtype, dst,
                                      MPIR_GATHER_TAG, comm_ptr);
            }
            else if (nbytes < MPIR_gather_vsmall_msg)
            {
                mpi_errno = MPIC_Send(tmp_buf, fixme_cast<int>(curr_cnt), g_hBuiltinTypes.MPI_Byte, dst,
                                      MPIR_GATHER_TAG, comm_ptr);
            }
            else
            {
                blocks[0] = sendcnt;
                struct_displs[0] = (MPI_Aint) sendbuf;
                types[0] = sendtype.GetMpiHandle();
                mpi_errno = CheckIntRange(curr_cnt - nbytes);
                if (mpi_errno != MPI_SUCCESS)
                {
                    return mpi_errno;
                }
                blocks[1] = fixme_cast<int>(curr_cnt - nbytes);
                struct_displs[1] = (MPI_Aint) tmp_buf;
                types[1] = MPI_BYTE;

                NMPI_Type_create_struct(2, blocks, struct_displs, types, &tmp_type);
                NMPI_Type_commit(&tmp_type);

                mpi_errno = MPIC_Send(
                    MPI_BOTTOM,
                    1,
                    TypeHandle::Create( tmp_type ),
                    dst,
                    MPIR_GATHER_TAG,
                    comm_ptr
                    );

                NMPI_Type_free(&tmp_type);
            }
            ON_ERROR_FAIL(mpi_errno);

            break;
        }
        mask <<= 1;
    }

    if ((rank == root) && root && (nbytes < MPIR_gather_vsmall_msg) && copy_blks)
    {
        /* reorder and copy from tmp_buf into recvbuf */
        MPIR_Localcopy(tmp_buf,
                       fixme_cast<int>(nbytes * (comm_size - copy_offset)), g_hBuiltinTypes.MPI_Byte,
                       (static_cast<char *>(recvbuf) + extent * recvcnt * copy_offset),
                       recvcnt * (comm_size - copy_offset), recvtype);
        MPIR_Localcopy(tmp_buf + nbytes * (comm_size - copy_offset),
                       fixme_cast<int>(nbytes * (copy_blks - comm_size + copy_offset)), g_hBuiltinTypes.MPI_Byte,
                       recvbuf,
                       recvcnt * (copy_blks - comm_size + copy_offset), recvtype);
    }


 fn_fail:
    return (mpi_errno);
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Gather_inter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root >= 0, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root) int  root,
    _In_      const MPID_Comm* comm_ptr
    )
{
/*  Intercommunicator gather.
    For short messages, remote group does a local intracommunicator
    gather to rank 0. Rank 0 then sends data to root.

    Cost: (lgp+1).alpha + n.((p-1)/p).beta + n.beta

    For long messages, we use linear gather to avoid the extra n.beta.

    Cost: p.alpha + n.beta
*/
    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    int rank, local_size, remote_size;
    MPI_RESULT mpi_errno=MPI_SUCCESS;
    int i;
    MPI_Count nbytes, sendtype_size, recvtype_size;
    MPI_Status status;
    MPI_Count extent, true_extent;
    MPI_Count true_lb = 0;
    StackGuardArray<BYTE> auto_buf;
    BYTE *tmp_buf=NULL;

    int MPIR_gather_short_msg = comm_ptr->SwitchPoints()->MPIR_gather_short_msg;

    if (root == MPI_PROC_NULL)
    {
        /* local processes other than root do nothing */
        return MPI_SUCCESS;
    }

    remote_size = comm_ptr->remote_size;
    local_size = comm_ptr->inter.local_size;

    if (root == MPI_ROOT)
    {
        recvtype_size = recvtype.GetSize();
        nbytes = recvtype_size * recvcnt * remote_size;
    }
    else
    {
        /* remote side */
        sendtype_size = sendtype.GetSize();
        nbytes = sendtype_size * sendcnt * local_size;
    }

    if (nbytes < MPIR_gather_short_msg)
    {
        if (root == MPI_ROOT)
        {
            /* root receives data from rank 0 on remote group */
            mpi_errno = MPIC_Recv(recvbuf, recvcnt*remote_size,
                                  recvtype, 0, MPIR_GATHER_TAG, comm_ptr,
                                  &status);
            return mpi_errno;
        }
        else
        {
            /* remote group. Rank 0 allocates temporary buffer, does
               local intracommunicator gather, and then sends the data
               to root. */

            rank = comm_ptr->rank;

            if (rank == 0)
            {
                true_extent = sendtype.GetTrueExtentAndLowerBound( &true_lb );
                extent = sendtype.GetExtent();

                auto_buf = new BYTE[static_cast<size_t>((max(extent,true_extent))*sendcnt*local_size)];
                if( auto_buf == nullptr )
                {
                    mpi_errno = MPIU_ERR_NOMEM();
                    goto fn_fail;
                }

                /* adjust for potential negative lower bound in datatype */
                tmp_buf = auto_buf - true_lb;
            }

            /* now do the a local gather on the local intracommunicator */
            mpi_errno = MPIR_Gather_intra(sendbuf, sendcnt, sendtype,
                                    tmp_buf, sendcnt, sendtype, 0,
                                    comm_ptr->inter.local_comm);
            if (rank == 0)
            {
                mpi_errno = MPIC_Send(tmp_buf, sendcnt*local_size,
                                      sendtype, root,
                                      MPIR_GATHER_TAG, comm_ptr);
                ON_ERROR_FAIL(mpi_errno);

            }
        }
    }
    else
    {
        /* long message. use linear algorithm. */
        if (root == MPI_ROOT)
        {
            extent = recvtype.GetExtent();
            for (i=0; i<remote_size; i++)
            {
                mpi_errno = MPIC_Recv((static_cast<char *>(recvbuf) + extent * recvcnt * i),
                                      recvcnt, recvtype, i,
                                      MPIR_GATHER_TAG, comm_ptr, &status);
                ON_ERROR_FAIL(mpi_errno);
            }
        }
        else
        {
            mpi_errno = MPIC_Send(sendbuf,sendcnt,sendtype,root,
                                  MPIR_GATHER_TAG,comm_ptr);
        }
    }

fn_fail:
    return mpi_errno;
}


static
unsigned
IgatherTaskCount(
    _In_range_( <, pComm->remote_size ) unsigned root,
    _In_range_( >, 0 ) size_t cbBuffer,
    _In_ bool isInPlace,
    _In_ MPID_Comm* pComm
    )
{
    unsigned size = pComm->remote_size;

    MPIU_Assert( root < size );
    MPIU_Assert( cbBuffer > 0 );

    unsigned rank = pComm->rank;
    unsigned relativeRank = pComm->RankSub( root );
    unsigned treeSize;
    unsigned childCount = ChildCount( relativeRank, size );

    //
    // Receive from all the children.
    //
    unsigned taskCount = childCount;
    unsigned childRank;
    unsigned childRelativeRank;
    unsigned MPIR_gather_vsmall_msg = pComm->SwitchPoints()->MPIR_gather_vsmall_msg;

    if( rank == root )
    {
        if( !isInPlace )
        {
            //
            // Root, not in place, needs one task to copy root's send buffer
            // to root's receive buffer.
            //
            ++taskCount;
        }

        if( root != 0 && cbBuffer < MPIR_gather_vsmall_msg )
        {
            while( childCount != 0 )
            {
                childRelativeRank = relativeRank + ( 1 << ( --childCount ) );
                childRank = pComm->RankAdd( root, childRelativeRank );
                treeSize = TreeSize( childRelativeRank, size );
                if( childRank + treeSize > size )
                {
                    //
                    // Small message size and data rotation happens. Need two tasks
                    // to copy from the scratch buffer to the receive buffer.
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
        // Not root, needs one task to send to the parent.
        //
        ++taskCount;

        if( childCount > 0 && cbBuffer < MPIR_gather_vsmall_msg )
        {
            //
            // Not leaf, small message size, need one task to copy the send
            // buffer to the scratch buffer.
            //
            ++taskCount;
        }
    }

    return taskCount;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IgatherBuildTaskList(
    _Inout_   MPID_Request*    pReq,
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
              unsigned         root,
    _In_      MPID_Comm*       pComm
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_GATHER_TAG ) );

    unsigned size = pComm->remote_size;
    unsigned MPIR_gather_vsmall_msg = pComm->SwitchPoints()->MPIR_gather_vsmall_msg;
    MPI_Count extent = 0;
    size_t cbBuffer = 0;
    MPI_RESULT mpi_errno;

    unsigned rank = pComm->rank;
    unsigned relativeRank = pComm->RankSub( root );
    unsigned childCount = ChildCount( relativeRank, size );
    unsigned treeSize = TreeSize( relativeRank, size );
    unsigned numTmpBufBlocks = treeSize - 1;

    if( rank == root )
    {
        extent = recvtype.GetExtent();
        MPI_Count recvTypeSize = recvtype.GetSize();
        cbBuffer = fixme_cast<size_t>( recvTypeSize * recvcnt );
    }
    else
    {
        MPI_Count sendTypeSize = sendtype.GetSize();
        cbBuffer = fixme_cast<size_t>( sendTypeSize * sendcnt );

        if( numTmpBufBlocks > 0 && cbBuffer < MPIR_gather_vsmall_msg )
        {
            //
            // Copy self data to the scratch buffer for small message size.
            //
            ++numTmpBufBlocks;
        }
    }

    bool isInPlace = ( sendbuf == MPI_IN_PLACE );
    unsigned nTasks = IgatherTaskCount( root, cbBuffer, isInPlace, pComm );
    if( rank == root && nTasks <= childCount + 1 )
    {
        //
        // Root, large message or no data rotation happens, don't need the
        // scratch buffer.
        //
        numTmpBufBlocks = 0;
    }

    if( numTmpBufBlocks > 0 )
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

    if( rank == root )
    {
        if( !isInPlace )
        {
            pReq->nbc.tasks[iCurrTask].InitLocalCopy(
                sendbuf,
                static_cast<BYTE*>( recvbuf ) + extent * recvcnt * rank,
                sendcnt,
                recvcnt,
                sendtype,
                recvtype
                );
            pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
            ++pReq->nbc.nTasks;
            ++iCurrTask;
        }
    }
    else if( numTmpBufBlocks > 0 && cbBuffer < MPIR_gather_vsmall_msg )
    {
        //
        // Copy self buffer into the scratch buffer.
        //
        pReq->nbc.tasks[iCurrTask].InitLocalCopy(
            sendbuf,
            pReq->nbc.tmpBuf,
            sendcnt,
            cbBuffer,
            sendtype,
            g_hBuiltinTypes.MPI_Byte
            );
        pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
        ++pReq->nbc.nTasks;
        ++iCurrTask;
    }

    unsigned copyOffset = 0;
    unsigned copyBlocks = 0;

    mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = iCurrTask;

            unsigned relativeTarget = relativeRank + offset;
            unsigned targetRank = pComm->RankAdd( relativeTarget, root );
            unsigned childTreeSize = TreeSize( relativeTarget, size );
            BYTE* pRecv;
            size_t count;
            TypeHandle datatype = g_hBuiltinTypes.MPI_Byte;
            bool isHelperDatatype = false;

            if( rank == root )
            {
                if( targetRank + childTreeSize <= size )
                {
                    //
                    // The child's subtree doesn't cross the boundary, just
                    // receive everything into the receive buffer directly.
                    //
                    pRecv = static_cast<BYTE*>( recvbuf ) + extent * recvcnt * targetRank;
                    count = childTreeSize * recvcnt;
                    datatype = recvtype;
                }
                else if( cbBuffer < MPIR_gather_vsmall_msg )
                {
                    //
                    // Small message size and the child's subtree crosses the
                    // boundary, receive it into the scratch buffer for now.
                    // Data in the scratch buffer will be copied into the
                    // receive buffer at the end in two batches.
                    //
                    pRecv = pReq->nbc.tmpBuf;
                    count = childTreeSize * cbBuffer;

                    copyOffset = targetRank;
                    copyBlocks = childTreeSize;
                }
                else
                {
                    //
                    // Large message size and the child's subtree crosses the
                    // boundary, receive it into the receive buffer with the
                    // help of an indexed data type.
                    //
                    pRecv = static_cast<BYTE*>( recvbuf );
                    count = 1;

                    int blocks[2];
                    int displs[2];
                    MPI_Datatype tmpType;

                    mpi_errno = CheckIntRange(static_cast<MPI_Count>(recvcnt) * (size - targetRank));
                    if (mpi_errno != MPI_SUCCESS)
                    {
                        return mpi_errno;
                    }
                    blocks[0] = ( size - targetRank ) * recvcnt;

                    mpi_errno = CheckIntRange(static_cast<MPI_Count>(recvcnt) * targetRank);
                    if (mpi_errno != MPI_SUCCESS)
                    {
                        return mpi_errno;
                    }
                    displs[0] = recvcnt * targetRank;

                    mpi_errno = CheckIntRange(static_cast<MPI_Count>(recvcnt)* (targetRank + childTreeSize - size));
                    if (mpi_errno != MPI_SUCCESS)
                    {
                        return mpi_errno;
                    }
                    blocks[1] = recvcnt * (targetRank + childTreeSize - size);
                    
                    displs[1] = 0;
                    NMPI_Type_indexed(
                        2,
                        blocks,
                        displs,
                        recvtype.GetMpiHandle(),
                        &tmpType
                        );
                    NMPI_Type_commit( &tmpType );
                    datatype = TypeHandle::Create( tmpType );
                    isHelperDatatype = true;
                }
            }
            else
            {
                //
                // Not root, use the scratch buffer.
                //
                if( cbBuffer < MPIR_gather_vsmall_msg )
                {
                    //
                    // Small message size, the rank's own data has been copied
                    // into the scratch buffer as well.
                    //
                    pRecv = pReq->nbc.tmpBuf + offset * cbBuffer;
                }
                else
                {
                    //
                    // Large message size, the rank's own data is not in the
                    // scratch buffer.
                    //
                    pRecv = pReq->nbc.tmpBuf + ( offset - 1 ) * cbBuffer;
                }

                count = childTreeSize * cbBuffer;
            }

            mpi_errno = pReq->nbc.tasks[iCurrTask].InitIrecv(
                pRecv,
                count,
                datatype,
                fixme_cast<int>( targetRank ),
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

    if( iCurrTask > 0 )
    {
        //
        // Not leaf, fix the last receive task to block before we send to
        // parent or in the root case, local copy scratch buffer to receive
        // buffer if needed.
        //
        pReq->nbc.tasks[iCurrTask - 1].m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pReq->nbc.tasks[iCurrTask - 1].m_iNextOnComplete;
    }

    mpi_errno = BinomialParentBuilder(
        [&]( _In_range_( >= , 0 ) int targetRank )
        {
            const BYTE* pSend;
            size_t count;
            TypeHandle datatype = g_hBuiltinTypes.MPI_Byte;
            bool isHelperDatatype = false;

            if( treeSize == 1 )
            {
                //
                // Leaf, send directly from sendbuf.
                //
                pSend = static_cast<const BYTE*>( sendbuf );
                count = sendcnt;
                datatype = sendtype;
            }
            else if( cbBuffer < MPIR_gather_vsmall_msg )
            {
                //
                // Not leaf, small message size, send the scratch buffer.
                //
                pSend = pReq->nbc.tmpBuf;
                count = treeSize * cbBuffer;
            }
            else
            {
                //
                // Not leaf, large message size, send rank's own data and the
                // scratch buffer with the help of a struct data type.
                //
                pSend = static_cast<BYTE*>( MPI_BOTTOM );
                count = 1;

                int blocks[2];
                MPI_Aint displs[2];
                MPI_Datatype tmpType;
                MPI_Datatype tmpTypes[2];

                blocks[0] = sendcnt;
                displs[0] = reinterpret_cast<MPI_Aint>( sendbuf );
                tmpTypes[0] = sendtype.GetMpiHandle();

                mpi_errno = CheckIntRange((treeSize - 1) * cbBuffer);
                if (mpi_errno != MPI_SUCCESS)
                {
                    return mpi_errno;
                }
                blocks[1] = fixme_cast<int>( ( treeSize - 1 ) * cbBuffer );
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

            mpi_errno = pReq->nbc.tasks[iCurrTask].InitIsend(
                pSend,
                count,
                datatype,
                targetRank,
                pComm,
                isHelperDatatype
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            *piNext = iCurrTask;
            pReq->nbc.tasks[iCurrTask].m_iNextOnComplete = NBC_TASK_NONE;
            piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnInit;
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

    if( rank == root && copyBlocks > 0 )
    {
        //
        // Reorder and copy scratch buffer into receive buffer.
        //
        pReq->nbc.tasks[iCurrTask].InitLocalCopy(
            pReq->nbc.tmpBuf,
            static_cast<BYTE*>( recvbuf ) + extent * recvcnt * copyOffset,
            ( size - copyOffset ) * cbBuffer,
            ( size - copyOffset ) * static_cast<size_t>(recvcnt),
            g_hBuiltinTypes.MPI_Byte,
            recvtype
            );
        *piNext = iCurrTask;
        pReq->nbc.tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pReq->nbc.tasks[iCurrTask].m_iNextOnComplete;
        ++pReq->nbc.nTasks;
        ++iCurrTask;

        pReq->nbc.tasks[iCurrTask].InitLocalCopy(
            pReq->nbc.tmpBuf + ( size - copyOffset ) * cbBuffer,
            recvbuf,
            ( copyBlocks + copyOffset - size ) * cbBuffer,
            ( copyBlocks + copyOffset - size ) * static_cast<size_t>(recvcnt),
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
IgatherBuildIntra(
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
    pRequest->comm = pComm;
    pRequest->comm->AddRef();

    MPI_RESULT mpi_errno;

    if( ( pComm->rank == root && recvcnt == 0 ) ||
         ( pComm->rank != root && sendcnt == 0 ) )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        //
        // fix OACR warning 6101 for recvbuf
        //
        OACR_USE_PTR(recvbuf);
    }
    else if( pComm->remote_size == 1 )
    {
        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MPIR_Localcopy(
                sendbuf,
                sendcnt,
                sendtype,
                recvbuf,
                recvcnt,
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
        mpi_errno = IgatherBuildTaskList(
            pRequest.get(),
            sendbuf,
            sendcnt,
            sendtype,
            recvbuf,
            recvcnt,
            recvtype,
            fixme_cast<unsigned>( root ),
            pComm
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
MPIR_Igather_intra(
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
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IgatherBuildIntra(
        sendbuf,
        sendcnt,
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


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
static
MPI_RESULT
IgatherBuildInter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root >= 0, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // Intercommunicator non-blocking gather.
    // For short messages, remote group does a local intracommunicator
    // gather to rank 0. Rank 0 then sends data to root.
    // For long messages, we use linear gather.
    //
    MPIU_Assert(pComm->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(pComm->inter.local_comm != nullptr);

    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_GATHER_TAG ) );

    unsigned remoteSize = pComm->remote_size;
    unsigned localSize = pComm->inter.local_size;
    size_t cbBuffer;
    MPI_Aint extent;

    if( root == MPI_ROOT )
    {
        MPI_Count recvTypeSize = recvtype.GetSize();
        cbBuffer = fixme_cast<size_t>( recvTypeSize * recvcnt * remoteSize );
    }
    else
    {
        MPI_Count sendTypeSize = sendtype.GetSize();
        cbBuffer = fixme_cast<size_t>( sendTypeSize * sendcnt * localSize );
    }

    if( cbBuffer < pComm->SwitchPoints()->MPIR_gather_short_msg )
    {
        if( root == MPI_ROOT && recvcnt > 0 )
        {
            return MPID_Recv(
                recvbuf,
                recvcnt * remoteSize,
                recvtype,
                0,
                tag | pComm->comm_kind,
                pComm,
                ppRequest
                );
        }

        if( root >= 0 && pComm->inter.local_comm->rank != 0 && sendcnt > 0 )
        {
            //
            // Non-root rank in the remote group, just do igather.
            //
            return IgatherBuildIntra(
                sendbuf,
                sendcnt,
                sendtype,
                recvbuf,
                recvcnt,
                recvtype,
                0,
                pComm->inter.local_comm,
                ppRequest
                );
        }
    }
    else
    {
        if( root >= 0 && sendcnt > 0 )
        {
            //
            // All ranks in the remote group send to root.
            //
            MPID_Request* sreq = MPIDI_Request_create_sreq(
                MPIDI_REQUEST_TYPE_SEND,
                sendbuf,
                sendcnt,
                sendtype,
                root,
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
        ( root == MPI_ROOT && recvcnt == 0 ) ||
        ( root >= 0 && sendcnt == 0 ) )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    MPI_RESULT mpi_errno;

    if( cbBuffer < pComm->SwitchPoints()->MPIR_gather_short_msg )
    {
        pRequest->nbc.tasks = new NbcTask[2];
        if( pRequest->nbc.tasks == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        //
        // Rank 0 of the remote group needs to allocate receive buffer.
        //
        MPI_Count trueLb;
        MPI_Aint trueExtent = fixme_cast<MPI_Aint>( sendtype.GetTrueExtentAndLowerBound( &trueLb ) );
        extent = fixme_cast<MPI_Aint>( sendtype.GetExtent() );
        if( extent >= trueExtent )
        {
            extent *= sendcnt * localSize;
        }
        else
        {
            extent = trueExtent * sendcnt * localSize;
        }

        pRequest->nbc.tmpBuf = new BYTE[extent];
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
        recvbuf = pRequest->nbc.tmpBuf - trueLb;

        //
        // Rank 0 in the remote group, do intracomm igather.
        //
        mpi_errno = pRequest->nbc.tasks[0].InitIgatherIntra(
            sendbuf,
            sendcnt,
            sendtype,
            recvbuf,
            sendcnt,
            sendtype,
            0,
            pComm->inter.local_comm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        pRequest->nbc.tasks[0].m_iNextOnInit = NBC_TASK_NONE;
        pRequest->nbc.tasks[0].m_iNextOnComplete = 1;
        pRequest->nbc.nTasks++;

        //
        // Rank 0 in the remote group sends to root.
        //
        mpi_errno = pRequest->nbc.tasks[1].InitIsend(
            recvbuf,
            sendcnt * localSize,
            sendtype,
            root,
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
    }
    else
    {
        //
        // Long messages - use linear gather.
        // Only MPI_ROOT can get here.
        //
        pRequest->nbc.tasks = new NbcTask[remoteSize];
        if( pRequest->nbc.tasks == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        extent = fixme_cast<MPI_Aint>( recvtype.GetExtent() ) * recvcnt;

        for( unsigned i = 0; i < remoteSize; i++ )
        {
            mpi_errno = pRequest->nbc.tasks[i].InitIrecv(
                static_cast<BYTE*>( recvbuf ) + extent * i,
                recvcnt,
                recvtype,
                i
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                pRequest->cancel_nbc_tasklist();
                return mpi_errno;
            }

            //
            // Initiate all the receives in parallel.
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
MPIR_Igather_inter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root >= 0, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IgatherBuildInter(
        sendbuf,
        sendcnt,
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


/* This is the default implementation of gatherv. The algorithm is:

   Algorithm: MPI_Gatherv

   Since the array of recvcounts is valid only on the root, we cannot
   do a tree algorithm without first communicating the recvcounts to
   other processes. Therefore, we simply use a linear algorithm for the
   gather, which takes (p-1) steps versus lgp steps for the tree
   algorithm. The bandwidth requirement is the same for both algorithms.

   Cost = (p-1).alpha + n.((p-1)/p).beta

   Possible improvements:

   End Algorithm: MPI_Gatherv
*/
MPI_RESULT
MPIR_Gatherv(
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root == comm_ptr->rank && _Old_(sendbuf) != MPI_IN_PLACE && sendcnt >= 0, _In_opt_)
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root != comm_ptr->rank && sendcnt >= 0, _In_opt_)
    _When_(comm_ptr->comm_kind == MPID_INTERCOMM && root > 0 && sendcnt >= 0, _In_opt_)
              const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(comm_ptr, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(comm_ptr, root, _In_)
              const int        recvcnts[],
    _mpi_when_root_(comm_ptr, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    )
{
    int        comm_size, rank, remote_comm_size;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPI_Aint   extent;
    int        i;

    rank = comm_ptr->rank;

    /* If rank == root, then I recv lots, otherwise I send */
    if ((comm_ptr->comm_kind != MPID_INTERCOMM) && (rank == root))
    {
        /* intracomm root */
        comm_size = comm_ptr->remote_size;
        extent = fixme_cast<MPI_Aint>(recvtype.GetExtent());

        for ( i=0; i<root; i++ )
        {
            if (recvcnts[i] > 0)
            {
                mpi_errno = MPIC_Recv((static_cast<char *>(recvbuf) + displs[i]*extent),
                                      recvcnts[i], recvtype, i,
                                      MPIR_GATHERV_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);
            }
        }
        if (sendbuf != MPI_IN_PLACE)
        {
            if (recvcnts[rank] > 0)
            {
                mpi_errno = MPIR_Localcopy(sendbuf, sendcnt, sendtype,
                                           (static_cast<char *>(recvbuf) + displs[rank]*extent),
                                           recvcnts[rank], recvtype);
                ON_ERROR_FAIL(mpi_errno);
            }
        }
        for ( i=root+1; i<comm_size; i++ )
        {
            if (recvcnts[i] > 0)
            {
                mpi_errno = MPIC_Recv((static_cast<char *>(recvbuf) + displs[i]*extent),
                                      recvcnts[i], recvtype, i,
                                      MPIR_GATHERV_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);
            }
        }
    }

    else if ((comm_ptr->comm_kind == MPID_INTERCOMM) && (root == MPI_ROOT))
    {
        /* intercommunicator root */
        remote_comm_size = comm_ptr->remote_size;
        extent = fixme_cast<MPI_Aint>(recvtype.GetExtent());

        for (i=0; i<remote_comm_size; i++)
        {
            if (recvcnts[i] > 0)
            {
                mpi_errno = MPIC_Recv((static_cast<char *>(recvbuf) + displs[i]*extent),
                                      recvcnts[i], recvtype, i,
                                      MPIR_GATHERV_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);
            }
        }
    }

    else if (root != MPI_PROC_NULL)
    {
        /* non-root nodes, and in the intercomm. case, non-root nodes on remote side */
        if (sendcnt > 0)
        {
            mpi_errno = MPIC_Send(sendbuf, sendcnt, sendtype, root,
                                  MPIR_GATHERV_TAG, comm_ptr);
        }
    }

fn_fail:
    //
    // fix OACR warning 6101 for recvbuf
    //
    OACR_USE_PTR(recvbuf);
    return (mpi_errno);
}


static inline
MPI_RESULT
InitIgathervRecvTask(
    _In_opt_ BYTE*         recvbuf,
    _In_     const int     recvcnts[],
    _In_     const int     displs[],
    _In_     unsigned      src,
    _In_     MPI_Count     extent,
    _In_     TypeHandle    recvtype,
    _In_     NbcTask**     ppCurTask,
    _In_     MPID_Request* pRequest
)
{
    if( recvcnts[src] > 0 )
    {
        MPI_RESULT mpi_errno = (*ppCurTask)->InitIrecv(
            recvbuf + displs[src] * extent,
            recvcnts[src],
            recvtype,
            src
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            pRequest->cancel_nbc_tasklist();
            return mpi_errno;
        }
        //
        // Initiate all the recvs in parallel.
        //
        pRequest->nbc.nTasks++;
        (*ppCurTask)->m_iNextOnInit = pRequest->nbc.nTasks;
        (*ppCurTask)->m_iNextOnComplete = NBC_TASK_NONE;
        (*ppCurTask)++;
    }

    return MPI_SUCCESS;
}


MPI_RESULT
IgathervBuildBoth(
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
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_GATHERV_TAG ) );

    if( ( ( pComm->comm_kind != MPID_INTERCOMM && pComm->rank != root ) ||
        ( pComm->comm_kind == MPID_INTERCOMM && root >= 0 ) ) &&
        sendcnt > 0 )
    {
        //
        // Intracomm non-root nodes or intercomm all nodes on the remote side
        // with the condition of sendcnt > 0.
        //
        MPID_Request* sreq = MPIDI_Request_create_sreq(
            MPIDI_REQUEST_TYPE_SEND,
            sendbuf,
            sendcnt,
            sendtype,
            root,
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

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pComm->AddRef();
    pRequest->nbc.tag = tag;

    if( root == MPI_PROC_NULL ||
        ( root != MPI_ROOT && sendcnt == 0 && ( pComm->comm_kind == MPID_INTERCOMM || pComm->rank != root ) ) )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    int size = pComm->remote_size;
    int i;
    unsigned taskCount = 0;
    MPI_Count extent = recvtype.GetExtent();
    MPI_RESULT mpi_errno;

    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        //
        // Intracomm and the current rank must be root if it reaches here.
        //
        MPIU_Assert( pComm->rank == root );

        if( sendbuf != MPI_IN_PLACE && sendcnt > 0 )
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
            if( recvcnts[i] > 0 )
            {
                taskCount++;
            }
        }
        for( ++i; i < size; i++ )
        {
            if( recvcnts[i] > 0 )
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

        if( sendbuf != MPI_IN_PLACE && sendcnt > 0 )
        {
            //
            // Taking care of root's own data.
            //
            curTask->InitLocalCopy(
                sendbuf,
                static_cast<BYTE*>( recvbuf ) + displs[root] * extent,
                sendcnt,
                recvcnts[root],
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
            mpi_errno = InitIgathervRecvTask(
                static_cast<BYTE*>( recvbuf ),
                recvcnts,
                displs,
                i,
                extent,
                recvtype,
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
            mpi_errno = InitIgathervRecvTask(
                static_cast<BYTE*>( recvbuf ),
                recvcnts,
                displs,
                i,
                extent,
                recvtype,
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

        for( i = size - 1; i >= 0; i-- )
        {
            if( recvcnts[i] > 0 )
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
            mpi_errno = InitIgathervRecvTask(
                static_cast<BYTE*>( recvbuf ),
                recvcnts,
                displs,
                i,
                extent,
                recvtype,
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
MPIR_Igatherv(
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
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
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


/* This is the default implementation of allgather. The algorithm is:

   Algorithm: MPI_Allgather

   For short messages and non-power-of-two no. of processes, we use
   the algorithm from the Jehoshua Bruck et al IEEE TPDS Nov 97
   paper. It is a variant of the disemmination algorithm for
   barrier. It takes ceiling(lg p) steps.

   Cost = lgp.alpha + n.((p-1)/p).beta
   where n is total size of data gathered on each process.

   For short or medium-size messages and power-of-two no. of
   processes, we use the recursive doubling algorithm.

   Cost = lgp.alpha + n.((p-1)/p).beta

   TODO: On TCP, we may want to use recursive doubling instead of the Bruck
   algorithm in all cases because of the pairwise-exchange property of
   recursive doubling (see Benson et al paper in Euro PVM/MPI
   2003).

   It is interesting to note that either of the above algorithms for
   MPI_Allgather has the same cost as the tree algorithm for MPI_Gather!

   For long messages or medium-size messages and non-power-of-two
   no. of processes, we use a ring algorithm. In the first step, each
   process i sends its contribution to process i+1 and receives
   the contribution from process i-1 (with wrap-around). From the
   second step onwards, each process i forwards to process i+1 the
   data it received from process i-1 in the previous step. This takes
   a total of p-1 steps.

   Cost = (p-1).alpha + n.((p-1)/p).beta

   We use this algorithm instead of recursive doubling for long
   messages because we find that this communication pattern (nearest
   neighbor) performs twice as fast as recursive doubling for long
   messages (on Myrinet and IBM SP).

   Possible improvements:

   End Algorithm: MPI_Allgather
*/
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgather_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    )
{
    int        comm_size, rank;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPI_Count   recvtype_extent;
    MPI_Count   recvtype_true_extent, recvbuf_extent;
    MPI_Count  recvtype_true_lb;
    int        j, i, pof2, src, rem;
    BYTE* tmp_buf = NULL;
    StackGuardArray<BYTE> auto_buf;
    int curr_cnt, dst, left, right, jnext;
    MPI_Count type_size;
    MPI_Status status;
    int mask, dst_tree_root, my_tree_root,
        last_recv_cnt = 0, nprocs_completed, k,
        tmp_mask, tree_root;
    MPI_Count send_offset, recv_offset, offset;

    MPIU_Assert( comm_ptr->comm_kind != MPID_INTERCOMM );

    if (((sendcount == 0) && (sendbuf != MPI_IN_PLACE)) || (recvcount == 0))
    {
        //
        // fix OACR warning 6101 for recvbuf
        //
        OACR_USE_PTR(recvbuf);
        return MPI_SUCCESS;
    }

    int MPIR_allgather_short_msg = comm_ptr->SwitchPoints()->MPIR_allgather_short_msg;
    int MPIR_allgather_long_msg = comm_ptr->SwitchPoints()->MPIR_allgather_long_msg;

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    recvtype_extent = recvtype.GetExtent();
    type_size = recvtype.GetSize();

    if ((type_size*recvcount*comm_size < MPIR_allgather_long_msg) &&
        IsPowerOf2( comm_size ))
    {

        /* Short or medium size message and power-of-two no. of processes. Use
         * recursive doubling algorithm */

            /* homogeneous. no need to pack into tmp_buf on each node. copy
               local data into recvbuf */
            if (sendbuf != MPI_IN_PLACE)
            {
                mpi_errno = MPIR_Localcopy (sendbuf, sendcount, sendtype,
                                            (static_cast<char *>(recvbuf) +
                                             recvtype_extent*rank*recvcount),
                                            recvcount, recvtype);
                ON_ERROR_FAIL(mpi_errno);
            }

            curr_cnt = recvcount;

            mask = 0x1;
            i = 0;
            while (mask < comm_size)
            {
                dst = rank ^ mask;

                /* find offset into send and recv buffers. zero out
                   the least significant "i" bits of rank and dst to
                   find root of src and dst subtrees. Use ranks of
                   roots as index to send from and recv into buffer */

                dst_tree_root = dst >> i;
                dst_tree_root <<= i;

                my_tree_root = rank >> i;
                my_tree_root <<= i;

                /* FIXME: saving an MPI_Aint into an int */
                send_offset = recvtype_extent * my_tree_root * recvcount;
                recv_offset = recvtype_extent * dst_tree_root * recvcount;

                if (dst < comm_size)
                {
                    mpi_errno = MPIC_Sendrecv((static_cast<char *>(recvbuf) + send_offset),
                                              curr_cnt, recvtype, dst,
                                              MPIR_ALLGATHER_TAG,
                                              (static_cast<char *>(recvbuf) + recv_offset),
                                              (comm_size-dst_tree_root)*recvcount,
                                              recvtype, dst,
                                              MPIR_ALLGATHER_TAG, comm_ptr, &status);
                    ON_ERROR_FAIL(mpi_errno);

                    last_recv_cnt = fixme_cast<int>( MPIR_Status_get_count( &status ) / type_size );
                    curr_cnt += last_recv_cnt;
                }

                /* if some processes in this process's subtree in this step
                   did not have any destination process to communicate with
                   because of non-power-of-two, we need to send them the
                   data that they would normally have received from those
                   processes. That is, the haves in this subtree must send to
                   the havenots. We use a logarithmic recursive-halfing algorithm
                   for this. */

                /* This part of the code will not currently be
                 executed because we are not using recursive
                 doubling for non power of two. Mark it as experimental
                 so that it doesn't show up as red in the coverage
                 tests. */

                /* --BEGIN EXPERIMENTAL-- */
                if (dst_tree_root + mask > comm_size)
                {
                    nprocs_completed = comm_size - my_tree_root - mask;
                    /* nprocs_completed is the number of processes in this
                       subtree that have all the data. Send data to others
                       in a tree fashion. First find root of current tree
                       that is being divided into two. k is the number of
                       least-significant bits in this process's rank that
                       must be zeroed out to find the rank of the root */
                    j = mask;
                    k = 0;
                    while (j)
                    {
                        j >>= 1;
                        k++;
                    }
                    k--;

                    offset = recvtype_extent * recvcount * (my_tree_root + mask);
                    tmp_mask = mask >> 1;

                    while (tmp_mask)
                    {
                        dst = rank ^ tmp_mask;

                        tree_root = rank >> k;
                        tree_root <<= k;

                        /* send only if this proc has data and destination
                           doesn't have data. at any step, multiple processes
                           can send if they have the data */
                        if ((dst > rank) &&
                            (rank < tree_root + nprocs_completed)
                            && (dst >= tree_root + nprocs_completed))
                        {
                            mpi_errno = MPIC_Send((static_cast<char *>(recvbuf) + offset),
                                                  last_recv_cnt,
                                                  recvtype, dst,
                                                  MPIR_ALLGATHER_TAG, comm_ptr);
                            /* last_recv_cnt was set in the previous
                               receive. that's the amount of data to be
                               sent now. */
                            ON_ERROR_FAIL(mpi_errno);
                        }
                        /* recv only if this proc. doesn't have data and sender
                           has data */
                        else if ((dst < rank) &&
                                 (dst < tree_root + nprocs_completed) &&
                                 (rank >= tree_root + nprocs_completed))
                        {
                            mpi_errno = MPIC_Recv((static_cast<char *>(recvbuf) + offset),
                                                  (comm_size - (my_tree_root + mask))*recvcount,
                                                  recvtype, dst,
                                                  MPIR_ALLGATHER_TAG,
                                                  comm_ptr, &status);
                            /* nprocs_completed is also equal to the
                               no. of processes whose data we don't have */
                            ON_ERROR_FAIL(mpi_errno);

                            last_recv_cnt = fixme_cast<int>(
                                MPIR_Status_get_count( &status ) / type_size
                                );
                            curr_cnt += last_recv_cnt;
                        }
                        tmp_mask >>= 1;
                        k--;
                    }
                }
                /* --END EXPERIMENTAL-- */

                mask <<= 1;
                i++;
            }
    }
    else if (type_size*recvcount*comm_size < MPIR_allgather_short_msg)
    {
        /* Short message and non-power-of-two no. of processes. Use
         * Bruck algorithm (see description above). */

        /* allocate a temporary buffer of the same size as recvbuf. */

        /* get true extent of recvtype */
        recvtype_true_extent = recvtype.GetTrueExtentAndLowerBound( &recvtype_true_lb );

        recvbuf_extent = (max(recvtype_true_extent, recvtype_extent)) *
            recvcount * comm_size;

        auto_buf = new BYTE[static_cast<size_t>(recvbuf_extent)];
        if( auto_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        /* adjust for potential negative lower bound in datatype */
        tmp_buf = auto_buf - recvtype_true_lb;

        /* copy local data to the top of tmp_buf */
        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MPIR_Localcopy (sendbuf, sendcount, sendtype,
                                        tmp_buf, recvcount, recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }
        else
        {
            mpi_errno = MPIR_Localcopy ((static_cast<char *>(recvbuf) +
                                         recvtype_extent * rank * recvcount),
                                        recvcount, recvtype, tmp_buf,
                                        recvcount, recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }

        /* do the first \floor(\lg p) steps */

        curr_cnt = recvcount;
        pof2 = 1;
        while (pof2 <= comm_size/2)
        {
            src = (rank + pof2) % comm_size;
            dst = (rank - pof2 + comm_size) % comm_size;

            mpi_errno = MPIC_Sendrecv(tmp_buf, curr_cnt, recvtype, dst,
                                      MPIR_ALLGATHER_TAG,
                                      (tmp_buf + curr_cnt*recvtype_extent),
                                      curr_cnt, recvtype,
                                      src, MPIR_ALLGATHER_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
            ON_ERROR_FAIL(mpi_errno);

            curr_cnt *= 2;
            pof2 *= 2;
        }

        /* if comm_size is not a power of two, one more step is needed */

        rem = comm_size - pof2;
        if (rem)
        {
            src = (rank + pof2) % comm_size;
            dst = (rank - pof2 + comm_size) % comm_size;

            //
            // OACR gets a bit overzealous here, and thinks src and dst could go negative.
            //
            _Analysis_assume_(src >= 0);
            _Analysis_assume_(dst >= 0);

            mpi_errno = MPIC_Sendrecv(tmp_buf, rem * recvcount, recvtype,
                                      dst, MPIR_ALLGATHER_TAG,
                                      (tmp_buf + curr_cnt*recvtype_extent),
                                      rem * recvcount, recvtype,
                                      src, MPIR_ALLGATHER_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
            ON_ERROR_FAIL(mpi_errno);
        }

        /* Rotate blocks in tmp_buf down by (rank) blocks and store
         * result in recvbuf. */

        mpi_errno = MPIR_Localcopy(tmp_buf, (comm_size-rank)*recvcount,
                  recvtype, static_cast<char *>(recvbuf) + recvtype_extent*rank*recvcount,
                                       (comm_size-rank)*recvcount, recvtype);
        ON_ERROR_FAIL(mpi_errno);

        if (rank)
        {
            mpi_errno = MPIR_Localcopy(tmp_buf +
                                   recvtype_extent*(comm_size-rank)*recvcount,
                                       rank*recvcount, recvtype, recvbuf,
                                       rank*recvcount, recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }
    }

    else {  /* long message or medium-size message and non-power-of-two
             * no. of processes. use ring algorithm. */

        /* First, load the "local" version in the recvbuf. */
        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MPIR_Localcopy(sendbuf, sendcount, sendtype,
                                       (static_cast<char *>(recvbuf) +
                                        recvtype_extent*rank*recvcount),
                                       recvcount, recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }

        /*
           Now, send left to right.  This fills in the receive area in
           reverse order.
        */
        left  = (comm_size + rank - 1) % comm_size;
        right = (rank + 1) % comm_size;

        j     = rank;
        jnext = left;
        for (i=1; i<comm_size; i++)
        {
            mpi_errno = MPIC_Sendrecv((static_cast<char *>(recvbuf) +
                                       recvtype_extent*j*recvcount),
                                      recvcount, recvtype, right,
                                      MPIR_ALLGATHER_TAG,
                                      (static_cast<char *>(recvbuf) +
                                       recvtype_extent*jnext*recvcount),
                                      recvcount, recvtype, left,
                                      MPIR_ALLGATHER_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
            ON_ERROR_FAIL(mpi_errno);

            j       = jnext;
            jnext = (comm_size + jnext - 1) % comm_size;
        }
    }

 fn_exit:
    return (mpi_errno);

 fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgather_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _When_(recvcount > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    )
{
    /* Intercommunicator Allgather.
       Each group does a gather to local root with the local
       intracommunicator, and then does an intercommunicator broadcast.
    */
    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int rank, local_size, remote_size, root;
    MPI_Aint true_extent, extent, send_extent;
    MPI_Count true_lb = 0;
    StackGuardArray<BYTE> auto_buf;
    BYTE* tmp_buf=NULL;

    local_size = comm_ptr->inter.local_size;
    remote_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    if ((rank == 0) && (sendcount != 0))
    {
        /* In each group, rank 0 allocates temp. buffer for local
           gather */
        true_extent = fixme_cast<MPI_Aint>(sendtype.GetTrueExtentAndLowerBound( &true_lb ));
        send_extent = fixme_cast<MPI_Aint>(sendtype.GetExtent());
        extent = max(send_extent, true_extent);

        auto_buf = new BYTE[extent*sendcount*local_size];
        if( auto_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        /* adjust for potential negative lower bound in datatype */
        tmp_buf = auto_buf - true_lb;
    }

    if (sendcount != 0)
    {
        mpi_errno = MPIR_Gather_intra(sendbuf, sendcount, sendtype, tmp_buf, sendcount,
                                      sendtype, 0, comm_ptr->inter.local_comm);
        ON_ERROR_FAIL(mpi_errno);
    }

    /* first broadcast from left to right group, then from right to
       left group */
    if (comm_ptr->inter.is_low_group)
    {
        /* bcast to right*/
        if (sendcount != 0)
        {
            root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
            mpi_errno = MPIR_Bcast_inter(tmp_buf, sendcount*local_size,
                                         sendtype, root, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);
        }

        /* receive bcast from right */
        if (recvcount != 0)
        {
            root = 0;
            mpi_errno = MPIR_Bcast_inter(recvbuf, recvcount*remote_size,
                                         recvtype, root, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);
        }
    }
    else
    {
        /* receive bcast from left */
        if (recvcount != 0)
        {
            root = 0;
            mpi_errno = MPIR_Bcast_inter(recvbuf, recvcount*remote_size,
                                         recvtype, root, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);
        }

        /* bcast to left */
        if (sendcount != 0)
        {
            root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
            mpi_errno = MPIR_Bcast_inter(tmp_buf, sendcount*local_size,
                                         sendtype, root, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);
        }
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgatherBuildRecursiveDoublingTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
)
{
    MPI_RESULT mpi_errno;
    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    MPI_Aint extent = fixme_cast<MPI_Aint>( recvtype.GetExtent() );
    ULONG lgpof2;
    _BitScanForward( &lgpof2, size );

    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            sendcount,
            sendtype,
            static_cast<BYTE*>( recvbuf ) + extent * rank * recvcount,
            recvcount,
            recvtype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    pReq->nbc.tasks = new NbcTask[lgpof2 * 2];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];
    ULONG* piNext = &pTask->m_iNextOnComplete;

    unsigned mask = UINT_MAX;
    mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = pReq->nbc.nTasks;

            //
            // offset is the distance to our peer. Figure out the actual
            // rank of our peer.
            //
            unsigned peer = rank ^ offset;
            //
            // Find offset into send and recv buffers. Zero out the least
            // significant n bits of rank and peer by masking with 'mask'
            // to find root of src and dst subtrees. Use ranks of roots as
            // index to send from and recv into buffer.
            //
            unsigned peerTreeRoot = peer & mask;
            unsigned myTreeRoot = rank & mask;

            MPI_RESULT mpi_errno = pTask->InitIsend(
                static_cast<BYTE*>( recvbuf ) + extent * myTreeRoot * recvcount,
                recvcount * offset,
                recvtype,
                peer,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            mpi_errno = pTask->InitIrecv(
                static_cast<BYTE*>( recvbuf ) + extent * peerTreeRoot * recvcount,
                recvcount * offset,
                recvtype,
                peer
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnComplete;
                ++pReq->nbc.nTasks;
                ++pTask;
                mask <<= 1;
            }

            return mpi_errno;
        },
        lgpof2
        );

    *piNext = NBC_TASK_NONE;
    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgatherBuildBruckTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
)
{
    MPI_RESULT mpi_errno;
    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    ULONG pof2 = PowerOf2Floor( size );
    ULONG lgpof2;
    _BitScanForward( &lgpof2, pof2 );

    MPI_Count trueLb;
    MPI_Aint extent = fixme_cast<MPI_Aint>( recvtype.GetExtent() );
    MPI_Aint trueExtent = fixme_cast<MPI_Aint>( recvtype.GetTrueExtentAndLowerBound( &trueLb ) );
    MPI_Aint fullExtent = max( extent, trueExtent ) * recvcount * size;

    pReq->nbc.tmpBuf = new BYTE[fullExtent];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    //
    // Adjust for potential negative lower bound in datatype.
    //
    BYTE* tmpBuf = pReq->nbc.tmpBuf - trueLb;

    //
    // Copy local data to the top of tmpBuf.
    //
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            sendcount,
            sendtype,
            tmpBuf,
            recvcount,
            recvtype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    else
    {
        mpi_errno = MPIR_Localcopy(
            static_cast<BYTE*>( recvbuf ) + extent * rank * recvcount,
            recvcount,
            recvtype,
            tmpBuf,
            recvcount,
            recvtype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    //
    // Number of tasks for the first lgpof2 steps, each step contains one send
    // and one recv.
    //
    ULONG nTasks = lgpof2 * 2;
    if( size > pof2 )
    {
        //
        // For non-pof2 number of ranks case, one more send and recv are needed.
        //
        nTasks += 2;
    }
    //
    // Copy the top of scratch buffer into recvbuf requires one task.
    //
    nTasks++;
    if( rank != 0 )
    {
        //
        // If it's not rank 0, copy the rest of the scratch buffer into recvbuf
        // requires one more task.
        //
        nTasks ++;
    }

    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];
    ULONG* piNext = &pTask->m_iNextOnComplete;

    //
    // Do the first lgpof2 steps.
    //
    mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = pReq->nbc.nTasks;

            unsigned src = pComm->RankAdd( offset );
            unsigned dst = pComm->RankSub( offset );

            MPI_RESULT mpi_errno = pTask->InitIsend(
                tmpBuf,
                offset * recvcount,
                recvtype,
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

            mpi_errno = pTask->InitIrecv(
                tmpBuf + extent * recvcount * offset,
                offset * recvcount,
                recvtype,
                src
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnComplete;
                ++pReq->nbc.nTasks;
                ++pTask;
            }

            return mpi_errno;
        },
        lgpof2
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // If number of ranks is not pof2, one more step is needed.
    //
    unsigned rem = size - pof2;
    if( rem > 0 )
    {
        *piNext = pReq->nbc.nTasks;

        unsigned src = pComm->RankAdd( pof2 );
        unsigned dst = pComm->RankSub( pof2 );

        mpi_errno = pTask->InitIsend(
            tmpBuf,
            rem * recvcount,
            recvtype,
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
        
        mpi_errno = pTask->InitIrecv(
            tmpBuf + extent * pof2 * recvcount,
            recvcount * rem,
            recvtype,
            src
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pTask->m_iNextOnComplete;
        ++pReq->nbc.nTasks;
        ++pTask;
    }

    *piNext = pReq->nbc.nTasks;

    //
    // Rotate blocks in tmpBuf down by "rank" blocks into recvbuf.
    //
    pTask->InitLocalCopy(
        tmpBuf,
        static_cast<BYTE*>( recvbuf ) + extent * rank * recvcount,
        ( size - rank ) * recvcount,
        ( size - rank ) * recvcount,
        recvtype,
        recvtype
        );
    ++pReq->nbc.nTasks;
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    pTask->m_iNextOnComplete = ( rank == 0 ? NBC_TASK_NONE : pReq->nbc.nTasks );

    if( rank > 0 )
    {
        ++pTask;

        pTask->InitLocalCopy(
            tmpBuf + extent * ( size - rank ) * recvcount,
            recvbuf,
            rank * recvcount,
            rank * recvcount,
            recvtype,
            recvtype
            );
        ++pReq->nbc.nTasks;
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
    }

    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgatherBuildRingTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
)
{
    MPI_RESULT mpi_errno;
    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    MPI_Aint extent = fixme_cast<MPI_Aint>( recvtype.GetExtent() );

    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            sendcount,
            sendtype,
            static_cast<BYTE*>( recvbuf ) + extent * rank * recvcount,
            recvcount,
            recvtype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    pReq->nbc.tasks = new NbcTask[( size - 1 ) * 2];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];
    ULONG* piNext = &pTask->m_iNextOnComplete;

    unsigned left  = pComm->RankSub( 1 );
    unsigned right = pComm->RankAdd( 1 );

    unsigned cur = rank;
    unsigned next = left;

    for( unsigned i = 1; i < size; i++ )
    {
        *piNext = pReq->nbc.nTasks;

        mpi_errno = pTask->InitIsend(
            static_cast<BYTE*>( recvbuf ) + extent * cur * recvcount,
            recvcount,
            recvtype,
            right,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        mpi_errno = pTask->InitIrecv(
            static_cast<BYTE*>( recvbuf ) + extent * next * recvcount,
            recvcount,
            recvtype,
            left
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pTask->m_iNextOnComplete;
        ++pReq->nbc.nTasks;
        ++pTask;

        cur = next;
        next = pComm->RankSub( 1, next );
    }

    *piNext = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgatherBuildTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_ALLGATHER_TAG ) );

    size_t cbBuffer = fixme_cast<size_t>(recvtype.GetSize() * recvcount * pComm->remote_size);
    unsigned MPIR_allgather_short_msg = pComm->SwitchPoints()->MPIR_allgather_short_msg;
    unsigned MPIR_allgather_long_msg = pComm->SwitchPoints()->MPIR_allgather_long_msg;
    MPI_RESULT mpi_errno;

    if( cbBuffer < MPIR_allgather_long_msg && IsPowerOf2( pComm->remote_size ) )
    {
        //
        // Short or medium message and pof2 number of ranks,
        // use recursive doubling algorithm.
        //
        mpi_errno = IallgatherBuildRecursiveDoublingTaskList(
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
    else if( cbBuffer < MPIR_allgather_short_msg )
    {
        //
        // Short message and non-pof2 number of ranks,
        // use Bruck algorithm.
        //
        mpi_errno = IallgatherBuildBruckTaskList(
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
        // Long message or medium message with non-pof2 number of ranks,
        // use ring algorithm.
        //
        mpi_errno = IallgatherBuildRingTaskList(
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
IallgatherBuildIntra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _When_((sendcount != 0 || sendbuf == MPI_IN_PLACE ) && recvcount != 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
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
    pRequest->comm = pComm;
    pRequest->comm->AddRef();

    MPI_RESULT mpi_errno;

    if( ( sendcount == 0 && sendbuf != MPI_IN_PLACE ) || recvcount == 0 )
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
        mpi_errno = IallgatherBuildTaskList(
            pRequest.get(),
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            pComm
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
MPIR_Iallgather_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IallgatherBuildIntra(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcount,
        recvtype,
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


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
static
MPI_RESULT
IallgatherBuildInter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _When_(recvcount > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // Each group does a gather to local root with the local
    // intracommunicator, and then does an intercommunicator bcast.
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
    pRequest->nbc.tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_ALLGATHER_TAG ) );
    pRequest->nbc.tasks = new NbcTask[3];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    unsigned localSize = pComm->inter.local_size;
    unsigned remoteSize = pComm->remote_size;
    int local = ( pComm->rank == 0 ) ? MPI_ROOT : MPI_PROC_NULL;
    BYTE* tmpBuf = nullptr;

    if( pComm->rank == 0 && sendcount > 0 )
    {
        //
        // In each group, rank 0 allocates temp buffer for local gather.
        //
        MPI_Count trueLb;
        MPI_Aint trueExtent = fixme_cast<MPI_Aint>( sendtype.GetTrueExtentAndLowerBound( &trueLb ) );
        MPI_Aint extent = fixme_cast<MPI_Aint>( sendtype.GetExtent() );
        if( extent >= trueExtent )
        {
            extent *= sendcount * localSize;
        }
        else
        {
            extent = trueExtent * sendcount * localSize;
        }

        pRequest->nbc.tmpBuf = new BYTE[extent];
        if( pRequest->nbc.tmpBuf == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        //
        // Adjust for potential negative lower bound in datatype.
        //
        tmpBuf = pRequest->nbc.tmpBuf - trueLb;
    }

    MPI_RESULT mpi_errno = pRequest->nbc.tasks[0].InitIgatherIntra(
        sendbuf,
        sendcount,
        sendtype,
        tmpBuf,
        sendcount,
        sendtype,
        0,
        pComm->inter.local_comm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }
    pRequest->nbc.tasks[0].m_iNextOnInit = pComm->inter.is_low_group ? 1 : NBC_TASK_NONE;
    pRequest->nbc.tasks[0].m_iNextOnComplete = pComm->inter.is_low_group ? 2 : 1;
    pRequest->nbc.nTasks++;

    if( pComm->inter.is_low_group )
    {
        //
        // Left group, intercomm ibcast receive from right group together with
        // intracomm igather.
        //
        mpi_errno = pRequest->nbc.tasks[1].InitIbcastInter(
            recvbuf,
            recvcount * remoteSize,
            recvtype,
            0,
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
        // Upon completion of intracomm igather, intercomm ibcast send to
        // right group.
        //
        mpi_errno = pRequest->nbc.tasks[2].InitIbcastInter(
            tmpBuf,
            sendcount * localSize,
            sendtype,
            local,
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
    }
    else
    {
        //
        // Right group, upon completion of intracomm igather, intercomm ibcast
        // send to left group.
        //
        mpi_errno = pRequest->nbc.tasks[1].InitIbcastInter(
            tmpBuf,
            sendcount * localSize,
            sendtype,
            local,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            pRequest->cancel_nbc_tasklist();
            return mpi_errno;
        }
        pRequest->nbc.tasks[1].m_iNextOnInit = 2;
        pRequest->nbc.tasks[1].m_iNextOnComplete = NBC_TASK_NONE;
        pRequest->nbc.nTasks++;

        //
        // Intercomm ibcast receive from left group together with intercomm
        // ibcast send to left group.
        //
        mpi_errno = pRequest->nbc.tasks[2].InitIbcastInter(
            recvbuf,
            recvcount * remoteSize,
            recvtype,
            0,
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
    }

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Iallgather_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _When_(recvcount > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IallgatherBuildInter(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcount,
        recvtype,
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


/* This is the default implementation of allgatherv. The algorithm is:

   Algorithm: MPI_Allgatherv

   For short messages and non-power-of-two no. of processes, we use
   the algorithm from the Jehoshua Bruck et al IEEE TPDS Nov 97
   paper. It is a variant of the disemmination algorithm for
   barrier. It takes ceiling(lg p) steps.

   Cost = lgp.alpha + n.((p-1)/p).beta
   where n is total size of data gathered on each process.

   For short or medium-size messages and power-of-two no. of
   processes, we use the recursive doubling algorithm.

   Cost = lgp.alpha + n.((p-1)/p).beta

   TODO: On TCP, we may want to use recursive doubling instead of the Bruck
   algorithm in all cases because of the pairwise-exchange property of
   recursive doubling (see Benson et al paper in Euro PVM/MPI
   2003).

   For long messages or medium-size messages and non-power-of-two
   no. of processes, we use a ring algorithm. In the first step, each
   process i sends its contribution to process i+1 and receives
   the contribution from process i-1 (with wrap-around). From the
   second step onwards, each process i forwards to process i+1 the
   data it received from process i-1 in the previous step. This takes
   a total of p-1 steps.

   Cost = (p-1).alpha + n.((p-1)/p).beta

   Possible improvements:

   End Algorithm: MPI_Allgatherv
*/
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgatherv_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    )
{
    int        comm_size, rank, j, i, jnext, left, right;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    MPI_Aint recvbuf_extent, recvtype_extent, recvtype_true_extent;
    MPI_Count recvtype_true_lb;
    int curr_cnt, send_cnt, dst, recvtype_size, pof2, src, rem;
    int recv_cnt;
    StackGuardArray<BYTE> auto_buf;
    BYTE* tmp_buf;
    int mask, dst_tree_root, my_tree_root, position,
        last_recv_cnt = 0, nprocs_completed, k,
        tmp_mask, tree_root;
    size_t total_count;
    size_t send_offset;
    size_t recv_offset;
    size_t offset;

    MPIU_Assert( comm_ptr->comm_kind != MPID_INTERCOMM );

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    total_count = 0;
    for (i=0; i<comm_size; i++)
    {
        total_count += recvcounts[i];
    }

    if (total_count == 0)
    {
        //
        // The assumption below isn't quite right at runtime, but there is
        // no way of expressing the sum of the values in the recvcounts array
        // in a _When_ annotation.  By assuming the buffer is null, OACR
        // stops complaining that the function returns without writing an _Out_
        // parameter.
        //
        _Analysis_assume_(recvbuf == nullptr);
        return MPI_SUCCESS;
    }

    recvtype_extent = fixme_cast<MPI_Aint>(recvtype.GetExtent());
    recvtype_size = fixme_cast<int>(recvtype.GetSize());

    if ((total_count*recvtype_size < (SIZE_T)(comm_ptr->SwitchPoints()->MPIR_allgather_long_msg)) &&
        IsPowerOf2( comm_size ))
    {
        /* Short or medium size message and power-of-two no. of processes. Use
         * recursive doubling algorithm */

            /* need to receive contiguously into tmp_buf because
               displs could make the recvbuf noncontiguous */

            recvtype_true_extent = fixme_cast<MPI_Aint>(recvtype.GetTrueExtentAndLowerBound( &recvtype_true_lb ));

            auto_buf = new BYTE[total_count*(max(recvtype_true_extent,recvtype_extent))];
            if( auto_buf == nullptr )
            {
                return MPIU_ERR_NOMEM();
            }

            /* adjust for potential negative lower bound in datatype */
            tmp_buf = auto_buf - recvtype_true_lb;

            /* copy local data into right location in tmp_buf */
            position = 0;
            for (i=0; i<rank; i++) position += recvcounts[i];
            if (sendbuf != MPI_IN_PLACE)
            {
                mpi_errno = MPIR_Localcopy(sendbuf, sendcount, sendtype,
                                           (tmp_buf + position * recvtype_extent),
                                           recvcounts[rank], recvtype);
            }
            else
            {
                /* if in_place specified, local data is found in recvbuf */
                mpi_errno = MPIR_Localcopy((static_cast<char *>(recvbuf) +
                                            displs[rank] * recvtype_extent),
                                           recvcounts[rank], recvtype,
                                           (tmp_buf + position * recvtype_extent),
                                           recvcounts[rank], recvtype);
            }
            ON_ERROR_FAIL(mpi_errno);

            curr_cnt = recvcounts[rank];

            mask = 0x1;
            i = 0;
            while (mask < comm_size)
            {
                dst = rank ^ mask;

                /* find offset into send and recv buffers. zero out
                   the least significant "i" bits of rank and dst to
                   find root of src and dst subtrees. Use ranks of
                   roots as index to send from and recv into buffer */

                dst_tree_root = dst >> i;
                dst_tree_root <<= i;

                my_tree_root = rank >> i;
                my_tree_root <<= i;

                if (dst < comm_size)
                {
                    send_offset = 0;
                    for (j=0; j<my_tree_root; j++)
                        send_offset += recvcounts[j];

                    recv_offset = 0;
                    for (j=0; j<dst_tree_root; j++)
                        recv_offset += recvcounts[j];

                    /* for convenience, recv is posted for a bigger amount
                       than will be sent */
                    mpi_errno = MPIC_Sendrecv((tmp_buf + send_offset * recvtype_extent),
                                              curr_cnt, recvtype, dst,
                                              MPIR_ALLGATHERV_TAG,
                                              (tmp_buf + recv_offset * recvtype_extent),
                                              (int)(total_count - recv_offset), recvtype, dst,
                                              MPIR_ALLGATHERV_TAG,
                                              comm_ptr, &status);
                    ON_ERROR_FAIL(mpi_errno);

                    last_recv_cnt = fixme_cast<int>(
                        MPIR_Status_get_count( &status ) / recvtype_size
                        );
                    curr_cnt += last_recv_cnt;
                }

                /* if some processes in this process's subtree in this step
                   did not have any destination process to communicate with
                   because of non-power-of-two, we need to send them the
                   data that they would normally have received from those
                   processes. That is, the haves in this subtree must send to
                   the havenots. We use a logarithmic
                   recursive-halfing algorithm for this. */

                /* This part of the code will not currently be
                 executed because we are not using recursive
                 doubling for non power of two. Mark it as experimental
                 so that it doesn't show up as red in the coverage
                 tests. */

                /* --BEGIN EXPERIMENTAL-- */
                if (dst_tree_root + mask > comm_size)
                {
                    nprocs_completed = comm_size - my_tree_root - mask;
                    /* nprocs_completed is the number of processes in this
                       subtree that have all the data. Send data to others
                       in a tree fashion. First find root of current tree
                       that is being divided into two. k is the number of
                       least-significant bits in this process's rank that
                       must be zeroed out to find the rank of the root */
                    j = mask;
                    k = 0;
                    while (j)
                    {
                        j >>= 1;
                        k++;
                    }
                    k--;

                    tmp_mask = mask >> 1;

                    while (tmp_mask)
                    {
                        dst = rank ^ tmp_mask;

                        tree_root = rank >> k;
                        tree_root <<= k;

                        /* send only if this proc has data and destination
                           doesn't have data. at any step, multiple processes
                           can send if they have the data */
                        if ((dst > rank) &&
                            (rank < tree_root + nprocs_completed)
                            && (dst >= tree_root + nprocs_completed))
                        {

                            offset = 0;
                            for (j=0; j<(my_tree_root+mask); j++)
                                offset += recvcounts[j];
                            offset *= recvtype_extent;

                            /* last_recv_cnt was set in the previous
                               receive. that's the amount of data to be
                               sent now. */
                            mpi_errno = MPIC_Send((tmp_buf + offset),
                                                  last_recv_cnt,
                                                  recvtype, dst,
                                                  MPIR_ALLGATHERV_TAG, comm_ptr);
                            ON_ERROR_FAIL(mpi_errno);
                        }

                        /* recv only if this proc. doesn't have data and sender
                           has data */
                        else if ((dst < rank) &&
                                 (dst < tree_root + nprocs_completed) &&
                                 (rank >= tree_root + nprocs_completed))
                        {

                            offset = 0;
                            for (j=0; j<(my_tree_root+mask); j++)
                                offset += recvcounts[j];

                            /* for convenience, recv is posted for a
                               bigger amount than will be sent */
                            mpi_errno = MPIC_Recv((tmp_buf + offset * recvtype_extent),
                                                  (int)(total_count - offset), recvtype,
                                                  dst, MPIR_ALLGATHERV_TAG,
                                                  comm_ptr, &status);
                            ON_ERROR_FAIL(mpi_errno);

                            last_recv_cnt = fixme_cast<int>(
                                MPIR_Status_get_count( &status ) / recvtype_size
                                );
                            curr_cnt += last_recv_cnt;
                        }
                        tmp_mask >>= 1;
                        k--;
                    }
                }
                /* --END EXPERIMENTAL-- */

                mask <<= 1;
                i++;
            }

            /* copy data from tmp_buf to recvbuf */
            position = 0;
            for (j=0; j<comm_size; j++)
            {
                if ((sendbuf != MPI_IN_PLACE) || (j != rank))
                {
                    /* not necessary to copy if in_place and
                       j==rank. otherwise copy. */
                    MPIR_Localcopy((tmp_buf + position*recvtype_extent),
                                   recvcounts[j], recvtype,
                                   (static_cast<char *>(recvbuf) + displs[j]*recvtype_extent),
                                   recvcounts[j], recvtype);
                }
                position += recvcounts[j];
            }
            //
            // fix OACR warning 6101
            //
            OACR_USE_PTR(recvbuf);
    }
    else if (total_count*recvtype_size < (SIZE_T)(comm_ptr->SwitchPoints()->MPIR_allgather_short_msg))
    {
        /* Short message and non-power-of-two no. of processes. Use
         * Bruck algorithm (see description above). */

        /* allocate a temporary buffer of the same size as recvbuf. */

        /* get true extent of recvtype */
        recvtype_true_extent = fixme_cast<MPI_Aint>(recvtype.GetTrueExtentAndLowerBound( &recvtype_true_lb ));

        recvbuf_extent = total_count *
            (max(recvtype_true_extent, recvtype_extent));

        auto_buf = new BYTE[recvbuf_extent];
        if( auto_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        /* adjust for potential negative lower bound in datatype */
        tmp_buf = auto_buf - recvtype_true_lb;

        /* copy local data to the top of tmp_buf */
        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MPIR_Localcopy (sendbuf, sendcount, sendtype,
                                        tmp_buf, recvcounts[rank], recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }
        else
        {
            mpi_errno = MPIR_Localcopy((static_cast<char *>(recvbuf) +
                                        displs[rank]*recvtype_extent),
                                       recvcounts[rank], recvtype,
                                       tmp_buf, recvcounts[rank], recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }

        /* do the first \floor(\lg p) steps */

        curr_cnt = recvcounts[rank];
        pof2 = 1;
        while (pof2 <= comm_size/2)
        {
            src = (rank + pof2) % comm_size;
            dst = (rank - pof2 + comm_size) % comm_size;

            mpi_errno = MPIC_Sendrecv(tmp_buf, curr_cnt, recvtype, dst,
                                      MPIR_ALLGATHERV_TAG,
                                      (tmp_buf + curr_cnt*recvtype_extent),
                                      (int)total_count - curr_cnt, recvtype,
                                      src, MPIR_ALLGATHERV_TAG, comm_ptr, &status);
            ON_ERROR_FAIL(mpi_errno);

            recv_cnt = fixme_cast<int>(
                MPIR_Status_get_count( &status ) / recvtype_size
                );
            curr_cnt += recv_cnt;

            pof2 *= 2;
        }

        /* if comm_size is not a power of two, one more step is needed */

        rem = comm_size - pof2;
        if (rem)
        {
            src = (rank + pof2) % comm_size;
            dst = (rank - pof2 + comm_size) % comm_size;

            send_cnt = 0;
            for (i=0; i<rem; i++)
                send_cnt += recvcounts[(rank+i)%comm_size];

            mpi_errno = MPIC_Sendrecv(tmp_buf, send_cnt, recvtype,
                                      dst, MPIR_ALLGATHERV_TAG,
                                      (tmp_buf + curr_cnt*recvtype_extent),
                                      (int)total_count - curr_cnt, recvtype,
                                      src, MPIR_ALLGATHERV_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
            ON_ERROR_FAIL(mpi_errno);
        }

        /* Rotate blocks in tmp_buf down by (rank) blocks and store
         * result in recvbuf. */

        send_cnt = 0;
        //
        // OACR loses track of things, so remind it that rank is always < comm_size.
        //
        _Analysis_assume_(rank < comm_size);
        for (i=0; i < (comm_size-rank); i++)
        {
            j = (rank+i)%comm_size;
            mpi_errno = MPIR_Localcopy(tmp_buf + send_cnt*recvtype_extent,
                                       recvcounts[j], recvtype,
                                  static_cast<char *>(recvbuf) + displs[j]*recvtype_extent,
                                       recvcounts[j], recvtype);
            ON_ERROR_FAIL(mpi_errno);

            send_cnt += recvcounts[j];
        }

        for (i=0; i<rank; i++)
        {
            mpi_errno = MPIR_Localcopy(tmp_buf + send_cnt*recvtype_extent,
                                       recvcounts[i], recvtype,
                                  static_cast<char *>(recvbuf) + displs[i]*recvtype_extent,
                                       recvcounts[i], recvtype);
            ON_ERROR_FAIL(mpi_errno);

            send_cnt += recvcounts[i];
        }

    }

    else {  /* long message or medium-size message and non-power-of-two
             * no. of processes. Use ring algorithm. */

        if (sendbuf != MPI_IN_PLACE)
        {
            /* First, load the "local" version in the recvbuf. */
            mpi_errno = MPIR_Localcopy(sendbuf, sendcount, sendtype,
                              (static_cast<char *>(recvbuf) + displs[rank]*recvtype_extent),
                                       recvcounts[rank], recvtype);
            ON_ERROR_FAIL(mpi_errno);
        }

        left  = (comm_size + rank - 1) % comm_size;
        right = (rank + 1) % comm_size;

        j     = rank;
        jnext = left;
        for (i=1; i<comm_size; i++)
        {
            mpi_errno = MPIC_Sendrecv((static_cast<char *>(recvbuf) + displs[j]*recvtype_extent),
                                      recvcounts[j], recvtype, right,
                                      MPIR_ALLGATHERV_TAG,
                                 (static_cast<char *>(recvbuf) + displs[jnext]*recvtype_extent),
                                      recvcounts[jnext], recvtype, left,
                                      MPIR_ALLGATHERV_TAG, comm_ptr, &status );
            ON_ERROR_FAIL(mpi_errno);

            j       = jnext;
            jnext = (comm_size + jnext - 1) % comm_size;
        }
    }

fn_fail:
    return (mpi_errno);
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgatherv_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    )
{
/* Intercommunicator Allgatherv.
   This is done differently from the intercommunicator allgather
   because we don't have all the information to do a local
   intracommunictor gather (sendcount can be different on each
   process). Therefore, we do the following:
   Each group first does an intercommunicator gather to rank 0
   and then does an intracommunicator broadcast.
*/
    MPI_RESULT mpi_errno;
    int remote_size, root, rank;
    MPI_Datatype newtype;

    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    remote_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    /* first do an intercommunicator gatherv from left to right group,
       then from right to left group */
    if (comm_ptr->inter.is_low_group)
    {
        /* gatherv from right group */
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Gatherv(sendbuf, sendcount, sendtype, recvbuf,
                                 recvcounts, displs, recvtype, root,
                                 comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* gatherv to right group */
        root = 0;
        mpi_errno = MPIR_Gatherv(sendbuf, sendcount, sendtype, recvbuf,
                                 recvcounts, displs, recvtype, root,
                                 comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* gatherv to left group  */
        root = 0;
        mpi_errno = MPIR_Gatherv(sendbuf, sendcount, sendtype, recvbuf,
                                 recvcounts, displs, recvtype, root,
                                 comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* gatherv from left group */
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Gatherv(sendbuf, sendcount, sendtype, recvbuf,
                                 recvcounts, displs, recvtype, root,
                                 comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }

    /* now do an intracommunicator broadcast within each group. we use
       a derived datatype to handle the displacements */

    NMPI_Type_indexed(remote_size, recvcounts, displs, recvtype.GetMpiHandle(),
                      &newtype);
    NMPI_Type_commit(&newtype);

    mpi_errno = MPIR_Bcast_intra(
        recvbuf,
        1,
        TypeHandle::Create( newtype ),
        0,
        comm_ptr->inter.local_comm
        );

    NMPI_Type_free(&newtype);

fn_fail:
    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgathervBuildRecursiveDoublingTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      int              totalCount,
    _In_      MPI_Aint         recvTypeExtent
    )
{
    //
    // Short or medium size message and power-of-two num of processes.
    // Use recursive doubling algorithm.
    //
    MPI_RESULT mpi_errno;
    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    ULONG lgpof2;
    _BitScanForward( &lgpof2, size );

    MPI_Count recvTypeTrueLb;
    MPI_Aint recvTypeTrueExtent = fixme_cast<MPI_Aint>( recvtype.GetTrueExtentAndLowerBound( &recvTypeTrueLb ) );
    MPI_Aint recvBufExtent = totalCount * max( recvTypeExtent, recvTypeTrueExtent );

    pReq->nbc.tmpBuf = new BYTE[recvBufExtent];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    //
    // Adjust for potential negative lower bound in datatype.
    //
    BYTE* tmpBuf = pReq->nbc.tmpBuf - recvTypeTrueLb;

    unsigned i;
    unsigned position = 0;
    for( i = 0; i < rank; i++ )
    {
        position += recvcounts[i];
    }

    //
    // Copy local data into tmpBuf at the correct offset.
    //
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            sendcount,
            sendtype,
            tmpBuf + recvTypeExtent * position,
            recvcounts[rank],
            recvtype
            );
    }
    else
    {
        mpi_errno = MPIR_Localcopy(
            static_cast<BYTE*>( recvbuf ) + displs[rank] * recvTypeExtent,
            recvcounts[rank],
            recvtype,
            tmpBuf + recvTypeExtent * position,
            recvcounts[rank],
            recvtype
            );
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Number of tasks for the first lgpof2 steps, each step contains one send
    // and one recv. Plus "size" number of tasks to copy data from scratch
    // buffer into recvbuf at the end.
    //
    ULONG nTasks = lgpof2 * 2 + size;
    if( sendbuf == MPI_IN_PLACE )
    {
        //
        // For MPI_IN_PLACE case, don't need to copy its own data from
        // scratch buffer to recvbuf at the end.
        //
        nTasks--;
    }
    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];
    ULONG* piNext = &pTask->m_iNextOnComplete;

    //
    // The currCount array keeps tracks of how much data each rank has at
    // a particular step. In the sync version, it's not necessary since
    // it can calculate how much data it received at each step, but this
    // won't work for the async implementation because we need to know
    // the amount at task list setup time.
    //
    StackGuardArray<unsigned> currCount = new unsigned[size];
    if( currCount == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    StackGuardArray<unsigned> prevCount = new unsigned[size];
    if( prevCount == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    for( i = 0; i < size; i++ )
    {
        currCount[i] = recvcounts[i];
        prevCount[i] = recvcounts[i];
    }

    unsigned mask = UINT_MAX;
    mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = pReq->nbc.nTasks;

            //
            // offset is the distance to our peer. Figure out the actual
            // rank of our peer.
            //
            unsigned peer = rank ^ offset;
            //
            // Find offset into send and recv buffers. Zero out the least
            // significant n bits of rank and peer by masking with 'mask'
            // to find root of src and dst subtrees. Use ranks of roots as
            // index to send from and recv into buffer.
            //
            unsigned peerTreeRoot = peer & mask;
            unsigned myTreeRoot = rank & mask;

            unsigned sendOffset = 0;
            for( i = 0; i < myTreeRoot; i++ )
            {
                sendOffset += recvcounts[i];
            }

            unsigned recvOffset = 0;
            for( i = 0; i < peerTreeRoot; i++ )
            {
                recvOffset += recvcounts[i];
            }

            MPI_RESULT mpi_errno = pTask->InitIsend(
                tmpBuf + sendOffset * recvTypeExtent,
                currCount[rank],
                recvtype,
                peer,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            mpi_errno = pTask->InitIrecv(
                tmpBuf + recvOffset * recvTypeExtent,
                currCount[peer],
                recvtype,
                peer
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnComplete;
                ++pReq->nbc.nTasks;
                ++pTask;
                mask <<= 1;

                //
                // Update currCount array after each step.
                //
                for( i = 0; i < size; i++ )
                {
                    peer = i ^ offset;
                    currCount[i] += prevCount[peer];
                }
                //
                // Sync prevCount array to be the same as currCount array.
                //
                for( i = 0; i < size; i++ )
                {
                    prevCount[i] = currCount[i];
                }
            }

            return mpi_errno;
        },
        lgpof2
        );

    *piNext = pReq->nbc.nTasks;

    //
    // Copy data from tmpBuf to recvbuf.
    //
    unsigned currOffset = 0;
    for( i = 0; i < size; i++ )
    {
        if( sendbuf != MPI_IN_PLACE || i != rank )
        {
            pTask->InitLocalCopy(
                tmpBuf + currOffset * recvTypeExtent,
                static_cast<BYTE*>( recvbuf ) + displs[i] * recvTypeExtent,
                recvcounts[i],
                recvcounts[i],
                recvtype,
                recvtype
                );
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            ++pTask;
        }

        currOffset += recvcounts[i];
    }

    //
    // Fix the last task's m_iNextOnComplete
    //
    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgathervBuildBruckTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      const int        totalCount,
    _In_      const MPI_Aint   recvTypeExtent
    )
{
    //
    // Short message and non-power-of-two num of processes.
    // Use Bruck algorithm.
    //
    MPI_RESULT mpi_errno;
    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    ULONG pof2 = PowerOf2Floor( size );
    ULONG lgpof2;
    _BitScanForward( &lgpof2, pof2 );

    MPI_Count recvTypeTrueLb;
    MPI_Aint recvTypeTrueExtent = fixme_cast<MPI_Aint>( recvtype.GetTrueExtentAndLowerBound( &recvTypeTrueLb ) );
    MPI_Aint recvBufExtent = totalCount * max( recvTypeExtent, recvTypeTrueExtent );

    pReq->nbc.tmpBuf = new BYTE[recvBufExtent];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    //
    // Adjust for potential negative lower bound in datatype.
    //
    BYTE* tmpBuf = pReq->nbc.tmpBuf - recvTypeTrueLb;

    //
    // Copy local data to the top of tmpBuf.
    //
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            sendcount,
            sendtype,
            tmpBuf,
            recvcounts[rank],
            recvtype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    else
    {
        mpi_errno = MPIR_Localcopy(
            static_cast<BYTE*>( recvbuf ) + displs[rank] * recvTypeExtent,
            recvcounts[rank],
            recvtype,
            tmpBuf,
            recvcounts[rank],
            recvtype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    //
    // Number of tasks for the first lgpof2 steps, each step contains one send
    // and one recv. Plus "size" number of tasks to copy data from scratch
    // buffer into recvbuf at the end.
    //
    ULONG nTasks = lgpof2 * 2 + size;
    if( size > pof2 )
    {
        //
        // For non-pof2 number of ranks case, one more send and recv are needed.
        //
        nTasks += 2;
    }

    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];
    ULONG* piNext = &pTask->m_iNextOnComplete;

    //
    // The currCount array keeps tracks of how much data each rank has at
    // a particular step. In the sync version, it's not necessary since
    // it can calculate how much data it received at each step, but this
    // won't work for the async implementation because we need to know
    // the amount at task list setup time.
    //
    StackGuardArray<unsigned> currCount = new unsigned[size];
    if( currCount == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    StackGuardArray<unsigned> prevCount = new unsigned[size];
    if( prevCount == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    unsigned i;
    for( i = 0; i < size; i++ )
    {
        currCount[i] = recvcounts[i];
        prevCount[i] = recvcounts[i];
    }

    //
    // Do the first lgpof2 steps.
    //
    mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = pReq->nbc.nTasks;

            unsigned src = pComm->RankAdd( offset );
            unsigned dst = pComm->RankSub( offset );

            MPI_RESULT mpi_errno = pTask->InitIsend(
                tmpBuf,
                currCount[rank],
                recvtype,
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

            mpi_errno = pTask->InitIrecv(
                tmpBuf + currCount[rank] * recvTypeExtent,
                currCount[src],
                recvtype,
                src
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnComplete;
                ++pReq->nbc.nTasks;
                ++pTask;

                //
                // Update currCount array after each step.
                //
                for( i = 0; i < size; i++ )
                {
                    src = pComm->RankAdd( offset, i );
                    currCount[i] += prevCount[src];
                }
                //
                // Sync prevCount array to be the same as currCount array.
                //
                for( i = 0; i < size; i++ )
                {
                    prevCount[i] = currCount[i];
                }
            }

            return mpi_errno;
        },
        lgpof2
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // If number of ranks is not pof2, one more step is needed.
    //
    if( size - pof2 > 0 )
    {
        *piNext = pReq->nbc.nTasks;

        unsigned src = pComm->RankAdd( pof2 );
        unsigned dst = pComm->RankSub( pof2 );

        mpi_errno = pTask->InitIsend(
            tmpBuf,
            totalCount - currCount[dst],
            recvtype,
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

        mpi_errno = pTask->InitIrecv(
            tmpBuf + currCount[rank] * recvTypeExtent,
            totalCount - currCount[rank],
            recvtype,
            src
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pTask->m_iNextOnComplete;
        ++pReq->nbc.nTasks;
        ++pTask;
    }

    *piNext = pReq->nbc.nTasks;

    //
    // Rotate blocks in tmpBuf down by "rank" blocks into recvbuf.
    //
    unsigned currOffset = 0;
    unsigned j;
    for( i = 0; i < size - rank; i++ )
    {
        j = pComm->RankAdd( i );

        pTask->InitLocalCopy(
            tmpBuf + currOffset * recvTypeExtent,
            static_cast<BYTE*>( recvbuf ) + displs[j] * recvTypeExtent,
            recvcounts[j],
            recvcounts[j],
            recvtype,
            recvtype
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        currOffset += recvcounts[j];
    }

    for( i = 0; i < rank; i++ )
    {
        pTask->InitLocalCopy(
            tmpBuf + currOffset * recvTypeExtent,
            static_cast<BYTE*>( recvbuf ) + displs[i] * recvTypeExtent,
            recvcounts[i],
            recvcounts[i],
            recvtype,
            recvtype
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;

        currOffset += recvcounts[i];
    }

    //
    // Fix the last task's m_iNextOnComplete
    //
    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgathervBuildRingTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      const MPI_Aint   recvTypeExtent
    )
{
    //
    // Long or medium-size message and non-power-of-two
    // num of processes. Use ring algorithm.
    //
    MPI_RESULT mpi_errno;
    unsigned rank = pComm->rank;

    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            sendcount,
            sendtype,
            static_cast<BYTE*>( recvbuf ) + displs[rank] * recvTypeExtent,
            recvcounts[rank],
            recvtype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    unsigned size = pComm->remote_size;
    pReq->nbc.tasks = new NbcTask[( size - 1 ) * 2];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];
    ULONG* piNext = &pTask->m_iNextOnComplete;

    unsigned left  = pComm->RankSub( 1 );
    unsigned right = pComm->RankAdd( 1 );
    unsigned cur = rank;
    unsigned next = left;

    for( unsigned i = 1; i < size; i++ )
    {
        *piNext = pReq->nbc.nTasks;

        mpi_errno = pTask->InitIsend(
            static_cast<BYTE*>( recvbuf ) + displs[cur] * recvTypeExtent,
            recvcounts[cur],
            recvtype,
            right,
            pComm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pTask;

        mpi_errno = pTask->InitIrecv(
            static_cast<BYTE*>( recvbuf ) + displs[next] * recvTypeExtent,
            recvcounts[next],
            recvtype,
            left
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pTask->m_iNextOnComplete;
        ++pReq->nbc.nTasks;
        ++pTask;

        cur = next;
        next = pComm->RankSub( 1, next );
    }

    *piNext = NBC_TASK_NONE;
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgathervBuildTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      const int        totalCount,
    _In_      const MPI_Aint   recvTypeExtent
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_ALLGATHERV_TAG ) );

    MPI_RESULT mpi_errno;
    size_t cbBuffer = fixme_cast<size_t>( recvtype.GetSize() * totalCount );

    if( cbBuffer < pComm->SwitchPoints()->MPIR_allgather_long_msg &&
        IsPowerOf2( pComm->remote_size ) )
    {
        mpi_errno = IallgathervBuildRecursiveDoublingTaskList(
            pReq,
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            pComm,
            totalCount,
            recvTypeExtent
            );
    }
    else if( cbBuffer < pComm->SwitchPoints()->MPIR_allgather_short_msg )
    {
        mpi_errno = IallgathervBuildBruckTaskList(
            pReq,
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            pComm,
            totalCount,
            recvTypeExtent
            );
    }
    else
    {
        mpi_errno = IallgathervBuildRingTaskList(
            pReq,
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            pComm,
            recvTypeExtent
            );
    }

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallgathervBuildIntra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
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
    pRequest->comm = pComm;
    pRequest->comm->AddRef();

    int totalCount = 0;
    int size = pComm->remote_size;
    for( int i = 0; i < size; i++ )
    {
        totalCount += recvcounts[i];
    }

    MPI_Aint recvTypeExtent = fixme_cast<MPI_Aint>( recvtype.GetExtent() );
    MPI_RESULT mpi_errno;

    if( totalCount == 0 )
    {
        //
        // Suppress OACR C6101 for recvbuf here because there is no way
        // to express recvbuf properly in SAL due to totalCount is a
        // calculated value from the input array recvcnts.
        //
        OACR_USE_PTR( recvbuf );
        pRequest->kind = MPID_REQUEST_NOOP;
    }
    else if( size == 1 )
    {
        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MPIR_Localcopy(
                sendbuf,
                sendcount,
                sendtype,
                static_cast<BYTE*>( recvbuf ) + displs[0] * recvTypeExtent,
                totalCount,
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
        mpi_errno = IallgathervBuildTaskList(
            pRequest.get(),
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            pComm,
            totalCount,
            recvTypeExtent
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
MPIR_Iallgatherv_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IallgathervBuildIntra(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcounts,
        displs,
        recvtype,
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


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
static
MPI_RESULT
IallgathervBuildInter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // Each group first does an intercommunicator igather to rank 0
    // and then does an intracommunicator ibcast.
    //
    MPIU_Assert( pComm->comm_kind == MPID_INTERCOMM );
    MPIU_Assert( pComm->inter.local_comm != nullptr );

    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_ALLGATHERV_TAG ) );

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pComm->AddRef();
    pRequest->nbc.tag = tag;
    pRequest->nbc.tasks = new NbcTask[3];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    int rank = pComm->rank;
    int local = ( rank == 0 ) ? MPI_ROOT : MPI_PROC_NULL;
    int root1;
    int root2;
    if( pComm->inter.is_low_group )
    {
        root1 = local;
        root2 = 0;
    }
    else
    {
        root1 = 0;
        root2 = local;
    }

    MPI_RESULT mpi_errno = pRequest->nbc.tasks[0].InitIgathervBoth(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcounts,
        displs,
        recvtype,
        root1,
        pComm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }
    pRequest->nbc.tasks[0].m_iNextOnInit = 1;
    pRequest->nbc.tasks[0].m_iNextOnComplete = pComm->inter.is_low_group ? 2 : NBC_TASK_NONE;
    pRequest->nbc.nTasks++;

    mpi_errno = pRequest->nbc.tasks[1].InitIgathervBoth(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcounts,
        displs,
        recvtype,
        root2,
        pComm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        pRequest->cancel_nbc_tasklist();
        return mpi_errno;
    }
    pRequest->nbc.tasks[1].m_iNextOnInit = NBC_TASK_NONE;
    pRequest->nbc.tasks[1].m_iNextOnComplete = pComm->inter.is_low_group ? NBC_TASK_NONE : 2;
    pRequest->nbc.nTasks++;

    MPI_Datatype tmpType;
    NMPI_Type_indexed(
        pComm->remote_size,
        recvcounts,
        displs,
        recvtype.GetMpiHandle(),
        &tmpType
        );
    NMPI_Type_commit( &tmpType );

    mpi_errno = pRequest->nbc.tasks[2].InitIbcastIntra(
        recvbuf,
        1,
        TypeHandle::Create( tmpType ),
        0,
        pComm->inter.local_comm,
        true
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
MPIR_Iallgatherv_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IallgathervBuildInter(
        sendbuf,
        sendcount,
        sendtype,
        recvbuf,
        recvcounts,
        displs,
        recvtype,
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

