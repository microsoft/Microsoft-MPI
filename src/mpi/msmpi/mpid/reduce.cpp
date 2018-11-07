// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* This is the default implementation of reduce. The algorithm is:

   Algorithm: MPI_Reduce

   For long messages and for builtin ops and if count >= pof2 (where
   pof2 is the nearest power-of-two less than or equal to the number
   of processes), we use Rabenseifner's algorithm (see
   http://www.hlrs.de/organization/par/services/models/mpi/myreduce.html ).
   This algorithm implements the reduce in two steps: first a
   reduce-scatter, followed by a gather to the root. A
   recursive-halving algorithm (beginning with processes that are
   distance 1 apart) is used for the reduce-scatter, and a binomial tree
   algorithm is used for the gather. The non-power-of-two case is
   handled by dropping to the nearest lower power-of-two: the first
   few odd-numbered processes send their data to their left neighbors
   (rank-1), and the reduce-scatter happens among the remaining
   power-of-two processes. If the root is one of the excluded
   processes, then after the reduce-scatter, rank 0 sends its result to
   the root and exits; the root now acts as rank 0 in the binomial tree
   algorithm for gather.

   For the power-of-two case, the cost for the reduce-scatter is
   lgp.alpha + n.((p-1)/p).beta + n.((p-1)/p).gamma. The cost for the
   gather to root is lgp.alpha + n.((p-1)/p).beta. Therefore, the
   total cost is:
   Cost = 2.lgp.alpha + 2.n.((p-1)/p).beta + n.((p-1)/p).gamma

   For the non-power-of-two case, assuming the root is not one of the
   odd-numbered processes that get excluded in the reduce-scatter,
   Cost = (2.floor(lgp)+1).alpha + (2.((p-1)/p) + 1).n.beta +
           n.(1+(p-1)/p).gamma


   For short messages, user-defined ops, and count < pof2, we use a
   binomial tree algorithm for both short and long messages.

   Cost = lgp.alpha + n.lgp.beta + n.lgp.gamma


   We use the binomial tree algorithm in the case of user-defined ops
   because in this case derived datatypes are allowed, and the user
   could pass basic datatypes on one process and derived on another as
   long as the type maps are the same. Breaking up derived datatypes
   to do the reduce-scatter is tricky.

   Possible improvements:

   End Algorithm: MPI_Reduce
*/
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_intra_flat(
    _In_opt_  const void*      sendbuf,
    _When_(root == comm_ptr->rank, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    )
{
    MPI_Status status;
    int        comm_size, rank, type_size, pof2, rem, newrank;
    int        mask, relrank, source, lroot, send_idx=0;
    int        recv_idx, last_idx=0, newdst;
    int    dst, send_cnt, recv_cnt, newroot, newdst_tree_root,
        newroot_tree_root;
    int reduceSize, endSize, idx_shift;
    MPI_Count   true_lb;
    MPI_Aint    true_extent, extent, bufferExtent;
    StackGuardArray<BYTE>   auto_buf;
    BYTE       *tmp_buf;

    if (count == 0)
    {
        return MPI_SUCCESS;
    }

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    /* set op_errno to 0. stored in perthread structure */
    Mpi.CallState->op_errno = 0;

    /* Create a temporary buffer */

    true_extent = fixme_cast<MPI_Aint>(datatype.GetTrueExtentAndLowerBound( &true_lb ));
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    extent = fixme_cast<MPI_Aint>(datatype.GetExtent());
    bufferExtent = count * max(extent, true_extent);

    if( rank == root )
    {
        //
        // Receive buffer is valid - need just a scratch buffer.
        //
        auto_buf = new BYTE[bufferExtent];
        if( auto_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
    }
    else
    {
        //
        // Receive buffer may not be valid, so allocate space for that too.
        //
        auto_buf = new BYTE[2 * bufferExtent];
        if( auto_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        recvbuf = auto_buf + bufferExtent - true_lb;
    }

    /* adjust for potential negative lower bound in datatype */
    tmp_buf = auto_buf - true_lb;

    if ((rank != root) || (sendbuf != MPI_IN_PLACE))
    {
        mpi_errno = MPIR_Localcopy(sendbuf, count, datatype, recvbuf,
                                   count, datatype);
        ON_ERROR_FAIL(mpi_errno);
    }

    type_size = fixme_cast<int>(datatype.GetSize());

    MPI_Datatype hType = datatype.GetMpiHandle();

    /* find nearest power-of-two less than or equal to comm_size */
    pof2 = PowerOf2Floor( comm_size );

    if ((static_cast<unsigned int>(count*type_size) > comm_ptr->SwitchPoints()->MPIR_reduce_short_msg) &&
        (pOp->IsBuiltin() == true) && (count >= pof2))
    {
        /* do a reduce-scatter followed by gather to root. */

        rem = comm_size - pof2;

        /* In the non-power-of-two case, all odd-numbered
           processes of rank < 2*rem send their data to
           (rank-1). These odd-numbered processes no longer
           participate in the algorithm until the very end. The
           remaining processes form a nice power-of-two.

           Note that in MPI_Allreduce we have the even-numbered processes
           send data to odd-numbered processes. That is better for
           non-commutative operations because it doesn't require a
           buffer copy. However, for MPI_Reduce, the most common case
           is commutative operations with root=0. Therefore we want
           even-numbered processes to participate the computation for
           the root=0 case, in order to avoid an extra send-to-root
           communication after the reduce-scatter. In MPI_Allreduce it
           doesn't matter because all processes must get the result. */

        if (rank < 2*rem)
        {
            if ((rank & 1) != 0)
            { /* odd */
                mpi_errno = MPIC_Send(recvbuf, count,
                                      datatype, rank-1,
                                      MPIR_REDUCE_TAG, comm_ptr);
                ON_ERROR_FAIL(mpi_errno);

                /* temporarily set the rank to -1 so that this
                   process does not pariticipate in recursive
                   doubling */
                newrank = -1;
            }
            else
            { /* even */
                mpi_errno = MPIC_Recv(tmp_buf, count,
                                      datatype, rank+1,
                                      MPIR_REDUCE_TAG, comm_ptr,
                                      MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);

                /* do the reduction on received data. */
                /* This algorithm is used only for predefined ops
                   and predefined ops are always commutative. */

                MPID_Uop_call(pOp, tmp_buf, recvbuf, &count, &hType);

                /* change the rank */
                newrank = rank / 2;
            }
        }
        else
        { /* rank >= 2*rem */
            newrank = rank - rem;
        }

        //
        // for the reduce-scatter, calculate the count that
        // each process receives and the displacement.
        // All processes will receive the same amount, except the
        // last one, which takes the remaining amount, equal or greater
        // than the amount all others have received.  Because all but
        // the last are equal, displacement amounts are just a multiple
        // of the rank of the processor.
        //

        reduceSize = count / pof2;
        endSize = count % pof2;

        if (newrank >= 0)
        {
            mask = 0x1;
            send_idx = recv_idx = 0;
            last_idx = pof2;
            idx_shift = pof2 >> 1;
            while (mask < pof2)
            {
                newdst = newrank ^ mask;
                /* find real rank of dest */
                dst = (newdst < rem) ? newdst*2 : newdst + rem;

                if (newrank < newdst)
                {
                    send_idx = recv_idx + idx_shift;
                    recv_cnt = (send_idx - recv_idx) * reduceSize;
                    send_cnt = (last_idx - send_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        send_cnt += endSize;
                    }
                }
                else
                {
                    recv_idx = send_idx + idx_shift;
                    send_cnt = (recv_idx - send_idx) * reduceSize;
                    recv_cnt = (last_idx - recv_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        recv_cnt += endSize;
                    }
                }

/*                    printf("Rank %d, send_idx %d, recv_idx %d, send_cnt %d, recv_cnt %d, last_idx %d\n", newrank, send_idx, recv_idx,
                      send_cnt, recv_cnt, last_idx);
*/
                /* Send data from recvbuf. Recv into tmp_buf */
                mpi_errno = MPIC_Sendrecv(reinterpret_cast<char *>(recvbuf) +
                                          reduceSize * send_idx * extent,
                                          send_cnt, datatype,
                                          dst, MPIR_REDUCE_TAG,
                                          reinterpret_cast<char *>(tmp_buf) +
                                          reduceSize * recv_idx * extent,
                                          recv_cnt, datatype, dst,
                                          MPIR_REDUCE_TAG, comm_ptr,
                                          MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);

                /* tmp_buf contains data received in this step.
                   recvbuf contains data accumulated so far */

                /* This algorithm is used only for predefined ops
                   and predefined ops are always commutative. */

                MPID_Uop_call(
                    pOp,
                    reinterpret_cast<char *>(tmp_buf) + reduceSize * recv_idx * extent,
                    reinterpret_cast<char *>(recvbuf) + reduceSize * recv_idx * extent,
                    &recv_cnt,
                    &hType
                    );

                /* update send_idx for next iteration */
                send_idx = recv_idx;
                mask <<= 1;

                /* update last_idx, but not in last iteration
                   because the value is needed in the gather
                   step below. */
                if (mask < pof2)
                {
                    last_idx = recv_idx + idx_shift;
                    idx_shift >>= 1;
                }

            }
        }

        /* now do the gather to root */

        /* Is root one of the processes that was excluded from the
           computation above? If so, send data from newrank=0 to
           the root and have root take on the role of newrank = 0 */

        if (root < 2*rem)
        {
            if ((root & 1) != 0)
            {
                if (rank == root) {    /* recv */
                    mpi_errno = MPIC_Recv(recvbuf, reduceSize, datatype,
                                          0, MPIR_REDUCE_TAG, comm_ptr,
                                          MPI_STATUS_IGNORE);
                    newrank = 0;
                    send_idx = 0;
                    last_idx = 2;
                }
                else if (newrank == 0)
                {  /* send */
                    mpi_errno = MPIC_Send(recvbuf, reduceSize, datatype,
                                          root, MPIR_REDUCE_TAG, comm_ptr);
                    newrank = -1;
                }
                newroot = 0;
            }
            else
            {
                newroot = root / 2;
            }
        }
        else
        {
            newroot = root - rem;
        }

        if (newrank >= 0)
        {
            //
            // Find the next lower power of 2, as well as the index of its set bit.
            //
            ULONG j;
            mask = pof2 >> 1;
            _BitScanForward( &j, pof2 );
            MPIU_Assert( mask == 0 || j >= 1 );
            --j;
            idx_shift = 1;

            while (mask > 0)
            {
                newdst = newrank ^ mask;

                /* find real rank of dest */
                dst = (newdst < rem) ? newdst*2 : newdst + rem;
                /* if root is playing the role of newdst=0, adjust for
                   it */
                if ((newdst == 0) && (root < 2 * rem) && ((root & 1) != 0))
                {
                    dst = root;
                }

                /* if the root of newdst's half of the tree is the
                   same as the root of newroot's half of the tree, send to
                   newdst and exit, else receive from newdst. */

                newdst_tree_root = newdst >> j;
                newdst_tree_root <<= j;

                newroot_tree_root = newroot >> j;
                newroot_tree_root <<= j;

                if (newrank < newdst)
                {
                    /* update last_idx except on first iteration */
                    if ((idx_shift & 1) == 0)
                    {
                        last_idx += idx_shift;
                    }

                    recv_idx = send_idx + idx_shift;
                    send_cnt = (recv_idx - send_idx) * reduceSize;
                    recv_cnt = (last_idx - recv_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        recv_cnt += endSize;
                    }
                }
                else
                {
                    recv_idx = send_idx - idx_shift;
                    recv_cnt = (send_idx - recv_idx) * reduceSize;
                    send_cnt = (last_idx - send_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        send_cnt += endSize;
                    }
                }

                if (newdst_tree_root == newroot_tree_root)
                {
                    /* send and exit */
/*                     printf("Rank %d, send_idx %d, send_cnt %d, last_idx %d\n", newrank, send_idx, send_cnt, last_idx);
                       fflush(stdout); */
                    /* Send data from recvbuf. Recv into tmp_buf */
                    mpi_errno = MPIC_Send(reinterpret_cast<char *>(recvbuf) +
                                          reduceSize * send_idx *extent,
                                          send_cnt, datatype,
                                          dst, MPIR_REDUCE_TAG,
                                          comm_ptr);
                    ON_ERROR_FAIL(mpi_errno);
                    break;
                }
                else
                {
                    /* recv and continue */
                    /* printf("Rank %d, recv_idx %d, recv_cnt %d, last_idx %d\n", newrank, recv_idx, recv_cnt, last_idx);
                       fflush(stdout); */
                    mpi_errno = MPIC_Recv(reinterpret_cast<char *>(recvbuf) +
                                          reduceSize * recv_idx * extent,
                                          recv_cnt, datatype, dst,
                                          MPIR_REDUCE_TAG, comm_ptr,
                                          MPI_STATUS_IGNORE);
                    ON_ERROR_FAIL(mpi_errno);
                }

                if (newrank > newdst)
                {
                    send_idx = recv_idx;
                }

                idx_shift <<= 1;
                j--;
                mask >>= 1;
            }
        }
    }

    else
    {  /* use a binomial tree algorithm */

        /* This code is from MPICH-1. */

        /* Here's the algorithm.  Relative to the root, look at the bit pattern in
           my rank.  Starting from the right (lsb), if the bit is 1, send to
           the node with that bit zero and exit; if the bit is 0, receive from the
           node with that bit set and combine (as long as that node is within the
           group)

           Note that by receiving with source selection, we guarantee that we get
           the same bits with the same input.  If we allowed the parent to receive
           the children in any order, then timing differences could cause different
           results (roundoff error, over/underflows in some cases, etc).

           Because of the way these are ordered, if root is 0, then this is correct
           for both commutative and non-commutative operations.  If root is not
           0, then for non-commutative, we use a root of zero and then send
           the result to the root.  To see this, note that the ordering is
           mask = 1: (ab)(cd)(ef)(gh)            (odds send to evens)
           mask = 2: ((ab)(cd))((ef)(gh))        (3,6 send to 0,4)
           mask = 4: (((ab)(cd))((ef)(gh)))      (4 sends to 0)

           Comments on buffering.
           If the datatype is not contiguous, we still need to pass contiguous
           data to the user routine.
           In this case, we should make a copy of the data in some format,
           and send/operate on that.

           In general, we can't use MPI_PACK, because the alignment of that
           is rather vague, and the data may not be re-usable.  What we actually
           need is a "squeeze" operation that removes the skips.
        */
        mask    = 0x1;
        if( pOp->IsCommutative() == true )
        {
            lroot = root;
        }
        else
        {
            lroot = 0;
        }

        relrank = rank - lroot;
        if (relrank < 0)
        {
            relrank += comm_size;
        }

        while (/*(mask & relrank) == 0 && */mask < comm_size)
        {
            /* Receive */
            if ((mask & relrank) == 0)
            {
                source = (relrank | mask);
                if (source < comm_size)
                {
                    source += lroot;
                    if (source >= comm_size)
                    {
                        source -= comm_size;
                    }
                    mpi_errno = MPIC_Recv(tmp_buf, count, datatype, source,
                        MPIR_REDUCE_TAG, comm_ptr, &status);
                    ON_ERROR_FAIL(mpi_errno);

                    /* The sender is above us, so the received buffer must be
                       the second argument (in the noncommutative case). */
                    if( pOp->IsCommutative() == true )
                    {
                        MPID_Uop_call(pOp, tmp_buf, recvbuf, &count, &hType);
                    }
                    else
                    {
                        MPID_Uop_call(pOp, recvbuf, tmp_buf, &count, &hType);

                        mpi_errno = MPIR_Localcopy(tmp_buf, count, datatype,
                            recvbuf, count, datatype);
                        ON_ERROR_FAIL(mpi_errno);
                    }
                }
            }
            else
            {
                /* I've received all that I'm going to.  Send my result to
                   my parent */
                source = (relrank & (~mask)) + lroot;
                if (source >= comm_size)
                {
                    source -= comm_size;
                }
                mpi_errno = MPIC_Send(recvbuf, count, datatype,
                    source, MPIR_REDUCE_TAG, comm_ptr);
                ON_ERROR_FAIL(mpi_errno);
                break;
            }
            mask <<= 1;
        }

        if( pOp->IsCommutative() == false && (root != 0) )
        {
            if (rank == 0)
            {
                mpi_errno  = MPIC_Send( recvbuf, count, datatype, root,
                                        MPIR_REDUCE_TAG, comm_ptr );
            }
            else if (rank == root)
            {
                mpi_errno = MPIC_Recv ( recvbuf, count, datatype, 0,
                                        MPIR_REDUCE_TAG, comm_ptr, &status);
            }
            ON_ERROR_FAIL(mpi_errno);
        }
    }

    if (Mpi.CallState->op_errno)
    {
        mpi_errno = Mpi.CallState->op_errno;
        goto fn_fail;
    }

  fn_exit:
    return (mpi_errno);

  fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static MPI_RESULT
MPIR_Reduce_intra_HA(
    _In_opt_  const void*      sendbuf,
    _When_(root == comm_ptr->rank, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    )
{
    StackGuardArray<BYTE> auto_buf;
    MPI_Count true_lb;
    MPI_Aint true_extent, extent;
    //
    // fix OACR warning 6101
    //
    OACR_USE_PTR(recvbuf);

    if( comm_ptr->intra.leaders_subcomm == nullptr )
    {
        //
        // we're just a lowly leaf.
        //
        MPIU_Assert( comm_ptr->intra.local_subcomm != nullptr );
        MPIU_Assert( comm_ptr->intra.ha_mappings[root].isLocal == 1 );
        return MPIR_Reduce_intra_flat(
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
            comm_ptr->intra.ha_mappings[root].rank,
            comm_ptr->intra.local_subcomm
            );
    }

    //
    // Leaders use a temporary buffer
    //
    true_extent = fixme_cast<MPI_Aint>(datatype.GetTrueExtentAndLowerBound( &true_lb ));
    extent = fixme_cast<MPI_Aint>(datatype.GetExtent());

    auto_buf = new BYTE[count*(max(extent,true_extent))];
    MPI_RESULT mpi_errno;
    if( auto_buf == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    /* adjust for potential negative lower bound in datatype */
    BYTE* tmp_buf = auto_buf - true_lb;

    //
    // Do the intranode reduce on all nodes if the root is not local.  Note that
    // leaders are always non-local in the mapping, so if the root is a leader,
    // it will perform the local reduction first, which should speed things up
    // since the remote leaders will all do their local reductions first too.
    //
    if (comm_ptr->intra.ha_mappings[root].isLocal == 0)
    {
        if (comm_ptr->intra.local_subcomm != nullptr)
        {
            if( sendbuf == MPI_IN_PLACE )
            {
                MPIU_Assert( comm_ptr->rank == root );
                sendbuf = recvbuf;
            }

            mpi_errno = MPIR_Reduce_intra_flat(
                sendbuf,
                tmp_buf,
                count,
                datatype,
                pOp,
                0,
                comm_ptr->intra.local_subcomm
                );
            ON_ERROR_FAIL(mpi_errno);

            //
            // Use tmp_buf as send buffer for the reductions between leaders.
            //
            sendbuf = tmp_buf;
        }

        mpi_errno = MPIR_Reduce_intra_flat(
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
            comm_ptr->intra.ha_mappings[root].rank,
            comm_ptr->intra.leaders_subcomm
            );
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        //
        // I am on the root's node (but am not the root).
        //
        MPIU_Assert(comm_ptr->rank != root);

        //
        // We must do the reduction from the remote leaders first, since
        // those values must be accounted for in the final reduction to the
        // local root.
        //
        // Note that recvbuf here is not valid, so we use the temp buffer.
        //
        mpi_errno = MPIR_Reduce_intra_flat(
            sendbuf,
            tmp_buf,
            count,
            datatype,
            pOp,
            comm_ptr->intra.leaders_subcomm->rank,
            comm_ptr->intra.leaders_subcomm
            );
        ON_ERROR_FAIL(mpi_errno);

        //
        // The result of the intranode reduction becomes the send buffer
        // for the local reduction.
        //
        sendbuf = tmp_buf;

        if (comm_ptr->intra.local_subcomm != nullptr)
        {
            MPIU_Assert( comm_ptr->intra.ha_mappings[root].isLocal == 1 || comm_ptr->rank == root);
            mpi_errno = MPIR_Reduce_intra_flat(
                sendbuf,
                recvbuf,
                count,
                datatype,
                pOp,
                comm_ptr->intra.ha_mappings[root].rank,
                comm_ptr->intra.local_subcomm
                );
            ON_ERROR_FAIL(mpi_errno);
        }
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_intra(
    _In_opt_  const void*      sendbuf,
    _When_(root == comm_ptr->rank, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    )
{
    MPIU_Assert(comm_ptr->comm_kind != MPID_INTERCOMM);

    bool is_contig;
    MPI_Aint true_lb;
    MPIDI_msg_sz_t nbytes = fixme_cast<MPIDI_msg_sz_t>(
        datatype.GetSizeAndInfo( count, &is_contig, &true_lb )
        );

    if (Mpi.SwitchoverSettings.SmpReduceEnabled &&
        comm_ptr->IsNodeAware() &&
        nbytes >= comm_ptr->SwitchPoints()->MPIR_reduce_smp_threshold &&
        nbytes < comm_ptr->SwitchPoints()->MPIR_reduce_smp_ceiling &&
        pOp->IsCommutative())
    {
        return MPIR_Reduce_intra_HA(
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
            root,
            comm_ptr
            );
    }
    return MPIR_Reduce_intra_flat(
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
            root,
            comm_ptr
            );
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_inter(
    _When_(root > 0, _In_opt_)
              const void*      sendbuf,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _mpi_coll_rank_(root) int  root,
    _In_      const MPID_Comm* comm_ptr
    )
{
/*  Intercommunicator reduce.
    Remote group does a local intracommunicator
    reduce to rank 0. Rank 0 then sends data to root.

    Cost: (lgp+1).alpha + n.(lgp+1).beta
*/
    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    MPI_RESULT mpi_errno;
    int rank;
    MPI_Status status;
    MPI_Aint true_extent, extent;
    MPI_Count true_lb;
    StackGuardArray<BYTE> auto_buf;
    BYTE *tmp_buf=NULL;

    if (root == MPI_PROC_NULL)
    {
        /* local processes other than root do nothing */
        return MPI_SUCCESS;
    }

    if (root == MPI_ROOT)
    {
            /* root receives data from rank 0 on remote group */
        mpi_errno = MPIC_Recv(recvbuf, count, datatype, 0,
                              MPIR_REDUCE_TAG, comm_ptr, &status);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* remote group. Rank 0 allocates temporary buffer, does
           local intracommunicator reduce, and then sends the data
           to root. */

        rank = comm_ptr->rank;

        if (rank == 0)
        {
            true_extent = fixme_cast<MPI_Aint>(datatype.GetTrueExtentAndLowerBound( &true_lb ));
            extent = fixme_cast<MPI_Aint>(datatype.GetExtent());

            auto_buf = new BYTE[count*(max(extent,true_extent))];
            if( auto_buf == nullptr )
            {
                mpi_errno = MPIU_ERR_NOMEM();
                goto fn_fail;
            }

            /* adjust for potential negative lower bound in datatype */
            tmp_buf = auto_buf - true_lb;
        }

        /* now do a local reduce on this intracommunicator */
        mpi_errno = MPIR_Reduce_intra(sendbuf, tmp_buf, count, datatype,
                                      pOp, 0, comm_ptr->inter.local_comm);
        ON_ERROR_FAIL(mpi_errno);

        if (rank == 0)
        {
            mpi_errno = MPIC_Send(tmp_buf, count, datatype, root,
                                  MPIR_REDUCE_TAG, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);
        }
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


/* This is the default implementation of reduce_scatter. The algorithm is:

   Algorithm: MPI_Reduce_scatter

   If the operation is commutative, for short and medium-size
   messages, we use a recursive-halving
   algorithm in which the first p/2 processes send the second n/2 data
   to their counterparts in the other half and receive the first n/2
   data from them. This procedure continues recursively, halving the
   data communicated at each step, for a total of lgp steps. If the
   number of processes is not a power-of-two, we convert it to the
   nearest lower power-of-two by having the first few even-numbered
   processes send their data to the neighboring odd-numbered process
   at (rank+1). Those odd-numbered processes compute the result for
   their left neighbor as well in the recursive halving algorithm, and
   then at  the end send the result back to the processes that didn't
   participate.
   Therefore, if p is a power-of-two,
   Cost = lgp.alpha + n.((p-1)/p).beta + n.((p-1)/p).gamma
   If p is not a power-of-two,
   Cost = (floor(lgp)+2).alpha + n.(1+(p-1+n)/p).beta + n.(1+(p-1)/p).gamma
   The above cost in the non power-of-two case is approximate because
   there is some imbalance in the amount of work each process does
   because some processes do the work of their neighbors as well.

   For commutative operations and very long messages we use
   we use a pairwise exchange algorithm similar to
   the one used in MPI_Alltoall. At step i, each process sends n/p
   amount of data to (rank+i) and receives n/p amount of data from
   (rank-i).
   Cost = (p-1).alpha + n.((p-1)/p).beta + n.((p-1)/p).gamma


   If the operation is not commutative, we do the following:

   For very short messages, we use a recursive doubling algorithm, which
   takes lgp steps. At step 1, processes exchange (n-n/p) amount of
   data; at step 2, (n-2n/p) amount of data; at step 3, (n-4n/p)
   amount of data, and so forth.

   Cost = lgp.alpha + n.(lgp-(p-1)/p).beta + n.(lgp-(p-1)/p).gamma

   For medium and long messages, we use pairwise exchange as above.

   Possible improvements:

   End Algorithm: MPI_Reduce_scatter
*/
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
MPIR_Reduce_scatter_commutative_short(
    _In_opt_  const void*      sendbuf,
    _When_( recvcnts[comm_ptr->rank] != 0, _Out_opt_ )
              void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      int              total_count,
    _In_      TypeHandle       datatype,
    _In_      MPI_Aint         true_extent,
    _In_      MPI_Aint         extent,
    _In_      MPI_Aint         true_lb,
    _In_      void*            tmp_recvbuf,
    _In_      const MPID_Comm* comm_ptr,
    _In_      const MPID_Op*   pOp,
    _In_      const int*       disps,
    _In_      int              tag
    )
{
    //
    // There is a success case where we don't touch recvbuf:
    // sendbuf != MPI_IN_PLACE && rank < 2 * rem && (rank & 1) == 0
    // Thus suppress OACR C6101 for recvbuf here.
    //
    OACR_USE_PTR( recvbuf );

    StackGuardArray<BYTE> auto_buf;
    BYTE *tmp_results;
    int pof2;
    int rem;
    int newrank;
    int mask;
    int dst;
    int newdst;
    int send_idx;
    int recv_idx;
    int last_idx;
    int *newdisps;
    MPI_RESULT mpi_errno;
    int i;
    int old_i;
    int send_cnt;
    int recv_cnt;
    int comm_size;
    int rank;
    StackGuardArray<int> newcnts;

    //
    // Commutative and short message, use recursive halving algorithm.
    //

    /* need to allocate another temporary buffer to accumulate
    results because recvbuf may not be big enough */
    auto_buf = new BYTE[total_count*(max(true_extent,extent))];
    if( auto_buf == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    /* adjust for potential negative lower bound in datatype */
    tmp_results = auto_buf - true_lb;

    /* copy sendbuf into tmp_results */
    if (sendbuf != MPI_IN_PLACE)
    {
        mpi_errno = MPIR_Localcopy(sendbuf, total_count, datatype,
                                    tmp_results, total_count, datatype);
    }
    else
    {
        mpi_errno = MPIR_Localcopy(recvbuf, total_count, datatype,
                                    tmp_results, total_count, datatype);
    }

    ON_ERROR_FAIL(mpi_errno);

    MPI_Datatype hType = datatype.GetMpiHandle();

    comm_size = comm_ptr->remote_size;
    pof2 = PowerOf2Floor( comm_size );

    rem = comm_size - pof2;

    /* In the non-power-of-two case, all even-numbered
        processes of rank < 2*rem send their data to
        (rank+1). These even-numbered processes no longer
        participate in the algorithm until the very end. The
        remaining processes form a nice power-of-two. */

    rank = comm_ptr->rank;

    if (rank < 2*rem)
    {
        if ((rank & 1) == 0)
        { /* even */
            mpi_errno = MPIC_Send(tmp_results, total_count,
                                    datatype, rank+1,
                                    tag, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);

            /* temporarily set the rank to -1 so that this
                process does not pariticipate in recursive
                doubling */
            newrank = -1;
        }
        else
        { /* odd */
            mpi_errno = MPIC_Recv(tmp_recvbuf, total_count,
                                    datatype, rank-1,
                                    tag, comm_ptr,
                                    MPI_STATUS_IGNORE);
            ON_ERROR_FAIL(mpi_errno);

            /* do the reduction on received data. since the
                ordering is right, it doesn't matter whether
                the operation is commutative or not. */

            MPID_Uop_call(pOp, tmp_recvbuf, tmp_results, &total_count, &hType);

            /* change the rank */
            newrank = rank / 2;
        }
    }
    else  /* rank >= 2*rem */
    {
        newrank = rank - rem;
    }

    if (newrank >= 0)
    {
        /* recalculate the recvcnts and disps arrays because the
            even-numbered processes who no longer participate will
            have their result calculated by the process to their
            right (rank+1). */

        //
        // We allocate both the new count and displacements as a single array.
        //
        newcnts = new int[pof2 * 2];
        if( newcnts == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        newdisps = newcnts + pof2;
        newdisps[0] = 0;

        //
        // Below rem, account for right & left combined
        //
        old_i = 0;
        for (i = 0; i < rem; i++)
        {
            newcnts[i] = recvcnts[old_i++];
            newcnts[i] += recvcnts[old_i++];
            newdisps[i+1] = newdisps[i] + newcnts[i];
        }
        //
        // i now equals rem; old_i equals 2*rem
        // Transfer the remaining recvcnts to newcnts
        // and account for newdisps
        //
        do
        {
            newcnts[i] = recvcnts[old_i];
            if (++old_i < comm_size)
            {
                newdisps[i+1] = newdisps[i] + newcnts[i];
                ++i;
            }
        } while (old_i < comm_size);

        mask = pof2 >> 1;
        send_idx = recv_idx = 0;
        last_idx = pof2;
        while (mask > 0)
        {
            newdst = newrank ^ mask;
            /* find real rank of dest */
            dst = (newdst < rem) ? newdst*2 + 1 : newdst + rem;

            send_cnt = recv_cnt = 0;
            if (newrank < newdst)
            {
                send_idx = recv_idx + mask;
                for (i = send_idx; i < last_idx; i++)
                {
                    send_cnt += newcnts[i];
                }
                for (i = recv_idx; i < send_idx; i++)
                {
                    recv_cnt += newcnts[i];
                }
            }
            else
            {
                recv_idx = send_idx + mask;
                for (i = send_idx; i < recv_idx; i++)
                {
                    send_cnt += newcnts[i];
                }
                for (i = recv_idx; i < last_idx; i++)
                {
                    recv_cnt += newcnts[i];
                }
            }

            /* Send data from tmp_results. Recv into tmp_recvbuf */
            if ((send_cnt != 0) && (recv_cnt != 0))
            {
                mpi_errno = MPIC_Sendrecv(reinterpret_cast<char *>(tmp_results) +
                                        newdisps[send_idx]*extent,
                                        send_cnt, datatype,
                                        dst, tag,
                                        reinterpret_cast<char *>(tmp_recvbuf) +
                                        newdisps[recv_idx]*extent,
                                        recv_cnt, datatype, dst,
                                        tag, comm_ptr,
                                        MPI_STATUS_IGNORE);
            }
            else if ((send_cnt == 0) && (recv_cnt != 0))
            {
                mpi_errno = MPIC_Recv(reinterpret_cast<char *>(tmp_recvbuf)+
                                        newdisps[recv_idx]*extent,
                                        recv_cnt, datatype, dst,
                                        tag, comm_ptr,
                                        MPI_STATUS_IGNORE);
            }
            else if ((recv_cnt == 0) && (send_cnt != 0))
            {
                mpi_errno = MPIC_Send(reinterpret_cast<char *>(tmp_results)+
                                        newdisps[send_idx]*extent,
                                        send_cnt, datatype,
                                        dst, tag,
                                        comm_ptr);
            }

            ON_ERROR_FAIL(mpi_errno);

            /* tmp_recvbuf contains data received in this step.
                tmp_results contains data accumulated so far */

            if (recv_cnt)
            {

                MPID_Uop_call(
                    pOp,
                    reinterpret_cast<char *>(tmp_recvbuf) + newdisps[recv_idx] * extent,
                    reinterpret_cast<char *>(tmp_results) + newdisps[recv_idx] * extent,
                    &recv_cnt,
                    &hType
                    );

            }

            /* update send_idx for next iteration */
            send_idx = recv_idx;
            last_idx = recv_idx + mask;
            mask >>= 1;
        }

        /* copy this process's result from tmp_results to recvbuf */
        if (recvcnts[rank])
        {
            mpi_errno = MPIR_Localcopy(reinterpret_cast<char *>(tmp_results) +
                                        disps[rank]*extent,
                                        recvcnts[rank], datatype, recvbuf,
                                        recvcnts[rank], datatype);
            ON_ERROR_FAIL(mpi_errno);
        }

    }
    /* In the non-power-of-two case, all odd-numbered
        processes of rank < 2*rem send to (rank-1) the result they
        calculated for that process */
    if (rank < 2*rem)
    {
        if ((rank & 1) != 0)
        { /* odd */
            if (recvcnts[rank-1])
            {
                mpi_errno = MPIC_Send(reinterpret_cast<char *>(tmp_results)+
                                    disps[rank-1]*extent, recvcnts[rank-1],
                                    datatype, rank-1,
                                    tag, comm_ptr);
            }
        }
        else
        { /* even */
            if (recvcnts[rank])
            {
                mpi_errno = MPIC_Recv(recvbuf, recvcnts[rank],
                                    datatype, rank+1,
                                    tag, comm_ptr,
                                    MPI_STATUS_IGNORE);
            }
        }

        ON_ERROR_FAIL(mpi_errno);
    }

fn_fail:
    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
MPIR_Reduce_scatter_commutative_long(
    _In_opt_  const void*      sendbuf,
    _When_( recvcnts[comm_ptr->rank] != 0, _Out_opt_ )
              void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPI_Aint         extent,
    _In_      void*            tmp_recvbuf,
    _In_      const MPID_Comm* comm_ptr,
    _In_      const MPID_Op*   pOp,
    _In_      const int*       disps,
    _In_      int              tag
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int i;
    int src;
    int dst;
    int comm_size;
    int rank = comm_ptr->rank;
    //
    // Commutative and long message, use (p-1) pairwise exchanges.
    //

    if (sendbuf != MPI_IN_PLACE)
    {
        /* copy local data into recvbuf */
        mpi_errno = MPIR_Localcopy(((char *)(sendbuf) + disps[rank] * extent),
                                    recvcnts[rank], datatype, recvbuf,
                                    recvcnts[rank], datatype);
        ON_ERROR_FAIL(mpi_errno);
    }

    src = dst = rank;
    comm_size = comm_ptr->remote_size;
    for (i = comm_size - 1; i > 0; i--)
    {
        src--;
        if (src < 0)
        {
            src += comm_size;
        }
        dst++;
        if (dst == comm_size)
        {
            dst = 0;
        }

        /* send the data that dst needs. recv data that this process
            needs from src into tmp_recvbuf */
        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MPIC_Sendrecv(((char *)(sendbuf) + disps[dst] * extent),
                recvcnts[dst], datatype, dst,
                tag, tmp_recvbuf,
                recvcnts[rank], datatype, src,
                tag, comm_ptr,
                MPI_STATUS_IGNORE);
        }
        else
        {
            mpi_errno = MPIC_Sendrecv((reinterpret_cast<char *>(recvbuf)+disps[dst] * extent),
                recvcnts[dst], datatype, dst,
                tag, tmp_recvbuf,
                recvcnts[rank], datatype, src,
                tag, comm_ptr,
                MPI_STATUS_IGNORE);
        }

        ON_ERROR_FAIL(mpi_errno);

        MPI_Datatype hType = datatype.GetMpiHandle();
        if (sendbuf != MPI_IN_PLACE)
        {
            MPID_Uop_call(pOp, tmp_recvbuf, recvbuf, &recvcnts[rank], &hType);
        }
        else
        {

            MPID_Uop_call(
                pOp,
                tmp_recvbuf,
                reinterpret_cast<char *>(recvbuf) + disps[rank] * extent,
                &recvcnts[rank],
                &hType
                );

            /* we can't store the result at the beginning of
                recvbuf right here because there is useful data
                there that other process/processes need. at the
                end, we will copy back the result to the
                beginning of recvbuf. */
        }
    }

    /* if MPI_IN_PLACE, move output data to the beginning of
        recvbuf. already done for rank 0. */
    if ((sendbuf == MPI_IN_PLACE) && (rank != 0))
    {
        mpi_errno = MPIR_Localcopy((reinterpret_cast<char *>(recvbuf) +
                                    disps[rank]*extent),
                                    recvcnts[rank], datatype,
                                    recvbuf,
                                    recvcnts[rank], datatype);

        ON_ERROR_FAIL(mpi_errno);
    }
fn_fail:
    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
MPIR_Reduce_scatter_non_commutative(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_range_(>, 0)
              int              total_count,
    _In_      TypeHandle       datatype,
    _In_      MPI_Aint         true_extent,
    _In_      MPI_Aint         extent,
    _In_      MPI_Aint         true_lb,
    _In_      void*            tmp_recvbuf,
    _In_      const MPID_Comm* comm_ptr,
    _In_      const MPID_Op*   pOp,
    _In_      const int*       disps,
    _In_      int              tag
    )
{
    StackGuardArray<BYTE> auto_buf;
    BYTE *tmp_results;
    int mask;
    int dst;
    MPI_RESULT mpi_errno;
    int i;
    int j;
    ULONG k;
    int blklens[2];
    int tree_root;
    int my_tree_root;
    int dis[2];
    int dst_tree_root;
    int tree_root_cap;
    MPI_Datatype sendtype;
    MPI_Datatype recvtype;
    int received;
    int tmp_mask;
    int nprocs_completed;
    int comm_size;
    int rank;

    //
    // Non-commutative, use recursive doubling.
    //

    /* need to allocate another temporary buffer to accumulate
        results */
    auto_buf = new BYTE[total_count*(max(true_extent,extent))];
    if( auto_buf == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    /* adjust for potential negative lower bound in datatype */
    tmp_results = auto_buf - true_lb;

    /* copy sendbuf into tmp_results */
    if (sendbuf != MPI_IN_PLACE)
    {
        mpi_errno = MPIR_Localcopy(sendbuf, total_count, datatype,
                                    tmp_results, total_count, datatype);
    }
    else
    {
        mpi_errno = MPIR_Localcopy(recvbuf, total_count, datatype,
                                    tmp_results, total_count, datatype);
    }

    ON_ERROR_FAIL(mpi_errno);

    rank = comm_ptr->rank;
    mask = 0x1;
    i = 0;
    comm_size = comm_ptr->remote_size;
    while (mask < comm_size)
    {
        dst = rank ^ mask;

        dst_tree_root = dst >> i;
        dst_tree_root <<= i;

        my_tree_root = rank >> i;
        my_tree_root <<= i;

        /* At step 1, processes exchange (n-n/p) amount of
            data; at step 2, (n-2n/p) amount of data; at step 3, (n-4n/p)
            amount of data, and so forth. We use derived datatypes for this.

            At each step, a process does not need to send data
            indexed from my_tree_root to
            my_tree_root+mask-1. Similarly, a process won't receive
            data indexed from dst_tree_root to dst_tree_root+mask-1. */

        /* calculate sendtype */
        blklens[0] = blklens[1] = 0;
        for (j = 0; j < my_tree_root; j++)
        {
            blklens[0] += recvcnts[j];
        }
        for (j = my_tree_root + mask; j<comm_size; j++)
        {
            blklens[1] += recvcnts[j];
        }

        dis[0] = 0;
        dis[1] = blklens[0];
        tree_root_cap = my_tree_root + mask;
        if (tree_root_cap > comm_size)
        {
            tree_root_cap = comm_size;
        }
        for (j = my_tree_root; j < tree_root_cap; j++)
        {
            dis[1] += recvcnts[j];
        }

        //
        // TODO:  Free if things fail
        //
        NMPI_Type_indexed(2, blklens, dis, datatype.GetMpiHandle(), &sendtype);
        NMPI_Type_commit(&sendtype);

        /* calculate recvtype */
        blklens[0] = blklens[1] = 0;
        tree_root_cap = dst_tree_root;
        if (tree_root_cap > comm_size)
        {
            tree_root_cap = comm_size;
        }
        for (j = 0; j < tree_root_cap; j++)
        {
            blklens[0] += recvcnts[j];
        }
        for (j = dst_tree_root + mask; j < comm_size; j++)
        {
            blklens[1] += recvcnts[j];
        }

        dis[0] = 0;
        dis[1] = blklens[0];
        tree_root_cap = dst_tree_root + mask;
        if (tree_root_cap > comm_size)
        {
            tree_root_cap = comm_size;
        }
        for (j = dst_tree_root; j < tree_root_cap; j++)
        {
            dis[1] += recvcnts[j];
        }

        //
        // TODO:  Free if things fail
        //
        NMPI_Type_indexed(2, blklens, dis, datatype.GetMpiHandle(), &recvtype);
        NMPI_Type_commit(&recvtype);

        received = 0;
        if (dst < comm_size)
        {
            /* tmp_results contains data to be sent in each step. Data is
                received in tmp_recvbuf and then accumulated into
                tmp_results. accumulation is done later below.   */

            mpi_errno = MPIC_Sendrecv(tmp_results, 1,
                                        TypeHandle::Create( sendtype ),
                                        dst,
                                        tag,
                                        tmp_recvbuf, 1,
                                        TypeHandle::Create( recvtype ),
                                        dst,
                                        tag, comm_ptr,
                                        MPI_STATUS_IGNORE);
            received = 1;

            ON_ERROR_FAIL(mpi_errno);
        }

        /* if some processes in this process's subtree in this step
            did not have any destination process to communicate with
            because of non-power-of-two, we need to send them the
            result. We use a logarithmic recursive-halfing algorithm
            for this. */

        if (dst_tree_root + mask > comm_size)
        {
            nprocs_completed = comm_size - my_tree_root - mask;
            /* nprocs_completed is the number of processes in this
                subtree that have all the data. Send data to others
                in a tree fashion. First find root of current tree
                that is being divided into two. k is the number of
                least-significant bits in this process's rank that
                must be zeroed out to find the rank of the root */

            _BitScanReverse(&k, mask);

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
                    /* send the current result */
                    mpi_errno = MPIC_Send(tmp_recvbuf, 1,
                                            TypeHandle::Create( recvtype ),
                                            dst, tag,
                                            comm_ptr);
                    ON_ERROR_FAIL(mpi_errno);
                }
                /* recv only if this proc. doesn't have data and sender
                    has data */
                else if ((dst < rank) &&
                            (dst < tree_root + nprocs_completed) &&
                            (rank >= tree_root + nprocs_completed))
                {
                    mpi_errno = MPIC_Recv(tmp_recvbuf, 1,
                                            TypeHandle::Create( recvtype ),
                                            dst, tag,
                                            comm_ptr, MPI_STATUS_IGNORE);
                    received = 1;

                    ON_ERROR_FAIL(mpi_errno);
                }
                tmp_mask >>= 1;
                k--;
            }
        }

        /* The following reduction is done here instead of after
            the MPIC_Sendrecv or MPIC_Recv above. This is
            because to do it above, in the noncommutative
            case, we would need an extra temp buffer so as not to
            overwrite temp_recvbuf, because temp_recvbuf may have
            to be communicated to other processes in the
            non-power-of-two case. To avoid that extra allocation,
            we do the reduce here. */
        if (received)
        {
            MPI_Datatype hType = datatype.GetMpiHandle();
            if( (pOp->IsCommutative() == true) || (dst_tree_root < my_tree_root) )
            {
                MPID_Uop_call(pOp, tmp_recvbuf, tmp_results, &blklens[0], &hType);
                MPID_Uop_call(
                    pOp,
                    reinterpret_cast<char *>(tmp_recvbuf) + dis[1] * extent,
                    reinterpret_cast<char *>(tmp_results) + dis[1] * extent,
                    &blklens[1],
                    &hType
                    );
            }
            else
            {
                MPID_Uop_call(pOp, tmp_results, tmp_recvbuf, &blklens[0], &hType);
                MPID_Uop_call(
                    pOp,
                    reinterpret_cast<char *>(tmp_results) + dis[1] * extent,
                    reinterpret_cast<char *>(tmp_recvbuf) + dis[1] * extent,
                    &blklens[1],
                    &hType
                    );

                /* copy result back into tmp_results */
                mpi_errno = MPIR_Localcopy(tmp_recvbuf, 1, TypeHandle::Create( recvtype ),
                                           tmp_results, 1, TypeHandle::Create( recvtype ) );
                ON_ERROR_FAIL(mpi_errno);
            }
        }

        NMPI_Type_free(&sendtype);
        NMPI_Type_free(&recvtype);

        mask <<= 1;
        i++;
    }

    /* now copy final results from tmp_results to recvbuf */
    mpi_errno = MPIR_Localcopy((reinterpret_cast<char *>(tmp_results) + disps[rank] * extent),
                                recvcnts[rank], datatype, recvbuf,
                                recvcnts[rank], datatype);

    ON_ERROR_FAIL(mpi_errno);
fn_fail:
    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
MPIR_Reduce_scatter_intra_impl(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr,
    _In_      int              tag
    )
{
    int comm_size;
    int i;
    MPI_Aint extent;
    MPI_Aint true_extent;
    MPI_Count true_lb;
    StackGuardArray<int> disps;
    StackGuardArray<BYTE> auto_buf;
    BYTE *tmp_recvbuf;
    int type_size;
    int total_count;
    unsigned int nbytes;
    MPI_RESULT mpi_errno;

    MPIU_Assert(comm_ptr->comm_kind != MPID_INTERCOMM);

    comm_size = comm_ptr->remote_size;

    /* set op_errno to 0. stored in perthread structure */
    Mpi.CallState->op_errno = 0;

    extent = fixme_cast<MPI_Aint>(datatype.GetExtent());
    true_extent = fixme_cast<MPI_Aint>(datatype.GetTrueExtentAndLowerBound( &true_lb ));

    disps = new int[comm_size];
    if( disps == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    total_count = 0;
    for (i=0; i<comm_size; i++)
    {
        disps[i] = total_count;
        total_count += recvcnts[i];
    }

    if (total_count == 0)
    {
        mpi_errno = MPI_SUCCESS;
        //
        // fix OACR warning 6101 for recvbuf
        //
        OACR_USE_PTR(recvbuf);
        goto fn_exit;
    }

    /* need to allocate temporary buffer to receive incoming data*/
    auto_buf = new BYTE[total_count*(max(true_extent,extent))];
    if( auto_buf == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    /* adjust for potential negative lower bound in datatype */
    tmp_recvbuf = auto_buf - true_lb;

    type_size = fixme_cast<int>(datatype.GetSize());
    nbytes = static_cast<unsigned int>(total_count * type_size);

    if (pOp->IsCommutative() == true)
    {
        if (nbytes < comm_ptr->SwitchPoints()->MPIR_redscat_commutative_long_msg)
        {
            mpi_errno = MPIR_Reduce_scatter_commutative_short(
                sendbuf,
                recvbuf,
                recvcnts,
                total_count,
                datatype,
                true_extent,
                extent,
                fixme_cast<MPI_Aint>(true_lb),
                tmp_recvbuf,
                comm_ptr,
                pOp,
                disps,
                tag
            );
        }
        else
        {
            mpi_errno = MPIR_Reduce_scatter_commutative_long(
                sendbuf,
                recvbuf,
                recvcnts,
                datatype,
                extent,
                tmp_recvbuf,
                comm_ptr,
                pOp,
                disps,
                tag
                );
        }
    }
    else
    {
        mpi_errno = MPIR_Reduce_scatter_non_commutative(
            sendbuf,
            recvbuf,
            recvcnts,
            total_count,
            datatype,
            true_extent,
            extent,
            fixme_cast<MPI_Aint>(true_lb),
            tmp_recvbuf,
            comm_ptr,
            pOp,
            disps,
            tag
        );
    }

    if (Mpi.CallState->op_errno)
    {
        mpi_errno = Mpi.CallState->op_errno;
    }

fn_exit:
fn_fail:
    return (mpi_errno);
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_block_intra(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
    //
    // A reduce_scatter_block is the same as a reduce_scatter where all the scattered blocks are of the same size
    // so take advantage of that to avoid duplicating code
    //
    int size = comm_ptr->remote_size;
    StackGuardArray<int> recvcnts = new int[size];
    if (recvcnts == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    for (int i = 0; i < size; i++)
    {
        recvcnts[i] = recvcount;
    }

    return MPIR_Reduce_scatter_intra_impl(sendbuf, recvbuf, recvcnts, datatype, pOp, comm_ptr, MPIR_REDUCE_SCATTER_BLOCK_TAG);
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_intra(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
    return MPIR_Reduce_scatter_intra_impl(sendbuf, recvbuf, recvcnts, datatype, pOp, comm_ptr, MPIR_REDUCE_SCATTER_TAG);
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_block_inter(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
    //
    // A reduce_scatter_block is the same as a reduce_scatter where all the scattered blocks are of the same size
    // so take advantage of that to avoid duplicating code
    //
    int size = comm_ptr->inter.local_size;
    StackGuardArray<int> recvcnts = new int[size];
    if (recvcnts == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    for (int i = 0; i < size; i++)
    {
        recvcnts[i] = recvcount;
    }
    return MPIR_Reduce_scatter_inter(sendbuf, recvbuf, recvcnts, datatype, pOp, comm_ptr);
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_inter(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
/* Intercommunicator Reduce_scatter.
   We first do an intercommunicator reduce to rank 0 on left group,
   then an intercommunicator reduce to rank 0 on right group, followed
   by local intracommunicator scattervs in each group.
*/
    MPI_RESULT mpi_errno;
    int rank, root, local_size, total_count, i;
    MPI_Aint true_extent;
    MPI_Count true_lb = 0;
    MPI_Aint extent;
    StackGuardArray<BYTE> auto_buf;
    BYTE *tmp_buf;
    StackGuardArray<int> disps;

    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    rank = comm_ptr->rank;
    local_size = comm_ptr->inter.local_size;

    total_count = 0;

    if (rank == 0)
    {
        /* In each group, rank 0 allocates a temp. buffer for the
           reduce */
        disps = new int[local_size];
        if( disps == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        for (i=0; i<local_size; i++)
        {
            disps[i] = total_count;
            total_count += recvcnts[i];
        }

        true_extent = fixme_cast<MPI_Aint>(datatype.GetTrueExtentAndLowerBound( &true_lb ));
        extent = fixme_cast<MPI_Aint>(datatype.GetExtent());

        auto_buf = new BYTE[total_count*(max(true_extent,extent))];
        if( auto_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        /* adjust for potential negative lower bound in datatype */
        tmp_buf = auto_buf - true_lb;
    }
    else
    {
        tmp_buf = nullptr;
        for (i = 0; i < local_size; i++)
        {
            total_count += recvcnts[i];
        }
    }

    /* first do a reduce from right group to rank 0 in left group,
       then from left group to rank 0 in right group*/
    if (comm_ptr->inter.is_low_group)
    {
        /* reduce from right group to rank 0*/
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Reduce_inter(sendbuf, tmp_buf, total_count, datatype, pOp,
                                root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* reduce to rank 0 of right group */
        root = 0;
        mpi_errno = MPIR_Reduce_inter(sendbuf, tmp_buf, total_count, datatype, pOp,
                                root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* reduce to rank 0 of left group */
        root = 0;
        mpi_errno = MPIR_Reduce_inter(sendbuf, tmp_buf, total_count, datatype, pOp,
                                root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* reduce from right group to rank 0 */
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Reduce_inter(sendbuf, tmp_buf, total_count, datatype, pOp,
                                root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }

    mpi_errno = MPIR_Scatterv(tmp_buf, recvcnts, disps, datatype, recvbuf,
                              recvcnts[rank], datatype, 0, comm_ptr->inter.local_comm);
    ON_ERROR_FAIL(mpi_errno);

fn_fail:
    return mpi_errno;
}


static inline
unsigned
TrimmedToOriginalRankEven(
    _In_ unsigned rem,
    _In_ unsigned rank
    )
{
    return rank < rem ? rank << 1 : rank + rem;
}


static inline
unsigned
TrimmedToOriginalRankOdd(
    _In_ unsigned rem,
    _In_ unsigned rank
    )
{
    return rank < rem ? rank << 1 | 1 : rank + rem;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IreduceScatterBuildCommutativeShortTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _When_(recvcnts[pComm->rank] > 0, _Out_opt_)
              void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              totalCount,
    _In_      MPI_Aint         extent,
    _In_      MPI_Aint         fullExtent,
    _In_      MPI_Count        trueLb,
    _In_      const int*       disps
    )
{
    //
    // Commutative reduce operation, short message size,
    // use recursive-halving algorithm.
    //

    //
    // Allocate scratch buffer to hold received data and intermediate reduce data.
    //
    pReq->nbc.tmpBuf = new BYTE[fullExtent * 2];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    //
    // Adjust for potential negative lower bound in datatype.
    //
    BYTE* tmpRecvBuf = pReq->nbc.tmpBuf - trueLb;
    BYTE* tmpReduceBuf = pReq->nbc.tmpBuf + fullExtent - trueLb;

    MPI_RESULT mpi_errno;
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            totalCount,
            datatype,
            tmpReduceBuf,
            totalCount,
            datatype
            );
    }
    else
    {
        mpi_errno = MPIR_Localcopy(
            recvbuf,
            totalCount,
            datatype,
            tmpReduceBuf,
            totalCount,
            datatype
            );
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    ULONG pof2 = PowerOf2Floor( size );
    ULONG lgpof2;
    _BitScanForward(&lgpof2, pof2);

    ULONG taskCount = 0;

    unsigned rem = size - pof2;
    int newRank;
    if( rank < 2 * rem )
    {
        if( ( rank & 1 ) != 0 )
        {
            taskCount += 3;
            newRank = rank >> 1;
        }
        else
        {
            taskCount += 2;
            newRank = -1;
        }
    }
    else
    {
        newRank = rank - rem;
    }

    if( newRank >= 0 )
    {
        //
        // This rank will be involved in the recursive-halving steps, increase
        // the task count for send, recv, reduce and the final local copy.
        //
        taskCount += lgpof2 * 3 + 1;
    }

    pReq->nbc.tasks = new NbcTask[taskCount];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    MPI_Datatype hType = datatype.GetMpiHandle();

    if( rank < 2 * rem )
    {
        if( ( rank & 1 ) != 0 )
        {
            //
            // Odd rank, receive from rank - 1 and then reduce.
            //
            mpi_errno = pTask->InitIrecv(
                tmpRecvBuf,
                totalCount,
                datatype,
                rank - 1
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            ++pTask;

            mpi_errno = pTask->InitReduce(
                tmpRecvBuf,
                tmpReduceBuf,
                totalCount,
                datatype,
                pOp,
                hType,
                true
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            ++pTask;
        }
        else
        {
            //
            // Even rank, send to rank + 1, then receive from rank + 1.
            //
            mpi_errno = pTask->InitIsend(
                tmpReduceBuf,
                totalCount,
                datatype,
                rank + 1,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            if( recvcnts[rank] > 0 )
            {
                mpi_errno = pTask->InitIrecv(
                    recvbuf,
                    recvcnts[rank],
                    datatype,
                    rank + 1
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
            }
            else
            {
                pTask->InitNoOp();
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pReq->nbc.nTasks;

            return MPI_SUCCESS;
        }
    }

    //
    // Recalculate the recvcnts and disps arrays because the
    // even-numbered processes who no longer participate will
    // have their result calculated by the process to their
    // right (rank + 1).
    //
    StackGuardArray<int> newcnts = new int[pof2 * 2];
    if( newcnts == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    int* newdisps = newcnts + pof2;
    newdisps[0] = 0;

    //
    // Combine even and odd ranks' recvcnts and create new disps for ranks
    // below rem * 2.
    //
    unsigned oldIdx = 0;
    unsigned newIdx;
    for( newIdx = 0; newIdx < rem; newIdx++ )
    {
        newcnts[newIdx] = recvcnts[oldIdx++];
        newcnts[newIdx] += recvcnts[oldIdx++];
        newdisps[newIdx + 1] = newdisps[newIdx] + newcnts[newIdx];
    }
    //
    // Copy the rest of the recvcnts into the newcnts array and keep
    // populating the new newdisps array.
    //
    do
    {
        newcnts[newIdx] = recvcnts[oldIdx];
        if( ++oldIdx < size )
        {
            newdisps[newIdx + 1] = newdisps[newIdx] + newcnts[newIdx];
            ++newIdx;
        }
    }
    while( oldIdx < size );

    //
    // Recursive-halving algorithm.
    //
    unsigned sendIdx = 0;
    unsigned recvIdx = 0;
    unsigned lastIdx = pof2;

    mpi_errno = BinomialChildBuilderDescending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            unsigned newDst = newRank ^ offset;
            unsigned dst = TrimmedToOriginalRankOdd( rem, newDst );
            bool rightOrder = dst < rank ? true : false;

            int sendCount = 0;
            int recvCount = 0;
            unsigned i;

            if( static_cast<unsigned>( newRank ) < newDst )
            {
                sendIdx = recvIdx + offset;
                for( i = sendIdx; i < lastIdx; i++ )
                {
                    sendCount += newcnts[i];
                }
                for( i = recvIdx; i < sendIdx; i++ )
                {
                    recvCount += newcnts[i];
                }
            }
            else
            {
                recvIdx = sendIdx + offset;
                for( i = sendIdx; i < recvIdx; i++ )
                {
                    sendCount += newcnts[i];
                }
                for( i = recvIdx; i < lastIdx; i++ )
                {
                    recvCount += newcnts[i];
                }
            }

            BYTE* pRecvBuf = tmpRecvBuf + newdisps[recvIdx] * extent;
            const BYTE* pSendBuf = tmpReduceBuf + newdisps[sendIdx] * extent;
            BYTE* pReduceBuf = tmpReduceBuf + newdisps[recvIdx] * extent;

            if( recvCount > 0 )
            {
                mpi_errno = pTask->InitIrecv(
                    pRecvBuf,
                    recvCount,
                    datatype,
                    dst
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
            }
            else
            {
                pTask->InitNoOp();
            }
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            ++pTask;

            if( sendCount > 0 )
            {
                mpi_errno = pTask->InitIsend(
                    pSendBuf,
                    sendCount,
                    datatype,
                    dst,
                    pComm
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    //
                    // We aren't going to initialize the reduce task, so
                    // fix up the number of tasks that will need to be
                    // cleaned up.
                    //
                    --pReq->nbc.nTasks;
                    return mpi_errno;
                }
            }
            else
            {
                pTask->InitNoOp();
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            if( recvCount > 0 )
            {
                mpi_errno = pTask->InitReduce(
                    pRecvBuf,
                    pReduceBuf,
                    recvCount,
                    datatype,
                    pOp,
                    hType,
                    rightOrder
                    );
                if(mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
            }
            else
            {
                pTask->InitNoOp();
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            ++pTask;

            sendIdx = recvIdx;
            lastIdx = recvIdx + offset;

            return MPI_SUCCESS;
        },
        lgpof2
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( recvcnts[rank] > 0 )
    {
        pTask->InitLocalCopy(
            tmpReduceBuf + disps[rank] * extent,
            recvbuf,
            recvcnts[rank],
            recvcnts[rank],
            datatype,
            datatype
            );
    }
    else
    {
        pTask->InitNoOp();
    }
    pTask->m_iNextOnInit = NBC_TASK_NONE;
    ++pReq->nbc.nTasks;

    if( rank < 2 * rem )
    {
        pTask->m_iNextOnComplete = pReq->nbc.nTasks;
        ++pTask;

        //
        // The current rank has to be an odd rank if it reaches here
        // becauser if it's an even rank, it would have already returned.
        // Send data to the even rank on its left (rank - 1).
        //
        MPIU_Assert( rank & 1 );

        if( recvcnts[rank - 1] > 0 )
        {
            mpi_errno = pTask->InitIsend(
                tmpReduceBuf + disps[rank - 1] * extent,
                recvcnts[rank - 1],
                datatype,
                rank - 1,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        ++pReq->nbc.nTasks;
    }
    pTask->m_iNextOnComplete = NBC_TASK_NONE;

    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IreduceScatterBuildCommutativeLongTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _When_(sendbuf != MPI_IN_PLACE && recvcnts[pComm->rank] > 0, _Out_opt_)
              void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      MPI_Aint         extent,
    _In_      MPI_Count        trueLb,
    _In_      const int*       disps
    )
{
    //
    // The recvbuf SAL should have satisfied OACR, unfortunately it doesn't.
    // Have no choice but to suppress C6101 for recvbuf here.
    //
    OACR_USE_PTR( recvbuf );

    //
    // Commutative reduce operation, long message size,
    // use pairwise exchange algorithm.
    //
    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    MPI_RESULT mpi_errno;

    BYTE* tmpRecvBuf;
    if( recvcnts[rank] > 0 )
    {
        //
        // Allocate scratch buffer to hold received data.
        //
        pReq->nbc.tmpBuf = new BYTE[extent * recvcnts[rank]];
        if( pReq->nbc.tmpBuf == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
        //
        // Adjust for potential negative lower bound in datatype.
        //
        tmpRecvBuf = pReq->nbc.tmpBuf - trueLb;
    }
    else
    {
        tmpRecvBuf = nullptr;
    }

    BYTE* pReduceBuf;
    if( sendbuf != MPI_IN_PLACE )
    {
        pReduceBuf = static_cast<BYTE*>( recvbuf );
    }
    else
    {
        pReduceBuf = static_cast<BYTE*>( recvbuf ) + disps[rank] * extent;
    }

    if( sendbuf != MPI_IN_PLACE && recvcnts[rank] > 0 )
    {
        mpi_errno = MPIR_Localcopy(
            static_cast<BYTE*>( const_cast<void*>( sendbuf ) ) + disps[rank] * extent,
            recvcnts[rank],
            datatype,
            recvbuf,
            recvcnts[rank],
            datatype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    //
    // Will do size - 1 iterations, performs send, recv and reduce per iteration.
    //
    ULONG taskCount = 3 * ( size - 1 );
    if( sendbuf == MPI_IN_PLACE && recvcnts[rank] > 0 && rank != 0 )
    {
        //
        // For non-zero rank, one local copy task needed for the MPI_IN_PLACE
        // case if the current rank expects scatter data.
        //
        taskCount++;
    }

    pReq->nbc.tasks = new NbcTask[taskCount];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    MPI_Datatype hType = datatype.GetMpiHandle();
    unsigned src = rank;
    unsigned dst = rank;
    bool rightOrder;
    bool inPlaceSendingToRankZero;
    const BYTE* pSendBuf;

    for( unsigned i = size - 1; i > 0; i-- )
    {
        src = pComm->RankSub( 1, src );
        dst = pComm->RankAdd( 1, dst );
        rightOrder = src < rank ? true : false;

        //
        // Send the data that dst needs, recv data that this rank needs from
        // src into tmpRecvBuf.
        //
        if( sendbuf != MPI_IN_PLACE )
        {
            pSendBuf = static_cast<const BYTE*>(sendbuf) + disps[dst] * extent;
        }
        else
        {
            pSendBuf = static_cast<BYTE*>( recvbuf ) + disps[dst] * extent;
        }

        if( recvcnts[rank] > 0 )
        {
            mpi_errno = pTask->InitIrecv(
                tmpRecvBuf,
                recvcnts[rank],
                datatype,
                src
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = ++pReq->nbc.nTasks;

        inPlaceSendingToRankZero = false;
        if( recvcnts[dst] > 0 )
        {
            //
            // In the MPI_IN_PLACE case, for non-zero rank, if the current
            // rank expects scatter data and the target is rank 0, make sure
            // this send blocks the task list execution, so that when it
            // reaches the last local copy step, the current rank has already
            // finished sending to rank 0 to avoid sending the reduction
            // result to rank 0.
            //
            if( sendbuf == MPI_IN_PLACE &&
                recvcnts[rank] > 0 &&
                dst == 0 )
            {
                //
                // Arrange the previous recv's m_iNextOnComplete.
                //
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
                inPlaceSendingToRankZero = true;
            }
            else
            {
                //
                // Arrange the previous recv's m_iNextOnComplete.
                //
                pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            }
            pTask++;

            mpi_errno = pTask->InitIsend(
                pSendBuf,
                recvcnts[dst],
                datatype,
                dst,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                if( !inPlaceSendingToRankZero )
                {
                    //
                    // We aren't going to initialize the reduce task, so
                    // fix up the number of tasks that will need to be
                    // cleaned up.
                    //
                    --pReq->nbc.nTasks;
                }
                return mpi_errno;
            }
        }
        else
        {
            //
            // Arrange the previous recv's m_iNextOnComplete.
            //
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            pTask++;

            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        if( inPlaceSendingToRankZero )
        {
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        }
        else
        {
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
        }
        pTask++;

        if( recvcnts[rank] > 0 )
        {
            mpi_errno = pTask->InitReduce(
                tmpRecvBuf,
                pReduceBuf,
                recvcnts[rank],
                datatype,
                pOp,
                hType,
                rightOrder
                );
            if(mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            pTask->InitNoOp();
        }
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
        ++pTask;
    }

    //
    // In the MPI_IN_PLACE case, for non-zero rank, move output data to the
    // beginning of the recvbuf if the current rank expects scatter data.
    //
    if( sendbuf == MPI_IN_PLACE && rank != 0 && recvcnts[rank] > 0 )
    {
        pTask->InitLocalCopy(
            pReduceBuf,
            recvbuf,
            recvcnts[rank],
            recvcnts[rank],
            datatype,
            datatype
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        pTask->m_iNextOnComplete = NBC_TASK_NONE;
        ++pReq->nbc.nTasks;
    }
    else
    {
        //
        // Fix the last task's m_iNextOnComplete.
        //
        pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;
    }

    return MPI_SUCCESS;
}


static
unsigned
IreduceScatterNonCommutativeTaskCount(
    _In_ MPID_Comm* pComm,
    _In_ int        recvCount
    )
{
    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    unsigned taskCount = 0;
    ULONG lgpof2;
    _BitScanReverse( &lgpof2, size );
    if( !IsPowerOf2( size ) )
    {
        lgpof2++;
    }

    unsigned mask = UINT_MAX;
    BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            unsigned peer = rank ^ offset;
            unsigned peerTreeRoot = peer & mask;
            unsigned myTreeRoot = rank & mask;
            bool received = false;

            if( peer < size )
            {
                //
                // One send and one recv task needed.
                //
                taskCount += 2;
                received = true;
            }

            if( peerTreeRoot + offset > size )
            {
                unsigned numRanksCompleted = size - myTreeRoot - offset;
                ULONG k;
                _BitScanReverse( &k, offset );
                int subMask = -1 << k;

                unsigned subOffset = offset >> 1;
                unsigned treeRoot;
                while( subOffset )
                {
                    peer = rank ^ subOffset;
                    treeRoot = rank & subMask;

                    if( peer > rank &&
                        rank < treeRoot + numRanksCompleted &&
                        peer >= treeRoot + numRanksCompleted )
                    {
                        //
                        // One send task needed.
                        //
                        taskCount++;
                    }
                    else if ( peer < rank &&
                        peer < treeRoot + numRanksCompleted &&
                        rank >= treeRoot + numRanksCompleted )
                    {
                        //
                        // One recv task needed.
                        //
                        taskCount++;
                        received = true;
                    }

                    subOffset >>= 1;
                    subMask >>= 1;
                }
            }

            if( received )
            {
                //
                // Two reduce tasks needed.
                //
                taskCount += 2;
            }

            mask <<= 1;

            return MPI_SUCCESS;
        },
        lgpof2
        );

    if( recvCount > 0 )
    {
        //
        // One last local copy task needed.
        //
        taskCount++;
    }

    return taskCount;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IreduceScatterBuildNonCommutativeTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _When_(recvcnts[pComm->rank] > 0, _Out_opt_)
              void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              totalCount,
    _In_      MPI_Aint         extent,
    _In_      MPI_Aint         fullExtent,
    _In_      MPI_Count        trueLb,
    _In_      const int*       disps
    )
{
    //
    // Non-commutative reduce operation, use recursive doubling algorithm with
    // internal logarithmic recursive-halfing algorithm.
    //

    //
    // Allocate scratch buffer to hold received data and intermediate reduce data.
    //
    pReq->nbc.tmpBuf = new BYTE[fullExtent * 2];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    //
    // Adjust for potential negative lower bound in datatype.
    //
    BYTE* tmpRecvBuf = pReq->nbc.tmpBuf - trueLb;
    BYTE* tmpReduceBuf = pReq->nbc.tmpBuf + fullExtent - trueLb;

    MPI_RESULT mpi_errno;
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            totalCount,
            datatype,
            tmpReduceBuf,
            totalCount,
            datatype
            );
    }
    else
    {
        mpi_errno = MPIR_Localcopy(
            recvbuf,
            totalCount,
            datatype,
            tmpReduceBuf,
            totalCount,
            datatype
            );
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    unsigned rank = pComm->rank;
    unsigned taskCount = IreduceScatterNonCommutativeTaskCount( pComm, recvcnts[rank] );
    pReq->nbc.tasks = new NbcTask[taskCount];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    NbcTask* pTask = &pReq->nbc.tasks[0];

    unsigned size = pComm->remote_size;
    ULONG lgpof2;
    _BitScanReverse( &lgpof2, size );
    if( !IsPowerOf2( size ) )
    {
        lgpof2++;
    }

    MPI_Datatype hType = datatype.GetMpiHandle();
    unsigned mask = UINT_MAX;
    BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            unsigned peer = rank ^ offset;
            unsigned peerTreeRoot = peer & mask;
            unsigned myTreeRoot = rank & mask;
            unsigned treeRootCap;
            int blklens[2];
            int dis[2];
            unsigned i;

            //
            // At step 1, processes exchange (n-n/p) amount of data;
            // at step 2, (n-2n/p) amount of data;
            // at step 3, (n-4n/p) amount of data, and so forth.
            // We use derived datatypes for this.
            //
            // At each step, a process does not need to send data
            // indexed from myTreeRoot to myTreeRoot + offset - 1.
            // Similarly, a process won't receive data indexed from
            // peerTreeRoot to peerTreeRoot + offset - 1.
            //

            //
            // Create sendType.
            //
            blklens[0] = 0;
            blklens[1] = 0;
            for( i = 0; i < myTreeRoot; i++ )
            {
                blklens[0] += recvcnts[i];
            }
            for( i = myTreeRoot + offset; i < size; i++ )
            {
                blklens[1] += recvcnts[i];
            }

            dis[0] = 0;
            dis[1] = blklens[0];
            treeRootCap = myTreeRoot + offset;
            if( treeRootCap > size )
            {
                treeRootCap = size;
            }
            for( i = myTreeRoot; i < treeRootCap; i++ )
            {
                dis[1] += recvcnts[i];
            }

            //
            // TODO:  Free if things fail
            //
            MPI_Datatype sendType;
            NMPI_Type_indexed(
                2,
                blklens,
                dis,
                hType,
                &sendType
                );
            NMPI_Type_commit( &sendType );
            TypeHandle sendTypeHandle = TypeHandle::Create( sendType );

            //
            // Create recvType.
            //
            blklens[0] = 0;
            blklens[1] = 0;
            treeRootCap = peerTreeRoot;
            if( treeRootCap > size )
            {
                treeRootCap = size;
            }
            for( i = 0; i < treeRootCap; i++ )
            {
                blklens[0] += recvcnts[i];
            }
            for( i = peerTreeRoot + offset; i < size; i++ )
            {
                blklens[1] += recvcnts[i];
            }

            dis[0] = 0;
            dis[1] = blklens[0];
            treeRootCap = peerTreeRoot + offset;
            if( treeRootCap > size )
            {
                treeRootCap = size;
            }
            for( i = peerTreeRoot; i < treeRootCap; i++ )
            {
                dis[1] += recvcnts[i];
            }

            //
            // TODO:  Free if things fail
            //
            MPI_Datatype recvType;
            NMPI_Type_indexed(
                2,
                blklens,
                dis,
                hType,
                &recvType
                );
            NMPI_Type_commit( &recvType );
            TypeHandle recvTypeHandle = TypeHandle::Create( recvType );

            bool received = false;
            if( peer < size )
            {
                //
                // tmpReduceBuf contains data to be sent in each step.
                // Data is received in tmpRecvBuf and then accumulated into
                // tmpReduceBuf. Accumulation is done later below.
                //
                mpi_errno = pTask->InitIrecv(
                    tmpRecvBuf,
                    1,
                    recvTypeHandle,
                    peer,
                    true
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
                pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
                pTask++;
                received = true;

                //
                // Need to finish send then start reduction because of
                // re-used buffer.
                //
                mpi_errno = pTask->InitIsend(
                    tmpReduceBuf,
                    1,
                    sendTypeHandle,
                    peer,
                    pComm,
                    true
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
                pTask++;
            }

            //
            // If some processes in this process's subtree in this step
            // did not have any destination process to communicate with
            // because of non-power-of-two, we need to send them the
            // result. We use a logarithmic recursive-halfing algorithm
            // for this.
            //
            if( peerTreeRoot + offset > size )
            {
                unsigned numRanksCompleted = size - myTreeRoot - offset;
                ULONG k;
                _BitScanReverse( &k, offset );
                int subMask = -1 << k;

                unsigned subOffset = offset >> 1;
                unsigned treeRoot;
                while( subOffset )
                {
                    peer = rank ^ subOffset;
                    treeRoot = rank & subMask;

                    if( peer > rank &&
                        rank < treeRoot + numRanksCompleted &&
                        peer >= treeRoot + numRanksCompleted )
                    {
                        //
                        // Send only if this process has data and the peer
                        // doesn't have data. At any step, multiple processes
                        // can send if they have the data.
                        //
                        mpi_errno = pTask->InitIsend(
                            tmpRecvBuf,
                            1,
                            recvTypeHandle,
                            peer,
                            pComm,
                            true
                            );
                        if( mpi_errno != MPI_SUCCESS )
                        {
                            return mpi_errno;
                        }
                        pTask->m_iNextOnInit = NBC_TASK_NONE;
                        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
                        pTask++;
                    }
                    else if ( peer < rank &&
                        peer < treeRoot + numRanksCompleted &&
                        rank >= treeRoot + numRanksCompleted )
                    {
                        //
                        // Recv only if this process doesn't have data and the
                        // peer has data.
                        //
                        mpi_errno = pTask->InitIrecv(
                            tmpRecvBuf,
                            1,
                            recvTypeHandle,
                            peer,
                            true
                            );
                        if( mpi_errno != MPI_SUCCESS )
                        {
                            return mpi_errno;
                        }
                        pTask->m_iNextOnInit = NBC_TASK_NONE;
                        pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
                        pTask++;
                        received = true;
                    }

                    subOffset >>= 1;
                    subMask >>= 1;
                }
            }

            if( received )
            {
                //
                // The following reductions are done here instead of after
                // recv task above. This is because to do it above, we would
                // need an extra temp buffer so as not to overwrite tmpRecvBuf,
                // because tmpRecvBuf may have to be communicated to other
                // processes in the non-power-of-two case. To avoid that extra
                // allocation, we do the reduce here.
                //
                bool rightOrder = ( peerTreeRoot < myTreeRoot );

                mpi_errno = pTask->InitReduce(
                    tmpRecvBuf,
                    tmpReduceBuf,
                    blklens[0],
                    datatype,
                    pOp,
                    hType,
                    rightOrder
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
                ++pTask;

                mpi_errno = pTask->InitReduce(
                    tmpRecvBuf + dis[1] * extent,
                    tmpReduceBuf + dis[1] * extent,
                    blklens[1],
                    datatype,
                    pOp,
                    hType,
                    rightOrder
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
                ++pTask;
            }

            mask <<= 1;
            return MPI_SUCCESS;
        },
        lgpof2
        );

    if( recvcnts[rank] > 0 )
    {
        //
        // Copy final results from tmpReduceBuf to recvbuf.
        //
        pTask->InitLocalCopy(
            tmpReduceBuf + disps[rank] * extent,
            recvbuf,
            recvcnts[rank],
            recvcnts[rank],
            datatype,
            datatype
            );
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        ++pReq->nbc.nTasks;
    }

    //
    // Fix the last task's m_iNextOnComplete.
    //
    pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IreduceScatterBuildTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _When_( totalCount > 0, _Out_opt_)
              void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              tag,
    _In_      int              totalCount,
    _In_      const int*       disps
    )
{
    pReq->nbc.tag = pComm->GetNextNBCTag( tag );

    MPI_RESULT mpi_errno;
    MPI_Count trueLb;
    MPI_Aint trueExtent = fixme_cast<MPI_Aint>( datatype.GetTrueExtentAndLowerBound( &trueLb ) );
    MPI_Aint extent = fixme_cast<MPI_Aint>( datatype.GetExtent() );
    if( extent < trueExtent )
    {
        extent = trueExtent;
    }
    MPI_Aint fullExtent = extent * totalCount;
    unsigned cbBuffer = fixme_cast<unsigned>( datatype.GetSize() * totalCount );

    if( pOp->IsCommutative() )
    {
        if( cbBuffer < pComm->SwitchPoints()->MPIR_redscat_commutative_long_msg )
        {
            mpi_errno = IreduceScatterBuildCommutativeShortTaskList(
                pReq,
                sendbuf,
                recvbuf,
                recvcnts,
                datatype,
                pOp,
                pComm,
                totalCount,
                extent,
                fullExtent,
                trueLb,
                disps
                );
        }
        else
        {
            mpi_errno = IreduceScatterBuildCommutativeLongTaskList(
                pReq,
                sendbuf,
                recvbuf,
                recvcnts,
                datatype,
                pOp,
                pComm,
                extent,
                trueLb,
                disps
                );
        }
    }
    else
    {
        mpi_errno = IreduceScatterBuildNonCommutativeTaskList(
            pReq,
            sendbuf,
            recvbuf,
            recvcnts,
            datatype,
            pOp,
            pComm,
            totalCount,
            extent,
            fullExtent,
            trueLb,
            disps
            );
    }

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IreduceScatterBuildIntra(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              tag,
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
    StackGuardArray<int> disps = new int[size];
    if( disps == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    for( int i = 0; i < size; i++ )
    {
        disps[i] = totalCount;
        totalCount += recvcnts[i];
    }

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
                totalCount,
                datatype,
                recvbuf,
                totalCount,
                datatype
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
        mpi_errno = IreduceScatterBuildTaskList(
            pRequest.get(),
            sendbuf,
            recvbuf,
            recvcnts,
            datatype,
            pOp,
            pComm,
            tag,
            totalCount,
            disps.get()
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
static
MPI_RESULT
MPIR_Ireduce_scatter_intra_impl(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              tag,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IreduceScatterBuildIntra(
        sendbuf,
        recvbuf,
        recvcnts,
        datatype,
        pOp,
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
MPI_RESULT
MPIR_Ireduce_scatter_intra(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    return MPIR_Ireduce_scatter_intra_impl(
        sendbuf,
        recvbuf,
        recvcnts,
        datatype,
        pOp,
        pComm,
        MPIR_GET_ASYNC_TAG( MPIR_REDUCE_SCATTER_TAG ),
        ppRequest );
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
static
MPI_RESULT
IreduceScatterBuildInter(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              tag,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // First do an intercommunicator reduce to rank 0 on left group,
    // then an intercommunicator reduce to rank 0 on right group,
    // followed by local intracommunicator scattervs in each group.
    //
    MPIU_Assert( pComm->comm_kind == MPID_INTERCOMM );
    MPIU_Assert( pComm->inter.local_comm != nullptr );

    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned newTag = pComm->GetNextNBCTag( tag );

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pComm->AddRef();
    pRequest->nbc.tag = newTag;
    pRequest->nbc.tasks = new NbcTask[3];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    StackGuardArray<int> disps;
    int rank = pComm->rank;
    int localSize = pComm->inter.local_size;
    int i;
    int totalCount = 0;
    BYTE* tmpBuf = nullptr;

    if( rank == 0 )
    {
        //
        // In each group, rank 0 allocates a temp buffer for the
        // intercommunicator reduce.
        //
        disps = new int[localSize];
        if( disps == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        for( i = 0; i < localSize; i++ )
        {
            disps[i] = totalCount;
            totalCount += recvcnts[i];
        }

        MPI_Count trueLb;
        MPI_Aint trueExtent = fixme_cast<MPI_Aint>( datatype.GetTrueExtentAndLowerBound( &trueLb ) );
        MPI_Aint extent = fixme_cast<MPI_Aint>( datatype.GetExtent() );
        if( extent >= trueExtent )
        {
            extent *= totalCount;
        }
        else
        {
            extent = trueExtent * totalCount;
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
    else
    {
        for( i = localSize - 1; i >= 0; i-- )
        {
            totalCount += recvcnts[i];
        }
    }

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

    MPI_RESULT mpi_errno = pRequest->nbc.tasks[0].InitIreduceInter(
        sendbuf,
        tmpBuf,
        totalCount,
        datatype,
        pOp,
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

    mpi_errno = pRequest->nbc.tasks[1].InitIreduceInter(
        sendbuf,
        tmpBuf,
        totalCount,
        datatype,
        pOp,
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

    mpi_errno = pRequest->nbc.tasks[2].InitIscattervBoth(
        tmpBuf,
        recvcnts,
        disps,
        datatype,
        recvbuf,
        recvcnts[rank],
        datatype,
        0,
        pComm->inter.local_comm
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
static
MPI_RESULT
MPIR_Ireduce_scatter_inter_impl(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              tag,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IreduceScatterBuildInter(
        sendbuf,
        recvbuf,
        recvcnts,
        datatype,
        pOp,
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
MPI_RESULT
MPIR_Ireduce_scatter_inter(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    return MPIR_Ireduce_scatter_inter_impl(
        sendbuf,
        recvbuf,
        recvcnts,
        datatype,
        pOp,
        pComm,
        MPIR_GET_ASYNC_TAG( MPIR_REDUCE_SCATTER_TAG ),
        ppRequest );
}


MPI_RESULT
MPIR_Ireduce_scatter_block(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      MPID_Comm*       comm_ptr,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // A reduce_scatter_block is the same as a reduce_scatter where
    // all the scattered blocks are of the same size so take advantage
    // of that to avoid duplicating code
    //
    int size = comm_ptr->remote_size;
    StackGuardArray<int> recvcnts = new int[size];
    if (recvcnts == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    for (int i = 0; i < size; i++)
    {
        recvcnts[i] = recvcount;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        return MPIR_Ireduce_scatter_intra_impl(
            sendbuf,
            recvbuf,
            recvcnts,
            datatype,
            const_cast<MPID_Op*>(pOp),
            comm_ptr,
            MPIR_GET_ASYNC_TAG( MPIR_REDUCE_SCATTER_BLOCK_TAG ),
            ppRequest
            );
    }

    return MPIR_Ireduce_scatter_inter_impl(
        sendbuf,
        recvbuf,
        recvcnts,
        datatype,
        const_cast<MPID_Op*>(pOp),
        comm_ptr,
        MPIR_GET_ASYNC_TAG( MPIR_REDUCE_SCATTER_BLOCK_TAG ),
        ppRequest
        );
}


/* This is the default implementation of allreduce. The algorithm is:

   Algorithm: MPI_Allreduce

   For the heterogeneous case, we call MPI_Reduce followed by MPI_Bcast
   in order to meet the requirement that all processes must have the
   same result. For the homogeneous case, we use the following algorithms.


   For long messages and for builtin ops and if count >= pof2 (where
   pof2 is the nearest power-of-two less than or equal to the number
   of processes), we use Rabenseifner's algorithm (see
   http://www.hlrs.de/organization/par/services/models/mpi/myreduce.html ).
   This algorithm implements the allreduce in two steps: first a
   reduce-scatter, followed by an allgather. A recursive-halving
   algorithm (beginning with processes that are distance 1 apart) is
   used for the reduce-scatter, and a recursive doubling
   algorithm is used for the allgather. The non-power-of-two case is
   handled by dropping to the nearest lower power-of-two: the first
   few even-numbered processes send their data to their right neighbors
   (rank+1), and the reduce-scatter and allgather happen among the remaining
   power-of-two processes. At the end, the first few even-numbered
   processes get the result from their right neighbors.

   For the power-of-two case, the cost for the reduce-scatter is
   lgp.alpha + n.((p-1)/p).beta + n.((p-1)/p).gamma. The cost for the
   allgather lgp.alpha + n.((p-1)/p).beta. Therefore, the
   total cost is:
   Cost = 2.lgp.alpha + 2.n.((p-1)/p).beta + n.((p-1)/p).gamma

   For the non-power-of-two case,
   Cost = (2.floor(lgp)+2).alpha + (2.((p-1)/p) + 2).n.beta + n.(1+(p-1)/p).gamma


   For short messages, for user-defined ops, and for count < pof2
   we use a recursive doubling algorithm (similar to the one in
   MPI_Allgather). We use this algorithm in the case of user-defined ops
   because in this case derived datatypes are allowed, and the user
   could pass basic datatypes on one process and derived on another as
   long as the type maps are the same. Breaking up derived datatypes
   to do the reduce-scatter is tricky.

   Cost = lgp.alpha + n.lgp.beta + n.lgp.gamma

   Possible improvements:

   End Algorithm: MPI_Allreduce
*/
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allreduce_intra_flat(
    _In_opt_  const BYTE*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              BYTE*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
    int        comm_size, rank, type_size;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int mask, dst, pof2, newrank, rem, newdst, idx_shift,
        send_idx, recv_idx, last_idx, send_cnt, recv_cnt;
    MPI_Aint true_extent, extent;
    MPI_Count true_lb;
    StackGuardArray<BYTE> auto_buf;
    BYTE* tmp_buf;

    if (count == 0)
    {
        return MPI_SUCCESS;
    }

    /* set op_errno to 0. stored in perthread structure */
    Mpi.CallState->op_errno = 0;

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    /* need to allocate temporary buffer to store incoming data*/
    true_extent = fixme_cast<MPI_Aint>( datatype.GetTrueExtentAndLowerBound( &true_lb ) );
    extent = fixme_cast<MPI_Aint>( datatype.GetExtent() );

    auto_buf = new BYTE[count*(max(extent,true_extent))];
    if( auto_buf == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    /* adjust for potential negative lower bound in datatype */
    tmp_buf = auto_buf - true_lb;

    /* copy local data into recvbuf */
    if (sendbuf != MPI_IN_PLACE)
    {
        mpi_errno = MPIR_Localcopy(sendbuf, count, datatype, recvbuf,
                                    count, datatype);
        ON_ERROR_FAIL(mpi_errno);
    }

    type_size = fixme_cast<int>( datatype.GetSize() );

    /* find nearest power-of-two less than or equal to comm_size */
    pof2 = PowerOf2Floor( comm_size );

    rem = comm_size - pof2;

    /* In the non-power-of-two case, all even-numbered
        processes of rank < 2*rem send their data to
        (rank+1). These even-numbered processes no longer
        participate in the algorithm until the very end. The
        remaining processes form a nice power-of-two. */

    MPI_Datatype hType = datatype.GetMpiHandle();
    if (rank < 2*rem)
    {
        if ((rank & 1) == 0)
        { /* even */
            mpi_errno = MPIC_Send(recvbuf, count,
                                    datatype, rank+1,
                                    MPIR_ALLREDUCE_TAG, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);

            /* temporarily set the rank to -1 so that this
                process does not participate in recursive
                doubling */
            newrank = -1;
        }
        else
        { /* odd */
            mpi_errno = MPIC_Recv(tmp_buf, count,
                                    datatype, rank-1,
                                    MPIR_ALLREDUCE_TAG, comm_ptr,
                                    MPI_STATUS_IGNORE);
            ON_ERROR_FAIL(mpi_errno);

            /* do the reduction on received data. since the
                ordering is right, it doesn't matter whether
                the operation is commutative or not. */

            MPID_Uop_call(pOp, tmp_buf, recvbuf, &count, &hType);

            /* change the rank */
            newrank = rank / 2;
        }
    }
    else
    {
        /* rank >= 2*rem */
        newrank = rank - rem;
    }

    /* If op is user-defined or count is less than pof2, use
        recursive doubling algorithm. Otherwise do a reduce-scatter
        followed by allgather. (If op is user-defined,
        derived datatypes are allowed and the user could pass basic
        datatypes on one process and derived on another as long as
        the type maps are the same. Breaking up derived
        datatypes to do the reduce-scatter is tricky, therefore
        using recursive doubling in that case.) */

    if (newrank >= 0)
    {
        if ((static_cast<unsigned int>(count*type_size) <=
                comm_ptr->SwitchPoints()->MPIR_allreduce_short_msg) ||
            (pOp->IsBuiltin() == false) ||
            (count < pof2))
        {
            /* use recursive doubling */
            mask = 0x1;
            while (mask < pof2)
            {
                newdst = newrank ^ mask;
                /* find real rank of dest */
                dst = (newdst < rem) ? newdst*2 + 1 : newdst + rem;

                /* Send the most current data, which is in recvbuf. Recv
                    into tmp_buf */
                mpi_errno = MPIC_Sendrecv(recvbuf, count, datatype,
                                            dst, MPIR_ALLREDUCE_TAG, tmp_buf,
                                            count, datatype, dst,
                                            MPIR_ALLREDUCE_TAG, comm_ptr,
                                            MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);

                /* tmp_buf contains data received in this step.
                    recvbuf contains data accumulated so far */

                if( pOp->IsCommutative() == true || (dst < rank) )
                {
                    /* op is commutative OR the order is already right */
                    MPID_Uop_call(pOp, tmp_buf, recvbuf, &count, &hType);
                }
                else
                {
                    /* op is noncommutative and the order is not right */
                    MPID_Uop_call(pOp, recvbuf, tmp_buf, &count, &hType);

                    /* copy result back into recvbuf */
                    mpi_errno = MPIR_Localcopy(tmp_buf, count, datatype,
                                                recvbuf, count, datatype);
                    ON_ERROR_FAIL(mpi_errno);
                }
                mask <<= 1;
            }
        }
        else
        {
            /* do a reduce-scatter followed by allgather */

            /* for the reduce-scatter, calculate the count that
                each process receives and the displacement within
                the buffer */

            int reduceSize = count / pof2;
            int endSize = count % pof2;

            send_idx = recv_idx = 0;
            last_idx = pof2;
            idx_shift = pof2 >> 1;
            mask = 0x1;
            while (mask < pof2)
            {
                newdst = newrank ^ mask;
                /* find real rank of dest */
                dst = (newdst < rem) ? newdst * 2 + 1 : newdst + rem;

                if (newrank < newdst)
                {
                    send_idx = recv_idx + idx_shift;
                    recv_cnt = (send_idx - recv_idx) * reduceSize;
                    send_cnt = (last_idx - send_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        send_cnt += endSize;
                    }
                }
                else
                {
                    recv_idx = send_idx + idx_shift;
                    send_cnt = (recv_idx - send_idx) * reduceSize;
                    recv_cnt = (last_idx - recv_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        recv_cnt += endSize;
                    }
                }

                /*                    printf("Rank %d, send_idx %d, recv_idx %d, send_cnt %d, recv_cnt %d, last_idx %d\n", newrank, send_idx, recv_idx,
                                        send_cnt, recv_cnt, last_idx);
                                        */
                /* Send data from recvbuf. Recv into tmp_buf */
                mpi_errno = MPIC_Sendrecv(
                    reinterpret_cast<BYTE *>(recvbuf) + reduceSize * send_idx * extent,
                    send_cnt, datatype,
                    dst, MPIR_ALLREDUCE_TAG,
                    reinterpret_cast<BYTE *>(tmp_buf) + reduceSize * recv_idx * extent,
                    recv_cnt, datatype, dst,
                    MPIR_ALLREDUCE_TAG, comm_ptr,
                    MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);

                /* tmp_buf contains data received in this step.
                    recvbuf contains data accumulated so far */

                /* This algorithm is used only for predefined ops
                    and predefined ops are always commutative. */

                MPID_Uop_call(
                    pOp,
                    reinterpret_cast<BYTE *>(tmp_buf) + reduceSize * recv_idx * extent,
                    reinterpret_cast<BYTE *>(recvbuf) + reduceSize * recv_idx * extent,
                    &recv_cnt,
                    &hType
                    );

                /* update send_idx for next iteration */
                send_idx = recv_idx;
                mask <<= 1;

                /* update last_idx, but not in last iteration
                    because the value is needed in the allgather
                    step below. */
                if (mask < pof2)
                {
                    last_idx = recv_idx + idx_shift;
                    idx_shift >>= 1;
                }
            }

            /* now do the allgather */

            mask >>= 1;
            while (mask > 0)
            {
                newdst = newrank ^ mask;
                /* find real rank of dest */
                dst = (newdst < rem) ? newdst * 2 + 1 : newdst + rem;

                if (newrank < newdst)
                {
                    /* update last_idx except on first iteration */
                    if (mask != pof2 >> 1)
                    {
                        last_idx = last_idx + idx_shift;
                    }

                    recv_idx = send_idx + idx_shift;
                    send_cnt = (recv_idx - send_idx) * reduceSize;
                    recv_cnt = (last_idx - recv_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        recv_cnt += endSize;
                    }
                }
                else
                {
                    recv_idx = send_idx - idx_shift;
                    recv_cnt = (send_idx - recv_idx) * reduceSize;
                    send_cnt = (last_idx - send_idx) * reduceSize;
                    if (last_idx == pof2)
                    {
                        send_cnt += endSize;
                    }
                }

                mpi_errno = MPIC_Sendrecv(
                    reinterpret_cast<char *>(recvbuf) + reduceSize * send_idx * extent,
                    send_cnt, datatype,
                    dst, MPIR_ALLREDUCE_TAG,
                    reinterpret_cast<char *>(recvbuf) + reduceSize * recv_idx * extent,
                    recv_cnt, datatype, dst,
                    MPIR_ALLREDUCE_TAG, comm_ptr,
                    MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);

                if (newrank > newdst)
                {
                    send_idx = recv_idx;
                }

                mask >>= 1;
                idx_shift <<= 1;
            }
        }
    }

    /* In the non-power-of-two case, all odd-numbered
        processes of rank < 2*rem send the result to
        (rank-1), the ranks who didn't participate above. */
    if (rank < 2*rem)
    {
        if ((rank & 1) != 0)  /* odd */
        {
            //
            // OACR thinks rank could be zero here, leading the subtraction
            // below to go negative.
            //
            _Analysis_assume_( rank >= 1 );
            mpi_errno = MPIC_Send(recvbuf, count,
                                    datatype, rank-1,
                                    MPIR_ALLREDUCE_TAG, comm_ptr);
        }
        else  /* even */
        {
            mpi_errno = MPIC_Recv(recvbuf, count,
                                    datatype, rank+1,
                                    MPIR_ALLREDUCE_TAG, comm_ptr,
                                    MPI_STATUS_IGNORE);
        }
        ON_ERROR_FAIL(mpi_errno);
    }

    if (Mpi.CallState->op_errno)
    {
        mpi_errno = Mpi.CallState->op_errno;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Allreduce_inter(
    _In_opt_  const BYTE*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              BYTE*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
/* Intercommunicator Allreduce.
   We first do an intercommunicator reduce to rank 0 on left group,
   then an intercommunicator reduce to rank 0 on right group, followed
   by local intracommunicator broadcasts in each group.

   We don't do local reduces first and then intercommunicator
   broadcasts because it would require allocation of a temporary buffer.
*/
    MPI_RESULT mpi_errno;
    int rank, root;

    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    rank = comm_ptr->rank;

    /* first do a reduce from right group to rank 0 in left group,
       then from left group to rank 0 in right group*/
    if (comm_ptr->inter.is_low_group)
    {
        /* reduce from right group to rank 0*/
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Reduce_inter(sendbuf, recvbuf, count, datatype, pOp,
                                      root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* reduce to rank 0 of right group */
        root = 0;
        mpi_errno = MPIR_Reduce_inter(sendbuf, recvbuf, count, datatype, pOp,
                                      root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* reduce to rank 0 of left group */
        root = 0;
        mpi_errno = MPIR_Reduce_inter(sendbuf, recvbuf, count, datatype, pOp,
                                      root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* reduce from right group to rank 0 */
        root = (rank == 0) ? MPI_ROOT : MPI_PROC_NULL;
        mpi_errno = MPIR_Reduce_inter(sendbuf, recvbuf, count, datatype, pOp,
                                      root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }

    mpi_errno = MPIR_Bcast_intra(recvbuf, count, datatype, 0, comm_ptr->inter.local_comm);
    ON_ERROR_FAIL(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static MPI_RESULT
MPIR_Allreduce_intra_HA(
    _In_opt_  const BYTE*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              BYTE*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
    if(count == 0)
    {
        return MPI_SUCCESS;
    }

    MPI_RESULT mpi_errno;
    /* on each node, do a reduce to the local root */
    if(comm_ptr->intra.local_subcomm != nullptr)
    {
        /* take care of the MPI_IN_PLACE case. For reduce,
            MPI_IN_PLACE is specified only on the root;
            for allreduce it is specified on all processes. */
        if ((sendbuf == MPI_IN_PLACE) && (comm_ptr->intra.local_subcomm->rank != 0))
        {
            /* IN_PLACE and not root of reduce. Data supplied to this
            allreduce is in recvbuf. Pass that as the sendbuf to reduce. */
            mpi_errno = MPIR_Reduce_intra(
                recvbuf,
                nullptr,
                count,
                datatype,
                pOp,
                0,
                comm_ptr->intra.local_subcomm
                );
        }
        else
        {
            mpi_errno = MPIR_Reduce_intra(
                sendbuf,
                recvbuf,
                count,
                datatype,
                pOp,
                0,
                comm_ptr->intra.local_subcomm
                );
        }

        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    else
    {
        MPIU_Assert( comm_ptr->intra.leaders_subcomm != nullptr );

        /* only one process on the node. copy sendbuf to recvbuf */
        if(sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MPIR_Localcopy(
                sendbuf,
                count,
                datatype,
                recvbuf,
                count,
                datatype
                );

            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
    }

    /* now do an IN_PLACE allreduce among the local roots of all nodes */
    if (comm_ptr->intra.leaders_subcomm != nullptr)
    {
        mpi_errno = MPIR_Allreduce_intra_flat(
            reinterpret_cast<const BYTE*>(MPI_IN_PLACE),
            recvbuf,
            count,
            datatype,
            pOp,
            comm_ptr->intra.leaders_subcomm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    /* now broadcast the result among local processes */
    if (comm_ptr->intra.local_subcomm != nullptr)
    {
        mpi_errno = MPIR_Bcast_intra(
            recvbuf,
            count,
            datatype,
            0,
            comm_ptr->intra.local_subcomm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    return MPI_SUCCESS;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allreduce_intra(
    _In_opt_  const BYTE*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              BYTE*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    )
{
    MPIU_Assert(comm_ptr->comm_kind != MPID_INTERCOMM);

    bool is_contig;
    MPI_Aint true_lb;
    MPIDI_msg_sz_t nbytes = fixme_cast<MPIDI_msg_sz_t>(
        datatype.GetSizeAndInfo( count, &is_contig, &true_lb )
        );

    if (Mpi.SwitchoverSettings.SmpAllreduceEnabled &&
        comm_ptr->IsNodeAware() &&
        nbytes >= comm_ptr->SwitchPoints()->MPIR_allreduce_smp_threshold &&
        nbytes < comm_ptr->SwitchPoints()->MPIR_allreduce_smp_ceiling &&
        pOp->IsCommutative() == true )
    {
        return MPIR_Allreduce_intra_HA(
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
            comm_ptr
            );
    }

    return MPIR_Allreduce_intra_flat(
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
            comm_ptr
            );
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
IallreduceBuildReduceScatterAllGatherTaskList(
    _In_      BYTE*           tmpBuf,
    _Out_opt_ void*           recvbuf,
    _In_range_( >, 0 )
              int             count,
    _In_      TypeHandle      datatype,
    _In_      MPI_Datatype    hType,
    _In_      MPI_Aint        extent,
    _In_      MPID_Op*        pOp,
    _In_      MPID_Comm*      pComm,
    _In_range_( pComm->remote_size / 2, pComm->remote_size )
              unsigned        pof2,
    _In_range_( 1, pof2 / 2 )            // Is there a way to annotate it properly?
              ULONG           lgpof2,
    _In_range_( 0, pComm->remote_size / 2 )
              unsigned        rem,
    _In_range_( 0, pof2 - 1 )
              unsigned        newRank,
    _Inout_   NbcTask**       ppCurTask,
    _Inout_   ULONG*          pNumTasks,
    _Inout_   ULONG**         ppiNext
)
{
    int reduceCount = count / pof2;
    int endCount = count % pof2;
    unsigned sendIdx = 0;
    unsigned recvIdx = 0;
    unsigned lastIdx = pof2;
    unsigned idxShift = pof2 >> 1;
    NbcTask* pTask = *ppCurTask;
    ULONG nTasks = *pNumTasks;
    ULONG* piNext = *ppiNext;

    //
    // Note that every node exchanges data with its peers, where the number of
    // peers is determined by the number of nodes in the tree (pof2).  This is
    // very much like a reduce-scatter.
    //
    MPI_RESULT mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = nTasks;

            //
            // offset is the distance to our peer.  Figure out the actual
            // rank of our peer.
            //
            unsigned newScatterPeer = newRank ^ offset;
            unsigned scatterPeer = TrimmedToOriginalRankOdd( rem, newScatterPeer );
            bool rightOrder = scatterPeer < fixme_cast<unsigned>( pComm->rank ) ? true : false;

            int sendCount;
            int recvCount;

            if( newRank < newScatterPeer )
            {
                sendIdx = recvIdx + idxShift;
                recvCount = idxShift * reduceCount;
                sendCount = ( lastIdx - sendIdx ) * reduceCount;
                if( lastIdx == pof2 )
                {
                    sendCount += endCount;
                }
            }
            else
            {
                recvIdx = sendIdx + idxShift;
                sendCount = idxShift * reduceCount;
                recvCount = ( lastIdx - recvIdx ) * reduceCount;
                if( lastIdx == pof2 )
                {
                    recvCount += endCount;
                }
            }

            MPI_RESULT mpi_errno = pTask->InitIrecv(
                tmpBuf,
                recvCount,
                datatype,
                scatterPeer
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++nTasks;
            pTask->m_iNextOnComplete = ++nTasks;
            ++pTask;

            const BYTE* pSendBuf = static_cast<BYTE*>( recvbuf ) + ( extent * sendIdx * reduceCount );
            mpi_errno = pTask->InitIsend(
                pSendBuf,
                sendCount,
                datatype,
                scatterPeer,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                //
                // We aren't going to initialize the reduce task, so
                // fixup the number of tasks that will need to be cleaned up.
                //
                --nTasks;
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            BYTE* pReduceBuf = static_cast<BYTE*>( recvbuf ) + ( extent * recvIdx * reduceCount );
            mpi_errno = pTask->InitReduce(
                tmpBuf,
                pReduceBuf,
                recvCount,
                datatype,
                pOp,
                hType,
                rightOrder
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnComplete;
                ++nTasks;
                ++pTask;
            }

            sendIdx = recvIdx;

            //
            // Update lastIdx and idxShift except for the last iteration
            // because the value is needed in the allgather step below.
            //
            if( ( offset << 1 ) < pof2 )
            {
                lastIdx = recvIdx + idxShift;
                idxShift >>= 1;
            }

            return mpi_errno;
        },
        lgpof2
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        *ppCurTask = pTask;
        *pNumTasks = nTasks;
        *ppiNext = piNext;
        return mpi_errno;
    }

    //
    // Now do the allgather.
    //
    mpi_errno = BinomialChildBuilderDescending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = nTasks;

            //
            // offset is the distance to our peer.  Figure out the actual
            // rank of our peer.
            //
            unsigned newAllgatherPeer = newRank ^ offset;
            unsigned allgatherPeer = TrimmedToOriginalRankOdd( rem, newAllgatherPeer );

            int sendCount;
            int recvCount;

            if( newRank < newAllgatherPeer )
            {
                //
                // Update lastIdx except on first iteration
                //
                if( offset != pof2 >> 1 )
                {
                    lastIdx = lastIdx + idxShift;
                }

                recvIdx = sendIdx + idxShift;
                sendCount = idxShift * reduceCount;
                recvCount = ( lastIdx - recvIdx ) * reduceCount;
                if( lastIdx == pof2 )
                {
                    recvCount += endCount;
                }
            }
            else
            {
                recvIdx = sendIdx - idxShift;
                recvCount = idxShift * reduceCount;
                sendCount = ( lastIdx - sendIdx ) * reduceCount;
                if( lastIdx == pof2 )
                {
                    sendCount += endCount;
                }
            }

            const BYTE* pSendBuf = static_cast<BYTE*>( recvbuf ) + ( extent * sendIdx * reduceCount );
            BYTE* pRecvBuf = static_cast<BYTE*>( recvbuf ) + ( extent * recvIdx * reduceCount );

            mpi_errno = pTask->InitIsend(
                pSendBuf,
                sendCount,
                datatype,
                allgatherPeer,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            MPI_RESULT mpi_errno = pTask->InitIrecv(
                pRecvBuf,
                recvCount,
                datatype,
                allgatherPeer
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            piNext = &pTask->m_iNextOnComplete;
            ++nTasks;
            ++pTask;

            if( newRank > newAllgatherPeer )
            {
                sendIdx = recvIdx;
            }

            idxShift <<= 1;

            return MPI_SUCCESS;
        },
        lgpof2
        );

    *ppCurTask = pTask;
    *pNumTasks = nTasks;
    *ppiNext = piNext;
    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
IallreduceBuildRecursiveDoublingTaskList(
    _In_      BYTE*           tmpBuf,
    _Out_opt_ void*           recvbuf,
    _In_range_( >, 0 )
              int             count,
    _In_      TypeHandle      datatype,
    _In_      MPI_Datatype    hType,
    _In_      MPID_Op*        pOp,
    _In_      MPID_Comm*      pComm,
    _In_range_( 1, ( pComm->remote_size - rem ) / 2 ) // Is there a way to annotate it properly?
              ULONG           lgpof2,
    _In_range_( 0, pComm->remote_size / 2 )
              unsigned        rem,
    _In_range_( 0, pComm->remote_size - rem - 1 )
              unsigned        newRank,
    _Inout_   NbcTask**       ppCurTask,
    _Inout_   ULONG*          pNumTasks,
    _Inout_   ULONG**         ppiNext
)
{
    NbcTask* pTask = *ppCurTask;
    ULONG nTasks = *pNumTasks;
    ULONG* piNext = *ppiNext;

    MPI_RESULT mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = nTasks;

            //
            // offset is the distance to our peer.  Figure out the actual
            // rank of our peer.
            //
            unsigned newPeer = newRank ^ offset;
            unsigned peer = TrimmedToOriginalRankOdd( rem, newPeer );
            bool rightOrder = peer < fixme_cast<unsigned>( pComm->rank ) ? true : false;

            MPI_RESULT mpi_errno = pTask->InitIrecv(
                tmpBuf,
                count,
                datatype,
                peer
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++nTasks;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            mpi_errno = pTask->InitIsend(
                recvbuf,
                count,
                datatype,
                peer,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++nTasks;
            ++pTask;

            mpi_errno = pTask->InitReduce(
                tmpBuf,
                recvbuf,
                count,
                datatype,
                pOp,
                hType,
                rightOrder
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnComplete;
                ++nTasks;
                ++pTask;
            }

            return mpi_errno;
        },
        lgpof2
        );

    *ppCurTask = pTask;
    *pNumTasks = nTasks;
    *ppiNext = piNext;
    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallreduceBuildTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_range_( >, 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm
    )
{
    MPI_Count true_lb;
    MPI_Aint true_extent = fixme_cast<MPI_Aint>( datatype.GetTrueExtentAndLowerBound( &true_lb ) );
    MPI_Aint extent = fixme_cast<MPI_Aint>( datatype.GetExtent() );
    if( extent < true_extent )
    {
        extent = true_extent;
    }
    MPI_Aint fullExtent = extent * count;

    pReq->nbc.tag = pReq->comm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_ALLREDUCE_TAG ) );

    //
    // Allocate a scratch buffer for receiving data from our peers so that we
    // can perform the reduction operation.
    //
    pReq->nbc.tmpBuf = new BYTE[fullExtent];
    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_RESULT mpi_errno;
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy( sendbuf, count, datatype, recvbuf, count, datatype );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    BYTE* tmpBuf = pReq->nbc.tmpBuf - true_lb;

    unsigned size = pComm->remote_size;
    unsigned rank = pComm->rank;
    unsigned pof2 = PowerOf2Floor( size );
    unsigned rem = size - pof2;
    int newRank;
    ULONG nTasks = 0;
    ULONG lgpof2;
    _BitScanForward( &lgpof2, pof2 );

    if( rank < 2 * rem )
    {
        //
        // In the non-pof2 case, all even-numbered processes of rank < 2 * rem
        // send their data to rank + 1, then they no longer participate in the
        // algorithm until the very end where they receive the final reduced
        // buffer from rank + 1 so 2 tasks in total. All odd-numbered processes
        // of rank < 2 * rem receive data from rank - 1 then reduce, and at the
        // very end, they send the final reduced buffer to rank - 1 so 3 tasks
        // in total.
        //
        if( ( rank & 1 ) != 0 )
        {
            nTasks += 3;
            newRank = rank >> 1;
        }
        else
        {
            nTasks += 2;
            newRank = -1;
        }
    }
    else
    {
        newRank = rank - rem;
    }

    //
    // If op is user-defined or count is less than pof2, use recursive doubling
    // algorithm which requires lgpof2 * 3 (send x 1 + recv x 1 + reduce x 1)
    // tasks. Otherwise do a reduce-scatter followed by allgather which requires
    // lgpof2 * 5 (send x 2 + recv x 2 + reduce x 1) tasks. (If op is
    // user-defined, derived datatypes are allowed and the user could pass basic
    // datatypes on one process and derived on another as long as the type maps
    // are the same. Breaking up derived datatypes to do the reduce-scatter is
    // tricky, therefore using recursive doubling in that case.)
    //
    if( newRank >= 0 )
    {
        if( fixme_cast<unsigned>( fullExtent ) > pComm->SwitchPoints()->MPIR_allreduce_short_msg &&
            pOp->IsBuiltin() == true &&
            fixme_cast<unsigned>( count ) >= pof2 )
        {
            //
            // For the reduce-scatter followed by allgather case, we need
            // lgpof2 * 5 tasks.
            //
            nTasks += lgpof2 * 5;
        }
        else
        {
            nTasks += lgpof2 * 3;
        }
    }

    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Handle the non-pof2 case first - all even-numbered processes of
    // rank < 2 * rem send their data to rank + 1.
    //
    NbcTask* pTask = &pReq->nbc.tasks[pReq->nbc.nTasks];
    MPI_Datatype hType = datatype.GetMpiHandle();
    ULONG* piNext = &pTask->m_iNextOnComplete;
    if( rank < 2 * rem )
    {
        if( newRank >= 0 )
        {
            //
            // Odd ranks receive from rank - 1.
            //
            mpi_errno = pTask->InitIrecv(
                tmpBuf,
                count,
                datatype,
                rank - 1
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            *piNext = ++pReq->nbc.nTasks;
            ++pTask;

            mpi_errno = pTask->InitReduce(
                tmpBuf,
                recvbuf,
                count,
                datatype,
                pOp,
                hType,
                true
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            //
            // Even ranks send to rank + 1.
            //
            mpi_errno = pTask->InitIsend(
                recvbuf,
                count,
                datatype,
                rank + 1,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }

        pTask->m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pTask->m_iNextOnComplete;
        ++pReq->nbc.nTasks;
        ++pTask;
    }

    if( newRank >= 0 )
    {
        if( fixme_cast<unsigned>( fullExtent ) > pComm->SwitchPoints()->MPIR_allreduce_short_msg &&
            pOp->IsBuiltin() == true &&
            fixme_cast<unsigned>( count ) >= pof2 )
        {
            mpi_errno = IallreduceBuildReduceScatterAllGatherTaskList(
                tmpBuf,
                recvbuf,
                count,
                datatype,
                hType,
                extent,
                pOp,
                pComm,
                pof2,
                lgpof2,
                rem,
                newRank,
                &pTask,
                &pReq->nbc.nTasks,
                &piNext
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            mpi_errno = IallreduceBuildRecursiveDoublingTaskList(
                tmpBuf,
                recvbuf,
                count,
                datatype,
                hType,
                pOp,
                pComm,
                lgpof2,
                rem,
                newRank,
                &pTask,
                &pReq->nbc.nTasks,
                &piNext
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
    }

    //
    // In the non-pof2 case, all odd-numbered processes of rank < 2 * rem send
    // the result to rank - 1, the ranks which didn't participate above.
    //
    if( rank < 2 * rem )
    {
        if( newRank >= 0 )
        {
            //
            // Odd ranks send to rank - 1.
            //
            mpi_errno = pTask->InitIsend(
                recvbuf,
                count,
                datatype,
                rank - 1,
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
            // Even ranks receive from rank + 1.
            //
            mpi_errno = pTask->InitIrecv(
                recvbuf,
                count,
                datatype,
                rank + 1
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }

        *piNext = pReq->nbc.nTasks;
        pTask->m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pTask->m_iNextOnComplete;
        ++pReq->nbc.nTasks;
    }

    *piNext = NBC_TASK_NONE;

    MPIU_Assert( pReq->nbc.nTasks == nTasks );

    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IallreduceBuildIntra(
    _In_opt_  const void*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >=, 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
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

    if( count == 0 || pComm->remote_size == 1 )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        if( sendbuf != MPI_IN_PLACE )
        {
            //
            // MPI_Localcopy handles 0 count gracefully.
            //
            MPI_RESULT mpi_errno = MPIR_Localcopy(
                sendbuf,
                count,
                datatype,
                recvbuf,
                count,
                datatype
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
    }
    else
    {
        MPI_RESULT mpi_errno = IallreduceBuildTaskList(
            pRequest.get(),
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
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
MPIR_Iallreduce_intra(
    _In_opt_  const void*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >=, 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IallreduceBuildIntra( sendbuf, recvbuf, count, datatype, pOp, pComm, &pRequest );
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
IallreduceBuildInter(
    _In_opt_  const void*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // First do an intercommunicator ireduce to rank 0 on left group,
    // then an intercommunicator ireduce to rank 0 on right group, followed
    // by local intracommunicator ibcast in each group.
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
    pRequest->nbc.tag = pComm->GetNextNBCTag(MPIR_GET_ASYNC_TAG( MPIR_ALLREDUCE_TAG ) );
    pRequest->nbc.tasks = new NbcTask[3];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    int root1 = 0;
    int root2 = 0;
    int local = ( pComm->rank == 0 ) ? MPI_ROOT : MPI_PROC_NULL;
    if( pComm->inter.is_low_group )
    {
        root1 = local;
    }
    else
    {
        root2 = local;
    }

    MPI_RESULT mpi_errno = pRequest->nbc.tasks[0].InitIreduceInter(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
        root1,
        pComm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }
    pRequest->nbc.tasks[0].m_iNextOnInit = 1;
    pRequest->nbc.tasks[0].m_iNextOnComplete = ( root1 == 0 )? NBC_TASK_NONE : 2;
    pRequest->nbc.nTasks++;

    mpi_errno = pRequest->nbc.tasks[1].InitIreduceInter(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
        root2,
        pComm
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        pRequest->cancel_nbc_tasklist();
        return mpi_errno;
    }
    pRequest->nbc.tasks[1].m_iNextOnInit = NBC_TASK_NONE;
    pRequest->nbc.tasks[1].m_iNextOnComplete = ( root2 == 0 )? NBC_TASK_NONE : 2;
    pRequest->nbc.nTasks++;

    pRequest->nbc.tasks[2].InitIbcastIntra(
        recvbuf,
        count,
        datatype,
        0,
        pComm->inter.local_comm
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
MPIR_Iallreduce_inter(
    _In_opt_  const void*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IallreduceBuildInter(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
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


/* This is the default implementation of scan. The algorithm is:

   Algorithm: MPI_Scan

   We use a lgp recursive doubling algorithm. The basic algorithm is
   given below. (You can replace "+" with any other scan operator.)
   The result is stored in recvbuf.

 .vb
   recvbuf = sendbuf;
   partial_scan = sendbuf;
   mask = 0x1;
   while (mask < size)
   {
      dst = rank^mask;
      if (dst < size)
      {
         send partial_scan to dst;
         recv from dst into tmp_buf;
         if (rank > dst)
         {
            partial_scan = tmp_buf + partial_scan;
            recvbuf = tmp_buf + recvbuf;
         }
         else
         {
            if (op is commutative)
               partial_scan = tmp_buf + partial_scan;
            else
            {
               tmp_buf = partial_scan + tmp_buf;
               partial_scan = tmp_buf;
            }
         }
      }
      mask <<= 1;
   }
 .ve

   End Algorithm: MPI_Scan
*/
static
_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IscanBuildTaskList(
    _In_opt_    const void*      sendbuf,
    _Inout_opt_ void*            recvbuf,
    _In_range_(>=, 0)
                int              count,
    _In_        TypeHandle       datatype,
    _In_        const MPID_Op*   pOp,
    _In_        MPID_Comm*       pComm,
    _In_        int              tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    int comm_size = pComm->remote_size;
    int rank      = pComm->rank;
    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pRequest->comm->AddRef();
    pRequest->nbc.tag = pComm->GetNextNBCTag( tag );

    if( count == 0 )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    //
    // Calculating how much buffer to store partial scan and incoming data
    //
    MPI_Count              true_lb;
    MPI_Aint true_extent = fixme_cast<MPI_Aint>( datatype.GetTrueExtentAndLowerBound( &true_lb ) );
    MPI_Aint extent      = fixme_cast<MPI_Aint>( datatype.GetExtent() );
    ULONG bufSize        = static_cast<ULONG>( count * max( extent, true_extent ) );

    pRequest->nbc.tmpBuf = new BYTE[2 * bufSize];
    if( pRequest->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Adjust for potential negative lower bound in datatype
    //
    BYTE* partial_scan = pRequest->nbc.tmpBuf - true_lb;
    BYTE* tmp_buf      = partial_scan + bufSize;

    //
    // Since this is an inclusive scan, copy local distribution into recvbuf
    //
    MPI_RESULT mpi_errno;
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            count,
            datatype,
            recvbuf,
            count,
            datatype );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        mpi_errno = MPIR_Localcopy(
            sendbuf,
            count,
            datatype,
            partial_scan,
            count,
            datatype );
    }
    else
    {
        mpi_errno = MPIR_Localcopy(
            recvbuf,
            count,
            datatype,
            partial_scan,
            count,
            datatype );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Calculating the exact number of tasks required
    //
    int nTasks = 0;
    int mask = 0x1;
    int dst;
    while (mask < comm_size)
    {
        dst = rank ^ mask;
        if( dst < comm_size )
        {
            nTasks += 3;

            if( (mask << 1) >= comm_size )
            {
                //
                // At the last round the smaller rank processes do not
                // need to receive, only need to send. On the other
                // hand the higher rank proceses only need to recv.
                //
                if( rank < dst )
                {
                    nTasks -= 2;
                }
                else
                {
                    nTasks -= 1;
                }
            }
        }
        mask <<= 1;
    }
    if( nTasks == 0 )
    {
        //
        // This can happen when comm_size is 1
        //
        MPIU_Assert( comm_size == 1 );
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    pRequest->nbc.tasks = new NbcTask[nTasks];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPIU_Assert( pRequest->nbc.nTasks == 0 );
    NbcTask* pTask = &pRequest->nbc.tasks[0];

    mask = 0x1;
    while( mask < comm_size )
    {
        dst = rank ^ mask;

        if( dst < comm_size )
        {
            bool lastIteration = (mask << 1) >= comm_size;
            if( !lastIteration )
            {
                //
                // Queue the receive task
                //
                mpi_errno = pTask->InitIrecv(
                    tmp_buf,
                    count,
                    datatype,
                    dst );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto CancelTaskList;
                }

                //
                // Next task is a send which will be initiated with the recv
                //
                pTask->m_iNextOnInit = ++pRequest->nbc.nTasks;

                //
                // Only proceed to the reduction step when both receive and send are done
                //
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
                ++pTask;

                //
                // Queue the send task
                //
                mpi_errno = pTask->InitIsend(
                    partial_scan,
                    count,
                    datatype,
                    dst,
                    pComm );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto CancelTaskList;
                }
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;;
                ++pTask;
            }
            else
            {
                //
                // In the last iteration smaller ranks only send,
                // larger ranks only receive
                //
                if( rank < dst )
                {
                    //
                    // Queue the send task, don't need to wait for anything after that
                    //
                    mpi_errno = pTask->InitIsend(
                        partial_scan,
                        count,
                        datatype,
                        dst,
                        pComm );
                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto CancelTaskList;
                    }
                    pTask->m_iNextOnInit = NBC_TASK_NONE;
                    pTask->m_iNextOnComplete = NBC_TASK_NONE;
                    ++pRequest->nbc.nTasks;
                    break;
                }
                else
                {
                    //
                    // Queue the receive task
                    //
                    mpi_errno = pTask->InitIrecv(
                        tmp_buf,
                        count,
                        datatype,
                        dst );

                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto CancelTaskList;
                    }
                    pTask->m_iNextOnInit = NBC_TASK_NONE;
                    pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
                    ++pTask;
                }
            }

            //
            // Queue the reduction task
            //
            if( !lastIteration || rank >= dst )
            {
                mpi_errno = pTask->InitScan(
                    recvbuf,
                    partial_scan,
                    tmp_buf,
                    count,
                    datatype,
                    const_cast<MPID_Op*>(pOp),
                    datatype.GetMpiHandle(),
                    (rank > dst) ? true : false
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto CancelTaskList;
                }
            }

            //
            // Note that at the last iteration of the while loop we'll point OnComplete
            // to a task that won't exist. This will get fixed after the while loop
            //
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
            ++pTask;
        }

        mask = mask << 1;
    }

    //
    // Fix the last reduction step where we queued an extra step
    //
    pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;

CancelTaskList:
    pRequest->cancel_nbc_tasklist();
    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iscan(
    _In_opt_    const void*      sendbuf,
    _Inout_opt_ void*            recvbuf,
    _In_range_(>=, 0)
                int              count,
    _In_        TypeHandle       datatype,
    _In_        const MPID_Op*   pOp,
    _In_        MPID_Comm*       comm_ptr,
    _In_        int              tag,
    _Outptr_    MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IscanBuildTaskList(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
        comm_ptr,
        tag,
        &pRequest );
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


/* This is the default implementation of exscan. The algorithm is:

   Algorithm: MPI_Exscan

   We use a lgp recursive doubling algorithm. The basic algorithm is
   given below. (You can replace "+" with any other scan operator.)
   The result is stored in recvbuf.

 .vb
   partial_scan = sendbuf;
   mask = 0x1;
   flag = 0;
   while (mask < size)
   {
      dst = rank^mask;
      if (dst < size)
      {
         send partial_scan to dst;
         recv from dst into tmp_buf;
         if (rank > dst)
         {
            partial_scan = tmp_buf + partial_scan;
            if (rank != 0)
            {
               if (flag == 0)
               {
                   recv_buf = tmp_buf;
                   flag = 1;
               }
               else
                   recv_buf = tmp_buf + recvbuf;
            }
         }
         else
         {
            if (op is commutative)
               partial_scan = tmp_buf + partial_scan;
            else
            {
               tmp_buf = partial_scan + tmp_buf;
               partial_scan = tmp_buf;
            }
         }
      }
      mask <<= 1;
   }
.ve

   End Algorithm: MPI_Exscan
*/
static
_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IexscanBuildTaskList(
    _In_opt_  const void*      sendbuf,
    _In_opt_  void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      MPID_Comm*       pComm,
    _In_      int              tag,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    int comm_size = pComm->remote_size;
    int rank      = pComm->rank;
    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pRequest->comm = pComm;
    pRequest->comm->AddRef();
    pRequest->nbc.tag = pComm->GetNextNBCTag( tag );

    if( count == 0 )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    //
    // Calculating how much buffer to store partial scan and incoming data
    //
    MPI_Count              true_lb;
    MPI_Aint true_extent = fixme_cast<MPI_Aint>( datatype.GetTrueExtentAndLowerBound( &true_lb ) );
    MPI_Aint extent      = fixme_cast<MPI_Aint>( datatype.GetExtent() );
    ULONG bufSize        = static_cast<ULONG>(count * max( extent, true_extent ));

    pRequest->nbc.tmpBuf = new BYTE[2 * bufSize];
    if( pRequest->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Adjust for potential negative lower bound in datatype
    //
    BYTE* partial_scan = pRequest->nbc.tmpBuf - true_lb;
    BYTE* tmp_buf      = partial_scan + bufSize;

    MPI_RESULT mpi_errno;
    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy(
            sendbuf,
            count,
            datatype,
            partial_scan,
            count,
            datatype );
    }
    else
    {
        mpi_errno = MPIR_Localcopy(
            recvbuf,
            count,
            datatype,
            partial_scan,
            count,
            datatype );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Calculating the exact number of tasks required
    //
    int nTasks = 0;
    int mask = 0x1;
    int dst;
    while (mask < comm_size)
    {
        dst = rank ^ mask;
        if( dst < comm_size )
        {
            nTasks += 3;

            if( (mask << 1) >= comm_size )
            {
                //
                // At the last round the smaller rank processes do not
                // need to receive, only need to send. On the other
                // hand the higher rank proceses only need to recv.
                //
                if( rank < dst )
                {
                    nTasks -= 2;
                }
                else
                {
                    nTasks -= 1;
                }
            }
        }
        mask <<= 1;
    }
    if( nTasks == 0 )
    {
        //
        // This can happen when comm_size is 1
        //
        MPIU_Assert( comm_size == 1 );
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        return MPI_SUCCESS;
    }

    pRequest->nbc.tasks = new NbcTask[nTasks];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPIU_Assert( pRequest->nbc.nTasks == 0 );
    NbcTask* pTask = &pRequest->nbc.tasks[0];

    mask = 0x1;
    bool flag = false;
    bool copyOnly = false;
    while( mask < comm_size )
    {
        dst = rank ^ mask;

        if( dst < comm_size )
        {
            bool lastIteration = (mask << 1) >= comm_size;
            if( !lastIteration )
            {
                //
                // Queue the receive task
                //
                mpi_errno = pTask->InitIrecv(
                    tmp_buf,
                    count,
                    datatype,
                    dst );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto CancelTaskList;
                }

                //
                // Next task is a send which will be initiated with the recv
                //
                pTask->m_iNextOnInit = ++pRequest->nbc.nTasks;

                //
                // Only proceed to the reduction step when both receive and send are done
                //
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
                ++pTask;

                //
                // Queue the send task
                //
                mpi_errno = pTask->InitIsend(
                    partial_scan,
                    count,
                    datatype,
                    dst,
                    pComm );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto CancelTaskList;
                }
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;;
                ++pTask;
            }
            else
            {
                //
                // In the last iteration smaller ranks only send,
                // larger ranks only receive
                //
                if( rank < dst )
                {
                    //
                    // Queue the send task, don't need to wait for anything after that
                    //
                    mpi_errno = pTask->InitIsend(
                        partial_scan,
                        count,
                        datatype,
                        dst,
                        pComm );
                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto CancelTaskList;
                    }
                    pTask->m_iNextOnInit = NBC_TASK_NONE;
                    pTask->m_iNextOnComplete = NBC_TASK_NONE;
                    ++pRequest->nbc.nTasks;
                    break;
                }
                else
                {
                    //
                    // Queue the receive task
                    //
                    mpi_errno = pTask->InitIrecv(
                        tmp_buf,
                        count,
                        datatype,
                        dst );

                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto CancelTaskList;
                    }
                    pTask->m_iNextOnInit = NBC_TASK_NONE;
                    pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
                    ++pTask;
                }
            }

            //
            // Queue the reduction task
            //
            if( !lastIteration || rank >= dst )
            {
                if( flag == false && rank > dst )
                {
                    copyOnly = true;
                    flag = true;
                }
                else
                {
                    copyOnly = false;
                }

                mpi_errno = pTask->InitScan(
                    recvbuf,
                    partial_scan,
                    tmp_buf,
                    count,
                    datatype,
                    const_cast<MPID_Op*>(pOp),
                    datatype.GetMpiHandle(),
                    (rank > dst) ? true : false,
                    copyOnly
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto CancelTaskList;
                }
            }

            //
            // Note that at the last iteration of the while loop we'll point OnComplete
            // to a task that won't exist. This will get fixed after the while loop
            //
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pRequest->nbc.nTasks;
            ++pTask;
        }

        mask = mask << 1;
    }

    //
    // Fix the last reduction step where we queued an extra step
    //
    pRequest->nbc.tasks[pRequest->nbc.nTasks - 1].m_iNextOnComplete = NBC_TASK_NONE;

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;

CancelTaskList:
    pRequest->cancel_nbc_tasklist();
    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iexscan(
    _In_opt_  const void*      sendbuf,
    _In_opt_  void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      MPID_Comm*       comm_ptr,
    _In_      int              tag,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IexscanBuildTaskList(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
        comm_ptr,
        tag,
        &pRequest );
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
static inline
MPI_RESULT
IreduceBuildBinomialTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_      BYTE*            tmpBuf,
    _When_( root == pComm->rank, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >= , 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_range_( 0, pComm->remote_size - 1 )
              int              root,
    _In_      MPID_Comm*       pComm
    )
{
    int localRoot;
    ULONG nTasks = 0;

    if( pOp->IsCommutative() == false && root != 0 )
    {
        if( pComm->rank == 0 || pComm->rank == root )
        {
            //
            // non-commutative reduction performs the reduction with root == 0,
            // then sends the result to the requested root.
            //
            nTasks = 1;
        }
        localRoot = 0;
    }
    else
    {
        localRoot = root;
    }

    int relativeRank = pComm->rank - localRoot;
    if( relativeRank < 0 )
    {
        relativeRank += pComm->remote_size;
    }

    //
    // Calculate the size of the task array up front.  At a minimum there is one receive
    // and one reduction per child.
    //
    unsigned nChildren = ChildCount( relativeRank, pComm->remote_size );
    nTasks += nChildren * 2;

    if( relativeRank != 0 )
    {
        //
        // Non-root processes must send the data to their parent.
        //
        nTasks += 1;
    }

    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_Datatype hType = datatype.GetMpiHandle();
    ULONG* piNext = &pReq->nbc.tasks[0].m_iNextOnInit;
    MPI_RESULT mpi_errno = BinomialChildBuilderAscending(
        [&](
            _In_range_( >=, 1 ) unsigned offset
            )
        {
            int targetRank = pComm->rank + offset;
            if( targetRank >= pComm->remote_size )
            {
                targetRank -= pComm->remote_size;
            }

            *piNext = pReq->nbc.nTasks;

            NbcTask* pTask = &pReq->nbc.tasks[pReq->nbc.nTasks];

            MPI_RESULT mpi_errno = pTask->InitIrecv(
                tmpBuf,
                count,
                datatype,
                targetRank
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }

            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            pTask++;
            mpi_errno = pTask->InitReduce(
                tmpBuf,
                recvbuf,
                count,
                datatype,
                pOp,
                hType,
                false
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                //
                // TODO: It would be nice if we could issue all receives in parallel
                // but find a way to delay the send to parent until all reductions have
                // completed.  This would require a mechanism for many-to-one relationships
                // which we don't have yet.
                //
                piNext = &pTask->m_iNextOnComplete;
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pReq->nbc.nTasks++;
            }

            return mpi_errno;
        },
        nChildren
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    mpi_errno = BinomialParentBuilder(
        [&]( _In_range_( 0, pComm->remote_size - 1 ) int targetRank )
        {
            *piNext = pReq->nbc.nTasks;

            MPI_RESULT mpi_errno = pReq->nbc.tasks[pReq->nbc.nTasks].InitIsend( recvbuf, count, datatype, targetRank, pComm );
            if( mpi_errno == MPI_SUCCESS )
            {
                pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnComplete = NBC_TASK_NONE;
                piNext = &pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnInit;
                pReq->nbc.nTasks++;
            }

            return mpi_errno;
        },
        pComm->rank,
        pComm->remote_size,
        relativeRank
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( localRoot != root )
    {
        MPIU_Assert( pOp->IsCommutative() == false );
        *piNext = pReq->nbc.nTasks;
        if( pComm->rank == 0 )
        {
            mpi_errno = pReq->nbc.tasks[pReq->nbc.nTasks].InitIsend( recvbuf, count, datatype, root, pComm );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }

            pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnComplete = NBC_TASK_NONE;
            piNext = &pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnInit;
            pReq->nbc.nTasks++;
        }
        else if( pComm->rank == root )
        {
            mpi_errno = pReq->nbc.tasks[pReq->nbc.nTasks].InitIrecv( recvbuf, count, datatype, 0 );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }

            pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnInit = NBC_TASK_NONE;
            piNext = &pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnComplete;
            pReq->nbc.nTasks++;
        }
    }

    //
    // Terminate the chain.
    //
    *piNext = NBC_TASK_NONE;

    pReq->nbc.tag = pReq->comm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_REDUCE_TAG ) );

    //
    // Make sure we used up all tasks in the array.
    //
    MPIU_Assert( nTasks == pReq->nbc.nTasks );
    MPIU_Assert( pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_state == NbcTask::NBC_TASK_STATE_NOTSTARTED );

    return MPI_SUCCESS;
}


/*
   This function converts scatter rank to gather rank and vice versa since
   the conversion to both directions is symmetrical.
*/
static inline
unsigned
TranslateScatterGatherRank(
    _In_range_( >, 1 )        unsigned numRanks,
    _In_range_( <, numRanks ) unsigned rank
    )
{
    //
    // The scatter step distributes data chunks to ranks not in the rank order.
    // A regular binomial tree will be used at the gather step which means
    // the data chunks numbers will be used as the rank number so that rank n
    // has the nth chunk of the data.
    //
    // For example:
    // 2 ranks
    // - scatter rank 0  1
    // - gather rank  0  1
    // 4 ranks
    // - scatter rank 0  1  2  3
    // - gather rank  0  2  1  3
    // 8 ranks
    // - scatter rank 0  1  2  3  4  5  6  7
    // - gather rank  0  4  2  6  1  5  3  7
    // 16 ranks
    // - scatter rank 0  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15
    // - gather rank  0  8  4  12 2  10 6  14 1  9  5   13  3   11  7   15
    //
    // Notice the second half of the gather rank number equals to the
    // corresponding first half number + 1, for example the 16 ranks case,
    // you can see this clearly if the rank sequence is broken down to two lines:
    // 0  8  4  12  2  10  6  14
    // 1  9  5  13  3  11  7  15
    //
    // Also notice the first half of the gather rank number equals to 2 * the
    // rank number of numRanks = numRanks / 2, for example:
    // 4 ranks gather rank               0  2  1  3
    // first half of 8 ranks gather rank 0  4  2  6
    //
    MPIU_Assert( numRanks > 1 && IsPowerOf2( numRanks ) == true );
    MPIU_Assert( rank < numRanks );

    unsigned result = 0;
    unsigned mask = 1;

    do
    {
        result <<= 1;
        if( ( rank & mask ) != 0 )
        {
            result++;
        }
        mask <<= 1;
    }
    while( mask < numRanks );

    return result;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
IreduceBuildScatterGatherTaskList(
    _Inout_   MPID_Request*     pReq,
    _In_      BYTE*             tmpBuf,
    _In_      MPI_Aint          extent,
    _When_( root == pComm->rank, _Out_opt_ )
              void*             recvbuf,
    _In_      unsigned          count,
    _In_      TypeHandle        datatype,
    _In_      MPID_Op*          pOp,
    _In_range_( 0, pComm->remote_size - 1 )
              unsigned          root,
    _In_range_( pComm->remote_size / 2, pComm->remote_size )
              unsigned          pof2,
    _In_      MPID_Comm*        pComm
    )
{
    MPIU_Assert( IsPowerOf2( pof2 ) == true );

    //
    // This algorithm is a slight deviation from the Rabenseifner's algorithm as follows:
    // - everything operates on relative rank
    // - the non-pof2 case is handled by dropping to the nearest lower pof2: the
    //   first few odd-numbered processes send their data to their left neighbors
    //   (relativeRank-1)
    // - a recursive-halving algorithm (beginning with processes that are
    //   distance 1 apart) is used for the reduce scatter phase. New rank number
    //   is obtained after reduce scatter so that new rank n contains the nth
    //   chunk of the data. A normal binomial tree can be used for the subsequent
    //   gather.
    // - there are easier implementations like over pof2 ranks send data to
    //   rank - pof2 to form the pof2 tree, or in the recursive-halving algorithm
    //   beginning with processes that are half the tree apart and that will form
    //   a normal binomial tree without doing any rank translation. But we stick
    //   with sending to neighbor and make sure we always send neighbor as large
    //   amount of data as possible because neighbor processes have better chance
    //   to reside on the same node thus SHM will be used for communication and
    //   this helps overall performance.
    //

    pReq->nbc.tag = pReq->comm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_REDUCE_TAG ) );

    unsigned size = pComm->remote_size;
    unsigned relativeRank = pComm->RankSub( root );
    unsigned rem = size - pof2;
    int scatterRelativeRank;
    int gatherRelativeRank;

    //
    // Calculate the number of reduce/scatter steps.  This is used to calculate the number
    // of tasks needed as well as building the tasks.
    //
    // The number of children is simply the index of the only bit set in pof2.  We scan
    // forward as we expect pof2 to be relatively small.
    //
    ULONG nRedScatSteps;
    _BitScanForward( &nRedScatSteps, pof2 );

    ULONG nTasks;
    ULONG nChildren;
    if( relativeRank < 2 * rem && ( relativeRank & 1 ) != 0 )
    {
        //
        // Odd relative rank within 2 * rem range, send its data to
        // relativeRank - 1. It doesn't participate in the scatter/gather.
        //
        nTasks = 1;
        nChildren = 0;
        scatterRelativeRank = -1;
        gatherRelativeRank = -1;
    }
    else
    {
        nTasks = nRedScatSteps * 3;

        if( relativeRank < 2 * rem )
        {
            //
            // Even relative rank within 2 * rem range, receive from
            // relativeRank + 1 then reduce. It will participate in
            // the scatter/gather.
            //
            nTasks += 2;
            scatterRelativeRank = relativeRank / 2;
        }
        else
        {
            //
            // Relative ranks that are outside of the 2 * rem range which
            // are the normal case and will participate in the scatter/gather.
            //
            scatterRelativeRank = relativeRank - rem;
        }

        gatherRelativeRank = TranslateScatterGatherRank( pof2, static_cast<unsigned>( scatterRelativeRank ) );
        nChildren = ChildCount( gatherRelativeRank, pof2 );
        nTasks += nChildren;

        //
        // If we have a parent, we must send to it during the gather phase.
        //
        if( gatherRelativeRank != 0 )
        {
            nTasks += 1;
        }
    }

    pReq->nbc.tasks = new NbcTask[nTasks];
    if( pReq->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // First handle the unbalanced tree case.
    //
    NbcTask* pTask = &pReq->nbc.tasks[pReq->nbc.nTasks];
    MPI_Datatype hType = datatype.GetMpiHandle();
    ULONG* piNext = &pTask->m_iNextOnComplete;
    MPI_RESULT mpi_errno;
    if( relativeRank < 2 * rem )
    {
        if( scatterRelativeRank >= 0 )
        {
            mpi_errno = pTask->InitIrecv(
                tmpBuf,
                count,
                datatype,
                pComm->RankAdd( 1 )
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            ++pTask;
            mpi_errno = pTask->InitReduce(
                tmpBuf,
                recvbuf,
                count,
                datatype,
                pOp,
                hType,
                false
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
        else
        {
            //
            // TODO: It would be nice to just convert the request to an MPIDI_REQUEST_TYPE_SEND here.
            //
            mpi_errno = pTask->InitIsend(
                recvbuf,
                count,
                datatype,
                pComm->RankSub( 1 ),
                pComm
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
                ++pReq->nbc.nTasks;
            }
            return mpi_errno;
        }
    }

    //
    // Now we can work as if we had a balanced tree. This is important in order
    // for the computation to be evenly distributed.
    // We may have uneven distribution of data, with the last child receiving the
    // remaining data.
    //
    int reduceCount = count / pof2;
    int endCount = count % pof2;
    unsigned sendIdx = 0;
    unsigned recvIdx = 0;
    unsigned lastIdx = pof2;
    unsigned idxShift = pof2 >> 1;

    //
    // Note that every node exchanges data with its peers, where the number of peers
    // is determined by the number of nodes in the tree (pof2).  This is very much
    // like a reduce-scatter.
    //
    mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) unsigned offset )
        {
            *piNext = pReq->nbc.nTasks;

            //
            // offset is the distance to our peer.  Figure out the actual
            // rank of our peer.
            //
            unsigned scatterRelativePeer = (scatterRelativeRank ^ offset);
            unsigned peerRank = pComm->RankAdd( TrimmedToOriginalRankEven( rem, scatterRelativePeer ), root );
            bool rightOrder = peerRank < fixme_cast<unsigned>( pComm->rank ) ? true : false;

            int sendCount;
            int recvCount;

            if( static_cast<unsigned>( scatterRelativeRank ) < scatterRelativePeer )
            {
                sendIdx = recvIdx + idxShift;
                recvCount = ( sendIdx - recvIdx ) * reduceCount;
                sendCount = ( lastIdx - sendIdx ) * reduceCount;
                if( lastIdx == pof2 )
                {
                    sendCount += endCount;
                }
            }
            else
            {
                recvIdx = sendIdx + idxShift;
                sendCount = ( recvIdx - sendIdx ) * reduceCount;
                recvCount = ( lastIdx - recvIdx ) * reduceCount;
                if( lastIdx == pof2 )
                {
                    recvCount += endCount;
                }
            }

            const BYTE* pSendBuf = static_cast<BYTE*>( recvbuf ) + ( extent * sendIdx * reduceCount );
            BYTE* pReduceBuf = static_cast<BYTE*>( recvbuf ) + ( extent * recvIdx * reduceCount );

            MPI_RESULT mpi_errno = pTask->InitIrecv(
                tmpBuf,
                recvCount,
                datatype,
                peerRank
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            pTask->m_iNextOnInit = ++pReq->nbc.nTasks;
            pTask->m_iNextOnComplete = ++pReq->nbc.nTasks;
            ++pTask;

            mpi_errno = pTask->InitIsend(
                pSendBuf,
                sendCount,
                datatype,
                peerRank,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                //
                // We aren't going to initialize the reduce task, so
                // fixup the number of tasks that will need to be cleaned up.
                //
                --pReq->nbc.nTasks;
                return mpi_errno;
            }
            pTask->m_iNextOnInit = NBC_TASK_NONE;
            pTask->m_iNextOnComplete = NBC_TASK_NONE;
            ++pTask;

            mpi_errno = pTask->InitReduce(
                tmpBuf,
                pReduceBuf,
                recvCount,
                datatype,
                pOp,
                hType,
                rightOrder
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnInit = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnComplete;
                ++pReq->nbc.nTasks;
                ++pTask;
            }

            sendIdx = recvIdx;
            lastIdx = recvIdx + idxShift;
            idxShift >>= 1;

            return mpi_errno;
        },
        nRedScatSteps
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Now the gather part - behaves like a normal gather aside from handling
    // uneven data distribution.
    //
    mpi_errno = BinomialChildBuilderAscending(
        [&]( _In_range_( >=, 0 ) int offset )
        {
            *piNext = pReq->nbc.nTasks;

            unsigned gatherRelativeTarget = gatherRelativeRank + offset;
            unsigned relativeTarget = TrimmedToOriginalRankEven(
                rem,
                TranslateScatterGatherRank( pof2, gatherRelativeTarget )
                );
            MPIU_Assert( relativeTarget < size );
            unsigned targetRank = pComm->RankAdd( relativeTarget, root );

            int localCount = reduceCount * offset;
            //
            // If we are receiving the top of the buffer, account for the extra
            // data at the end (for uneven data distribution).
            //
            if( gatherRelativeTarget + offset == pof2 )
            {
                localCount += endCount;
            }

            MPI_RESULT mpi_errno = pTask->InitIrecv(
                static_cast<BYTE*>( recvbuf ) + ( extent * reduceCount * gatherRelativeTarget ),
                localCount,
                datatype,
                targetRank
                );

            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnInit;
                ++pReq->nbc.nTasks;
                ++pTask;
            }
            return mpi_errno;
        },
        nChildren
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( nChildren > 0 )
    {
        //
        // Not leaf, fix the last receive task to stop initiating the next task.
        //
        pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pReq->nbc.tasks[pReq->nbc.nTasks - 1].m_iNextOnComplete;
    }

    mpi_errno = BinomialParentBuilder(
        [&]( _In_range_( >= , 0 ) int gatherRelativeTarget )
        {
            *piNext = pReq->nbc.nTasks;

            unsigned relativeTarget = TrimmedToOriginalRankEven(
                rem,
                TranslateScatterGatherRank( pof2, gatherRelativeTarget )
                );
            MPIU_Assert( relativeTarget < size );
            unsigned targetRank = pComm->RankAdd( relativeTarget, root );

            unsigned treeSize = TreeSize( gatherRelativeRank, pof2 );
            int localCount = reduceCount * treeSize;
            if( treeSize + gatherRelativeRank == pof2 )
            {
                localCount += endCount;
            }

            MPI_RESULT mpi_errno = pTask->InitIsend(
                static_cast<BYTE*>( recvbuf ) + ( extent * reduceCount * gatherRelativeRank ),
                localCount,
                datatype,
                targetRank,
                pComm
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                pTask->m_iNextOnComplete = NBC_TASK_NONE;
                piNext = &pTask->m_iNextOnInit;
                ++pReq->nbc.nTasks;
                ++pTask;
            }
            return mpi_errno;
        },
        gatherRelativeRank,
        pof2,
        gatherRelativeRank
        );

    *piNext = NBC_TASK_NONE;

    MPIU_Assert( mpi_errno != MPI_SUCCESS || pReq->nbc.nTasks == nTasks );
    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
IreduceBuildTaskList(
    _Inout_   MPID_Request*    pReq,
    _In_opt_  const void*      sendbuf,
    _When_( root == pComm->rank, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >= , 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_range_( 0, pComm->remote_size - 1 )
              int              root,
    _In_      MPID_Comm*       pComm
    )
{
    //
    // Allocate a scratch buffer for receiving data from our peers so that we
    // can perform the reduction operation.
    //
    MPI_Count true_lb;
    MPI_Aint true_extent = fixme_cast<MPI_Aint>(datatype.GetTrueExtentAndLowerBound( &true_lb ));
    MPI_Aint extent = fixme_cast<MPI_Aint>(datatype.GetExtent());
    if( extent < true_extent )
    {
        extent = true_extent;
    }
    MPI_Aint fullExtent = extent * count;

    if( pComm->rank != root )
    {
        //
        // The receive buffer may not be valid, so we will need a scratch buffer for that too.
        //
        pReq->nbc.tmpBuf = new BYTE[2 * fullExtent];
        recvbuf = pReq->nbc.tmpBuf + fullExtent - true_lb;
    }
    else
    {
        pReq->nbc.tmpBuf = new BYTE[fullExtent];
    }

    if( pReq->nbc.tmpBuf == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_RESULT mpi_errno;
    if( pComm->rank != root || sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MPIR_Localcopy( sendbuf, count, datatype, recvbuf, count, datatype );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    BYTE* tmpBuf = pReq->nbc.tmpBuf - true_lb;

    unsigned pof2 = PowerOf2Floor( pComm->remote_size );

    //
    // We restrict the large algorithm to basic datatypes, not simply commutative
    // operations, because user-defined operators could use derived datatypes that
    // might be tricky to scatter in a rational way.
    //
    if( fixme_cast<unsigned>( fullExtent ) > pComm->SwitchPoints()->MPIR_reduce_short_msg &&
        pOp->IsBuiltin() == true &&
        fixme_cast<unsigned>( count ) >= pof2 )
    {
        return IreduceBuildScatterGatherTaskList(
            pReq,
            tmpBuf,
            extent,
            recvbuf,
            fixme_cast<unsigned>( count ),
            datatype,
            pOp,
            fixme_cast<unsigned>( root ),
            pof2,
            pComm
            );
    }

    return IreduceBuildBinomialTaskList(
        pReq,
        tmpBuf,
        recvbuf,
        count,
        datatype,
        pOp,
        root,
        pComm
        );
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IreduceBuildIntra(
    _In_opt_  const void*      sendbuf,
    _When_( root == pComm->rank, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >= , 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_range_( 0, pComm->remote_size - 1 )
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

    if( count == 0 || pComm->remote_size == 1 )
    {
        pRequest->kind = MPID_REQUEST_NOOP;
        if( sendbuf != MPI_IN_PLACE )
        {
            //
            // MPI_Localcopy handles 0 count gracefully.
            //
            MPI_RESULT mpi_errno = MPIR_Localcopy(
                sendbuf,
                count,
                datatype,
                recvbuf,
                count,
                datatype
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
    }
    else
    {
        MPI_RESULT mpi_errno = IreduceBuildTaskList(
            pRequest.get(),
            sendbuf,
            recvbuf,
            count,
            datatype,
            pOp,
            root,
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
MPIR_Ireduce_intra(
    _In_opt_  const void*      sendbuf,
    _When_( root == pComm->rank, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >= , 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_range_( 0, pComm->remote_size - 1 )
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
)
{
    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IreduceBuildIntra( sendbuf, recvbuf, count, datatype, pOp, root, pComm, &pRequest );
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
MPI_RESULT
IreduceBuildInter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _When_( root != MPI_PROC_NULL, _In_ )
              MPID_Op*         pOp,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    //
    // Intercommunicator non-blocking reduction.
    // Root receives from rank 0 in remote group.
    // Remote group does local intracommunicator reduction.
    //
    MPIU_Assert(pComm->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(pComm->inter.local_comm != nullptr);

    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_REDUCE_TAG ) );

    if( count > 0 )
    {
        if( root == MPI_ROOT )
        {
            return MPID_Recv(
                recvbuf,
                count,
                datatype,
                0,
                tag | pComm->comm_kind,
                pComm,
                ppRequest
                );
        }

        if( root >= 0 && pComm->inter.local_comm->rank != 0 )
        {
            //
            // we're a leaf in the non-root group - just a simple ireduce for us amongst
            // our local group. Note that only the send buffer is valid here.
            //
            return IreduceBuildIntra(
                sendbuf,
                nullptr,
                count,
                datatype,
                pOp,
                0,
                pComm->inter.local_comm,
                ppRequest
                );
        }
    }

    StackGuardRef<MPID_Request> pRequest( MPID_Request_create( MPID_REQUEST_NBC ) );
    if( pRequest == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    if( root == MPI_PROC_NULL || count <= 0 )
    {
        //
        // Peers to the root or count <= 0, do nothing.
        //
        pRequest->kind = MPID_REQUEST_NOOP;
        *ppRequest = pRequest.detach();
        //
        // fix OACR warning 6101 for recvbuf
        //
        OACR_USE_PTR(recvbuf);
        return MPI_SUCCESS;
    }

    pRequest->comm = pComm;
    pComm->AddRef();

    pRequest->nbc.tag = tag;
    pRequest->nbc.tasks = new NbcTask[2];
    if( pRequest->nbc.tasks == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Rank 0 in the non-root group needs a valid receive buffer, which may not have been
    // provided by the user - the standard only requires a valid send buffer.
    // Allocate a temporary.
    //
    MPI_Count true_lb;
    MPI_Aint true_extent = fixme_cast<MPI_Aint>(datatype.GetTrueExtentAndLowerBound( &true_lb ));
    MPI_Aint extent = fixme_cast<MPI_Aint>(datatype.GetExtent());
    if( extent >= true_extent )
    {
        extent *= count;
    }
    else
    {
        extent = true_extent * count;
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
    recvbuf = pRequest->nbc.tmpBuf - true_lb;

    //
    // Rank 0 on non-root group is root of reduction amongst its local group.
    //
    MPI_RESULT mpi_errno = pRequest->nbc.tasks[0].InitIreduceIntra(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
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
    // Rank 0 on non-root group sends to root.
    //
    mpi_errno = pRequest->nbc.tasks[1].InitIsend(
        recvbuf,
        count,
        datatype,
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

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ireduce_inter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _When_( root != MPI_PROC_NULL, _In_ )
              MPID_Op*         pOp,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IreduceBuildInter(
        sendbuf,
        recvbuf,
        count,
        datatype,
        pOp,
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
