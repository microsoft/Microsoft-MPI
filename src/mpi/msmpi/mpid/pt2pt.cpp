// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* MPIDI_COPY_BUFFER_SZ is the size of the buffer that is allocated when
   copying from one non-contiguous buffer to another.  In this case, an
   intermediate contiguous buffer is used of this size */
#if !defined(MPIDI_COPY_BUFFER_SZ)
#define MPIDI_COPY_BUFFER_SZ 16384
#endif


/*
 * Description of the Bsend data structures.
 *
 * Bsend is buffered send; a buffer, provided by the user, is used to store
 * both the user's message and information that my be needed to send that
 * message.  In addition, space within that buffer must be allocated, so
 * additional information is required to manage that space allocation.
 * In the following, the term "segment" denotes a fragment of the user buffer
 * that has been allocated either to free (unused) space or to a particular
 * user message.
 *
 * The following datastructures are used:
 *
 *  BsendData_t - Describes a segment of the user buffer.  This data structure
 *                contains a BsendMsg_t for segments that contain a user
 *                message.  Each BsendData_t segment belongs to one of
 *                two lists: avail (unused and free) and active (currently
 *                sending).
 *  BsendBuffer - This global structure contains pointers to the user buffer
 *                and the three lists, along with the size of the user buffer.
 *
 * Miscellaneous comments
 * By storing total_size along with "size available for messages", we avoid
 * any complexities associated with alignment, since we must ensure that each
 * BsendData_t structure is properly aligned (i.e., we can't simply
 * do (sizeof(BsendData_t) + size) to get total_size).
 *
 * Function Summary
 *   MPIR_Bsend_attach - Performs the work of MPI_Buffer_attach
 *   MPIR_Bsend_detach - Performs the work of MPI_Buffer_detach
 *   MPIR_Bsend_isend  - Essentially performs an MPI_Ibsend.  Returns
 *                an MPID_Request that is also stored internally in the
 *                corresponding BsendData_t entry
 *   MPIR_Bsend_free_segment - Free a buffer that is no longer needed,
 *                merging with adjacent segments
 *   MPIR_Bsend_check_active - Check for completion of any active sends
 *                for bsends (all bsends, both MPI_Ibsend and MPI_Bsend,
 *                are internally converted into Isends on the data
 *                in the Bsend buffer)
 *   MPIR_Bsend_find_buffer - Find a buffer in the bsend buffer large
 *                enough for the message.  However, does not acquire that
 *                buffer (see MPIR_Bsend_take_buffer)
 *   MPIR_Bsend_take_buffer - Find and acquire a buffer for a message
 *   MPIR_Bsend_finalize - Finalize handler when Bsend routines are used
 */

/* Private structures for the bsend buffers */

/* BsendData describes a bsend request */
/*
   FIXME: Changes to BsendData_t size can also be made to MPI_BSEND_OVERHEAD
   in the public header files mpi.h, mpif.h, mpi.f90.

   Note that for backward compatability BsendData_t size cannot grow beyond the
   value in MPI_BSEND_OVERHEAD but only to shrink.
*/
struct BsendData_t
{

    struct BsendData_t* next;
    struct BsendData_t* prev;
    MPID_Request* request;
    int total_size;       /* total size of this segment, including all headers */

    //
    // The message buffer starts here.
    // Note that msgbuf is just a symbol and does not take struct space. However, the symbol
    // is pointer aligned and as such it imposes padding as neccessay.
    //
    void* msgbuf[];
};

//
// Make sure that BsendData_t has pointer alignment
//
C_ASSERT((sizeof(BsendData_t) % sizeof(void*)) == 0);
#define BSENDDATA_HEADER_TRUE_SIZE ((int)sizeof(BsendData_t))

/* BsendBuffer is the structure that describes the overall Bsend buffer */
/*
 * We use separate buffer and origbuffer because we may need to align
 * the buffer (we *could* always memcopy the header to an aligned region,
 * but it is simpler to just align it internally.  This does increase the
 * BSEND_OVERHEAD, but that is already relatively large.  We could instead
 * make sure that the initial header was set at an aligned location (
 * taking advantage of the "alignpad"), but this would require more changes.
 */

struct BsendBuffer_t
{

    void* buffer;           /* Pointer to the begining of the user-provided buffer */
    void* origbuffer;       /* Pointer to the buffer provided by the user */
    BsendData_t* avail;     /* Pointer to the first available block of space */
    BsendData_t* active;    /* Pointer to the first active (sending) message */
    int buffer_size;        /* Size of the user-provided buffer */
    int origbuffer_size;    /* Size of the buffer as provided by the user */
    int initialized;   /* keep track of the first call to any bsend routine */
};

static BsendBuffer_t BsendBuffer = { 0, 0, 0, 0, 0, 0, 0 };


MPI_RESULT MPIR_Bsend_detach(
    _Outptr_result_bytebuffer_(*size) void** bufferp,
    _Out_ int *size
    );


/* Ignore p */
static MPI_RESULT MPIR_Bsend_finalize( void* /*p*/ )
{
    void *b;
    int  s;

    if (BsendBuffer.buffer)
    {
        /* Use detach to complete any communication */
        MPIR_Bsend_detach( &b, &s );
    }
    return MPI_SUCCESS;
}


/*
 * Attach a buffer.  This checks for the error conditions and then
 * initialized the avail buffer.
 */

MPI_RESULT
MPIR_Bsend_attach(
    _In_reads_bytes_(buffer_size) void *buffer,
    _In_ int buffer_size
    )
{
    long offset;
    BsendData_t* p;

    if (BsendBuffer.buffer)
    {
        return MPIU_ERR_CREATE(MPI_ERR_BUFFER,  "**bufexists" );
    }
    if (buffer_size < MPI_BSEND_OVERHEAD)
    {
        /* MPI_ERR_OTHER is another valid choice for this error,
            but the Intel test wants MPI_ERR_BUFFER, and it seems
            to violate the principle of least surprise to not use
            MPI_ERR_BUFFER for errors with the Buffer */
        return MPIU_ERR_CREATE(MPI_ERR_BUFFER,  "**bsendbufsmall %d %d", buffer_size, MPI_BSEND_OVERHEAD );
    }

    if (!BsendBuffer.initialized)
    {
        BsendBuffer.initialized = 1;
        MPIR_Add_finalize( MPIR_Bsend_finalize, (void *)0, 10 );
    }

    BsendBuffer.origbuffer      = buffer;
    BsendBuffer.origbuffer_size = buffer_size;
    BsendBuffer.buffer          = buffer;
    BsendBuffer.buffer_size     = buffer_size;
    offset = (long)(((MPI_Aint)buffer) % sizeof(void *));
    if (offset)
    {
        /* Make sure that the buffer that we use is aligned for pointers,
           because the code assumes that */
        offset = sizeof(void *) - offset;
        buffer = (char *)buffer + offset;
        BsendBuffer.buffer      = buffer;
        BsendBuffer.buffer_size -= offset;
    }
    BsendBuffer.avail           = (BsendData_t*)buffer;
    BsendBuffer.active          = 0;

    /* Set the first block */
    p             = (BsendData_t *)buffer;
    p->total_size = buffer_size;
    p->next       = p->prev = 0;

    return MPI_SUCCESS;
}

/*
 * Detach a buffer.  This routine must wait until all active bsends are complete.
 */

MPI_RESULT
MPIR_Bsend_detach(
    _Outptr_result_bytebuffer_(*size) void** bufferp,
    _Out_ int *size
    )
{
    if (BsendBuffer.active)
    {
        /* Loop through each active element and wait on it */
        BsendData_t *p = BsendBuffer.active;

        while (p)
        {
            //
            // We must only wait on the internal completion to know when the real
            //  send is completed, else we could skip over actually sending the data
            // The outer request could be canceled or marked done, but until the
            //  p->request->cc value hits 0, we can't be sure we have actually sent
            //  the data
            //
            MPIR_WaitInternal( p->request );
            p->request->Release();
            p = p->next;
        }
    }

    /* Note that this works even when the buffer does not exist */
    *bufferp  = BsendBuffer.origbuffer;
    *size = BsendBuffer.origbuffer_size;
    BsendBuffer.origbuffer = NULL;
    BsendBuffer.origbuffer_size = 0;
    BsendBuffer.buffer  = 0;
    BsendBuffer.buffer_size  = 0;
    BsendBuffer.avail   = 0;
    BsendBuffer.active  = 0;

    return MPI_SUCCESS;
}


/*
 * The following routines are used to manage the allocation of bsend segments
 * in the user buffer.  These routines handle, for example, merging segments
 * when an active segment that is adjacent to a free segment becomes free.
 *
 */


/*
 * Find a slot in the avail buffer that can hold size bytes.  Does *not*
 * remove the slot from the avail buffer (see MPIR_Bsend_take_buffer)
 */

static BsendData_t *MPIR_Bsend_find_buffer( int size )
{
    BsendData_t *p = BsendBuffer.avail;

    while (p)
    {
        if (p->total_size - BSENDDATA_HEADER_TRUE_SIZE >= size)
            return p;

        p = p->next;
    }
    return NULL;
}


/* This is the minimum number of bytes that a segment must be able to
   hold. */
#define MIN_BUFFER_BLOCK 8
/*
 * Carve off size bytes from buffer p and leave the remainder
 * on the avail list.  Handle the head/tail cases.
 * If there isn't enough left of p, remove the entire segment from
 * the avail list.
 */

static void MPIR_Bsend_take_buffer( BsendData_t *p, int size  )
{
    BsendData_t *prev;
    int         alloc_size;

    /* Compute the remaining size.  This must include any padding
       that must be added to make the new block properly aligned */
    alloc_size = size;
    if (alloc_size & 0x7)
    {
        alloc_size += (8 - (alloc_size & 0x7));
    }

    /* alloc_size is the amount of space (out of size) that we will
       allocate for this buffer. */

    /* Is there enough space left to create a new block? */
    if (p->total_size - BSENDDATA_HEADER_TRUE_SIZE - alloc_size >= BSENDDATA_HEADER_TRUE_SIZE + MIN_BUFFER_BLOCK)
    {
        /* Yes, the available space (p->size) is large enough to
           carve out a new block */
        BsendData_t *newp;
        newp = (BsendData_t *)( (char *)p + BSENDDATA_HEADER_TRUE_SIZE + alloc_size );

        newp->total_size = p->total_size - alloc_size - BSENDDATA_HEADER_TRUE_SIZE;

        /* Insert this new block after p (we'll remove p from the avail list
           next) */
        newp->next = p->next;
        newp->prev = p;
        if (p->next)
        {
            p->next->prev = newp;
        }

        p->next       = newp;
        p->total_size = (int)((char *)newp - (char*)p);
    }

    /* Remove p from the avail list and add it to the active list */
    prev = p->prev;
    if (prev)
    {
        prev->next = p->next;
    }
    else
    {
        BsendBuffer.avail = p->next;
    }

    if (p->next)
    {
        p->next->prev = p->prev;
    }

    if (BsendBuffer.active)
    {
        BsendBuffer.active->prev = p;
    }
    p->next            = BsendBuffer.active;
    p->prev            = 0;
    BsendBuffer.active = p;
}


/* Add block p to the free list. Merge into adjacent blocks.  Used only
   within the check_active */
static void MPIR_Bsend_free_segment( BsendData_t *p )
{
    BsendData_t* prev = p->prev;

    /* Remove the segment from the active list */
    if (prev)
    {
        prev->next = p->next;
    }
    else
    {
        /* p was at the head of the active list */
        BsendBuffer.active = p->next;
        /* The next test sets the prev pointer to null */
    }

    if (p->next)
    {
        p->next->prev = prev;
    }

    /* Merge into the avail list */
    /* Find avail_prev, avail, such that p is between them.
       either may be null if p is at either end of the list */
    BsendData_t* avail_prev = 0;
    BsendData_t* avail = BsendBuffer.avail;
    while (avail)
    {
        if (avail > p)
            break;

        avail_prev = avail;
        avail      = avail->next;
    }

    /* Try to merge p with the next block */
    if (avail)
    {
        if ((char *)p + p->total_size == (char *)avail)
        {
            p->total_size += avail->total_size;
            p->next = avail->next;
            if (avail->next) avail->next->prev = p;
            avail = 0;
        }
        else
        {
            p->next = avail;
            avail->prev = p;
        }
    }
    else
    {
        p->next = 0;
    }
    /* Try to merge p with the previous block */
    if (avail_prev)
    {
        if ((char *)avail_prev + avail_prev->total_size == (char *)p)
        {
            avail_prev->total_size += p->total_size;
            avail_prev->next = p->next;
            if (p->next) p->next->prev = avail_prev;
        }
        else
        {
            avail_prev->next = p;
            p->prev          = avail_prev;
        }
    }
    else
    {
        /* p is the new head of the list */
        BsendBuffer.avail = p;
        p->prev           = 0;
    }
}


/*
 * The following routine tests for completion of active sends and
 * frees the related storage
 *
 * To make it easier to identify the source of the request, we keep
 * track of the type of MPI routine (ibsend, bsend, or bsend_init/start)
 * that created the bsend entry.
 */

static void MPIR_Bsend_check_active( void )
{
    BsendData_t* active = BsendBuffer.active;
    BsendData_t* next_active;

    while (active)
    {
        next_active = active->next;

        //
        // Invoke the progress engine to make progress on the active requests
        //
        MPID_Progress_pump( );

        if(active->request->is_internal_complete())
        {
            /* We're done.  Remove this segment */
            active->request->Release();
            MPIR_Bsend_free_segment( active );
        }
        active = next_active;
    }
}


/*
 * Initiate an ibsend.  We'll used this for Bsend as well.
 */
MPI_RESULT
MPIR_Bsend_isend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle dtype,
    _In_ int dest,
    _In_ int tag,
    _In_ MPID_Comm* comm_ptr,
    _Outptr_ MPID_Request** request
    )
{
    MPI_RESULT mpi_errno;
    BsendData_t* p = NULL;

    /* We check the active buffer first.  This helps avoid storage
       fragmentation */
    MPIR_Bsend_check_active();

    int packsize;

    if( dtype != g_hBuiltinTypes.MPI_Packed )
    {
        packsize = fixme_cast<int>(dtype.GetSize() * count);
    }
    else
    {
        packsize = count;
    }

    for(int pass = 0; pass < 2; pass++)
    {
        p = MPIR_Bsend_find_buffer( packsize );
        if(p != NULL)
            break;

        /* Try to complete some active bsends */
        MPIR_Bsend_check_active( );
    }

    if(p == NULL)
    {
        /* Return error for no buffer space found */

        /* Generate a traceback of the allocated space, explaining why packsize could not be found */
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**bufbsend %d %d", packsize,  BsendBuffer.buffer_size );
        goto fn_exit;
    }

    /* Pack the data into the buffer */
    int bufsize = 0;

    //
    // MPI API functions aren't const correct, thus the const_cast.
    //
    if( dtype != g_hBuiltinTypes.MPI_Packed )
    {
        NMPI_Pack(
            buf,
            count,
            dtype.GetMpiHandle(),
            p->msgbuf,
            packsize,
            &bufsize,
            comm_ptr->handle
            );
    }
    else
    {
        memcpy( p->msgbuf, buf, count );
    }

    mpi_errno = MPID_Send(
                    p->msgbuf,
                    bufsize,
                    g_hBuiltinTypes.MPI_Packed,
                    dest,
                    tag,
                    comm_ptr,
                    &p->request
                    );
    ON_ERROR_FAIL(mpi_errno);

    //
    // Set the request kind to enable specific completion indication in
    // MPID_Request::is_complete() for ibsend requests.
    //
    p->request->kind = MPID_BUFFERED_SEND;

    MPIR_Bsend_take_buffer( p, bufsize );
    p->request->AddRef();
    *request = p->request;

fn_exit:
    return mpi_errno;

fn_fail:
    MPIR_Bsend_free_segment(p);
    goto fn_exit;
}


//
// Summary:
//  Returns the required overhead for Bsend operations.
//
int MPIR_Get_bsend_overhead()
{
    //we want this size <= the fixed sizes from V1 and V2.
    //this allows applications compilied with V1 and V2 header
    //to still function. Because V1 and V2 where the same size
    //we only assert the one (or we get an error because of
    //duplicate asserts).
    C_ASSERT((BSENDDATA_HEADER_TRUE_SIZE + sizeof(void*) - 1) <= MSMPI_BSEND_OVERHEAD_V2);

    //
    // Because of the padding algorithm used, the caller could
    //  allocate an unaligned size (say 66 bytes), this would push
    //  the start of the header to the nearest aligned start after
    //  the callers data which could overflow the buffer. So we give
    //  and extra sizeof(void*) for padding.
    //
    return BSENDDATA_HEADER_TRUE_SIZE + sizeof(void*) - 1;
}


//
// These functions are used in the implementation of collective
// operations. They are wrappers around MPID send/recv functions. They do
// sends/receives by setting the communicator type bits in the tag.
//

MPI_RESULT
MPIC_Send(
    _In_opt_ const void*       buf,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, comm_ptr->remote_size - 1)
             int               dest,
    _In_     int               tag,
    _In_     const MPID_Comm*  comm_ptr
    )
{
    MPID_Request* request_ptr = NULL;

    MPI_RESULT mpi_errno = MPID_Send(
                    buf,
                    count,
                    datatype,
                    dest,
                    tag | comm_ptr->comm_kind,
                    const_cast<MPID_Comm *>(comm_ptr),
                    &request_ptr
                    );

    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIR_Wait(request_ptr);
    ON_ERROR_FAIL(mpi_errno);

    request_ptr->Release();

fn_exit:
    return mpi_errno;

fn_fail:
    if (request_ptr)
    {
        request_ptr->Release();
    }
    goto fn_exit;
}


MPI_RESULT
MPIC_Recv(
    _Out_opt_ void*            buf,
    _In_range_(>=, 0)
             int               count,
    _In_      TypeHandle       datatype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              source,
    _In_      int              tag,
    _In_      const MPID_Comm* comm_ptr,
    _Out_     MPI_Status*      status
    )
{
    MPID_Request* request_ptr = NULL;
    //
    // fix OACR warning 6101 for status
    //
    OACR_USE_PTR(status);
    MPI_RESULT mpi_errno = MPID_Recv(
                    buf,
                    count,
                    datatype,
                    source,
                    tag | comm_ptr->comm_kind,
                    const_cast<MPID_Comm *>(comm_ptr),
                    &request_ptr
                    );

    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIR_Wait(request_ptr);
    ON_ERROR_FAIL(mpi_errno);

    if (status != MPI_STATUS_IGNORE)
    {
        *status = *( reinterpret_cast<MPI_Status*>( &request_ptr->status ) );
    }

    mpi_errno = request_ptr->status.MPI_ERROR;

    request_ptr->Release();

fn_exit:
    return mpi_errno;

fn_fail:
    if (request_ptr)
    {
        request_ptr->Release();
    }
    goto fn_exit;
}


MPI_RESULT
MPIC_Sendrecv(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              dest,
    _In_      int              sendtag,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              source,
    _In_      int              recvtag,
    _In_      const MPID_Comm* comm_ptr,
    _Out_     MPI_Status*      status
    )
{
    MPID_Request* recv_req_ptr = NULL;
    MPID_Request* send_req_ptr = NULL;
    //
    // fix OACR warning 6101 for status
    //
    OACR_USE_PTR(status);

    MPI_RESULT mpi_errno = MPID_Recv(
                    recvbuf,
                    recvcount,
                    recvtype,
                    source,
                    recvtag | comm_ptr->comm_kind,
                    const_cast<MPID_Comm *>(comm_ptr),
                    &recv_req_ptr
                    );

    /* BUGBUG: erezh - need to release the request on error */
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPID_Send(
                    sendbuf,
                    sendcount,
                    sendtype,
                    dest,
                    sendtag | comm_ptr->comm_kind,
                    const_cast<MPID_Comm *>(comm_ptr),
                    &send_req_ptr
                    );

    /* BUGBUG: erezh - need to cancel the receive and release the requests on error */
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIR_Wait(send_req_ptr);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIR_Wait(recv_req_ptr);
    ON_ERROR_FAIL(mpi_errno);

    if (status != MPI_STATUS_IGNORE)
    {
        *status = *( reinterpret_cast<MPI_Status*>( &recv_req_ptr->status ) );
    }

    mpi_errno = recv_req_ptr->status.MPI_ERROR;

    send_req_ptr->Release();
    recv_req_ptr->Release();

fn_fail:
    return mpi_errno;
}


static void
MPIDI_CH3U_Buffer_copy(
    _In_opt_ const void* sbuf,
    _In_range_(>=, 0) int scount,
    _In_ TypeHandle sdt,
    _Out_ MPI_RESULT* smpi_errno,
    _Out_opt_ void* rbuf,
    _In_range_(>=, 0) int rcount,
    _In_ TypeHandle rdt,
    _Out_ MPIDI_msg_sz_t* rsz,
    _Out_ MPI_RESULT* rmpi_errno
    )
{
    bool sdt_contig;
    bool rdt_contig;
    MPI_Aint sdt_true_lb, rdt_true_lb;
    MPIDI_msg_sz_t copy_sz;
    MPIDI_msg_sz_t rdata_sz;

    *smpi_errno = MPI_SUCCESS;
    *rmpi_errno = MPI_SUCCESS;

    copy_sz = fixme_cast<MPIDI_msg_sz_t>( sdt.GetSizeAndInfo( scount, &sdt_contig, &sdt_true_lb ) );
    rdata_sz = fixme_cast<MPIDI_msg_sz_t>( rdt.GetSizeAndInfo( rcount, &rdt_contig, &rdt_true_lb ) );

    if (copy_sz > rdata_sz)
    {
        copy_sz = rdata_sz;
        *rmpi_errno = MPIU_ERR_CREATE(MPI_ERR_TRUNCATE, "**truncate %d %d", copy_sz, rdata_sz );
    }

    if (copy_sz == 0)
    {
        *rsz = 0;
        //
        // fix OACR warning 6101 for rbuf
        //
        OACR_USE_PTR(rbuf);
        goto fn_exit;
    }

    if (sdt_contig && rdt_contig)
    {
        memcpy((char *)rbuf + rdt_true_lb, (const char *)sbuf + sdt_true_lb, copy_sz);
        *rsz = copy_sz;
    }
    else if (sdt_contig)
    {
        MPID_Segment seg;
        MPIDI_msg_sz_t last;

        MPID_Segment_init(rbuf, rcount, rdt.GetMpiHandle(), &seg, 0);
        last = copy_sz;
        MPID_Segment_unpack_msg(&seg, 0, &last, (const char*)sbuf + sdt_true_lb);
        if (last != copy_sz)
        {
            *rmpi_errno = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
        }

        *rsz = last;
    }
    else if (rdt_contig)
    {
        MPID_Segment seg;
        MPIDI_msg_sz_t last;

        MPID_Segment_init(sbuf, scount, sdt.GetMpiHandle(), &seg, 0);
        last = copy_sz;
        MPID_Segment_pack_msg(&seg, 0, &last, (char*)rbuf + rdt_true_lb);
        if (last != copy_sz)
        {
            *rmpi_errno = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
        }

        *rsz = last;
    }
    else
    {
        char * buf;
        MPIDI_msg_sz_t buf_off;
        MPID_Segment sseg;
        MPIDI_msg_sz_t sfirst;
        MPID_Segment rseg;
        MPIDI_msg_sz_t rfirst;

        buf = (char*)MPIU_Malloc(MPIDI_COPY_BUFFER_SZ);
        if (buf == NULL)
        {
            *smpi_errno = MPIU_ERR_NOMEM();
            *rmpi_errno = *smpi_errno;
            *rsz = 0;
            //
            // fix OACR warning 6101 for rbuf
            //
            OACR_USE_PTR(rbuf);
            goto fn_exit;
        }

        MPID_Segment_init(sbuf, scount, sdt.GetMpiHandle(), &sseg, 0);
        MPID_Segment_init(rbuf, rcount, rdt.GetMpiHandle(), &rseg, 0);

        sfirst = 0;
        rfirst = 0;
        buf_off = 0;

        for(;;)
        {
            MPIDI_msg_sz_t last;
            const char * buf_end;

            if (copy_sz - sfirst > MPIDI_COPY_BUFFER_SZ - buf_off)
            {
                last = sfirst + (MPIDI_COPY_BUFFER_SZ - buf_off);
            }
            else
            {
                last = copy_sz;
            }

            MPID_Segment_pack_msg(&sseg, sfirst, &last, buf + buf_off);
            MPIU_Assert(last > sfirst);

            buf_end = buf + buf_off + (last - sfirst);
            sfirst = last;

            MPID_Segment_unpack_msg(&rseg, rfirst, &last, buf);
            MPIU_Assert(last > rfirst);

            rfirst = last;

            if (rfirst == copy_sz)
            {
                /* successful completion */
                break;
            }

            if (sfirst == copy_sz)
            {
                /* datatype mismatch -- remaining bytes could not be unpacked */
                *rmpi_errno = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
                break;
            }

            buf_off = sfirst - rfirst;
            if (buf_off > 0)
            {
                MoveMemory(buf, buf_end - buf_off, buf_off);
            }
        }

        *rsz = rfirst;
        MPIU_Free(buf);
    }

  fn_exit:
    ;
}


MPI_RESULT
MPIR_Localcopy(
    _In_opt_          const void*      sendbuf,
    _In_range_(>=, 0) int              sendcount,
    _In_              TypeHandle       sendtype,
    _Out_opt_         void*            recvbuf,
    _In_range_(>=, 0) int              recvcount,
    _In_              TypeHandle       recvtype
    )
{
    MPI_RESULT serr;
    MPI_RESULT rerr;
    MPIDI_msg_sz_t cb;

    MPIDI_CH3U_Buffer_copy(
        sendbuf, sendcount, sendtype, &serr,
        recvbuf, recvcount, recvtype, &cb, &rerr
        );

    return rerr;
}


//
// This routine is called when a request matches a send-to-self message
//
static void
MPIDI_CH3_RecvFromSelf(
    _Inout_ MPID_Request* rreq,
    _When_(rreq->partner_request != nullptr, _Out_opt_) void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype
    )
{
    MPID_Request* sreq = rreq->partner_request;

    if (sreq != NULL)
    {
        MPIDI_msg_sz_t data_sz;

        MPIDI_CH3U_Buffer_copy(sreq->dev.user_buf, sreq->dev.user_count,
                               sreq->dev.datatype, &sreq->status.MPI_ERROR,
                               buf, count, datatype, &data_sz,
                               &rreq->status.MPI_ERROR);

        //
        // TODO: When we support transferring more than 2GB
        // status.count should be using uint64
        //
        rreq->status.count = data_sz;

        sreq->signal_completion();
    }
    else
    {
        //
        // The sreq is missing which means an error occurred during send.
        // (see MPIDI_Isend_self w/ MPIDI_REQUEST_TYPE_RSEND).
        // rreq->status.MPI_ERROR was set when the error was detected.
        //
    }

    //
    // The receive is complete as we moved all the data (error case included).
    // Signal the receive request for completion.
    //
    rreq->signal_completion();

    //
    // Signal progress that we have completed the requests, potentially outside
    //  the progress loop.  This will enable the pending send on another thread
    //  to complete and unwind.
    //
    Mpi.SignalProgress();
}


//
// This routine is called by mpid_recv and mpid_imrecv when a request
// needs to be handled.
//
static
__forceinline
MPI_RESULT
MPIDI_CH3_HandleRecvMsgType(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _Inout_ MPID_Request* rreq
    )
{
    //
    // Suppress OACR C6101 for buf.
    // buf can be skipped at success cases which is a design issue.
    // TODO: fix in MQ.
    //
    OACR_USE_PTR( buf );

    MPI_RESULT mpi_errno;
    MPID_Comm *comm;

    comm = rreq->comm;

    switch( rreq->get_msg_type() )
    {
        case MPIDI_REQUEST_EAGER_MSG:
        {
            int recv_pending;

            //
            // If this is a eager synchronous message, then we need to send an
            // acknowledgement back to the sender.
            //
            if ( rreq->needs_sync_ack() )
            {
                MPIDI_VC_t* vc = MPIDI_Comm_get_vc( comm, rreq->dev.match.rank );
                mpi_errno = MPIDI_CH3_EagerSyncAck( vc, rreq );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }

                //
                // BUGBUG: erezh - We can not send a sync-ack back. Should this fail
                // the receive alltogehter? What can we do  here for FT? should we return
                // the rreq back to the queue? Should we retry indefinitly? what happens
                // in the case that the connection with the other side is down? we cannot
                // fulfill the Ssync message requirement.
                // However in any case we return an error we should release the rreq.
                //
            }

            recv_pending = rreq->dec_and_get_recv_pending();
            if ( recv_pending == 0 )
            {
                //
                // All of the data has arrived, we need to copy the data and
                // then free the buffer.
                // FIXME: if the user buffer is contiguous, just move the
                // data without using a separate routine call
                //
                if ( rreq->dev.recv_data_sz > 0 )
                {
                    MPIDI_CH3U_Request_unpack_uebuf( rreq );
                    MPIU_Free( rreq->dev.tmpbuf );
                }

                //
                // We successfully picked up the message, let the caller pick up the
                // error code from the request. (do not return the request error value)
                //
                return MPI_SUCCESS;
            }
            else
            {
                //
                // The data is still being transfered across the net.  We'll
                // leave it to the progress engine to handle once the
                // entire message has arrived.
                //
            }
            break;
        }

        case MPIDI_REQUEST_RNDV_MSG:
        {
            MPIDI_VC_t* vc = MPIDI_Comm_get_vc( comm, rreq->dev.match.rank );
            mpi_errno = MPIDI_CH3_RecvRndv( vc, rreq );
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }

            //
            // BUGBUG: erezh - We can not send a cts ack back.
            // What can we do  here for FT? should we return the rreq back to the queue?
            // Should we retry indefinitly? what happens in the case that the connection
            // with the other side is down? we cannot fulfill the rndv message
            // requirement.
            // However in any case we return an error we should release the rreq.
            //

            break;
        }

        case MPIDI_REQUEST_SELF_MSG:
        {
            MPIDI_CH3_RecvFromSelf( rreq, buf, count, datatype );
            break;
        }

        default:
        {
            //
            // Assert always; this is an internal error.
            //
            MPIU_Assert( 0 );
        }
    }

    return MPI_SUCCESS;
}


static
MPI_RESULT
MPIDI_RecvProcNull(
    _Outptr_ MPID_Request** request
    )
{
    MPID_Request* rreq;

    rreq = MPID_Request_create( MPID_REQUEST_RECV );
    if( rreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    rreq->signal_completion();
    MPIR_Status_set_procnull( reinterpret_cast<MPI_Status*>(&(rreq->status)) );

    *request = rreq;

    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Recv(
    _Inout_opt_ void* buf,
    _In_range_(>=, 0)  int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPID_Request * rreq;
    int found;

    MPIU_Assert(count >= 0);

    TraceRecvMsg(comm->handle,comm->rank,rank,tag,datatype.GetMpiHandle(),buf,count);

    if (rank == MPI_PROC_NULL)
    {
        return MPIDI_RecvProcNull(request);
    }

    mpi_errno = MPID_Recvq_dq_unexpected_or_new_posted(
                    rank,
                    tag,
                    comm->recvcontext_id,
                    &found,
                    &rreq,
                    [&](
                        _In_ MPID_Request*  rreq,
                        _In_ bool           found
                        )
                    {
                        rreq->comm           = comm;
                        comm->AddRef();
                        rreq->dev.user_buf   = buf;
                        rreq->dev.user_count = count;
                        rreq->dev.datatype   = datatype;
                        if( datatype.IsPredefined() == false )
                        {
                            datatype.Get()->AddRef();
                        }

                        if(!found)
                        {
                            //
                            // Message has yet to arrived.  The request has been placed on the
                            // list of posted receive requests and populated with
                            // information supplied in the arguments.
                            //
                            rreq->dev.recv_pending_count = 1;
                        }

                        return MPI_SUCCESS;
                    }
                    );
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }


    if(!found)
    {
        *request = rreq;
        return MPI_SUCCESS;
    }

    //
    // Message was found in the unexepected queue
    //
    mpi_errno = MPIDI_CH3_HandleRecvMsgType(
                               buf,
                               count,
                               datatype,
                               rreq
                               );

    if( mpi_errno == MPI_SUCCESS )
    {
        *request = rreq;
    }
    return mpi_errno;
}


MPI_RESULT
MPID_Imrecv(
    _Inout_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _Inout_opt_ MPID_Request* message,
    _Outptr_ MPID_Request** request
    )
{
    MPID_Request *rreq;

    //
    // message being NULL is equivalent to MPI_MESSAGE_NO_PROC
    //
    if ( message == nullptr )
    {
        return MPIDI_RecvProcNull(request);
    }

    MPIU_Assert( count >= 0 );

    rreq = message;

    rreq->dev.user_buf = buf;
    rreq->dev.user_count = count;
    rreq->dev.datatype = datatype;

    if( datatype.IsPredefined() == false )
    {
        datatype.Get()->AddRef();
    }

    MPI_RESULT mpi_errno = MPIDI_CH3_HandleRecvMsgType(
                               buf,
                               count,
                               datatype,
                               rreq
                               );

    if( mpi_errno == MPI_SUCCESS )
    {
        *request = rreq;
    }

    return mpi_errno;
}


MPI_RESULT
MPID_Rsend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    )
{
    MPID_Request* sreq;
    sreq = MPIDI_Request_create_sreq(
                MPIDI_REQUEST_TYPE_RSEND,
                buf,
                count,
                datatype,
                rank,
                tag,
                comm
                );
    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_RESULT mpi_errno = sreq->execute_rsend();
    if( mpi_errno != MPI_SUCCESS )
    {
        sreq->Release();
    }
    else
    {
        *request = sreq;
    }
    return mpi_errno;
}


MPI_RESULT
MPID_Send(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    )
{
    MPID_Request* sreq;
    sreq = MPIDI_Request_create_sreq(
                MPIDI_REQUEST_TYPE_SEND,
                buf,
                count,
                datatype,
                rank,
                tag,
                comm
                );
    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_RESULT mpi_errno = sreq->execute_send();
    if( mpi_errno != MPI_SUCCESS )
    {
        sreq->Release();
    }
    else
    {
        *request = sreq;
    }

    return mpi_errno;
}


MPI_RESULT
MPID_Ssend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    )
{
    MPID_Request* sreq;
    sreq = MPIDI_Request_create_sreq(
                MPIDI_REQUEST_TYPE_SSEND,
                buf,
                count,
                datatype,
                rank,
                tag,
                comm
                );
    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPI_RESULT mpi_errno = sreq->execute_ssend();
    if( mpi_errno != MPI_SUCCESS )
    {
        sreq->Release();
    }
    else
    {
        *request = sreq;
    }
    return mpi_errno;
}


/* FIXME: Explain this function */

/* FIXME: should there be a simpler version of this for short messages,
   particularly contiguous ones?  See also the FIXME about buffering
   short messages */

MPI_RESULT
MPIDI_Isend_self(
    MPID_Request* sreq
    )
{
    MPIDI_Message_match match;
    MPID_Request * rreq;
    int found;
    MPI_RESULT mpi_errno;

    sreq->set_msg_type(MPIDI_REQUEST_SELF_MSG);

    match.rank = sreq->comm->rank;
    match.tag = sreq->dev.match.tag;
    match.context_id = sreq->dev.match.context_id;


    mpi_errno = MPID_Recvq_dq_posted_or_new_unexpected(
                            &match,
                            &found,
                            &rreq,
                            [&](
                                _In_ MPID_Request*  rreq,
                                _In_ bool           found
                                )
                            {
                                rreq->status.MPI_SOURCE = match.rank;
                                rreq->status.MPI_TAG = match.tag;

                                if (!found)
                                {
                                    if (sreq->get_type() != MPIDI_REQUEST_TYPE_RSEND)
                                    {
                                        rreq->partner_request = sreq;
                                        rreq->dev.sender_req_id = sreq->handle;
                                        rreq->status.count = sreq->dev.user_count * sreq->dev.datatype.GetSize();
                                    }
                                    else
                                    {
                                        sreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rsendnomatch %d %d", match.rank, match.tag);
                                        rreq->status.MPI_ERROR = sreq->status.MPI_ERROR;

                                        rreq->partner_request = NULL;
                                        rreq->dev.sender_req_id = MPI_REQUEST_NULL;
                                        rreq->status.count = 0;

                                        sreq->signal_completion();
                                    }

                                    rreq->set_msg_type(MPIDI_REQUEST_SELF_MSG);
                                }

                                return MPI_SUCCESS;
                            });

    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    if (found)
    {
        MPIDI_msg_sz_t data_sz;

        MPIDI_CH3U_Buffer_copy(
            sreq->dev.user_buf,
            sreq->dev.user_count,
            sreq->dev.datatype,
            &sreq->status.MPI_ERROR,
            rreq->dev.user_buf,
            rreq->dev.user_count,
            rreq->dev.datatype,
            &data_sz,
            &rreq->status.MPI_ERROR
            );

        rreq->status.count = data_sz;
        rreq->signal_completion();

        sreq->signal_completion();
    }
    rreq->Release();

    Mpi.SignalProgress();

    return MPI_SUCCESS;
}


void
MPID_Cancel_recv(
    _In_ MPID_Request* rreq
    )
{
    MPIU_Assert(rreq->kind == MPID_REQUEST_RECV);

    rreq = MPID_Recvq_dq_recv(rreq);
    if(rreq == NULL)
    {
        return;
    }

    rreq->status.cancelled = TRUE;
    rreq->status.count = 0;
    rreq->signal_completion();
    rreq->Release();
}


MPI_RESULT
MPID_Cancel_send(
    _In_ MPID_Request* sreq
    )
{
    int proto;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPIU_Assert(sreq->kind == MPID_REQUEST_SEND || sreq->kind == MPID_BUFFERED_SEND);

    if(sreq->get_and_set_cancel_pending())
        goto fn_exit;

    /*
     * FIXME: user requests returned by MPI_Ibsend() have a NULL comm pointer
     * and no pointer to the underlying communication
     * request.  For now, we simply fail to cancel the request.  In the future,
     * we should add a new request kind to indicate that
     * the request is a BSEND.  Then we can properly cancel the request, much
     * in the way we do persistent requests.
     */
    if (sreq->comm == NULL)
    {
        goto fn_exit;
    }

    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(sreq->comm, sreq->dev.match.rank);

    proto = sreq->get_msg_type();

    if (proto == MPIDI_REQUEST_SELF_MSG)
    {
        MPID_Request * rreq;

        rreq = MPID_Recvq_dq_msg_by_id(sreq->handle, &sreq->dev.match);
        if (rreq)
        {
            MPIU_Assert(rreq->partner_request == sreq);

            rreq->Release();

            sreq->status.cancelled = TRUE;
            sreq->signal_completion();
        }
        else
        {
            sreq->status.cancelled = FALSE;
        }

        goto fn_exit;
    }

    /* Part or all of the message has already been sent, so we need to send a cancellation request to the receiver in an attempt
       to catch the message before it is matched. */
    {
        MPIDI_CH3_Pkt_t upkt;
        MPIDI_CH3_Pkt_cancel_send_req_t* csr_pkt = &upkt.cancel_send_req;

        MPIDI_Pkt_init(csr_pkt, MPIDI_CH3_PKT_CANCEL_SEND_REQ);
        csr_pkt->match.rank = sreq->comm->rank;
        csr_pkt->match.tag = sreq->dev.match.tag;
        csr_pkt->match.context_id = sreq->dev.match.context_id;
        csr_pkt->sender_req_id = sreq->handle;

        mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
        ON_ERROR_FAIL(mpi_errno);

        //
        // N.B. The handle value is sent to be matched by the target and sent back
        //      with the cancel-response.  Take a reference on the send request to
        //      keep it alive until the cancel-response comes back.
        //      The code adds the reference here, after the actual send, to avoid
        //      the need to release the request in case of an error.
        //
        sreq->postpone_completion();
        sreq->AddRef();
    }

 fn_fail:
 fn_exit:
    return mpi_errno;
}
