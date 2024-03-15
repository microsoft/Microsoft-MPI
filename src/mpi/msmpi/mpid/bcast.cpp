// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#define MPIDI_BINOMIAL_MAX_OUTSTANDING 4

_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
MPIR_Bcast_binomial(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
    void *buffer,
    _In_range_(>=, 0) int nBytes,
    _In_range_(0, comm_ptr->remote_size - 1) int root,
    _In_ const MPID_Comm *comm_ptr
    )
{
    int        rank, comm_size, src, dst;
    int        relative_rank, mask;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    /* Algorithm:
        This uses a fairly basic recursive subdivision algorithm.
        The root sends to the process comm_size/2 away; the receiver becomes
        a root for a subtree and applies the same process.

        So that the new root can easily identify the size of its
        subtree, the (subtree) roots are all powers of two (relative
        to the root) If m = the first power of 2 such that 2^m >= the
        size of the communicator, then the subtree at root at 2^(m-k)
        has size 2^k (with special handling for subtrees that aren't
        a power of two in size).

        Do subdivision.  There are two phases:
        1. Wait for arrival of data.  Because of the power of two nature
        of the subtree roots, the source of this message is alwyas the
        process whose relative rank has the least significant 1 bit CLEARED.
        That is, process 4 (100) receives from process 0, process 7 (111)
        from process 6 (110), etc.
        2. Forward to my subtree

        Note that the process that is the tree root is handled automatically
        by this code, since it has no bits set.  */

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;
    relative_rank = rank - root;
    if (relative_rank < 0)
    {
        relative_rank += comm_size;
    }

    mask = 0x1;
    while (mask < comm_size)
    {
        if (relative_rank & mask)
        {
            src = rank - mask;
            if (src < 0)
            {
                src += comm_size;
            }
            mpi_errno = MPIC_Recv(
                buffer,
                nBytes,
                g_hBuiltinTypes.MPI_Byte,
                src,
                MPIR_BCAST_TAG,
                comm_ptr,
                MPI_STATUS_IGNORE
                );
            ON_ERROR_FAIL(mpi_errno);

            break;
        }
        mask <<= 1;
    }

    /* This process is responsible for all processes that have bits
        set from the LSB up to (but not including) mask.  Because of
        the "not including", we start by shifting mask back down one.

        We can easily change to a different algorithm at any power of two
        by changing the test (mask > 1) to (mask > block_size)

        One such version would use non-blocking operations for the last 2-4
        steps (this also bounds the number of MPI_Requests that would
        be needed).  */

    MPID_Request* req_array[MPIDI_BINOMIAL_MAX_OUTSTANDING];
    ZeroMemory(req_array, sizeof(req_array));

    int num_outstanding = 0;
    int empty_slot = 0;

    mask >>= 1;
    while (mask > 0)
    {
        if (relative_rank + mask < comm_size)
        {
            if(num_outstanding >= MPIDI_BINOMIAL_MAX_OUTSTANDING)
            {
                unsigned int index = UINT_MAX;
                mpi_errno = MPIR_Waitany(num_outstanding, req_array, &index);

                if( index != UINT_MAX )
                {
                    req_array[index]->Release();
                    req_array[index] = nullptr;
                }
                ON_ERROR_FAIL(mpi_errno);

                num_outstanding--;
                empty_slot = index;
            }

            dst = rank + mask;
            if (dst >= comm_size)
            {
                dst -= comm_size;
            }

            mpi_errno = MPID_Send(
                buffer,
                nBytes,
                g_hBuiltinTypes.MPI_Byte,
                dst,
                MPIR_BCAST_TAG | comm_ptr->comm_kind,
                const_cast<MPID_Comm*>(comm_ptr),
                &req_array[empty_slot]
                );
            ON_ERROR_FAIL(mpi_errno);

            empty_slot = ++num_outstanding;
        }
        mask >>= 1;
    }

    mpi_errno = MPIR_Waitall(num_outstanding, req_array);
    for( int i = 0; i < num_outstanding; ++i )
    {
        MPIU_Assert( req_array[i] != nullptr );
        req_array[i]->Release();
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
scatter_for_bcast(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
    BYTE *buffer,
    _In_range_(>=, 0) int nBytes,
    _In_range_(0, comm_ptr->remote_size - 1) int root,
    _In_ const MPID_Comm *comm_ptr
    )
{
    int scatter_size, comm_size, curr_size, recv_size, send_size;
    int mask, relative_rank, rank, src, dst;
    MPI_Status status;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    /* The scatter algorithm divides the buffer into nprocs pieces and
        scatters them among the processes. Root gets the first piece,
        root+1 gets the second piece, and so forth. Uses the same binomial
        tree algorithm as above. Ceiling division
        is used to compute the size of each piece. This means some
        processes may not get any data. For example if bufsize = 97 and
        nprocs = 16, ranks 15 and 16 will get 0 data. On each process, the
        scattered data is stored at the same offset in the buffer as it is
        on the root process. */

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    scatter_size = nBytes / comm_size;
    if ((nBytes % comm_size) != 0)
    {
        scatter_size++;
    }

    relative_rank = rank - root;
    if (relative_rank == 0)
    {
        curr_size = nBytes; /* root starts with all the data */
    }
    else
    {
        curr_size = 0;
        if (relative_rank < 0)
        {
            relative_rank += comm_size;
        }
    }

    mask = 0x1;
    while (mask < comm_size)
    {
        if (relative_rank & mask)
        {
            recv_size = nBytes - relative_rank*scatter_size;
            /* recv_size is larger than what might actually be sent by the
                sender. We don't need compute the exact value because MPI
                allows you to post a larger recv.*/
            if (recv_size <= 0)
            {
                //
                // This process receives no data due to uneven division
                //
                return mpi_errno;
            }

            src = rank - mask;
            if (src < 0)
            {
                src += comm_size;
            }

            mpi_errno = MPIC_Recv(
                (buffer + relative_rank*scatter_size),
                recv_size,
                g_hBuiltinTypes.MPI_Byte,
                src,
                MPIR_BCAST_TAG,
                comm_ptr,
                &status
                );
            ON_ERROR_FAIL(mpi_errno);

            /* query actual size of data received */
            NMPI_Get_count(&status, MPI_BYTE, &curr_size);
            break;
        }
        mask <<= 1;
    }

    /* This process is responsible for all processes that have bits
        set from the LSB upto (but not including) mask.  Because of
        the "not including", we start by shifting mask back down
        one. */

    MPID_Request* req_array[MPIDI_BINOMIAL_MAX_OUTSTANDING];
    ZeroMemory(req_array, sizeof(req_array));

    int num_outstanding = 0;
    int empty_slot = 0;

    mask >>= 1;
    while (mask > 0)
    {
        if (relative_rank + mask < comm_size)
        {
            send_size = curr_size - scatter_size * mask;
            /* mask is also the size of this process's subtree */

            if (send_size > 0)
            {
                if(num_outstanding >= MPIDI_BINOMIAL_MAX_OUTSTANDING)
                {
                    unsigned int index = UINT_MAX;
                    mpi_errno = MPIR_Waitany(num_outstanding, req_array, &index);

                    if( index != UINT_MAX )
                    {
                        req_array[index]->Release();
                        req_array[index] = nullptr;
                    }
                    ON_ERROR_FAIL(mpi_errno);

                    num_outstanding--;
                    empty_slot = index;
                }

                dst = rank + mask;
                if (dst >= comm_size) dst -= comm_size;
                mpi_errno = MPID_Send(
                    (buffer + scatter_size*(relative_rank+mask)),
                    send_size,
                    g_hBuiltinTypes.MPI_Byte,
                    dst,
                    MPIR_BCAST_TAG | comm_ptr->comm_kind,
                    const_cast<MPID_Comm*>(comm_ptr),
                    &req_array[empty_slot]
                    );
                ON_ERROR_FAIL(mpi_errno);

                empty_slot = ++num_outstanding;
                curr_size -= send_size;
            }
        }
        mask >>= 1;
    }

    mpi_errno = MPIR_Waitall(num_outstanding, req_array);
    for( int i = 0; i < num_outstanding; ++i )
    {
        MPIU_Assert( req_array[i] != nullptr );
        req_array[i]->Release();
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
doubling_allgather_for_bcast(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
    BYTE *buffer,
    _In_range_(>=, 0) int nBytes,
    _In_range_(0, comm_ptr->remote_size - 1) int root,
    _In_ const MPID_Comm *comm_ptr
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int i;
    int mask, comm_size;
    int rank, relative_rank, relative_dst, dst, dst_tree_root, my_tree_root;
    int send_offset, recv_offset, scatter_size, recv_size, curr_size;
    MPI_Status status;

    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;
    relative_rank = rank - root;
    if (relative_rank < 0)
    {
        relative_rank += comm_size;
    }
    scatter_size = nBytes / comm_size;
    if ((nBytes % comm_size) != 0)
    {
        scatter_size++;

        curr_size = nBytes - relative_rank * scatter_size;
        if (curr_size > scatter_size)
        {
            curr_size = scatter_size;
        }
        else if (curr_size < 0)
        {
            curr_size = 0;
        }
    }
    else
    {
        curr_size = scatter_size;
    }

    i = 0;
    for (mask = 0x1; mask < comm_size; mask <<= 1)
    {
        relative_dst = relative_rank ^ mask;

        if (relative_dst < comm_size)
        {
            dst = relative_dst + root;
            if (dst >= comm_size)
            {
                dst -= comm_size;
            }

            /* find offset into send and recv buffers.
                zero out the least significant "i" bits of relative_rank and
                relative_dst to find root of src and dst
                subtrees. Use ranks of roots as index to send from
                and recv into  buffer */

            dst_tree_root = relative_dst >> i;
            dst_tree_root <<= i;

            my_tree_root = relative_rank >> i;
            my_tree_root <<= i;

            send_offset = my_tree_root * scatter_size;
            recv_offset = dst_tree_root * scatter_size;

            mpi_errno = MPIC_Sendrecv(
                (buffer + send_offset),
                curr_size,
                g_hBuiltinTypes.MPI_Byte,
                dst,
                MPIR_BCAST_TAG,
                (buffer + recv_offset),
                ((nBytes - recv_offset <= 0) ? 0 : nBytes - recv_offset),
                g_hBuiltinTypes.MPI_Byte,
                dst,
                MPIR_BCAST_TAG,
                comm_ptr,
                &status
                );
            ON_ERROR_FAIL(mpi_errno);
            NMPI_Get_count(&status, MPI_BYTE, &recv_size);
            curr_size += recv_size;
        }

        i++;
    }
fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
ring_allgather_for_bcast(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
    BYTE *buffer,
    _In_range_(>=, 0) int nBytes,
    _In_range_(0, comm_ptr->remote_size - 1) int root,
    _In_ const MPID_Comm* comm_ptr
    )
{
    StackGuardArray<int> recvcnts;
    int* displs;
    int i;
    int comm_size, scatter_size, fractionalIndex, rank, left, right, ringIndex, nextRingIndex;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    rank = comm_ptr->rank;
    comm_size = comm_ptr->remote_size;
    recvcnts = new int[comm_size * 2];
    if (recvcnts == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    displs = recvcnts + comm_size;

    scatter_size = nBytes / comm_size;
    if ((nBytes % comm_size) != 0)
    {
        //
        // nBytes do not evenly divide into the current number of
        // processes. Increment the scatter_size of the receiving blocks
        // and determine the number of processes needed using that size,
        // including a fragment at the end.  All processes not in use should
        // be set to 0.
        //
        scatter_size++;
        fractionalIndex = nBytes / scatter_size;
        recvcnts[fractionalIndex] = nBytes % scatter_size;
        for (i = fractionalIndex + 1; i < comm_size; i++)
        {
            recvcnts[i] = 0;
        }
        displs[fractionalIndex] = fractionalIndex * scatter_size;
        for (i = fractionalIndex + 1; i < comm_size; i++)
        {
            displs[i] = nBytes;
        }
    }
    else
    {
        //
        // Evenly divided among the processes, give them all the
        // same scatter_size.
        //
        fractionalIndex = comm_size;
    }

    //
    // Keep this loop separate so the compiler can convert it
    // into 'rep stosd' for performance
    //
    for (i = 0; i < fractionalIndex; i++)
    {
        recvcnts[i] = scatter_size;
    }

    //
    // Keep this as an increment so the compiler can recognize
    // adding instead of multiplying
    //
    for (i = 0; i < fractionalIndex; i++)
    {
        displs[i] = scatter_size * i;
    }

    right = rank + 1;
    left = rank - 1;
    if (left < 0)
    {
        left += comm_size;
    }
    else if (right == comm_size)
    {
        right = 0;
    }

    ringIndex = rank - root;
    if (ringIndex == 0)
    {
        nextRingIndex = comm_size - 1;
    }
    else
    {
        if (ringIndex < 0)
        {
            ringIndex += comm_size;
        }
        nextRingIndex = ringIndex - 1;
    }

    for (i=1; i<comm_size; i++)
    {
        mpi_errno = MPIC_Sendrecv(
            buffer + displs[ringIndex],
            recvcnts[ringIndex],
            g_hBuiltinTypes.MPI_Byte,
            right,
            MPIR_BCAST_TAG,
            buffer + displs[nextRingIndex],
            recvcnts[nextRingIndex],
            g_hBuiltinTypes.MPI_Byte,
            left,
            MPIR_BCAST_TAG,
            comm_ptr,
            MPI_STATUS_IGNORE
            );
        ON_ERROR_FAIL(mpi_errno);

        ringIndex = nextRingIndex;
        --nextRingIndex;
        if (nextRingIndex < 0)
        {
            nextRingIndex += comm_size;
        }
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}

/* This is the default implementation of broadcast. The algorithm is:

   Algorithm: MPI_Bcast

   For short messages, we use a binomial tree algorithm.
   Cost = lgp.alpha + n.lgp.beta

   For long messages, we do a scatter followed by an allgather.
   We first scatter the buffer using a binomial tree algorithm. This costs
   lgp.alpha + n.((p-1)/p).beta
   If the datatype is contiguous and the communicator is homogeneous,
   we treat the data as bytes and divide (scatter) it among processes
   by using ceiling division. For the noncontiguous or heterogeneous
   cases, we first pack the data into a temporary buffer by using
   MPI_Pack, scatter it as bytes, and unpack it after the allgather.

   For the allgather, we use a recursive doubling algorithm for
   medium-size messages and power-of-two number of processes. This
   takes lgp steps. In each step pairs of processes exchange all the
   data they have (we take care of non-power-of-two situations). This
   costs approximately lgp.alpha + n.((p-1)/p).beta. (Approximately
   because it may be slightly more in the non-power-of-two case, but
   it's still a logarithmic algorithm.) Therefore, for long messages
   Total Cost = 2.lgp.alpha + 2.n.((p-1)/p).beta

   Note that this algorithm has twice the latency as the tree algorithm
   we use for short messages, but requires lower bandwidth: 2.n.beta
   versus n.lgp.beta. Therefore, for long messages and when lgp > 2,
   this algorithm will perform better.

   For long messages and for medium-size messages and non-power-of-two
   processes, we use a ring algorithm for the allgather, which
   takes p-1 steps, because it performs better than recursive doubling.
   Total Cost = (lgp+p-1).alpha + 2.n.((p-1)/p).beta

   Possible improvements:
   For clusters of SMPs, we may want to do something differently to
   take advantage of shared memory on each node.

   End Algorithm: MPI_Bcast
*/


/* The block size for the chunked bcast (used only in NUMA intra-socket case).
     is currently hardcoded and was chosen based on experiments on NHM and SND hardware
     as a value that yields best performance over a wide range of message sizes while
     still beating the (non-chunked) algorithm for the intra-socket case.

     There's some more headroom for improvements if this value is picked by the auto-tuner,
     but that requires non-trivial changes.
*/
#define INTRASOCKET_BCAST_CHUNK_SIZE (256 * 1024)

_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
MPIR_Bcast_impl(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
    BYTE *tmp_buf,
    _In_range_(>=, 0) MPIDI_msg_sz_t nbytes,
    _In_range_(0, comm_ptr->remote_size - 1) int root,
    _In_ const MPID_Comm *comm_ptr
    )
{
    int rank, comm_size;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    comm_size = comm_ptr->remote_size;
    rank = comm_ptr->rank;

    if (comm_ptr->IsIntraSocketComm())
    {
        /*
          For an intra-socket comm, chunked binomial tree cast works better
          because it reduces cache evictions, and repetitive fetching of the source buffer
          from main memory.
        */

        MPIDI_msg_sz_t send_size = INTRASOCKET_BCAST_CHUNK_SIZE;
        MPIDI_msg_sz_t send_offset = 0;
        do
        {
            if (nbytes - send_offset < INTRASOCKET_BCAST_CHUNK_SIZE)
            {
                send_size = nbytes - send_offset;
            }

            mpi_errno = MPIR_Bcast_binomial(tmp_buf + send_offset, send_size, root, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);

            send_offset += send_size;
        } while (send_offset < nbytes);
    }
    else if((nbytes < comm_ptr->SwitchPoints()->MPIR_bcast_short_msg) ||
        (static_cast<unsigned int>(comm_size) < comm_ptr->SwitchPoints()->MPIR_bcast_min_procs))
    {
        /* Use short message algorithm, namely, binomial tree */
        mpi_errno = MPIR_Bcast_binomial(tmp_buf, nbytes, root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* use long message algorithm: binomial tree scatter followed by an allgather */
        mpi_errno = scatter_for_bcast(tmp_buf, nbytes, root, comm_ptr);
        ON_ERROR_FAIL(mpi_errno);

        /* Scatter complete. Now do an allgather .  */
        if ((static_cast<int>(nbytes) < comm_ptr->SwitchPoints()->MPIR_bcast_long_msg) &&
            IsPowerOf2( comm_size ))
        {
            /* medium size allgather and pof2 comm_size. use recursive doubling. */
            mpi_errno = doubling_allgather_for_bcast(tmp_buf, nbytes, root, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);
        }
        else
        {
            //
            // OACR forgets the restrictions annotated on root, so remind it.
            //
            _Analysis_assume_( root < comm_ptr->remote_size );
            /* long-message allgather or medium-size but non-power-of-two. use ring algorithm. */
            mpi_errno = ring_allgather_for_bcast(tmp_buf, nbytes, root, comm_ptr);
            ON_ERROR_FAIL(mpi_errno);
        }
    }
fn_fail:
    return mpi_errno;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Bcast_intra_flat(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
             BYTE*             buffer,
    _In_range_(>=, 0)
             MPIDI_msg_sz_t    nbytes,
    _In_range_(0, comm_ptr->remote_size - 1)
             int               root,
    _In_     const MPID_Comm*  comm_ptr
    )
{
    if (nbytes == 0) return MPI_SUCCESS;

    if (comm_ptr->remote_size == 1) return MPI_SUCCESS;

    return MPIR_Bcast_impl(buffer, nbytes, root, comm_ptr);
}


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Bcast_inter(
    _When_(root == MPI_ROOT, _In_opt_)
    _When_(root >= 0, _Out_opt_)
            void               *buffer,
    _In_range_(>=, 0)
            int                count,
    _In_    TypeHandle         datatype,
    _mpi_coll_rank_(root)
            int                root,
    _In_    const MPID_Comm*   comm_ptr
    )
{
/*  Intercommunicator broadcast.
    Root sends to rank 0 in remote group. Remote group does local
    intracommunicator broadcast.
*/
    int rank;
    MPI_Status status;

    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(comm_ptr->inter.local_comm != NULL);

    if (root == MPI_PROC_NULL)
    {
        /* local processes other than root do nothing */
        return MPI_SUCCESS;
    }
    if (root == MPI_ROOT)
    {
        /* root sends to rank 0 on remote group and returns */
        return MPIC_Send(buffer, count, datatype, 0,
                         MPIR_BCAST_TAG, comm_ptr);
    }

    //
    // remote group. rank 0 on remote group receives from root
    //

    rank = comm_ptr->rank;

    if (rank == 0)
    {
        MPI_RESULT mpi_errno = MPIC_Recv(buffer, count, datatype, root,
                                             MPIR_BCAST_TAG, comm_ptr, &status);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    //
    // now do the usual broadcast on this intracommunicator
    // with rank 0 as root.
    //
    return MPIR_Bcast_intra(buffer, count, datatype, 0, comm_ptr->inter.local_comm);
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
static inline
MPI_RESULT
MPIR_Bcast_intra_HA(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
    BYTE *buffer,
    _In_ MPIDI_msg_sz_t nbytes,
    _In_range_(0, comm_ptr->remote_size - 1) int root,
    _In_ const MPID_Comm* comm_ptr
    )
{

    if (nbytes == 0) return MPI_SUCCESS;

    /* If there is only one process, return */
    if (comm_ptr->remote_size == 1) return MPI_SUCCESS;

    MPI_RESULT mpi_errno;
    if( comm_ptr->intra.leaders_subcomm != nullptr )
    {
        if( comm_ptr->intra.ha_mappings[root].isLocal == 1 && comm_ptr->rank != root )
        {
            //
            // We aren't the bcast root, but the root is local.
            //
            MPIU_Assert( comm_ptr->intra.local_subcomm != nullptr );
            MPIU_Assert( comm_ptr->intra.ha_mappings[root].isLocal == 1 );
            mpi_errno = MPIR_Bcast_intra_HA(
                buffer,
                nbytes,
                comm_ptr->intra.ha_mappings[root].rank,
                comm_ptr->intra.local_subcomm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MPIR_Bcast_impl(
                buffer,
                nbytes,
                comm_ptr->intra.leaders_subcomm->rank,
                comm_ptr->intra.leaders_subcomm
                );
        }
        else
        {
            //
            // The bcast root is a leader (potentially us, even).
            // Do the bcast across the leaders first, then across the local subcomm.
            //
            MPIU_Assert( comm_ptr->intra.ha_mappings[root].isLocal == 0 );
            mpi_errno = MPIR_Bcast_impl(
                buffer,
                nbytes,
                comm_ptr->intra.ha_mappings[root].rank,
                comm_ptr->intra.leaders_subcomm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            //
            // It's possible that we are a leader, but the only local process.
            //
            if( comm_ptr->intra.local_subcomm != nullptr )
            {
                MPIU_Assert( comm_ptr->intra.local_subcomm->rank == 0 );
                mpi_errno = MPIR_Bcast_intra_HA(
                    buffer,
                    nbytes,
                    0,
                    comm_ptr->intra.local_subcomm
                    );
            }
        }
    }
    else if( comm_ptr->intra.local_subcomm != nullptr )
    {
        //
        // We're aren't a leader, but at least we're not a leaf...
        //
        MPIU_Assert( comm_ptr->intra.ha_mappings[root].isLocal == 1 );
        mpi_errno = MPIR_Bcast_intra_HA(
            buffer,
            nbytes,
            comm_ptr->intra.ha_mappings[root].rank,
            comm_ptr->intra.local_subcomm
            );
    }
    else
    {
        //
        // We are a leaf (end of the HA hierarchy.)
        //
        mpi_errno = MPIR_Bcast_impl(
            buffer,
            nbytes,
            root,
            comm_ptr
            );
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Bcast_intra(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
             void              *buffer,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, comm_ptr->remote_size - 1)
             int               root,
    _In_     const MPID_Comm*  comm_ptr
    )
{
    StackGuardArray<BYTE> auto_buf;

    MPIU_Assert(comm_ptr->comm_kind != MPID_INTERCOMM);

    BYTE *tmp_buf;
    bool is_contig;
    MPI_Aint true_lb;
    MPIDI_msg_sz_t nbytes = fixme_cast<MPIDI_msg_sz_t>(
        datatype.GetSizeAndInfo( count, &is_contig, &true_lb )
        );

    /* MPI_Type_size() might not give the accurate size of the packed
    * datatype for heterogeneous systems (because of padding, encoding,
    * etc). On the other hand, MPI_Pack_size() can become very
    * expensive, depending on the implementation, especially for
    * heterogeneous systems. We want to use MPI_Type_size() wherever
    * possible, and MPI_Pack_size() in other places.
    */
    if (!is_contig)
    {
        auto_buf = new BYTE[nbytes];
        if( auto_buf == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
        tmp_buf = auto_buf;

        /* TODO: Pipeline the packing and communication */
        int position = 0;
        if (comm_ptr->rank == root)
        {
            NMPI_Pack(
                buffer,
                count,
                datatype.GetMpiHandle(),
                tmp_buf,
                nbytes,
                &position,
                comm_ptr->handle
                );
            nbytes = position;
        }
    }
    else
    {
        tmp_buf = static_cast<BYTE*>(buffer) + true_lb;
    }

    int mpi_errno;

    if (Mpi.SwitchoverSettings.SmpBcastEnabled &&
        comm_ptr->IsHaAware() &&
        nbytes >= comm_ptr->SwitchPoints()->MPIR_bcast_smp_threshold &&
        nbytes < comm_ptr->SwitchPoints()->MPIR_bcast_smp_ceiling)
    {
        mpi_errno =  MPIR_Bcast_intra_HA(
            tmp_buf,
            nbytes,
            root,
            comm_ptr
            );
    }
    else
    {
        mpi_errno = MPIR_Bcast_intra_flat(
            tmp_buf,
            nbytes,
            root,
            comm_ptr
            );
    }

    if (mpi_errno == MPI_SUCCESS && !is_contig)
    {
        if (comm_ptr->rank != root)
        {
            int position = 0;
            NMPI_Unpack(tmp_buf, nbytes, &position, buffer, count,
                        datatype.GetMpiHandle(), comm_ptr->handle);
        }
    }

    return mpi_errno;
}


static
MPI_RESULT
IbcastBuildBinomialScatterTaskList(
    _When_(relativeRank == 0, _In_opt_)
    _When_(relativeRank != 0, _Out_opt_)
        BYTE *pBuffer,
    _In_range_(>=, 0) size_t cbBuffer,
    _In_range_(<, pComm->remote_size) unsigned relativeRank,
    _In_ MPID_Comm *pComm,
    _In_ unsigned nChildren,
    _Out_ NbcTask tasks[],
    _Inout_ ULONG* piNextFreeTask,
    _Inout_ ULONG** ppiNext
    )
{
    int commSize = pComm->remote_size;
    size_t recvSize = cbBuffer;
    size_t scatterSize = cbBuffer / commSize;
    if( cbBuffer % commSize )
    {
        scatterSize++;
    }

    MPI_RESULT mpi_errno = BinomialParentBuilder(
        [&]( _In_range_( >=, 0 ) int targetRank )
        {
            ULONG i = *piNextFreeTask;
            BYTE* pRecv = pBuffer;
            size_t offset = relativeRank * scatterSize;

            //
            // Now calculate the exact receive size.
            //
            size_t theoreticalSize = TreeSize( relativeRank, commSize ) * scatterSize;
            if( cbBuffer < offset )
            {
                recvSize = 0;
            }
            else
            {
                pRecv += offset;
                recvSize = cbBuffer - offset;
                if( recvSize > theoreticalSize )
                {
                    recvSize = theoreticalSize;
                }
            }

            mpi_errno = tasks[i].InitIrecv(
                pRecv,
                recvSize,
                g_hBuiltinTypes.MPI_Byte,
                targetRank
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                //
                // We don't need to update piNextFreeTask since we didn't
                // use any entries.
                //
                return mpi_errno;
            }

            //
            // Next task will get chained to our completion.
            //
            **ppiNext = i;
            *ppiNext = &tasks[i].m_iNextOnComplete;

            //
            // Note that the order here is important, as the contig case
            // would come in with piNext == &tasks[0].iNextOnInit, so the
            // assignment above needs to happen before we clear it below.
            //
            tasks[i].m_iNextOnInit = NBC_TASK_NONE;

            *piNextFreeTask = ++i;
            return MPI_SUCCESS;
        },
        pComm->rank,
        pComm->remote_size,
        relativeRank
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    mpi_errno = BinomialChildBuilderDescending(
        [&](
            _In_range_( >=, 1 ) unsigned offset
            )
        {
            ULONG i = *piNextFreeTask;
            const BYTE* pSend = pBuffer;
            size_t sendSize;

            int targetRank = pComm->rank + offset;
            if( targetRank >= commSize )
            {
                targetRank -= commSize;
            }

            //
            // Now calculate the exact send size.
            //
            size_t theoreticalSize = TreeSize( relativeRank + offset, commSize ) * scatterSize;
            if( recvSize < scatterSize * offset )
            {
                sendSize = 0;
            }
            else
            {
                pSend += ( relativeRank + offset ) * scatterSize;
                sendSize = recvSize - scatterSize * offset;
                if( sendSize > theoreticalSize )
                {
                    sendSize = theoreticalSize;
                }
            }

            //
            // TODO: it would be nice if we knew the tag to use here.
            //
            mpi_errno = tasks[i].InitIsend(
                pSend,
                sendSize,
                g_hBuiltinTypes.MPI_Byte,
                targetRank,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                *piNextFreeTask = i;
                return mpi_errno;
            }

            //
            // Next task can start as soon as we're started.
            //
            **ppiNext = i;
            *ppiNext = &tasks[i].m_iNextOnInit;
            tasks[i].m_iNextOnComplete = NBC_TASK_NONE;

            *piNextFreeTask = ++i;
            return MPI_SUCCESS;
        },
        nChildren
        );

    return mpi_errno;
}


/*

root, !contig:
    PACK -[init]-> NONE
      \---[complete]-> SEND -[init]-> SEND -[init]-> NONE
                         |              \---[complete]-> NONE
                         \---[complete]-> NONE

root, contig:
      SEND -[init]-> SEND -[init]-> NONE
        |              \---[complete]-> NONE
        \---[complete]-> NONE

non-root, !contig:
    RECV -[init]-> NONE
      \---[complete]-> SEND -[init]-> SEND -[init]-> UNPACK -[init]-> NONE
                         |              |               \---[complete]-> NONE
                         |              \---[complete]-> NONE
                         \---[complete]-> NONE

non-root, contig:
    RECV -[init]-> NONE
      \---[complete]-> SEND -[init]-> SEND -[init]-> NONE
                         |              \---[complete]-> NONE
                         \---[complete]-> NONE

*/
static
MPI_RESULT
IbcastBuildBinomialTaskList(
    _When_(relativeRank == 0, _In_opt_)
    _When_(relativeRank != 0, _Out_opt_)
        BYTE *pBuffer,
    _In_range_(>=, 0) size_t cbBuffer,
    _In_range_(<, pComm->remote_size) unsigned relativeRank,
    _In_ MPID_Comm *pComm,
    _In_ unsigned nChildren,
    _Out_ NbcTask tasks[],
    _Inout_ ULONG* piNextFreeTask,
    _Inout_ ULONG** ppiNext
    )
{
    //
    // Small message algorithm:
    // This uses a fairly basic recursive subdivision algorithm.
    // The root sends to the process comm_size/2 away; the receiver becomes
    // a root for a subtree and applies the same process.
    //
    // So that the new root can easily identify the size of its
    // subtree, the (subtree) roots are all powers of two (relative
    // to the root) If m = the first power of 2 such that 2^m >= the
    // size of the communicator, then the subtree at root at 2^(m-k)
    // has size 2^k (with special handling for subtrees that aren't
    // a power of two in size).
    //
    // Do subdivision.  There are two phases:
    // 1. Wait for arrival of data.  Because of the power of two nature
    // of the subtree roots, the source of this message is always the
    // process whose relative rank has the least significant 1 bit CLEARED.
    // That is, process 4 (100) receives from process 0, process 7 (111)
    // from process 6 (110), etc.
    // 2. Forward to my subtree
    //
    // Note that the process that is the tree root is handled automatically
    // by this code, since it has no bits set.
    //
    MPI_RESULT mpi_errno = BinomialParentBuilder(
        [&]( _In_range_( >= , 0 ) int targetRank )
        {
            ULONG i = *piNextFreeTask;

            mpi_errno = tasks[i].InitIrecv(
                pBuffer,
                cbBuffer,
                g_hBuiltinTypes.MPI_Byte,
                targetRank
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                //
                // We don't need to update piNextFreeTask since we didn't
                // use any entries.
                //
                return mpi_errno;
            }

            //
            // Next task will get chained to our completion.
            //
            **ppiNext = i;
            *ppiNext = &tasks[i].m_iNextOnComplete;

            //
            // Note that the order here is important, as the contig case
            // would come in with piNext == &tasks[0].iNextOnInit, so the
            // assignment above needs to happen before we clear it below.
            //
            tasks[i].m_iNextOnInit = NBC_TASK_NONE;

            *piNextFreeTask += 1;
            return MPI_SUCCESS;
        },
        pComm->rank,
        pComm->remote_size,
        relativeRank
        );

    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = BinomialChildBuilderDescending(
            [&](
                _In_range_( >=, 1 ) unsigned offset
                )
            {
                int targetRank = pComm->rank + offset;
                if( targetRank >= pComm->remote_size )
                {
                    targetRank -= pComm->remote_size;
                }

                ULONG i = *piNextFreeTask;

                //
                // TODO: it would be nice if we knew the tag to use here.
                //
                mpi_errno = tasks[i].InitIsend(
                    pBuffer,
                    cbBuffer,
                    g_hBuiltinTypes.MPI_Byte,
                    targetRank,
                    pComm
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }

                //
                // Next task can start as soon as we're started.
                //
                **ppiNext = i;
                *ppiNext = &tasks[i].m_iNextOnInit;
                tasks[i].m_iNextOnComplete = NBC_TASK_NONE;

                *piNextFreeTask = ++i;
                return MPI_SUCCESS;
            },
            nChildren
            );
    }
    return mpi_errno;
}


/*

2 chunks.

root, !contig:
    PACK -[init]-> NONE
      \---[complete]-> SEND1 -[init]-> SEND1 -[init]-> SEND2 -[init]-> SEND2 -[init]-> NONE
                         |               |                |              \---[complete]-> NONE
                         |               |                \---[complete]-> NONE
                         |               \---[complete]-> NONE
                         \---[complete]-> NONE

root, contig:
    SEND1 -[init]-> SEND1 -[init]-> SEND2 -[init]-> SEND2 -[init]-> NONE
      |               |                |              \---[complete]-> NONE
      |               |                \---[complete]-> NONE
      |               \---[complete]-> NONE
      \---[complete]-> NONE


non-root, !contig:
    RECV1 -[init]-> RECV2 -[init]-> NONE
      |               \---[complete]-> SEND2 -[init]-> SEND2 -[init]-> UNPACK -[init]-> NONE
      |                                  |              |               \---[complete]-> NONE
      |                                  |              \---[complete]-> NONE
      |                                  \---[complete]-> NONE
      \---[complete]-> SEND1 -[init]-> SEND1 -[init]-> NONE
                         |              \---[complete]-> NONE
                         \---[complete]-> NONE

non-root, contig:
    RECV1 -[init]-> RECV2 -[init]-> NONE
      |               \---[complete]-> SEND2 -[init]-> SEND2 -[init]-> NONE
      |                                  |              |
      |                                  |              \---[complete]-> NONE
      |                                  \---[complete]-> NONE
      \---[complete]-> SEND1 -[init]-> SEND1 -[init]-> NONE
                         |              \---[complete]-> NONE
                         \---[complete]-> NONE

*/
static
MPI_RESULT
IbcastBuildIntraSocketTaskList(
    _When_(relativeRank == 0, _In_opt_)
    _When_(relativeRank != 0, _Out_opt_)
        BYTE *pBuffer,
    _In_range_(>=, 0) size_t cbBuffer,
    _In_range_(<, pComm->remote_size) unsigned relativeRank,
    _In_ MPID_Comm *pComm,
    _In_ unsigned nChildren,
    _Out_ NbcTask tasks[],
    _Inout_ ULONG* piNextFreeTask,
    _Inout_ ULONG** ppiNext
    )
{
    size_t cbChunk = INTRASOCKET_BCAST_CHUNK_SIZE;
    MPI_RESULT mpi_errno;
    for( ;; )
    {
        ULONG iLastChunkStart = *piNextFreeTask;
        if( cbChunk > cbBuffer )
        {
            cbChunk = cbBuffer;
        }

        mpi_errno = IbcastBuildBinomialTaskList(
            pBuffer,
            cbChunk,
            relativeRank,
            pComm,
            nChildren,
            tasks,
            piNextFreeTask,
            ppiNext
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        pBuffer += cbChunk;
        cbBuffer -= cbChunk;

        if( cbBuffer == 0 )
        {
            break;
        }

        if( tasks[iLastChunkStart].m_iNextOnInit == NBC_TASK_NONE )
        {
            //
            // Allow the next chunk to be initiated in parallel.
            // This specific logic handles the non-root cases that
            // begin with a receive.
            //
            // First terminate the current chain and then set things
            // up to chain after the receive of the current chunk.
            //
            **ppiNext = NBC_TASK_NONE;
            *ppiNext = &tasks[iLastChunkStart].m_iNextOnInit;
        }
    }
    return MPI_SUCCESS;
}


static
MPI_RESULT
IbcastBuildRingAllgatherTaskList(
    _When_(root == pComm->rank, _In_opt_)
    _When_(root != pComm->rank, _Out_opt_)
        BYTE *pBuffer,
    _In_range_(>=, 0) size_t cbBuffer,
    _In_range_(<, pComm->remote_size) unsigned root,
    _In_ MPID_Comm *pComm,
    _Out_ NbcTask tasks[],
    _Inout_ ULONG* piNextFreeTask,
    _Inout_ ULONG** ppiNext
    )
{
    size_t i;
    size_t fractionalIndex;
    int rank = pComm->rank;
    unsigned commSize = pComm->remote_size;
    ULONG iCurrTask = *piNextFreeTask;
    int ringIndex;
    int nextRingIndex;

    StackGuardArray<size_t> recvcnts = new size_t[commSize * 2];
    if( recvcnts == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    size_t* displs = recvcnts + commSize;

    size_t scatterSize = cbBuffer / commSize;
    if( cbBuffer % commSize )
    {
        //
        // cbBuffer does not evenly divide into the current number of
        // processes. Increase the scatterSize of the receiving blocks
        // and determine the number of processes needed using that size,
        // including a fragment at the end. All processes not in use should
        // be set to 0.
        //
        scatterSize++;
        fractionalIndex = cbBuffer / scatterSize;

        recvcnts[fractionalIndex] = cbBuffer % scatterSize;
        displs[fractionalIndex] = fractionalIndex * scatterSize;
        for( i = fractionalIndex + 1; i < commSize; i++ )
        {
            recvcnts[i] = 0;
            displs[i] = cbBuffer;
        }
    }
    else
    {
        //
        // Evenly divided among the processes, give them all the
        // same scatterSize.
        //
        fractionalIndex = commSize;
    }

    //
    // Keep this loop separate so the compiler can convert it
    // into 'rep stosd' for performance.
    //
    // Assembly:
    //    ; 418  :     for (i = 0; i < fractionalIndex; i++)
    //
    //      000fe 85 ed        test    ebp, ebp
    //      00100 7e 0b        jle     SHORT $LN16@ring_allga
    //
    //    ; 419  :     {
    //    ; 420  :         recvcnts[i] = scatter_size;
    //
    //      00102 48 63 cd     movsxd  rcx, ebp
    //      00105 49 63 c6     movsxd  rax, r14d
    //      00108 49 8b ff     mov     rdi, r15
    //      0010b f3 ab        rep stosd
    //    $LN16@ring_allga:
    //
    //    ; 421  :     }
    //
    for( i = 0; i < fractionalIndex; i++ )
    {
        recvcnts[i] = scatterSize;
    }

    //
    // Keep this as an increment so the compiler can recognize
    // adding instead of multiplying.
    //
    // Assembly:
    //    ; 427  :     for (i = 0; i < fractionalIndex; i++)
    //
    //      0010d 49 8b c5     mov     rax, r13
    //      00110 48 63 d5     movsxd  rdx, ebp
    //      00113 85 ed        test    ebp, ebp
    //      00115 7e 12        jle     SHORT $LN13@ring_allga
    //      00117 41 8b cd     mov     ecx, r13d
    //    $LL71@ring_allga:
    //
    //    ; 428  :     {
    //    ; 429  :         displs[i] = scatter_size * i;
    //
    //      0011a 41 89 0c 80  mov     DWORD PTR [r8+rax*4], ecx
    //      0011e 48 ff c0     inc     rax
    //      00121 41 03 ce     add     ecx, r14d
    //      00124 48 3b c2     cmp     rax, rdx
    //      00127 7c f1        jl  SHORT $LL71@ring_allga
    //    $LN13@ring_allga:
    //
    //    ; 430  :     }
    //
    for( i = 0; i < fractionalIndex; i++ )
    {
        displs[i] = scatterSize * i;
    }

    int left = rank - 1;
    int right = rank + 1;
    if( left < 0 )
    {
        left += commSize;
    }
    else if( right - commSize == 0 )
    {
        right = 0;
    }

    int relativeRank = rank - root;
    if( relativeRank < 0 )
    {
        relativeRank += commSize;
    }
    int rightRelativeRank = relativeRank + 1;
    if( rightRelativeRank - commSize == 0 )
    {
        rightRelativeRank = 0;
    }

    int treeSize = TreeSize( relativeRank, commSize );
    int rightTreeSize = TreeSize ( rightRelativeRank, commSize );

    ringIndex = relativeRank;
    if( ringIndex == 0 )
    {
        nextRingIndex = commSize - 1;
    }
    else
    {
        nextRingIndex = ringIndex - 1;
    }

    MPI_RESULT mpi_errno = MPI_SUCCESS;
    for( i = 1; i < commSize; i++ )
    {
        if( ringIndex < rightRelativeRank ||
            ringIndex >= rightRelativeRank + rightTreeSize )
        {
            //
            // Right rank doesn't have this data chunk, send it.
            //
            mpi_errno = tasks[iCurrTask].InitIsend(
                pBuffer + displs[ringIndex],
                recvcnts[ringIndex],
                g_hBuiltinTypes.MPI_Byte,
                right,
                pComm
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                //
                // We need to update the number of tasks used to account for any entries
                // we successfully initialized prior to this failure.
                //
                break;
            }
        }
        else
        {
            //
            // Right rank already has this data chunk, skip it.
            //
            tasks[iCurrTask].InitNoOp();
        }

        **ppiNext = iCurrTask;
        if( ringIndex != relativeRank )
        {
            tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
        }
        else
        {
            //
            // The very first send, m_iNextOnInit points to the next receive.
            //
            tasks[iCurrTask].m_iNextOnInit = iCurrTask + 1;
        }
        tasks[iCurrTask].m_iNextOnComplete = NBC_TASK_NONE;
        iCurrTask++;

        if( nextRingIndex < relativeRank ||
            nextRingIndex >= relativeRank + treeSize )
        {
            //
            // The current rank doesn't have this data chunk, receive it.
            //
            mpi_errno = tasks[iCurrTask].InitIrecv(
                pBuffer + displs[nextRingIndex],
                recvcnts[nextRingIndex],
                g_hBuiltinTypes.MPI_Byte,
                left
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                //
                // We need to update the number of tasks used to account for any entries
                // we successfully initialized prior to this failure.
                //
                break;
            }
        }
        else
        {
            //
            // The current rank already has this data chunk, skip it.
            //
            tasks[iCurrTask].InitNoOp();
        }

        *ppiNext = &tasks[iCurrTask].m_iNextOnComplete;
        if( i < commSize - 1 )
        {
            //
            // Not the last receive, initiate the next receive in parallel.
            //
            tasks[iCurrTask].m_iNextOnInit = iCurrTask + 2;
        }
        else
        {
            tasks[iCurrTask].m_iNextOnInit = NBC_TASK_NONE;
        }
        iCurrTask++;

        ringIndex = nextRingIndex;
        --nextRingIndex;
        if( nextRingIndex < 0 )
        {
            nextRingIndex += commSize;
        }
    }

    MPIU_Assert( *piNextFreeTask < iCurrTask );
    *piNextFreeTask = iCurrTask;

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
static
MPI_RESULT
IbcastBuildTaskList(
    _Inout_ MPID_Request* pReq,
    _When_(root == pComm->rank, _In_opt_)
    _When_(root != pComm->rank, _Out_opt_)
        void *buffer,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_range_(<, pComm->remote_size) unsigned root,
    _In_ MPID_Comm *pComm
    )
{
    int commSize = pComm->remote_size;
    int relativeRank = pComm->rank - root;
    if( relativeRank < 0 )
    {
        relativeRank += commSize;
    }

    //
    // Calculate the size of the task array up front.
    //
    unsigned nChildren = ChildCount( relativeRank, commSize );
    ULONG nTasks = nChildren;
    if( relativeRank != 0 )
    {
        //
        // Non-root processes must receive the data from their parent.
        //
        nTasks += 1;
    }

    //
    // Query type information to allow allocating a temporary buffer
    // if the user's datatype is not contiguous.
    //
    bool isContig;
    MPI_Aint trueLb;
    size_t cbBuffer;
    cbBuffer = datatype.GetSizeAndInfo( count, &isContig, &trueLb );

    //
    // Account for intra-socket chunking.
    //
    if( pComm->IsIntraSocketComm() && cbBuffer > INTRASOCKET_BCAST_CHUNK_SIZE )
    {
        nTasks *= static_cast<ULONG>(
            (cbBuffer + INTRASOCKET_BCAST_CHUNK_SIZE - 1) / INTRASOCKET_BCAST_CHUNK_SIZE
            );
    }
    else if( cbBuffer >= pComm->SwitchPoints()->MPIR_bcast_short_msg &&
        static_cast<unsigned>( commSize ) >= pComm->SwitchPoints()->MPIR_bcast_min_procs )
    {
        //
        // Calculate number of tasks needed for the ring algorithm.
        //
        nTasks += ( commSize - 1 ) * 2;
    }

    BYTE* tmpBuf;
    if( !isContig )
    {
        pReq->nbc.tmpBuf = new BYTE[cbBuffer];
        if( pReq->nbc.tmpBuf == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
        tmpBuf = pReq->nbc.tmpBuf;

        //
        // We will need to pack/unpack the data.
        //
        nTasks++;
    }
    else
    {
        pReq->nbc.tmpBuf = nullptr;
        tmpBuf = static_cast<BYTE*>( buffer ) + trueLb;
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

    MPI_RESULT mpi_errno;
    ULONG* piNext;
    if( !isContig && relativeRank == 0 )
    {
        mpi_errno = pReq->nbc.tasks[0].InitPack(
            tmpBuf,
            buffer,
            count,
            datatype
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        //
        // Every other task needs to wait until we're done packing before sending.
        //
        pReq->nbc.tasks[0].m_iNextOnInit = NBC_TASK_NONE;
        piNext = &pReq->nbc.tasks[0].m_iNextOnComplete;
        pReq->nbc.nTasks++;
    }
    else
    {
        piNext = &pReq->nbc.tasks[0].m_iNextOnInit;
    }

    if( pComm->IsIntraSocketComm() )
    {
        mpi_errno = IbcastBuildIntraSocketTaskList(
            tmpBuf,
            cbBuffer,
            relativeRank,
            pComm,
            nChildren,
            pReq->nbc.tasks,
            &pReq->nbc.nTasks,
            &piNext
            );
    }
    else if( cbBuffer < pComm->SwitchPoints()->MPIR_bcast_short_msg ||
        static_cast<unsigned>( commSize ) < pComm->SwitchPoints()->MPIR_bcast_min_procs )
    {
        //
        // Use short message algorithm - binomial tree.
        //
        mpi_errno = IbcastBuildBinomialTaskList(
            tmpBuf,
            cbBuffer,
            relativeRank,
            pComm,
            nChildren,
            pReq->nbc.tasks,
            &pReq->nbc.nTasks,
            &piNext
            );
    }
    else
    {
        //
        // Use long message algorithm - binomial tree scatter followed
        // by a ring allgather.
        //
        mpi_errno = IbcastBuildBinomialScatterTaskList(
            tmpBuf,
            cbBuffer,
            relativeRank,
            pComm,
            nChildren,
            pReq->nbc.tasks,
            &pReq->nbc.nTasks,
            &piNext
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        //
        // Scatter is complete, now do a ring allgather.
        //
        mpi_errno = IbcastBuildRingAllgatherTaskList(
            tmpBuf,
            cbBuffer,
            root,
            pComm,
            pReq->nbc.tasks,
            &pReq->nbc.nTasks,
            &piNext
            );
    }
    MPIU_Assert( pReq->nbc.nTasks <= nTasks );

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( !isContig && relativeRank != 0 )
    {
        MPIU_Assert( pReq->nbc.nTasks == nTasks - 1 );
        mpi_errno = pReq->nbc.tasks[pReq->nbc.nTasks].InitUnpack(
            tmpBuf,
            buffer,
            count,
            datatype
            );

        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        *piNext = pReq->nbc.nTasks;
        piNext = &pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnComplete;
        pReq->nbc.tasks[pReq->nbc.nTasks].m_iNextOnInit = NBC_TASK_NONE;
        pReq->nbc.nTasks++;
    }

    //
    // Terminate the chain.
    //
    *piNext = NBC_TASK_NONE;

    pReq->nbc.tag = pReq->comm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_BCAST_TAG ) );

    return mpi_errno;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IbcastBuildIntra(
    _When_(root == pComm->rank, _In_opt_)
    _When_(root != pComm->rank, _Out_opt_)
             void              *buffer,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, pComm->remote_size - 1)
             int               root,
    _In_     MPID_Comm*        pComm,
    _Outptr_
             MPID_Request**    ppRequest
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
    }
    else
    {
        MPI_RESULT mpi_errno = IbcastBuildTaskList(
            pRequest.get(),
            buffer,
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
    }

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibcast_intra(
    _When_(root == pComm->rank, _In_opt_)
    _When_(root != pComm->rank, _Out_opt_)
             void              *buffer,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, pComm->remote_size - 1)
             int               root,
    _In_     MPID_Comm*        pComm,
    _Outptr_
             MPID_Request**    ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IbcastBuildIntra( buffer, count, datatype, root, pComm, &pRequest );
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
IbcastBuildInter(
    _When_(root == MPI_ROOT, _In_opt_)
    _When_(root >= 0, _Out_opt_)
            void               *buffer,
    _In_range_(>=, 0)
            int                count,
    _In_    TypeHandle         datatype,
    _mpi_coll_rank_(root)
            int                root,
    _In_    MPID_Comm*         pComm,
    _Outptr_
            MPID_Request**     ppRequest
    )
{
    //
    //  Intercommunicator non-blocking broadcast.
    //  Root sends to rank 0 in remote group. Remote group does local
    //  intracommunicator broadcast.
    //
    MPIU_Assert(pComm->comm_kind == MPID_INTERCOMM);
    MPIU_Assert(pComm->inter.local_comm != nullptr);

    //
    // All processes must get the tag so that we remain in sync.
    //
    unsigned tag = pComm->GetNextNBCTag( MPIR_GET_ASYNC_TAG( MPIR_BCAST_TAG ) );

    if( count > 0 )
    {
        if( root == MPI_ROOT )
        {
            //
            // Root sends to rank 0 on remote group and returns.
            //
            MPID_Request* sreq = MPIDI_Request_create_sreq(
                MPIDI_REQUEST_TYPE_SEND,
                buffer,
                count,
                datatype,
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

        if( root >= 0 && pComm->inter.local_comm->rank != 0 )
        {
            //
            // we're a leaf in the non-root group - just a simple ibcast for us amongst
            // our local group.
            //
            return IbcastBuildIntra(
                buffer,
                count,
                datatype,
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
    // Rank 0 on non-root group receives from root.
    //
    MPI_RESULT mpi_errno = pRequest->nbc.tasks[0].InitIrecv(
        buffer,
        count,
        datatype,
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
    // Rank 0 on non-root group broadcasts data amongst its local group.
    //
    mpi_errno = pRequest->nbc.tasks[1].InitIbcastIntra(
        buffer,
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

    pRequest->nbc.tasks[1].m_iNextOnInit = NBC_TASK_NONE;
    pRequest->nbc.tasks[1].m_iNextOnComplete = NBC_TASK_NONE;
    pRequest->nbc.nTasks++;

    *ppRequest = pRequest.detach();
    return MPI_SUCCESS;
}


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibcast_inter(
    _When_(root == MPI_ROOT, _In_opt_)
    _When_(root >= 0, _Out_opt_)
            void               *buffer,
    _In_range_(>=, 0)
            int                count,
    _In_    TypeHandle         datatype,
    _mpi_coll_rank_(root)
            int                root,
    _In_    MPID_Comm*         pComm,
    _Outptr_
            MPID_Request**     ppRequest
    )
{
    MPID_Request* pRequest;
    MPI_RESULT mpi_errno = IbcastBuildInter(
        buffer,
        count,
        datatype,
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
