// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "mpidrma.h"
#include "ch3_compression.h"
#include "mpidi_ch3_impl.h"


/*
 * This file contains the dispatch routine called by the ch3 progress
 * engine to process messages.
 *
 * This file is in transition
 *
 * Where possible, the routines that create and send all packets of
 * a particular type are in the same file that contains the implementation
 * of the handlers for that packet type (for example, the CancelSend
 * packets are created and processed by routines in ch3/src/mpid_cancel_send.c)
 * This makes is easier to replace or modify functionality within
 * the ch3 device.
 */


class CRequestQueue
{

private:
    MPID_Request* m_pHead;
    MPID_Request* m_pTail;

public:

    //
    // Ctor: only m_pHead need to be initialized.
    // Note: if m_pHead is null, the value of m_pTail is not relevant.
    //
    CRequestQueue() : m_pHead(NULL) {}

    //
    // Enqueue the request
    //
    void enqueue(MPID_Request* pReq)
    {
        pReq->dev.next = NULL;
        if (m_pHead == NULL)
        {
            m_pHead = pReq;
        }
        else
        {
            m_pTail->dev.next = pReq;
        }
        m_pTail = pReq;
    }

    //
    // remove the request from the list.
    // Note: preq must be pointing to &m_pHead or to the &next member of the request.
    //
    void remove(MPID_Request** ppReq)
    {
        if((*ppReq) == m_pTail)
        {
            //
            // Note that m_pTail might get an invalid value here which happens
            // only when m_pHead becomes NULL.
            //
            m_pTail = CONTAINING_RECORD(ppReq, MPID_Request, dev.next);
        }

        *ppReq = (*ppReq)->dev.next;
    }

    //
    // Return a reference to the head (to enable efficiant entry removal)
    //
    MPID_Request*& head()
    {
        return m_pHead;
    }
};

//
// TODO: The following state should be tracked inside the MpiProcess class ideally.
//

//
// The posted receives queue.
//
extern "C"
{
CRequestQueue recvq_posted;
}

//
// The unexpected arrived messages queue.
//
extern "C"
{
CRequestQueue recvq_unexpected;
}

//
// RecvQ Lock
//
MPI_RWLOCK             g_RecvQLock = MPI_RWLOCK_INIT;



VOID MPID_Recvq_lock_exclusive()
{
    MpiRwLockAcquireExclusive(&g_RecvQLock);
}

VOID MPID_Recvq_unlock_exclusive()
{
    MpiRwLockReleaseExclusive(&g_RecvQLock);
}

VOID MPID_Recvq_lock_shared()
{
    MpiRwLockAcquireShared(&g_RecvQLock);
}

VOID MPID_Recvq_unlock_shared()
{
    MpiRwLockReleaseShared(&g_RecvQLock);
}


/* Export the location of the queue heads if debugger support is enabled.
 * This allows the queue code to rely on the local variables for the
 * queue heads while also exporting those variables to the debugger.
 * See src/mpi/debugger/dll_mpich2.c for how this is used to
 * access the message queues.
 */
inline bool matching(const MPIDI_Message_match& match, const MPIDI_Message_match& filter)
{
    if(match.context_id != filter.context_id)
        return false;

    if(filter.rank != MPI_ANY_SOURCE)
    {
        if(match.rank != filter.rank)
            return false;
    }

    if(filter.tag == MPI_ANY_TAG)
    {
        return match.tag >= 0;
    }

    return match.tag == filter.tag;
}

/*
 * MPID_Recvq_probe_msg_unsafe()
 *
 * Search for a matching request in the unexpected messages queue.  Return
 * true if one is found, false otherwise.  If the status arguement is
 * not MPI_STATUS_IGNORE, return information about the request in that
 * parameter.  This routine is used by mpid_probe and mpid_iprobe.
 *
 */
_Success_(return == TRUE)
BOOL
MPID_Recvq_probe_msg_unsafe(
    _In_ int source,
    _In_ int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Out_ MPI_Status* s
    )
{
    MPIDI_Message_match filter = { tag, source, context_id };

    for(MPID_Request* rreq = recvq_unexpected.head(); rreq != NULL; rreq = rreq->dev.next)
    {
        if(!matching(rreq->dev.match, filter))
            continue;

        MPIR_Request_extract_status( rreq, s );
        return TRUE;
    }

    return FALSE;
}


//
// MPID_Recvq_probe_dq_msg_unsafe()
//
// Search for a matching request in the unexpected messages queue.  Return
// TRUE if one is found, FALSE otherwise.  Dequeue the request from the UQ
// making sure that no other probe is honored for this request.
// This routine is used by MPID_Mprobe and MPID_Improbe.
//
_Success_(return == TRUE)
BOOL
MPID_Recvq_probe_dq_msg_unsafe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Outptr_ MPID_Request** message
    )
{
    MPIDI_Message_match filter = { tag, source, context_id };

    for( MPID_Request** preq = &recvq_unexpected.head(); *preq != nullptr; preq = &(*preq)->dev.next )
    {
        if( !matching( (*preq)->dev.match, filter ) )
        {
            continue;
        }

        *message = *preq;
        recvq_unexpected.remove( preq );

        return TRUE;
    }

    return FALSE;
}


/*
 * MPID_Recvq_dq_msg_by_id_unsafe()
 *
 * Find a request in the unexpected queue and dequeue it; otherwise return NULL.
 */
MPID_Request*
MPID_Recvq_dq_msg_by_id_unsafe(
    _In_ MPI_Request sreq_id,
    _In_ const MPIDI_Message_match *match
    )
{
    for(MPID_Request** preq = &recvq_unexpected.head(); *preq != NULL; preq = &(*preq)->dev.next)
    {
        if((*preq)->dev.sender_req_id != sreq_id)
            continue;

        if(!matching((*preq)->dev.match, *match))
            continue;

        MPID_Request* rreq = *preq;
        recvq_unexpected.remove(preq);
        return rreq;
    }

    return NULL;
}


/*
 * MPID_Recvq_dq_unexpected_or_new_posted_unsafe()
 *
 * Atomically find a request in the unexpected queue and dequeue it, or
 * allocate a new request and enqueue it in the posted queue
 */
MPID_Request*
MPID_Recvq_dq_unexpected_or_new_posted_unsafe(
    _In_ int source,
    _In_ int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Out_ int * foundp
    )
{
    MPIDI_Message_match filter = { tag, source, context_id };

    for(MPID_Request** preq = &recvq_unexpected.head(); *preq != NULL; preq = &(*preq)->dev.next)
    {
        if(!matching((*preq)->dev.match, filter))
            continue;

        MPID_Request* rreq = *preq;
        recvq_unexpected.remove(preq);
        *foundp = true;
        return rreq;
    }

    /* A matching request was not found in the unexpected queue, so we
       need to allocate a new request and add it to the posted queue */
    *foundp = FALSE;
    MPID_Request* rreq = MPIDI_Request_create_rreq();
    if( rreq == NULL )
        return NULL;

    rreq->dev.match.tag = tag;
    rreq->dev.match.rank = source;
    rreq->dev.match.context_id = context_id;

    rreq->AddRef();
    recvq_posted.enqueue(rreq);

    return rreq;
}


/*
 * MPID_Recvq_dq_recv_unsafe()
 *
 * Given an existing request, dequeue that request from the posted queue, or
 * return NULL if the request was not in the posted queued
 */
MPID_Request* MPID_Recvq_dq_recv_unsafe(
    _In_ MPID_Request* rreq
    )
{
    for(MPID_Request** preq = &recvq_posted.head(); *preq != NULL; preq = &(*preq)->dev.next)
    {
        if((*preq) != rreq)
            continue;

        recvq_posted.remove(preq);
        return rreq;
    }

    return NULL;
}


/*
 * MPID_Recvq_dq_posted_or_new_unexpected_unsafe()
 *
 * Locate a request in the posted queue and dequeue it, or allocate a new
 * request and enqueue it in the unexpected queue
 */
MPID_Request*
MPID_Recvq_dq_posted_or_new_unexpected_unsafe(
    _In_ const MPIDI_Message_match * match,
    _Out_ int * foundp
    )
{
    for(MPID_Request** preq = &recvq_posted.head(); *preq != NULL; preq = &(*preq)->dev.next)
    {
        if(!matching(*match, (*preq)->dev.match))
            continue;

        MPID_Request* rreq = *preq;
        recvq_posted.remove(preq);
        *foundp = true;
        return rreq;
    }

    /* A matching request was not found in the posted queue, so we
       need to allocate a new request and add it to the unexpected queue */
    *foundp = false;
    MPID_Request* rreq = MPIDI_Request_create_rreq();
    if( rreq == NULL )
        return NULL;

    rreq->dev.match = *match;

    rreq->AddRef();
    recvq_unexpected.enqueue(rreq);

    return rreq;
}


void
MPID_Recvq_erase_posted_unsafe(
    _In_ MPID_Request* rreq
    )
{
    for(MPID_Request** preq = &recvq_posted.head(); *preq != NULL; preq = &(*preq)->dev.next)
    {
        if( *preq != rreq )
            continue;

        recvq_posted.remove(preq);
        rreq->Release();
        break;
    }
}


void
MPID_Recvq_erase_unexpected_unsafe(
    _In_ MPID_Request* rreq
    )
{
    for(MPID_Request** preq = &recvq_unexpected.head(); *preq != NULL; preq = &(*preq)->dev.next)
    {
        if( *preq != rreq )
            continue;

        recvq_unexpected.remove(preq);
        rreq->Release();
        break;
    }

}


void
MPID_Recvq_flush_unsafe()
{
    MPID_Request* rreq;

    for(MPID_Request** preq = &recvq_unexpected.head(); *preq != NULL; preq = &recvq_unexpected.head())
    {
        rreq = *preq;

        recvq_unexpected.remove(preq);
        rreq->Release();
    }
}


/*
 * MPIDI_CH3U_Request_load_send_iov()
 *
 * Fill the provided IOV with the next (or remaining) portion of data described
 * by the segment contained in the request structure.
 * If the density of IOV is not sufficient, pack the data into a send/receive
 * buffer and point the IOV at the buffer.
 */
MPI_RESULT
MPIDI_CH3U_Request_load_send_iov(
    _In_ MPID_Request* sreq
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPIDI_msg_sz_t last;
    last = sreq->dev.segment_size;

    MPIU_Assert(sreq->dev.segment_first < last);
    MPIU_Assert(last > 0);
    MPIU_Assert(sreq->dev.iov_count < _countof(sreq->dev.iov));

    int iov_n = _countof(sreq->dev.iov) - sreq->dev.iov_count;
    MPID_IOV* iov = &sreq->dev.iov[sreq->dev.iov_count];
    MPID_Segment_pack_vector_msg(sreq->dev.segment_ptr, sreq->dev.segment_first, &last, iov, &iov_n);

    MPIU_Assert(iov_n > 0);
    MPIU_Assert(iov_n <= static_cast<int>(_countof(sreq->dev.iov)) - sreq->dev.iov_count);

    if (last == sreq->dev.segment_size)
    {
        sreq->dev.OnDataAvail = sreq->dev.OnFinal;
    }
    else if ((last - sreq->dev.segment_first) / iov_n >= MPIDI_IOV_DENSITY_MIN)
    {
        sreq->dev.segment_first = last;
        sreq->dev.OnDataAvail = MPIDI_CH3_SendHandler_ReloadIOV;
    }
    else
    {
        MPIDI_msg_sz_t data_sz;
        int i, iov_data_copied;

        data_sz = sreq->dev.segment_size - sreq->dev.segment_first;
        if (!sreq->using_srbuf())
        {
            MPIDI_CH3U_SRBuf_alloc(sreq);
            if (sreq->dev.tmpbuf_sz == 0)
            {
                mpi_errno = MPIU_ERR_NOMEM();
                sreq->status.MPI_ERROR = mpi_errno;
                goto fn_exit;
            }
        }

        iov_data_copied = 0;
        for (i = 0; i < iov_n; i++)
        {
            memcpy((char*) sreq->dev.tmpbuf + iov_data_copied, iov[i].buf, iov[i].len);
            iov_data_copied += iov[i].len;
        }
        sreq->dev.segment_first = last;

        last = (data_sz <= sreq->dev.tmpbuf_sz - iov_data_copied) ?
            sreq->dev.segment_size :
            sreq->dev.segment_first + sreq->dev.tmpbuf_sz - iov_data_copied;

        MPID_Segment_pack_msg(sreq->dev.segment_ptr, sreq->dev.segment_first,
                          &last, (char*) sreq->dev.tmpbuf + iov_data_copied);

        iov[0].buf = (iovsendbuf_t*)sreq->dev.tmpbuf;
        iov[0].len = last - sreq->dev.segment_first + iov_data_copied;
        iov_n = 1;

        if (last == sreq->dev.segment_size)
        {
            sreq->dev.OnDataAvail = sreq->dev.OnFinal;
        }
        else
        {
            sreq->dev.segment_first = last;
            sreq->dev.OnDataAvail = MPIDI_CH3_SendHandler_ReloadIOV;
        }
    }

    sreq->dev.iov_count += iov_n;

  fn_exit:
    return mpi_errno;
}

/*
 * MPIDI_CH3U_Request_load_recv_iov()
 *
 * Fill the request's IOV with the next (or remaining) portion of data
 * described by the segment (also contained in the request
 * structure).  If the density of IOV is not sufficient, allocate a
 * send/receive buffer and point the IOV at the buffer.
 */
MPI_RESULT
MPIDI_CH3U_Request_load_recv_iov(
    _In_ MPID_Request* rreq
    )
{
    MPIDI_msg_sz_t last;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    if (rreq->dev.segment_first < rreq->dev.segment_size)
    {
        /* still reading data that needs to go into the user buffer */

        if (rreq->using_srbuf())
        {
            MPIDI_msg_sz_t data_sz;
            MPIDI_msg_sz_t tmpbuf_sz;

            /* Once a SRBuf is in use, we continue to use it since a small
               amount of data may already be present at the beginning
               of the buffer.  This data is left over from the previous unpack,
               most like a result of alignment issues.  NOTE: we
               could force the use of the SRBuf only
               when (rreq->dev.tmpbuf_off > 0)... */

            data_sz = rreq->dev.segment_size - rreq->dev.segment_first -
                rreq->dev.tmpbuf_off;
            MPIU_Assert(data_sz > 0);
            tmpbuf_sz = rreq->dev.tmpbuf_sz - rreq->dev.tmpbuf_off;
            if (data_sz > tmpbuf_sz)
            {
                data_sz = tmpbuf_sz;
            }
            rreq->dev.iov[0].buf = (iovrecvbuf_t*)((char *) rreq->dev.tmpbuf + rreq->dev.tmpbuf_off);
            rreq->dev.iov[0].len = data_sz;
            rreq->dev.iov_count = 1;
            MPIU_Assert(rreq->dev.segment_first + data_sz +
                        rreq->dev.tmpbuf_off <= rreq->dev.recv_data_sz);
            if (rreq->dev.segment_first + data_sz + rreq->dev.tmpbuf_off ==
                rreq->dev.recv_data_sz)
            {
                rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_UnpackSRBufComplete;
            }
            else
            {
                rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_UnpackSRBufReloadIOV;
            }
            goto fn_exit;
        }

        last = rreq->dev.segment_size;
        rreq->dev.iov_count = MPID_IOV_LIMIT;

        MPIU_Assert(rreq->dev.segment_first < last);
        MPIU_Assert(last > 0);
        MPID_Segment_unpack_vector_msg(rreq->dev.segment_ptr,
                                   rreq->dev.segment_first,
                                   &last, rreq->dev.iov, &rreq->dev.iov_count);

        MPIU_Assert(rreq->dev.iov_count >= 0 && rreq->dev.iov_count <= MPID_IOV_LIMIT);

        if (rreq->dev.iov_count == 0)
        {
            /* If the data can't be unpacked, the we have a mis-match between
               the datatype and the amount of data received.  Adjust
               the segment info so that the remaining data is received and
               thrown away. */
            rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
            rreq->status.count = rreq->dev.segment_first;
            rreq->dev.segment_size = rreq->dev.segment_first;
            mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
            goto fn_exit;
        }

        if (last == rreq->dev.recv_data_sz)
        {
            /* Eventually, use OnFinal for this instead */
            rreq->dev.OnDataAvail = 0;
        }
        else if (last == rreq->dev.segment_size ||
                 (last - rreq->dev.segment_first) / rreq->dev.iov_count >= MPIDI_IOV_DENSITY_MIN)
        {
            rreq->dev.segment_first = last;
            rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_ReloadIOV;
        }
        else
        {
            /* Too little data would have been received using an IOV.
               We will start receiving data into a SRBuf and unpacking it
               later. */
            MPIU_Assert(!rreq->using_srbuf());

            MPIDI_CH3U_SRBuf_alloc(rreq);
            rreq->dev.tmpbuf_off = 0;
            if (rreq->dev.tmpbuf_sz == 0)
            {
                /* FIXME - we should drain the data off the pipe here, but we
                   don't have a buffer to drain it into.  should this be
                   a fatal error? */
                mpi_errno = MPIU_ERR_NOMEM();
                rreq->status.MPI_ERROR = mpi_errno;
                goto fn_exit;
            }

            /* fill in the IOV using a recursive call */
            mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
        }
    }
    else
    {
        /* receive and toss any extra data that does not fit in the user's
           buffer */
        MPIDI_msg_sz_t data_sz;

        data_sz = rreq->dev.recv_data_sz - rreq->dev.segment_first;
        if (!rreq->using_srbuf())
        {
            MPIDI_CH3U_SRBuf_alloc(rreq);
            if (rreq->dev.tmpbuf_sz == 0)
            {
                mpi_errno = MPIU_ERR_NOMEM();
                rreq->status.MPI_ERROR = mpi_errno;
                goto fn_exit;
            }
        }

        if (data_sz <= rreq->dev.tmpbuf_sz)
        {
            rreq->dev.iov[0].len = data_sz;
            MPIU_Assert(rreq->get_type() == MPIDI_REQUEST_TYPE_RECV || rreq->kind == MPID_REQUEST_RMA_PUT);
            /* Eventually, use OnFinal for this instead */
            rreq->dev.OnDataAvail = 0;
        }
        else
        {
            rreq->dev.iov[0].len = rreq->dev.tmpbuf_sz;
            rreq->dev.segment_first += rreq->dev.tmpbuf_sz;
            rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_ReloadIOV;
        }

        rreq->dev.iov[0].buf = (iovrecvbuf_t*)rreq->dev.tmpbuf;
        rreq->dev.iov_count = 1;
    }

  fn_exit:
    return mpi_errno;
}

/*
 * MPIDI_CH3U_Request_unpack_srbuf
 *
 * Unpack data from a send/receive buffer into the user buffer.
 */
void
MPIDI_CH3U_Request_unpack_srbuf(
    _In_ MPID_Request * rreq
    )
{
    MPIDI_msg_sz_t last;
    MPIDI_msg_sz_t tmpbuf_last;

    tmpbuf_last = rreq->dev.segment_first + rreq->dev.tmpbuf_sz;
    if (rreq->dev.segment_size < tmpbuf_last)
    {
        tmpbuf_last = rreq->dev.segment_size;
    }
    last = tmpbuf_last;
    MPID_Segment_unpack_msg(rreq->dev.segment_ptr, rreq->dev.segment_first,
                        &last, rreq->dev.tmpbuf);
    if (last == 0 || last == rreq->dev.segment_first)
    {
        /* If no data can be unpacked, then we have a datatype processing
           problem.  Adjust the segment info so that the remaining
           data is received and thrown away. */
        rreq->status.count = rreq->dev.segment_first;
        rreq->dev.segment_size = rreq->dev.segment_first;
        rreq->dev.segment_first += tmpbuf_last;
        rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
    }
    else if (tmpbuf_last == rreq->dev.segment_size)
    {
        if (last != tmpbuf_last)
        {
            /* received data was not entirely consumed by unpack() because too
               few bytes remained to fill the next basic datatype.
               Note: the segment_first field is set to segment_last so that if
               this is a truncated message, extra data will be read
               off the pipe. */
            rreq->status.count = last;
            rreq->dev.segment_size = last;
            rreq->dev.segment_first = tmpbuf_last;
            rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
        }
    }
    else
    {
        MPIU_Assert( tmpbuf_last >= last );
        rreq->dev.tmpbuf_off = tmpbuf_last - last;
        if (rreq->dev.tmpbuf_off > 0)
        {
            /* move any remaining data to the beginning of the buffer.
               Note: memmove() is used since the data regions could
               overlap. */
            MoveMemory(rreq->dev.tmpbuf, (char *) rreq->dev.tmpbuf +
                       (last - rreq->dev.segment_first), rreq->dev.tmpbuf_off);
        }
        rreq->dev.segment_first = last;
    }
}

/*
 * MPIDI_CH3U_Request_unpack_uebuf
 *
 * Copy/unpack data from an "unexpected eager buffer" into the user buffer.
 */
void
MPIDI_CH3U_Request_unpack_uebuf(
    _In_ MPID_Request * rreq
    )
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t unpack_sz;

    MPIDI_msg_sz_t userbuf_sz = fixme_cast<MPIDI_msg_sz_t>(
        rreq->dev.datatype.GetSizeAndInfo(
            rreq->dev.user_count,
            &dt_contig,
            &dt_true_lb
            )
        );

    if (rreq->dev.pkt.base.flags & MPIDI_CH3_PKT_FLAG_COMPRESSED)
    {
        //Decompress this request before unpacking it into the user buffer
        DecompressRequest(rreq);
    }

    if (rreq->dev.recv_data_sz <= userbuf_sz)
    {
        unpack_sz = rreq->dev.recv_data_sz;
    }
    else
    {
        unpack_sz = userbuf_sz;
        rreq->status.count = userbuf_sz;
        rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_TRUNCATE, "**truncate %d %d",  rreq->dev.recv_data_sz, userbuf_sz);
    }

    if (unpack_sz > 0)
    {
        if (dt_contig)
        {
            /* TODO - check that amount of data is consistent with datatype.
               In other words, if we were to use Segment_unpack()
               would last = unpack?  If not we should return an error
               (unless configured with --enable-fast) */
            memcpy((char *)rreq->dev.user_buf + dt_true_lb, rreq->dev.tmpbuf,
                   unpack_sz);
        }
        else
        {
            MPID_Segment seg;
            MPIDI_msg_sz_t last;

            MPID_Segment_init(rreq->dev.user_buf, rreq->dev.user_count,
                              rreq->dev.datatype.GetMpiHandle(), &seg, 0);
            last = unpack_sz;
            MPID_Segment_unpack_msg(&seg, 0, &last, rreq->dev.tmpbuf);
            if (last != unpack_sz)
            {
                /* received data was not entirely consumed by unpack()
                   because too few bytes remained to fill the next basic
                   datatype */
                rreq->status.count = last;
                rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
            }
        }
    }
}


/*
 * Handler routines called when cancel send packets arrive
 */

MPI_RESULT Handle_MPIDI_CH3_PKT_CANCEL_SEND_REQ(MPIDI_VC_t * vc, const MPIDI_CH3_Pkt_t * pkt, MPID_Request ** rreqp)
{
    const MPIDI_CH3_Pkt_cancel_send_req_t * req_pkt = &pkt->cancel_send_req;
    MPID_Request * rreq;
    int ack;
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_cancel_send_resp_t * resp_pkt = &upkt.cancel_send_resp;
    MPI_RESULT mpi_errno;

    rreq = MPID_Recvq_dq_msg_by_id(req_pkt->sender_req_id, &req_pkt->match);
    if (rreq != NULL)
    {
        if (rreq->get_msg_type() == MPIDI_REQUEST_EAGER_MSG && rreq->dev.recv_data_sz > 0)
        {
            MPIU_Free(rreq->dev.tmpbuf);
            rreq->dev.tmpbuf = NULL;
        }
        rreq->Release();
        ack = TRUE;
    }
    else
    {
        ack = FALSE;
    }

    MPIDI_Pkt_init(resp_pkt, MPIDI_CH3_PKT_CANCEL_SEND_RESP);
    resp_pkt->sender_req_id = req_pkt->sender_req_id;
    resp_pkt->ack = ack;
    mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
    ON_ERROR_FAIL(mpi_errno);

    *rreqp = NULL;

 fn_fail:
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_CANCEL_SEND_RESP(const MPIDI_CH3_Pkt_t * pkt, MPID_Request ** rreqp)
{
    const MPIDI_CH3_Pkt_cancel_send_resp_t * resp_pkt = &pkt->cancel_send_resp;
    MPID_Request * sreq;

    MPID_Request_get_ptr(resp_pkt->sender_req_id, sreq);
    MPIU_Assert(sreq != nullptr);
    if(sreq == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**nullptrtype %s", "request" );
    }

    if (resp_pkt->ack)
    {
        sreq->status.cancelled = TRUE;

        if (sreq->get_msg_type() == MPIDI_REQUEST_RNDV_MSG ||
            sreq->get_type() == MPIDI_REQUEST_TYPE_SSEND)
        {
            //
            // N.B. Rndv and Ssend messages take an extra refernce on the send request
            //      and postpone its completion until the CTS/sync ack comes back.
            //      However we go a cancel response which means that the CTS/sync
            //      is not comming back. Thus, we need to singal and release the
            //      send request here.
            //
            sreq->signal_completion();
            sreq->Release();
        }
    }
    else
    {
        sreq->status.cancelled = FALSE;
    }

    //
    // Cancel-send take an extra refernce on the send request and postpone its
    // completion until the cancel-resp ack comes back. Singal and release the
    // send request here.
    //
    sreq->signal_completion();
    sreq->Release();
    *rreqp = NULL;

    return MPI_SUCCESS;
}


/* MPIDI_CH3_SendRndv - Send a request to perform a rendezvous send */
MPI_RESULT
MPIDI_CH3_SendRndv(
    MPID_Request* sreq,
    MPIDI_msg_sz_t data_sz
    )
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_eager_send_t* rts_pkt = &upkt.eager_send;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    //
    // N.B. The handle value is sent to support the CTS-ack.  Take a reference on
    //      the send request to keep it alive until the CTS-ack comes back.
    //
    sreq->AddRef();

    sreq->partner_request = NULL;

    MPIDI_Pkt_init(rts_pkt, MPIDI_CH3_PKT_RNDV_REQ_TO_SEND);
    rts_pkt->match.rank       = sreq->comm->rank;
    rts_pkt->match.tag        = sreq->dev.match.tag;
    rts_pkt->match.context_id = sreq->dev.match.context_id;
    rts_pkt->sender_req_id    = sreq->handle;
    rts_pkt->data_sz          = data_sz;
    rts_pkt->flags            = MPIDI_CH3_PKT_FLAGS_CLEAR;

    sreq->set_msg_type(MPIDI_REQUEST_RNDV_MSG );
    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(sreq->comm, sreq->dev.match.rank);

    mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
    if (mpi_errno != MPI_SUCCESS)
    {
        sreq->Release();
        return mpi_errno;
    }

    /* FIXME: fill temporary IOV or pack temporary buffer after send to hide
       some latency.  This requires synchronization
       because the CTS packet could arrive and be processed before the above
       iStartmsg completes (depending on the progress
       engine, threads, etc.). */

    return mpi_errno;
}


/*
 * Here are the routines that are called by the progress engine to handle
 * the various rendezvous message requests (cancel of sends is in
 * mpid_cancel_send.c).
 */

#define set_request_info(rreq_, pkt_, msg_type_)                \
{                                                               \
    (rreq_)->status.MPI_SOURCE = (pkt_)->match.rank;            \
    (rreq_)->status.MPI_TAG = (pkt_)->match.tag;                \
    (rreq_)->status.count = (pkt_)->data_sz;                    \
    (rreq_)->dev.sender_req_id = (pkt_)->sender_req_id;         \
    (rreq_)->dev.recv_data_sz = (pkt_)->data_sz;                \
    (rreq_)->set_msg_type(msg_type_);                           \
}


MPI_RESULT Handle_MPIDI_CH3_PKT_RNDV_REQ_TO_SEND(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPID_Request * rreq;
    int found;
    const MPIDI_CH3_Pkt_eager_send_t * rts_pkt = &pkt->eager_send;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    mpi_errno = MPID_Recvq_dq_posted_or_new_unexpected(
                    &rts_pkt->match,
                    &found,
                    &rreq,
                    [&](
                        _In_ MPID_Request*  rreq,
                        _In_ bool           found
                        )
                    {
                        UNREFERENCED_PARAMETER(found);
                        //
                        // We have a ref count of 1 or 2 for posted receives, or 2 for unexpected receives.
                        //
                        // For the posted receive case, one reference is for us, and optionally a second
                        // one if the user has a valid handle to the request.  It is optional because
                        // the user could have released it (MPI_Request_free.)
                        //
                        // For the unexpected case, one reference is for us, the other for being in the
                        // unexpected queue.
                        //
                        MPIU_Assert( rreq->ref_count == 2 || (rreq->ref_count == 1 && found == TRUE) );

                        set_request_info(rreq, rts_pkt, MPIDI_REQUEST_RNDV_MSG);

                        return MPI_SUCCESS;
                    });
    if (mpi_errno != MPI_SUCCESS)
    {
        *rreqp  = nullptr;
        return mpi_errno;
    }

    if (found)
    {
        MPIDI_CH3_Pkt_t upkt;
        MPIDI_CH3_Pkt_rndv_clr_to_send_t * cts_pkt = &upkt.rndv_clr_to_send;

        //
        // N.B. The receive request handle value is sent to support the rndv message.
        //      Take a reference on the receive request to keep it alive until the
        //      rndv message comes back.
        //
        rreq->AddRef();


        /* FIXME: What if the receive user buffer is not big enough to
           hold the data about to be cleared for sending? */

        MPIDI_Pkt_init(cts_pkt, MPIDI_CH3_PKT_RNDV_CLR_TO_SEND);
        cts_pkt->sender_req_id = rts_pkt->sender_req_id;
        cts_pkt->receiver_req_id = rreq->handle;
        mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
        if (mpi_errno != MPI_SUCCESS)
        {
            //
            // release the error we took in the found case and reported an error.
            //
            rreq->Release();
            return mpi_errno;
        }
    }
    *rreqp  = nullptr;

    //
    // Release our reference (it's either in the unexpected queue, or is in-flight and
    // the CTS has a reference.
    //
    rreq->Release();
    return mpi_errno;
}

MPI_RESULT Handle_MPIDI_CH3_PKT_RNDV_CLR_TO_SEND(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    const MPIDI_CH3_Pkt_rndv_clr_to_send_t * cts_pkt = &pkt->rndv_clr_to_send;
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPID_Request* sreq;
    MPID_Request_get_ptr(cts_pkt->sender_req_id, sreq);
    MPIU_Assert(sreq != nullptr);
    if(sreq == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**nullptrtype %s", "request" );
        goto fn_fail;
    }

    sreq->init_pkt(MPIDI_CH3_PKT_RNDV_SEND);
    MPIDI_CH3_Pkt_rndv_send_t* rs_pkt = &sreq->get_pkt()->rndv_send;
    rs_pkt->receiver_req_id = cts_pkt->receiver_req_id;

    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        sreq->dev.datatype.GetSizeAndInfo(
            sreq->dev.user_count,
            &dt_contig,
            &dt_true_lb
            )
        );

    if (dt_contig)
    {
        sreq->dev.OnDataAvail = 0;
        sreq->add_send_iov((char *)sreq->dev.user_buf + dt_true_lb, data_sz);

        mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        sreq->dev.segment_ptr = MPID_Segment_alloc( );
        /* if (!sreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
        MPID_Segment_init(sreq->dev.user_buf, sreq->dev.user_count,
                          sreq->dev.datatype.GetMpiHandle(), sreq->dev.segment_ptr, 0);
        sreq->dev.segment_first = 0;
        sreq->dev.segment_size = data_sz;

        mpi_errno = MPIDI_CH3_SendNoncontig(vc, sreq);
        ON_ERROR_FAIL(mpi_errno);
    }

fn_exit:
    //
    // Release the reference taken at rndv message send time
    //
    if(sreq != NULL)
    {
        sreq->Release();
    }
    *rreqp = NULL;
    return mpi_errno;

fn_fail:
    if(sreq != NULL)
    {
        sreq->status.MPI_ERROR = mpi_errno;
        sreq->signal_completion();
    }
    goto fn_exit;
}

MPI_RESULT Handle_MPIDI_CH3_PKT_RNDV_SEND(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    const MPIDI_CH3_Pkt_rndv_send_t * rs_pkt = &pkt->rndv_send;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPID_Request* rreq;
    MPID_Request_get_ptr(rs_pkt->receiver_req_id, rreq);

    MPIU_Assert(rreq != NULL);
    if(rreq == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**nullptrtype %s", "request" );
        goto fn_fail;
    }

    rreq->dev.pkt.base.flags = rs_pkt->flags;
    rreq->dev.pCompressionBuffer = reinterpret_cast<CompressionBuffer *>(
        const_cast<MPIDI_CH3_Pkt_t*>(pkt)+1
        );

    mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, rreq);
    rreq->dev.pCompressionBuffer = NULL;
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( rreq->is_internal_complete() == true )
    {
        rreq->Release();
        rreq = NULL;
    }
    *rreqp = rreq;
    return MPI_SUCCESS;

fn_fail:
    if(rreq != NULL)
    {
        rreq->Release();
    }
    *rreqp = NULL;
    return mpi_errno;
}

/*
 * This routine processes a rendezvous message once the message is matched.
 * It is used in mpid_recv and mpid_irecv.
 */
MPI_RESULT MPIDI_CH3_RecvRndv( MPIDI_VC_t * vc, MPID_Request *rreq )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    /* A rendezvous request-to-send (RTS) message has arrived.  We need
       to send a CTS message to the remote process. */

    //
    // N.B. The receive request handle value is sent to support the rndv message.
    //      Take a reference on the receive request to keep it alive until the
    //      rndv message comes back.
    //
    rreq->AddRef();

    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_rndv_clr_to_send_t * cts_pkt = &upkt.rndv_clr_to_send;

    MPIDI_Pkt_init(cts_pkt, MPIDI_CH3_PKT_RNDV_CLR_TO_SEND);
    cts_pkt->sender_req_id = rreq->dev.sender_req_id;
    cts_pkt->receiver_req_id = rreq->handle;
    mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
    if (mpi_errno != MPI_SUCCESS)
    {
        rreq->Release();
    }

    return mpi_errno;
}


/*
 * MPIDI_CH3U_Handle_recv_pkt()
 *
 * NOTE: Multiple threads may NOT simultaneously call this routine with the same VC.
 * This constraint eliminates the need to
 * lock the VC.  If simultaneous upcalls are a possible, the calling routine
 * for serializing the calls.
 */

MPI_RESULT MPIDI_CH3U_Handle_recv_pkt(MPIDI_VC_t * vc, const MPIDI_CH3_Pkt_t * pkt, MPID_Request ** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    switch(pkt->type)
    {
        case MPIDI_CH3_PKT_EAGER_SEND:
            mpi_errno = Handle_MPIDI_CH3_PKT_EAGER_SEND(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_READY_SEND:
            mpi_errno = Handle_MPIDI_CH3_PKT_READY_SEND(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_EAGER_SYNC_SEND:
            mpi_errno = Handle_MPIDI_CH3_PKT_EAGER_SYNC_SEND(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_EAGER_SYNC_ACK:
            mpi_errno = Handle_MPIDI_CH3_PKT_EAGER_SYNC_ACK(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_RNDV_REQ_TO_SEND:
            mpi_errno = Handle_MPIDI_CH3_PKT_RNDV_REQ_TO_SEND(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_RNDV_CLR_TO_SEND:
            mpi_errno = Handle_MPIDI_CH3_PKT_RNDV_CLR_TO_SEND(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_RNDV_SEND:
            mpi_errno = Handle_MPIDI_CH3_PKT_RNDV_SEND(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_CANCEL_SEND_REQ:
            mpi_errno = Handle_MPIDI_CH3_PKT_CANCEL_SEND_REQ(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_CANCEL_SEND_RESP:
            mpi_errno = Handle_MPIDI_CH3_PKT_CANCEL_SEND_RESP(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_PUT:
            mpi_errno = Handle_MPIDI_CH3_PKT_PUT(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_ACCUMULATE:
            mpi_errno = Handle_MPIDI_CH3_PKT_ACCUMULATE(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_GET:
            mpi_errno = Handle_MPIDI_CH3_PKT_GET(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_RMA_OP_RESP:
            mpi_errno = Handle_MPIDI_CH3_PKT_RMA_RESP(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_GET_ACCUMULATE:
            mpi_errno = Handle_MPIDI_CH3_PKT_GET_ACCUMULATE(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_COMPARE_AND_SWAP:
            mpi_errno = Handle_MPIDI_CH3_PKT_COMPARE_AND_SWAP(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_LOCK:
            mpi_errno = Handle_MPIDI_CH3_PKT_LOCK(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_UNLOCK:
            mpi_errno = Handle_MPIDI_CH3_PKT_UNLOCK(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_LOCK_GRANTED:
            mpi_errno = Handle_MPIDI_CH3_PKT_LOCK_GRANTED(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_UNLOCK_DONE:
            mpi_errno = Handle_MPIDI_CH3_PKT_UNLOCK_DONE(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_PT_RMA_DONE:
            mpi_errno = Handle_MPIDI_CH3_PKT_PT_RMA_DONE(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_PT_RMA_ERROR:
            mpi_errno = Handle_MPIDI_CH3_PKT_PT_RMA_ERROR(pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_LOCK_PUT_UNLOCK:
            mpi_errno = Handle_MPIDI_CH3_PKT_LOCK_PUT_UNLOCK(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_LOCK_ACCUM_UNLOCK:
            mpi_errno = Handle_MPIDI_CH3_PKT_LOCK_ACCUM_UNLOCK(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_LOCK_GET_UNLOCK:
            mpi_errno = Handle_MPIDI_CH3_PKT_LOCK_GET_UNLOCK(vc, pkt, rreqp);
            break;

        case MPIDI_CH3_PKT_CLOSE:
            mpi_errno = Handle_MPIDI_CH3_PKT_CLOSE(vc, pkt, rreqp);
            break;

        default:
        {
            *rreqp = NULL;
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**ch3|unknownpkt %d", pkt->type);
            break;
        }
    }

    if(*rreqp == NULL)
    {
        Mpi.SignalProgress();
    }

    return mpi_errno;
}


/*
 * This function is used to post a receive operation on a request for the
 * next data to arrive.  In turn, this request is attached to a virtual
 * connection.
 */
MPI_RESULT MPIDI_CH3U_Post_data_receive(int found, MPID_Request * rreq)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    if (rreq->dev.recv_data_sz == 0)
    {
        //
        // There is no data to receive, signal the receive packet as complete.
        //
        rreq->signal_completion();
        MPIU_Assert( rreq->is_internal_complete() == true );
        return MPI_SUCCESS;
    }

    if (found)
    {
        mpi_errno = MPIDI_CH3U_Post_data_receive_found(rreq);
    }
    else /* if (!found) */
    {
        mpi_errno = MPIDI_CH3U_Post_data_receive_unexpected(rreq);
    }

    return mpi_errno;
}


static MPI_RESULT check_target_window_boundaries(
    _In_ MPI_Aint target_size,
    _In_ MPI_Aint target_disp,
    _In_ const MPID_Win* pWin
)
{
    if (pWin->createFlavor != MPI_WIN_FLAVOR_DYNAMIC)
    {
        if ((target_disp + target_size < target_disp) ||
            (target_disp + target_size > pWin->size))
        {
            goto fn_fail;
        }
    }
    else
    {
        if (pWin->size == 0)
        {
            goto fn_fail;
        }
        MPIDI_Win_memory_region_t* winMemoryRegion = pWin->regions;
        bool belongs = false;
        while (winMemoryRegion != nullptr)
        {
            if (target_disp < winMemoryRegion->base)
            {
                break;
            }
            if (target_disp >= winMemoryRegion->base &&
                target_disp + target_size <= winMemoryRegion->base + winMemoryRegion->size)
            {
                belongs = true;
                break;
            }
            winMemoryRegion = winMemoryRegion->next;
        }
        if (!belongs)
        {
            goto fn_fail;
        }
    }
    return MPI_SUCCESS;
fn_fail:
    return MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestrmaoutofbounds");
}


static MPI_RESULT check_target_window_boundaries_for_request(
    _In_ const MPID_Request* pReq,
    _In_ const MPID_Win* pWin
    )
{
    MPI_Aint type_size = static_cast<MPI_Aint>(pReq->dev.datatype.GetSize());
    MPI_Aint target_size = type_size * pReq->dev.user_count;
    MPI_Aint target_disp = pReq->dev.target_disp * pWin->disp_unit;

    return check_target_window_boundaries(target_size, target_disp, pWin);
}


static MPI_RESULT process_rma_error(
    _In_    MPIDI_VC_t *vc, 
    _Inout_ MPID_Request* pReq,
    _In_    MPI_RESULT rma_error
    )
{
    MPID_Win* win;
    MPID_Win_get_ptr_valid(pReq->dev.target_win_handle, win);
    VERIFY(win != NULL);
    win->dev.error_found = true;
    win->dev.rma_error = rma_error;

    pReq->status.MPI_ERROR = rma_error;

    return MPIDI_CH3I_Send_pt_rma_error_pkt(vc, pReq->dev.source_win_handle);
}


MPI_RESULT MPIDI_CH3U_Post_data_receive_found(MPID_Request * rreq)
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPIDI_msg_sz_t userbuf_sz = fixme_cast<MPIDI_msg_sz_t>(
        rreq->dev.datatype.GetSizeAndInfo(
            rreq->dev.user_count,
            &dt_contig,
            &dt_true_lb
            )
        );

    if (rreq->kind == MPID_REQUEST_RMA_PUT)
    {
        MPID_Win *win;
        MPID_Win_get_ptr_valid(rreq->dev.target_win_handle, win);

        //
        // make sure that the target end does not wrap around the address space
        //
        MPI_RESULT rma_error = check_target_window_boundaries_for_request(rreq, win);
        if (rma_error != MPI_SUCCESS)
        {
            win->dev.error_found = true;
            win->dev.rma_error = rma_error;
            rreq->status.MPI_ERROR = rma_error;
            data_sz = 0;
        }
        else
        {
            data_sz = rreq->dev.recv_data_sz;
        }
    }
    else
    {
        if (rreq->dev.recv_data_sz <= userbuf_sz)
        {
            data_sz = rreq->dev.recv_data_sz;
        }
        else
        {
            //
            // Receive buffer too small; message truncated
            //
            rreq->status.MPI_ERROR = MPIU_ERR_CREATE(
                    MPI_ERR_TRUNCATE, 
                    "**truncate %d %d %d %d", 
                    rreq->status.MPI_SOURCE, 
                    rreq->status.MPI_TAG, 
                    rreq->dev.recv_data_sz, 
                    userbuf_sz
                );
            rreq->status.count = userbuf_sz;
            data_sz = userbuf_sz;
        }
    }

    if(!(rreq->dev.pkt.base.flags & MPIDI_CH3_PKT_IS_COMPRESSED))
    {
        if (dt_contig && data_sz == rreq->dev.recv_data_sz)
        {
            /* user buffer is contiguous and large enough to store the
               entire message.  However, we haven't yet *read* the data
               (this code describes how to read the data into the destination) */
            rreq->dev.iov[0].buf = (iovrecvbuf_t*)((char*)(rreq->dev.user_buf) + dt_true_lb);
            rreq->dev.iov[0].len = data_sz;
            rreq->dev.iov_count = 1;
            /* FIXME: We want to set the OnDataAvail to the appropriate
               function, which depends on whether this is an RMA
               request or a pt-to-pt request. */
            rreq->dev.OnDataAvail = 0;
        }
        else
        {
            /* user buffer is not contiguous or is too small to hold the entire message */
            rreq->dev.segment_ptr = MPID_Segment_alloc( );
            /* if (!rreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
            MPID_Segment_init(rreq->dev.user_buf, rreq->dev.user_count,
                              rreq->dev.datatype.GetMpiHandle(), rreq->dev.segment_ptr, 0);
            rreq->dev.segment_first = 0;
            rreq->dev.segment_size = data_sz;
            mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
            ON_ERROR_FAIL(mpi_errno);
        }
    }
    else
    {
        //
        //Handle a compressed message with data as unexpected
        //
        MPIDI_CH3U_Post_data_receive_unexpected(rreq);
        rreq->dev.recv_pending_count = 1;
    }

fn_exit:
    return mpi_errno;

fn_fail:
    goto fn_exit;
}


MPI_RESULT MPIDI_CH3U_Post_data_receive_unexpected(MPID_Request * rreq)
{
    if (rreq->dev.pkt.base.flags & MPIDI_CH3_PKT_FLAG_COMPRESSED)
    {
        rreq->dev.recv_data_sz = rreq->dev.pCompressionBuffer->size;
    }

    /* FIXME: to improve performance, allocate temporary buffer from a
       specialized buffer pool. */
    /* FIXME: to avoid memory exhaustion, integrate buffer pool management
       with flow control */

    rreq->dev.tmpbuf = MPIU_Malloc(rreq->dev.recv_data_sz);
    if (rreq->dev.tmpbuf == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    rreq->dev.tmpbuf_sz = rreq->dev.recv_data_sz;

    rreq->dev.iov[0].buf = (iovrecvbuf_t*)rreq->dev.tmpbuf;
    rreq->dev.iov[0].len = rreq->dev.recv_data_sz;
    rreq->dev.iov_count = 1;
    rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_UnpackUEBufComplete;
    InterlockedExchange(&rreq->dev.recv_pending_count, 2);

    if (rreq->dev.pkt.base.flags & MPIDI_CH3_PKT_FLAG_COMPRESSED_ALL_ZEROS)
    {
        //
        //The data has already arrived.
        //
        RtlZeroMemory(rreq->dev.tmpbuf, rreq->dev.recv_data_sz);
        InterlockedExchange(&rreq->dev.recv_pending_count, 1);
        rreq->signal_completion();
    }

    return MPI_SUCCESS;
}


/* Check if requested lock can be granted. If it can, set
   win_ptr->dev.lock_state to the new lock state and return 1. Else return 0.

   FIXME: MT: This function must be atomic because two threads could be trying
   to do the same thing, e.g., the main thread in MPI_Win_lock(source=target)
   and another thread in the progress engine.
 */
bool MPIDI_CH3I_Try_acquire_win_lock(MPID_Win *win_ptr, int requested_lock)
{
    int state = win_ptr->dev.lock_state;

    /* Locking Rules:

    Requested          Existing             Action
    --------           --------             ------
    Shared             Exclusive            Queue it
    Shared             NoLock/Shared        Grant it
    Exclusive          NoLock               Grant it
    Exclusive          Exclusive/Shared     Queue it
    */

    if(( (requested_lock == MPI_LOCK_SHARED) &&
         ((state == MPIDI_LOCKED_NOT_LOCKED) || (state == MPIDI_LOCKED_SHARED))
      ))
    {
        /* grant lock. set new lock state on window */
        win_ptr->dev.lock_state = MPIDI_LOCKED_SHARED;
        win_ptr->dev.shared_lock_ref_cnt++;
        return true;
    }

    if((requested_lock == MPI_LOCK_EXCLUSIVE) && (state == MPIDI_LOCKED_NOT_LOCKED))
    {
        /* grant lock.  set new lock state on window */
        win_ptr->dev.lock_state = MPIDI_LOCKED_EXCLUSIVE;
        return true;
    }

    /* do not grant lock */
    return false;
}


MPI_RESULT MPIDI_CH3I_Send_lock_granted_pkt(MPIDI_VC_t *vc, MPI_Win source_win_handle, int rank, int lock_type)
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_lock_granted_t *lock_granted_pkt = &upkt.lock_granted;

    /* send lock granted packet */
    MPIDI_Pkt_init(lock_granted_pkt, MPIDI_CH3_PKT_LOCK_GRANTED);
    lock_granted_pkt->win_source = source_win_handle;
    lock_granted_pkt->rank = rank;
    lock_granted_pkt->remote_lock_type = lock_type;

    return MPIDI_CH3_ffSend(vc, &upkt);
}


MPI_RESULT MPIDI_CH3I_Send_unlock_done_pkt(MPIDI_VC_t * vc, MPI_Win source_win_handle, int rank)
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_unlock_done_t *unlock_done_pkt = &upkt.unlock_done;

    /* send unlock done packet */
    MPIDI_Pkt_init(unlock_done_pkt, MPIDI_CH3_PKT_UNLOCK_DONE);
    unlock_done_pkt->win_source = source_win_handle;
    unlock_done_pkt->rank = rank;

    return MPIDI_CH3_ffSend(vc, &upkt);
}


static inline void add_win_lock(MPID_Win* win, MPIDI_Win_lock_queue* lock)
{
    /* FIXME: MT: This may need to be done atomically. */

    MPIDI_Win_lock_queue** ppLock = &win->dev.lock_queue;
    while(*ppLock != NULL)
    {
        ppLock = &(*ppLock)->next;
    }

    *ppLock = lock;
}


static inline void set_request_win_basic_info(MPID_Request* req, const MPIDI_CH3_Win_hdr_t* hdr)
{
    req->dev.user_count = hdr->count;
    req->dev.target_win_handle = hdr->target;
    req->dev.source_win_handle = hdr->source;

    /*
     * Okay to cast to size_t. Wire size is always UINT64 regardless of
     * sender word size. Local size always depends on local word size.
     * Both sides negotiated window size on creation. Thus rma op with size
     * larger than max local word is an error and will be truncated (to some
     * wrap around number).
     */
    req->dev.target_disp = static_cast<size_t>(hdr->disp);
}


static void set_request_win_info_for_builtin_type(
    _Inout_ MPID_Request* req,
    _In_ const MPIDI_CH3_Win_hdr_t* hdr,
    _Out_ MPID_Win** win_ptr
    )
{
    MPI_Aint type_size;
    MPI_Aint target_disp;
    MPI_Aint target_size;
    MPID_Win* win;

    MPIU_Assert(MPID_Datatype_is_predefined(hdr->datatype));

    set_request_win_basic_info(req, hdr);
    MPID_Win_get_ptr_valid(req->dev.target_win_handle, win);
    VERIFY(win != NULL);

    req->dev.datatype.Init( TypePool::Get( hdr->datatype ) );
    type_size = static_cast<MPI_Aint>(req->dev.datatype.GetSize());
    target_size = type_size * req->dev.user_count;
    target_disp = req->dev.target_disp * win->disp_unit;

    /* FIXME: size mismatch, dev.recv_data_sz is limited to int */
    req->dev.recv_data_sz = (MPIDI_msg_sz_t)target_size;
    req->dev.user_buf = ((char*)win->base + target_disp);
    *win_ptr = win;
}


/* ------------------------------------------------------------------------ */
/* Here are the functions that implement the packet actions.  They'll be moved
 * to more modular places where it will be easier to replace subsets of the
 * in order to experiement with alternative data transfer methods, such as
 * sending some data with a rendezvous request or including data within
 * an eager message.
 *
 * The convention for the names of routines that handle packets is
 *   MPIDI_CH3_PktHandler_<type>( MPIDI_VC_t *vc, MPIDI_CH3_Pkt_t *pkt )
 * as in
 *   MPIDI_CH3_PktHandler_EagerSend
 *
 * Each packet type also has a routine that understands how to print that
 * packet type, this routine is
 *   MPIDI_CH3_PktPrint_<type>( FILE *, MPIDI_CH3_Pkt_t * )
 *                                                                          */
/* ------------------------------------------------------------------------ */

static void SetRequestRmaFlags(
    _In_ int pktFlags,
    _Inout_ MPID_Request* pReq
)
{
    if (pktFlags & MPIDI_CH3_PKT_FLAG_RMA_LAST_OP)
    {
        pReq->set_rma_last_op();
    }
    if (pktFlags & MPIDI_CH3_PKT_FLAG_RMA_UNLOCK)
    {
        pReq->set_rma_unlock();
    }
}


MPI_RESULT Handle_MPIDI_CH3_PKT_PUT(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_put_accum_t * put_pkt = &pkt->put_accum;
    MPID_Request *req = NULL;

    if (put_pkt->win.count == 0)
    {
        MPID_Win *win_ptr;

        /* it's a 0-byte message sent just to decrement the
           completion counter. This happens only in
           post/start/complete/wait sync model; therefore, no need
           to check lock queue. */
        if (put_pkt->win.target != MPI_WIN_NULL)
        {
            MPID_Win_get_ptr(put_pkt->win.target, win_ptr);
            MPIU_Assert(win_ptr != NULL);
            MPID_Win_valid_ptr(win_ptr, mpi_errno);
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            /* FIXME: MT: this has to be done atomically */
            win_ptr->dev.my_counter -= 1;
        }
        *rreqp = NULL;
        return MPI_SUCCESS;
    }

    req = MPID_Request_create(MPID_REQUEST_RMA_PUT);
    if( req == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    SetRequestRmaFlags(put_pkt->flags, req);

    if (MPID_Datatype_is_predefined(put_pkt->win.datatype))
    {
        MPID_Win *win_ptr;

        /* simple builtin datatype */
        req->set_type(MPIDI_REQUEST_TYPE_PUT_RESP);
        set_request_win_info_for_builtin_type(req, &put_pkt->win, &win_ptr);

        mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail2;
        }

        //
        // check for RMA errors
        //
        if (win_ptr->dev.error_found)
        {
            mpi_errno = MPIDI_CH3I_Send_pt_rma_error_pkt(vc, req->dev.source_win_handle);
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }
        }

        //
        // We trapped the case where count was zero above.
        //
        MPIU_Assert( req->is_internal_complete() == false );

        /* FIXME:  Only change the handling of completion if
           post_data_receive reset the handler.  There should
           be a cleaner way to do this */
        if (!req->dev.OnDataAvail)
        {
            req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_PutRespComplete;
        }
    }
    else
    {
        /* derived datatype */
        req->set_type(MPIDI_REQUEST_TYPE_PUT_RESP_DERIVED_DT);
        set_request_win_basic_info(req, &put_pkt->win);
        req->dev.user_buf = NULL;

        req->dev.dtype_info =
            static_cast<MPIDI_RMA_dtype_info*>( MPIU_Malloc( sizeof(MPIDI_RMA_dtype_info) ) );
        if( req->dev.dtype_info == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail2;
        }

        req->dev.dataloop =
            static_cast<DLOOP_Dataloop*>( MPIU_Malloc( put_pkt->dataloop_size ) );
        if( req->dev.dataloop == nullptr )
        {
            MPIU_Free( req->dev.dtype_info );
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail2;
        }

        req->dev.iov[0].buf = (iovrecvbuf_t*)req->dev.dtype_info;
        req->dev.iov[0].len = sizeof(MPIDI_RMA_dtype_info);
        req->dev.iov[1].buf = (iovrecvbuf_t*)req->dev.dataloop;
        req->dev.iov[1].len = put_pkt->dataloop_size;
        req->dev.iov_count = 2;

        req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_PutRespDerivedDTComplete;
    }
    *rreqp = req;
    return MPI_SUCCESS;

fn_fail2:
    req->Release();
fn_fail:
    *rreqp = NULL;
    return mpi_errno;
}


static MPI_RESULT InitRespPacket(
    _Inout_ MPID_Request *req,
    _In_    MPI_Request sourceReq
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    req->init_pkt(MPIDI_CH3_PKT_RMA_OP_RESP);
    MPIDI_CH3_Pkt_rma_resp_t* rma_resp_pkt = &req->get_pkt()->rma_resp;
    rma_resp_pkt->request_handle = sourceReq;

    MPID_Win* win_ptr;
    MPID_Win_get_ptr_valid(req->dev.target_win_handle, win_ptr);
    MPIU_Assert(win_ptr != NULL);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    rma_resp_pkt->rank = win_ptr->comm_ptr->rank;
    rma_resp_pkt->win_source = req->dev.source_win_handle;

    if (req->rma_unlock_needed())
    {
        rma_resp_pkt->flags |= MPIDI_CH3_PKT_FLAG_RMA_UNLOCK;
    }

    return MPI_SUCCESS;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_GET(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno;
    const MPIDI_CH3_Pkt_get_t * get_pkt = &pkt->get;
    MPID_Request *req = MPID_Request_create(MPID_REQUEST_UNDEFINED);
    if( req == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    SetRequestRmaFlags(get_pkt->flags, req);

    if (MPID_Datatype_is_predefined(get_pkt->win.datatype))
    {
        MPID_Win* win_ptr;

        /* basic datatype. send the data. */
        req->set_type(MPIDI_REQUEST_TYPE_GET_RESP);
        req->kind = MPID_REQUEST_SEND;
        req->dev.OnDataAvail = MPIDI_CH3_SendHandler_GetSendRespComplete;
        req->dev.OnFinal     = MPIDI_CH3_SendHandler_GetSendRespComplete;

        set_request_win_info_for_builtin_type(req, &get_pkt->win, &win_ptr);

       MPI_RESULT rma_error = check_target_window_boundaries_for_request(req, win_ptr);
       if (rma_error != MPI_SUCCESS)
       {
           mpi_errno = process_rma_error(vc, req, rma_error);
           ON_ERROR_FAIL(mpi_errno);

           req->Release();
           *rreqp = NULL;
           return MPI_SUCCESS;
       }

       mpi_errno = InitRespPacket(req, get_pkt->request_handle);
       ON_ERROR_FAIL(mpi_errno);

        req->add_send_iov(req->dev.user_buf, req->dev.recv_data_sz);

        mpi_errno = MPIDI_CH3_SendRequest(vc, req);

        req->Release();
        *rreqp = NULL;
        return mpi_errno;
    }

    /* derived datatype. first get the dtype_info and dataloop. */

    req->set_type(MPIDI_REQUEST_TYPE_GET_RESP_DERIVED_DT);
    req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_GetRespDerivedDTComplete;
    req->dev.OnFinal     = 0;
    set_request_win_basic_info(req, &get_pkt->win);
    req->dev.user_buf = NULL;
    req->dev.request_handle = get_pkt->request_handle;

    req->dev.source_win_handle = get_pkt->win.source;
    req->dev.target_win_handle = get_pkt->win.target;

    req->dev.dtype_info = static_cast<MPIDI_RMA_dtype_info*>(
            MPIU_Malloc( sizeof(MPIDI_RMA_dtype_info) )
            );
    if( req->dev.dtype_info == nullptr )
    {
        goto fn_fail;
    }

    req->dev.dataloop = static_cast<DLOOP_Dataloop*>(
            MPIU_Malloc( get_pkt->dataloop_size )
            );
    if( req->dev.dataloop == nullptr )
    {
        MPIU_Free( req->dev.dtype_info );
        goto fn_fail;
    }

    req->dev.iov[0].buf = (iovrecvbuf_t*)req->dev.dtype_info;
    req->dev.iov[0].len = sizeof(MPIDI_RMA_dtype_info);
    req->dev.iov[1].buf = (iovrecvbuf_t*)req->dev.dataloop;
    req->dev.iov[1].len = get_pkt->dataloop_size;
    req->dev.iov_count = 2;

    *rreqp = req;
    return MPI_SUCCESS;

fn_fail:
    req->Release();
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_RMA_RESP(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int type_size;
    const MPIDI_CH3_Pkt_rma_resp_t * rma_resp_pkt = &pkt->rma_resp;
    MPID_Request *req;

    MPID_Request_get_ptr(rma_resp_pkt->request_handle, req);
    if (req == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**nullptrtype %s", "request");
        goto fn_fail;
    }

    type_size = fixme_cast<int>(req->dev.datatype.GetSize());
    req->dev.recv_data_sz = type_size * req->dev.user_count;

    mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
    ON_ERROR_FAIL(mpi_errno);

    MPID_Win *win_ptr;

    MPID_Win_get_ptr(rma_resp_pkt->win_source, win_ptr);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    int target = rma_resp_pkt->rank;

    if (rma_resp_pkt->flags & MPIDI_CH3_PKT_FLAG_RMA_UNLOCK)
    {
        win_ptr->dev.remote_lock_state[target] = MPIDI_LOCKED_NOT_LOCKED;
    }

    if (req->is_internal_complete() == true)
    {
        req->Release();
        req = NULL;
    }

fn_fail:
    *rreqp = req;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_ACCUMULATE(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    const MPIDI_CH3_Pkt_put_accum_t * accum_pkt = &pkt->put_accum;
    MPID_Request *req;
    MPI_RESULT mpi_errno;

    req = MPID_Request_create(MPID_REQUEST_UNDEFINED);
    if( req == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    SetRequestRmaFlags(accum_pkt->flags, req);

    if (MPID_Datatype_is_predefined(accum_pkt->win.datatype))
    {
        MPID_Win *win_ptr;

        /* simple builtin datatype */
        req->set_type(MPIDI_REQUEST_TYPE_ACCUM_RESP);
        set_request_win_info_for_builtin_type(req, &accum_pkt->win, &win_ptr);

        req->dev.op = accum_pkt->op;
        req->dev.accum_user_buf = req->dev.user_buf;

        void* tmp_buf = MPIU_Malloc( req->dev.recv_data_sz );
        if( tmp_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        req->dev.user_buf = tmp_buf;

        mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
        //
        // BUGBUG: need to check that when an accumulate is initiated, we properly trap
        // for a count of zero.
        //
        MPIU_Assert( req->is_internal_complete() == false );
        /* FIXME:  Only change the handling of completion if
           post_data_receive reset the handler.  There should
           be a cleaner way to do this */
        if (!req->dev.OnDataAvail)
        {
            req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_AccumRespComplete;
        }

        if( mpi_errno != MPI_SUCCESS )
        {
            MPIU_Free( tmp_buf );
            req->dev.user_buf = nullptr;
            goto fn_fail;
        }
    }
    else
    {
        /* derived datatype */
        req->set_type(MPIDI_REQUEST_TYPE_ACCUM_RESP_DERIVED_DT);
        req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_AccumRespDerivedDTComplete;
        set_request_win_basic_info(req, &accum_pkt->win);
        req->dev.user_buf = NULL;
        req->dev.accum_user_buf = NULL;
        req->dev.op = accum_pkt->op;

        req->dev.dtype_info = static_cast<MPIDI_RMA_dtype_info*>(
                MPIU_Malloc( sizeof(MPIDI_RMA_dtype_info) )
                );
        if( req->dev.dtype_info == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        req->dev.dataloop = static_cast<DLOOP_Dataloop*>(
                MPIU_Malloc( accum_pkt->dataloop_size )
                );
        if( req->dev.dataloop == nullptr )
        {
            MPIU_Free( req->dev.dtype_info );
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        req->dev.iov[0].buf = (iovrecvbuf_t*)req->dev.dtype_info;
        req->dev.iov[0].len = sizeof(MPIDI_RMA_dtype_info);
        req->dev.iov[1].buf = (iovrecvbuf_t*)req->dev.dataloop;
        req->dev.iov[1].len = accum_pkt->dataloop_size;
        req->dev.iov_count = 2;
    }

    *rreqp = req;
    return MPI_SUCCESS;

fn_fail:
    req->Release();
    *rreqp = NULL;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_GET_ACCUMULATE(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    //
    // get part
    //
    MPI_RESULT mpi_errno;

    const MPIDI_CH3_Pkt_get_accum_t * get_accum_pkt = &pkt->get_accum;

    MPID_Request *req;

    req = MPID_Request_create(MPID_REQUEST_UNDEFINED);
    if (req == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    SetRequestRmaFlags(get_accum_pkt->flags, req);

    if (MPID_Datatype_is_predefined(get_accum_pkt->win.datatype))
    {
        MPID_Win* win_ptr;

        req->set_type(MPIDI_REQUEST_TYPE_GET_ACCUM_RESP);
        set_request_win_info_for_builtin_type(req, &get_accum_pkt->win, &win_ptr);
        MPI_RESULT rma_error = check_target_window_boundaries_for_request(req, win_ptr);
        if (rma_error != MPI_SUCCESS)
        {
            mpi_errno = process_rma_error(vc, req, rma_error);
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }
            req->Release();
            *rreqp = NULL;
            return MPI_SUCCESS;
        }

        req->dev.op = get_accum_pkt->op;
        req->dev.accum_user_buf = req->dev.user_buf;
        req->dev.request_handle = get_accum_pkt->request_handle;

        void* tmp_buf = MPIU_Malloc(req->dev.recv_data_sz);
        if (tmp_buf == nullptr)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        req->dev.user_buf = tmp_buf;

        mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
        //
        // BUGBUG: need to check that when an accumulate is initiated, we properly trap
        // for a count of zero.
        //
        MPIU_Assert(req->is_internal_complete() == false);
        /* FIXME:  Only change the handling of completion if
        post_data_receive reset the handler.  There should
        be a cleaner way to do this */
        if (!req->dev.OnDataAvail)
        {
            req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_GetAccumulateRespPredefinedDTComplete;
        }

        if (mpi_errno != MPI_SUCCESS)
        {
            MPIU_Free(tmp_buf);
            req->dev.user_buf = nullptr;
            goto fn_fail;
        }
    }
    else
    {
        //
        // Derived datatype: first get the dtype_info and dataloop.
        //
        req->set_type(MPIDI_REQUEST_TYPE_GET_ACCUM_RESP_DERIVED_DT);
        req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_GetAccumulateRespDerivedDTComplete;
        req->dev.OnFinal = 0;
        set_request_win_basic_info(req, &get_accum_pkt->win);
        req->dev.user_buf = NULL;
        req->dev.request_handle = get_accum_pkt->request_handle;
        req->dev.op = get_accum_pkt->op;

        req->dev.dtype_info = static_cast<MPIDI_RMA_dtype_info*>(
            MPIU_Malloc(sizeof(MPIDI_RMA_dtype_info))
            );
        if (req->dev.dtype_info == nullptr)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        req->dev.dataloop = static_cast<DLOOP_Dataloop*>(
            MPIU_Malloc(get_accum_pkt->dataloop_size)
            );
        if (req->dev.dataloop == nullptr)
        {
            MPIU_Free(req->dev.dtype_info);
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        req->dev.iov[0].buf = (iovrecvbuf_t*)req->dev.dtype_info;
        req->dev.iov[0].len = sizeof(MPIDI_RMA_dtype_info);
        req->dev.iov[1].buf = (iovrecvbuf_t*)req->dev.dataloop;
        req->dev.iov[1].len = get_accum_pkt->dataloop_size;
        req->dev.iov_count = 2;

        *rreqp = req;
        return MPI_SUCCESS;

    }

    *rreqp = req;
    return MPI_SUCCESS;

fn_fail:
    req->Release();
    *rreqp = nullptr;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_COMPARE_AND_SWAP(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    //
    // get part
    //
    MPI_RESULT mpi_errno;

    const MPIDI_CH3_Pkt_compare_and_swap_t * compare_and_swap_pkt = &pkt->compare_and_swap;

    MPID_Request *req;
    MPID_Win* win_ptr;

    //
    // compare and swap only works on predefined types
    //
    MPIU_Assert(MPID_Datatype_is_predefined(compare_and_swap_pkt->win.datatype));

    //
    // compare_and_swap part
    //
    req = MPID_Request_create(MPID_REQUEST_UNDEFINED);
    if (req == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    SetRequestRmaFlags(compare_and_swap_pkt->flags, req);

    req->set_type(MPIDI_REQUEST_TYPE_COMPARE_AND_SWAP_RESP);
    set_request_win_info_for_builtin_type(req, &compare_and_swap_pkt->win, &win_ptr);

    MPI_RESULT rma_error = check_target_window_boundaries(
        static_cast<MPI_Aint>(req->dev.datatype.GetSize()),
        req->dev.target_disp,
        win_ptr
        ); 
    if (rma_error != MPI_SUCCESS)
    {
        mpi_errno = process_rma_error(vc, req, rma_error);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
        req->Release();
        *rreqp = NULL;
        return MPI_SUCCESS;
    }

    req->dev.request_handle = compare_and_swap_pkt->request_handle;
    req->dev.accum_user_buf = req->dev.user_buf;

    void* tmp_buf = MPIU_Malloc(req->dev.recv_data_sz);
    if (tmp_buf == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    req->dev.user_buf = tmp_buf;

    mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
    if (mpi_errno != MPI_SUCCESS)
    {
        MPIU_Free(tmp_buf);
        req->dev.user_buf = nullptr;
        goto fn_fail;
    }

    MPIU_Assert(req->is_internal_complete() == false);

    if (!req->dev.OnDataAvail)
    {
        req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_CompareSwapRespComplete;
    }

    *rreqp = req;
    return MPI_SUCCESS;

fn_fail:
    req->Release();
    *rreqp = nullptr;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_lock_t * lock_pkt = &pkt->lock;
    MPID_Win *win_ptr;
    MPIDI_Win_lock_queue* lock;

    *rreqp = NULL;

    MPID_Win_get_ptr(lock_pkt->win_target, win_ptr);
    MPIU_Assert(win_ptr != NULL);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if (MPIDI_CH3I_Try_acquire_win_lock(win_ptr, lock_pkt->lock_type))
    {
        /* send lock granted packet. */
        mpi_errno = MPIDI_CH3I_Send_lock_granted_pkt(
            vc,
            lock_pkt->win_source,
            win_ptr->comm_ptr->rank,
            lock_pkt->lock_type
            );
        return mpi_errno;
    }

    /* queue the lock information */
    lock = MPIU_Malloc_obj(MPIDI_Win_lock_queue);
    if( lock == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    lock->next = NULL;
    lock->lock_type = lock_pkt->lock_type;
    lock->src_win = lock_pkt->win_source;
    lock->vc = vc;
    lock->pt_single_op = NULL;

    add_win_lock(win_ptr, lock);
    return MPI_SUCCESS;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_unlock_t * unlock_pkt = &pkt->unlock;
    MPID_Win *win_ptr;

    *rreqp = NULL;

    MPID_Win_get_ptr(unlock_pkt->win_target, win_ptr);
    MPIU_Assert(win_ptr != NULL);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    ON_ERROR_FAIL(mpi_errno);

    MPIU_Assert(win_ptr->dev.lock_state != MPIDI_LOCKED_NOT_LOCKED);
    mpi_errno =  MPIDI_CH3I_Release_lock(win_ptr);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_CH3I_Send_unlock_done_pkt(
        vc,
        unlock_pkt->win_source,
        win_ptr->comm_ptr->rank
    );

fn_fail:
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_GRANTED(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_lock_granted_t * lock_granted_pkt = &pkt->lock_granted;
    MPID_Win *win_ptr;

    MPID_Win_get_ptr_valid(lock_granted_pkt->win_source, win_ptr);
    MPIU_Assert(win_ptr != NULL);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    //
    // set the remote_lock_state on the window
    //
    int target = lock_granted_pkt->rank;
    win_ptr->dev.remote_lock_state[target] = lock_granted_pkt->remote_lock_type;

    *rreqp = NULL;
fn_fail:
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_UNLOCK_DONE(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_unlock_done_t * unlock_done_pkt = &pkt->unlock_done;
    MPID_Win *win_ptr;

    MPID_Win_get_ptr_valid(unlock_done_pkt->win_source, win_ptr);
    MPIU_Assert(win_ptr != NULL);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    //
    // set the remote_lock_state on the window
    //
    int target = unlock_done_pkt->rank;
    win_ptr->dev.remote_lock_state[target] = MPIDI_LOCKED_NOT_LOCKED;

    *rreqp = NULL;
fn_fail:
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_PT_RMA_DONE(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_pt_rma_done_t * pt_rma_done_pkt = &pkt->pt_rma_done;
    MPID_Win *win_ptr;

    MPID_Win_get_ptr(pt_rma_done_pkt->win_source, win_ptr);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    int target = pt_rma_done_pkt->rank;
    win_ptr->dev.remote_ops_done[target] = 1;

    //
    // reset the remote_lock_state value in the window
    //
    if (pt_rma_done_pkt->flags & MPIDI_CH3_PKT_FLAG_RMA_UNLOCK)
    {
        win_ptr->dev.remote_lock_state[target] = MPIDI_LOCKED_NOT_LOCKED;
    }

fn_fail:
    *rreqp = NULL;
    return MPI_SUCCESS;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_PT_RMA_ERROR(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_pt_rma_error_t * pt_rma_error_pkt = &pkt->pt_rma_error;
    MPID_Win *win_ptr;

    MPID_Win_get_ptr(pt_rma_error_pkt->win_source, win_ptr);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* set the error flag in the window */
    win_ptr->dev.error_found = true;

fn_fail:
    *rreqp = NULL;
    return MPI_SUCCESS;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_PUT_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno;
    const MPIDI_CH3_Pkt_lock_put_accum_unlock_t * lock_put_unlock_pkt = &pkt->lock_put_accum_unlock;
    MPID_Win *win_ptr;
    MPID_Request *req;

    req = MPID_Request_create(MPID_REQUEST_RMA_PUT);
    if( req == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    req->set_rma_last_op();
    req->set_rma_unlock();

    set_request_win_info_for_builtin_type(req, &lock_put_unlock_pkt->win, &win_ptr);

    if (MPIDI_CH3I_Try_acquire_win_lock(win_ptr, lock_put_unlock_pkt->lock_type))
    {
        /* do the put. for this optimization, only basic datatypes supported. */
        req->set_type(MPIDI_REQUEST_TYPE_PUT_RESP);
        req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_PutRespComplete;
        req->dev.single_op_opt = true;
        mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
        //
        // BUGBUG: need to check that when an accumulate is initiated, we properly trap
        // for a count of zero.
        //
        MPIU_Assert( req->is_internal_complete() == false );
        /* FIXME:  Only change the handling of completion if
           post_data_receive reset the handler.  There should
           be a cleaner way to do this */
        if (!req->dev.OnDataAvail)
        {
            req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_PutRespComplete;
        }

        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* queue the information */
        StackGuardPointer<MPIDI_PT_single_op, CppDeallocator<void>> single_put =
            static_cast<MPIDI_PT_single_op*>(
                MPIU_Malloc( sizeof(MPIDI_PT_single_op) + req->dev.recv_data_sz - 1 )
                );
        StackGuardPointer<MPIDI_Win_lock_queue, CppDeallocator<void>> lock =
            static_cast<MPIDI_Win_lock_queue*>(
                MPIU_Malloc( sizeof(MPIDI_Win_lock_queue) )
                );
        if( single_put == nullptr || lock == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        single_put->type = MPIDI_RMA_OP_PUT;
        single_put->addr = req->dev.user_buf;
        single_put->count = req->dev.user_count;
        single_put->datatype = req->dev.datatype;
        single_put->data_recd = 0;
        single_put->op = MPI_REPLACE;

        lock->next = NULL;
        lock->lock_type = lock_put_unlock_pkt->lock_type;
        lock->src_win = lock_put_unlock_pkt->win.source;
        lock->vc = vc;
        lock->pt_single_op = single_put;

        req->set_type(MPIDI_REQUEST_TYPE_PT_SINGLE_PUT);
        req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_SinglePutAccumComplete;
        req->dev.user_buf = single_put->data;
        req->dev.lock_queue_entry = lock;

        mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
        //
        // BUGBUG: need to check that when an accumulate is initiated, we properly trap
        // for a count of zero.
        //
        MPIU_Assert( req->is_internal_complete() == false );
        /* FIXME:  Only change the handling of completion if
           post_data_receive reset the handler.  There should
           be a cleaner way to do this */
        if (!req->dev.OnDataAvail)
        {
            req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_SinglePutAccumComplete;
        }

        ON_ERROR_FAIL(mpi_errno);

        add_win_lock(win_ptr, lock.detach());
        single_put.detach();
    }
    *rreqp = req;
    return MPI_SUCCESS;

fn_fail:
    req->Release();
    *rreqp = NULL;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_GET_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno;
    const MPIDI_CH3_Pkt_lock_get_unlock_t * lock_get_unlock_pkt = &pkt->lock_get_unlock;
    MPID_Request *req;
    MPID_Win *win_ptr;

    req = MPID_Request_create(MPID_REQUEST_UNDEFINED);
    if( req == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    req->set_rma_last_op();
    req->set_rma_unlock();

    set_request_win_info_for_builtin_type(req, &lock_get_unlock_pkt->win, &win_ptr);

    MPI_RESULT rma_error = check_target_window_boundaries_for_request(req, win_ptr);
    if (rma_error != MPI_SUCCESS)
    {
        mpi_errno = process_rma_error(vc, req, rma_error);
        ON_ERROR_FAIL(mpi_errno);

        req->Release();
        *rreqp = NULL;
        return MPI_SUCCESS;
    }

    if (MPIDI_CH3I_Try_acquire_win_lock(win_ptr, lock_get_unlock_pkt->lock_type))
    {
        /* do the get. for this optimization, only basic datatypes supported. */
        req->set_type(MPIDI_REQUEST_TYPE_GET_RESP);
        req->dev.OnDataAvail = MPIDI_CH3_SendHandler_GetSendRespComplete;
        req->dev.OnFinal     = MPIDI_CH3_SendHandler_GetSendRespComplete;
        req->kind = MPID_REQUEST_SEND;
        req->dev.single_op_opt = true;

        mpi_errno = InitRespPacket(req, lock_get_unlock_pkt->request_handle);
        ON_ERROR_FAIL(mpi_errno);

        req->add_send_iov(req->dev.user_buf, req->dev.recv_data_sz);

        mpi_errno = MPIDI_CH3_SendRequest(vc, req);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* queue the information */
        MPIDI_PT_single_op* single_get = static_cast<MPIDI_PT_single_op*>(
                MPIU_Malloc( sizeof(MPIDI_PT_single_op) )
                );
        if( single_get == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        MPIDI_Win_lock_queue* lock = static_cast<MPIDI_Win_lock_queue*>(
                MPIU_Malloc( sizeof(MPIDI_Win_lock_queue) )
                );
        if( lock == nullptr )
        {
            MPIU_Free( single_get );
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        single_get->type = MPIDI_RMA_OP_GET;
        single_get->addr = req->dev.user_buf;
        single_get->count = req->dev.user_count;
        single_get->datatype = req->dev.datatype;
        single_get->request_handle = lock_get_unlock_pkt->request_handle;
        single_get->data_recd = 1;

        lock->next = NULL;
        lock->lock_type = lock_get_unlock_pkt->lock_type;
        lock->src_win = lock_get_unlock_pkt->win.source;
        lock->vc = vc;
        lock->pt_single_op = single_get;

        add_win_lock(win_ptr, lock);
    }

    req->Release();
    *rreqp = NULL;
    return MPI_SUCCESS;

fn_fail:
    req->Release();
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_ACCUM_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno;
    const MPIDI_CH3_Pkt_lock_put_accum_unlock_t * lock_accum_unlock_pkt = &pkt->lock_put_accum_unlock;
    MPID_Request *req;
    MPID_Win *win_ptr;

    req = MPID_Request_create(MPID_REQUEST_RMA_PUT);
    if( req == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    req->set_rma_last_op();
    req->set_rma_unlock();

    set_request_win_info_for_builtin_type(req, &lock_accum_unlock_pkt->win, &win_ptr);

    /* no need to acquire the lock here because we need to receive the
       data into a temporary buffer first */

    /* queue the information */

    MPIDI_PT_single_op* single_accum = static_cast<MPIDI_PT_single_op*>(
            MPIU_Malloc( sizeof(MPIDI_PT_single_op) + req->dev.recv_data_sz - 1)
            );
    if( single_accum == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    MPIDI_Win_lock_queue* lock = static_cast<MPIDI_Win_lock_queue*>(
            MPIU_Malloc( sizeof(MPIDI_Win_lock_queue) )
            );
    if( lock == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail2;
    }

    single_accum->type = MPIDI_RMA_OP_ACCUMULATE;
    single_accum->addr = req->dev.user_buf;
    single_accum->count = req->dev.user_count;
    single_accum->datatype = req->dev.datatype;
    single_accum->data_recd = 0;
    single_accum->op = lock_accum_unlock_pkt->op;

    lock->next = NULL;
    lock->lock_type = lock_accum_unlock_pkt->lock_type;
    lock->src_win = lock_accum_unlock_pkt->win.source;
    lock->vc = vc;
    lock->pt_single_op = single_accum;

    req->set_type(MPIDI_REQUEST_TYPE_PT_SINGLE_ACCUM);
    req->dev.user_buf = single_accum->data;
    req->dev.lock_queue_entry = lock;

    mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, req);
    //
    // BUGBUG: need to check that when an accumulate is initiated, we properly trap
    // for a count of zero.
    //
    MPIU_Assert( req->is_internal_complete() == false );

    /* FIXME:  Only change the handling of completion if
       post_data_receive reset the handler.  There should
       be a cleaner way to do this */
    if (!req->dev.OnDataAvail)
    {
        req->dev.OnDataAvail = MPIDI_CH3_RecvHandler_SinglePutAccumComplete;
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail3;
    }

    add_win_lock(win_ptr, lock);

    *rreqp = req;
    return MPI_SUCCESS;

fn_fail3:
    MPIU_Free( lock );
fn_fail2:
    MPIU_Free( single_accum );
fn_fail:
    req->Release();
    *rreqp = NULL;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_CLOSE(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const MPIDI_CH3_Pkt_close_t * close_pkt = &pkt->close;

    if (vc->state == MPIDI_VC_STATE_LOCAL_CLOSE)
    {
        MPIDI_CH3_Pkt_t upkt;
        MPIDI_CH3_Pkt_close_t * resp_pkt = &upkt.close;

        MPIDI_Pkt_init(resp_pkt, MPIDI_CH3_PKT_CLOSE);
        resp_pkt->ack = TRUE;

        mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
        ON_ERROR_FAIL(mpi_errno);
    }

    if (close_pkt->ack == FALSE)
    {
        if (vc->state == MPIDI_VC_STATE_LOCAL_CLOSE)
        {
            vc->state = MPIDI_VC_STATE_CLOSE_ACKED;
        }
        else /* (vc->state == MPIDI_VC_STATE_ACTIVE) */
        {
            MPIU_Assert(vc->state == MPIDI_VC_STATE_ACTIVE);
            vc->state = MPIDI_VC_STATE_REMOTE_CLOSE;
        }
    }
    else
    {
        MPIU_Assert (vc->state == MPIDI_VC_STATE_LOCAL_CLOSE ||
                     vc->state == MPIDI_VC_STATE_CLOSE_ACKED);

        vc->state = MPIDI_VC_STATE_CLOSE_ACKED;
        /* For example, with sockets, Connection_terminate will close
           the socket */
        mpi_errno = MPIDI_CH3_Connection_terminate(vc);
    }

    *rreqp = NULL;
fn_fail:
    return mpi_errno;
}


/* ----------------------------------------------------------------------- */
/* Here are the functions that implement the actions that are taken when
 * data is available for a receive request (or other completion operations)
 * These include "receive" requests that are part of the RMA implementation.
 *
 * The convention for the names of routines that are called when data is
 * available is
 *    MPIDI_CH3_ReqHandler_<type>( MPIDI_VC_t *, MPID_Request *, int * )
 * as in
 *    MPIDI_CH3_ReqHandler_...
 *
 * ToDo:
 *    We need a way for each of these functions to describe what they are,
 *    so that given a pointer to one of these functions, we can retrieve
 *    a description of the routine.  We may want to use a static string
 *    and require the user to maintain thread-safety, at least while
 *    accessing the string.
 */
/* ----------------------------------------------------------------------- */
static MPI_RESULT create_derived_datatype(const MPID_Request *req, MPID_Datatype **dtp)
{
    MPIDI_RMA_dtype_info *dtype_info;
    const void *dataloop;
    MPID_Datatype *new_dtp;
    MPI_Aint ptrdiff;

    dtype_info = req->dev.dtype_info;
    dataloop = req->dev.dataloop;

    /* allocate new datatype object and handle */
    new_dtp = TypePool::Alloc();
    if( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_dtp->is_permanent = false;
    new_dtp->is_committed = true;
    new_dtp->attributes   = 0;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->is_contig = dtype_info->is_contig;
    new_dtp->max_contig_blocks = dtype_info->max_contig_blocks;
    new_dtp->size = dtype_info->size;
    new_dtp->extent = dtype_info->extent;
    new_dtp->dataloop_size = dtype_info->dataloop_size;
    new_dtp->dataloop_depth = dtype_info->dataloop_depth;
    new_dtp->eltype = dtype_info->eltype;
    /* set dataloop pointer */
    new_dtp->dataloop = req->dev.dataloop;

    new_dtp->ub = dtype_info->ub;
    new_dtp->lb = dtype_info->lb;
    new_dtp->true_ub = dtype_info->true_ub;
    new_dtp->true_lb = dtype_info->true_lb;
    new_dtp->has_sticky_ub = dtype_info->has_sticky_ub;
    new_dtp->has_sticky_lb = dtype_info->has_sticky_lb;
    /* update pointers in dataloop */
    ptrdiff = (MPI_Aint)((char *) (new_dtp->dataloop) - (char *)
                         (dtype_info->dataloop));

    /* FIXME: Temp to avoid SEGV when memory tracing */
    new_dtp->hetero_dloop = 0;

    MPID_Dataloop_update(new_dtp->dataloop, ptrdiff);

    new_dtp->contents = NULL;

    *dtp = new_dtp;

    return MPI_SUCCESS;
}


static MPI_RESULT do_accumulate_op(MPID_Request *rreq)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPI_Count true_lb;
    MPI_User_function *uop;

    if (rreq->dev.op == MPI_REPLACE)
    {
        /* simply copy the data */
        mpi_errno = MPIR_Localcopy(rreq->dev.user_buf, rreq->dev.user_count,
                                   rreq->dev.datatype,
                                   rreq->dev.accum_user_buf,
                                   rreq->dev.user_count,
                                   rreq->dev.datatype);
        ON_ERROR_FAIL(mpi_errno);
        goto fn_exit;
    }

    if (HANDLE_GET_TYPE(rreq->dev.op) == HANDLE_TYPE_BUILTIN)
    {
        /* get the function by indexing into the op table */
        uop = MPIR_Op_table[(rreq->dev.op)%16 - 1];
    }
    else
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opnotpredefined %d", rreq->dev.op );
        return mpi_errno;
    }

    if( rreq->dev.datatype.IsPredefined() )
    {
        MPI_Datatype hType = rreq->dev.datatype.GetMpiHandle();

        MPID_Win* win;
        MPID_Win_get_ptr_valid(rreq->dev.target_win_handle, win);
        VERIFY(win != NULL);

        mpi_errno = check_target_window_boundaries_for_request(rreq, win);
        if (mpi_errno != MPI_SUCCESS)
        {
            rreq->status.MPI_ERROR = mpi_errno;
            win->dev.error_found = true;
            win->dev.rma_error = mpi_errno;
            return MPI_SUCCESS;
        }

        (*uop)(rreq->dev.user_buf, rreq->dev.accum_user_buf,
               &(rreq->dev.user_count), &hType);
    }
    else
    {
        /* derived datatype */
        MPID_Segment *segp;
        MPID_IOV *dloop_vec;
        MPI_Aint first, last;
        int vec_len, i, type_size, count;
        MPI_Datatype type;
        const MPID_Datatype *dtp;

        segp = MPID_Segment_alloc();
        if (!segp)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            return mpi_errno;
        }
        MPID_Segment_init(NULL, rreq->dev.user_count,
                          rreq->dev.datatype.GetMpiHandle(), segp, 0);
        first = 0;
        last  = SEGMENT_IGNORE_LAST;

        dtp = rreq->dev.datatype.Get();
        vec_len = dtp->max_contig_blocks * rreq->dev.user_count + 1;
        /* +1 needed because Rob says so */
        dloop_vec = MPIU_Malloc_objn(vec_len, MPID_IOV);
        if (!dloop_vec)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            return mpi_errno;
        }

        MPID_Segment_pack_vector(segp, first, &last, dloop_vec, &vec_len);

        type = dtp->eltype;
        type_size = MPID_Datatype_get_size(type);
        for (i=0; i<vec_len; i++)
        {
            count = (dloop_vec[i].len)/type_size;
            (*uop)((char *)rreq->dev.user_buf + MPIU_PtrToAint(dloop_vec[i].buf),
                   (char *)rreq->dev.accum_user_buf + MPIU_PtrToAint(dloop_vec[i].buf),
                   &count, &type);
        }

        MPID_Segment_free(segp);
        MPIU_Free(dloop_vec);
    }

 fn_exit:
    /* free the temporary buffer */
    rreq->dev.datatype.GetTrueExtentAndLowerBound( &true_lb );
    MPIU_Free((char *) rreq->dev.user_buf + true_lb);

    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


static MPI_RESULT do_simple_accumulate(MPIDI_PT_single_op *single_op)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPI_User_function *uop;

    if (single_op->op == MPI_REPLACE)
    {
        /* simply copy the data */
        mpi_errno = MPIR_Localcopy(single_op->data,
                                   single_op->count,
                                   single_op->datatype,
                                   single_op->addr,
                                   single_op->count,
                                   single_op->datatype
                                   );
        ON_ERROR_FAIL(mpi_errno);
        goto fn_exit;
    }

    if (HANDLE_GET_TYPE(single_op->op) == HANDLE_TYPE_BUILTIN)
    {
        /* get the function by indexing into the op table */
        uop = MPIR_Op_table[(single_op->op)%16 - 1];
    }
    else
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opnotpredefined %d", single_op->op );
        goto fn_fail;
    }

    /* only basic datatypes supported for this optimization. */
    MPI_Datatype hType = single_op->datatype.GetMpiHandle();
    (*uop)(single_op->data, single_op->addr,
           &(single_op->count), &hType);

 fn_fail:
 fn_exit:
    return mpi_errno;
}


static MPI_RESULT do_simple_get(const MPID_Win *win_ptr, MPIDI_Win_lock_queue *lock_queue)
{
    MPID_Request* req;
    req = MPID_Request_create(MPID_REQUEST_SEND);
    if( req == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    req->dev.target_win_handle = win_ptr->handle;
    req->dev.source_win_handle = lock_queue->src_win;
    req->dev.single_op_opt = true;
    
    req->set_rma_last_op();
    req->set_rma_unlock();

    req->set_type(MPIDI_REQUEST_TYPE_GET_RESP);
    req->dev.OnDataAvail = MPIDI_CH3_SendHandler_GetSendRespComplete;
    req->dev.OnFinal     = MPIDI_CH3_SendHandler_GetSendRespComplete;

    MPI_RESULT mpi_errno = InitRespPacket(req, lock_queue->pt_single_op->request_handle);
    ON_ERROR_FAIL(mpi_errno);

    int type_size = fixme_cast<int>(lock_queue->pt_single_op->datatype.GetSize());
    req->add_send_iov(lock_queue->pt_single_op->addr, lock_queue->pt_single_op->count * type_size);

    mpi_errno = MPIDI_CH3_SendRequest(lock_queue->vc, req);
    ON_ERROR_FAIL(mpi_errno);
    req->Release();

fn_exit:
    return mpi_errno;

fn_fail:
    req->Release();
    goto fn_exit;
}


static
MPI_RESULT
set_request_win_buff_for_derived_type(
    _Inout_ MPID_Request* req,
    _In_ const MPID_Datatype* dt,
    _In_ size_t target_disp
    )
{
    MPI_Aint type_size = dt->size;
    MPI_Aint target_size;
    MPID_Win* win;

    MPIU_Assert(!MPID_Datatype_is_predefined(dt->handle));

    req->dev.datatype.Init(dt);

    MPID_Win_get_ptr_valid(req->dev.target_win_handle, win);
    VERIFY(win != NULL);

    target_size = type_size * req->dev.user_count;
    target_disp *= static_cast<size_t>(win->disp_unit);

    //
    // FIXME: size mismatch, dev.recv_data_sz is limited to int
    //
    req->dev.recv_data_sz = (MPIDI_msg_sz_t)target_size;
    req->dev.user_buf = ((char*)win->base + target_disp);

    //
    // make sure that the target end does not wrap around the address space
    //
    return check_target_window_boundaries(target_size, target_disp, win);
}


static MPI_RESULT rma_op_tail(
    MPIDI_VC_t * vc,
    const MPID_Request * rreq,
    int * complete,
    bool fAckNeeded
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPID_Win* win_ptr;

    MPID_Win_get_ptr_valid(rreq->dev.target_win_handle, win_ptr);
    MPIU_Assert(win_ptr != NULL);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* if passive target RMA, increment counter */
    if (win_ptr->dev.lock_state != MPIDI_LOCKED_NOT_LOCKED)
        win_ptr->dev.my_pt_rma_puts_accs++;

    if (rreq->is_rma_last_op())
    {
        /* Last RMA operation from source. If active
           target RMA, decrement window counter. If
           passive target RMA, release lock on window and
           grant next lock in the lock queue if there is
           any. If it's a shared lock or a lock-put-unlock
           type of optimization, we also need to send an
           ack to the source. */

        if (win_ptr->dev.lock_state == MPIDI_LOCKED_NOT_LOCKED)
        {
            /* FIXME: MT: this has to be done atomically */
            win_ptr->dev.my_counter -= 1;
        }
        else
        {
            bool unlock = rreq->rma_unlock_needed();
            if (fAckNeeded)
            {
                mpi_errno = MPIDI_CH3I_Send_pt_rma_done_pkt(
                    vc,
                    rreq->dev.source_win_handle,
                    win_ptr->comm_ptr->rank,
                    unlock
                );
                ON_ERROR_FAIL(mpi_errno);
            }
            if (unlock)
            {
                mpi_errno = MPIDI_CH3I_Release_lock(win_ptr);
            }
        }
    }

    *complete = TRUE;
fn_fail:
    return mpi_errno;
}


MPI_RESULT MPIDI_CH3U_Handle_recv_req(MPIDI_VC_t * vc, MPID_Request * rreq, int * complete)
{
    int (*reqFn)(MPIDI_VC_t *, MPID_Request *, int *);

    reqFn = rreq->dev.OnDataAvail;
    if (!reqFn)
    {
        MPIU_Assert(rreq->get_type() == MPIDI_REQUEST_TYPE_RECV);
        *complete = TRUE;
    }
    else
    {
        MPI_RESULT mpi_errno = reqFn( vc, rreq, complete );
        if(mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }

    if(*complete)
    {
        //
        // The receive packet is complete; signal the request and release the reference
        // returned by MPIDI_CH3U_Handle_recv_pkt().
        //
        // BUGBUG: erezh - We release the request here for convenience, however its probably
        // a better practice to release the request by the owner of the request (the caller).
        //
        rreq->signal_completion();
        rreq->Release();
        Mpi.SignalProgress();
    }

    return MPI_SUCCESS;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_PutRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    OACR_USE_PTR( rreq );
    return rma_op_tail(vc, rreq, complete, true);
}


MPI_RESULT
MPIDI_CH3_RecvHandler_AccumRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    //
    // accumulate data from tmp_buf into user_buf
    //
    MPI_RESULT mpi_errno = do_accumulate_op(rreq);
    ON_ERROR_FAIL(mpi_errno);

    return rma_op_tail(vc, rreq, complete, true);

fn_fail:
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_GetAccumRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
)
{
    MPI_RESULT mpi_errno;
    MPID_Request* sreq;
    sreq = MPID_Request_create(MPID_REQUEST_RMA_RESP);
    if (sreq == nullptr)
    {
        *complete = TRUE;
        return MPIU_ERR_NOMEM();
    }

    sreq->dev.datatype = rreq->dev.datatype;

    //
    // Update request with the buffer data.
    //
    sreq->dev.user_count = rreq->dev.user_count;
    sreq->dev.target_win_handle = rreq->dev.target_win_handle;
    sreq->dev.source_win_handle = rreq->dev.source_win_handle;
    if (rreq->is_rma_last_op())
    {
        sreq->set_rma_last_op();
    }
    if (rreq->rma_unlock_needed())
    {
        sreq->set_rma_unlock();
    }

    sreq->dev.recv_data_sz = rreq->dev.recv_data_sz;
    sreq->dev.target_disp = rreq->dev.target_disp;

    MPID_Win* win_ptr;
    MPID_Win_get_ptr_valid(rreq->dev.target_win_handle, win_ptr);
    VERIFY(win_ptr != nullptr);

    sreq->dev.user_buf = ((char*)win_ptr->base + sreq->dev.target_disp * win_ptr->disp_unit);
    MPI_Aint type_size = static_cast<MPI_Aint>(sreq->dev.datatype.GetSize());

    sreq->set_type(MPIDI_REQUEST_TYPE_GET_ACCUM_RESP_DERIVED_DT);
    sreq->dev.OnDataAvail = nullptr;
    sreq->dev.OnFinal = nullptr;

    mpi_errno = InitRespPacket(sreq, rreq->dev.request_handle);
    ON_ERROR_FAIL(mpi_errno);

    sreq->dev.segment_ptr = MPID_Segment_alloc();
    if (sreq->dev.segment_ptr == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        ON_ERROR_FAIL(mpi_errno);
    }

    MPID_Segment_init(sreq->dev.user_buf,
        sreq->dev.user_count,
        sreq->dev.datatype.GetMpiHandle(),
        sreq->dev.segment_ptr, 0);
    sreq->dev.segment_first = 0;
    sreq->dev.segment_size = static_cast<MPIDI_msg_sz_t>(type_size * sreq->dev.user_count);

    MPI_Count extent = sreq->dev.datatype.GetExtent();
    MPI_Count true_lb, true_extent;
    true_extent = sreq->dev.datatype.GetTrueExtentAndLowerBound(&true_lb);
    void* send_buf = MPIU_Malloc(
        static_cast<size_t>(sreq->dev.user_count * max(extent, true_extent))
    );
    if (send_buf == nullptr)
    {
        ON_ERROR_FAIL(MPIU_ERR_NOMEM());
    }

    sreq->dev.user_buf = send_buf;
    mpi_errno = MPIR_Localcopy(sreq->dev.user_buf, sreq->dev.user_count, sreq->dev.datatype,
        send_buf, sreq->dev.user_count, sreq->dev.datatype);
    ON_ERROR_FAIL(mpi_errno);

    //
    // Send GetAccumulate response
    //
    mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);

    //
    // accumulate data from tmp_buf into user_buf
    //
    mpi_errno = do_accumulate_op(rreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = rma_op_tail(vc, rreq, complete, false);
    ON_ERROR_FAIL(mpi_errno);

    sreq->Release();
    return MPI_SUCCESS;

fn_fail:
    sreq->Release();
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_CompareSwapRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
)
{
    //
    // user_buf contains of two elements:
    // user_buf[0] - origin_elem - need to be put into the target window
    // user_buf[1] - compare_elem
    // compare accum_buf to compare_elem, and if equal
    // put origin_elem to accum_buf
    //
    MPI_RESULT mpi_errno;
    MPI_Count extent = rreq->dev.datatype.GetExtent();

    MPID_Request *sreq;

    //
    // create separate request to send GET response back
    //
    sreq = MPID_Request_create(MPID_REQUEST_RMA_RESP);
    if (sreq == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    sreq->set_type(MPIDI_REQUEST_TYPE_COMPARE_AND_SWAP_RESP);

    //
    // post processing is done in "compare and swap" part
    //
    sreq->dev.OnDataAvail = nullptr;
    sreq->dev.OnFinal = nullptr;

    MPID_Win* win_ptr;

    sreq->dev.user_count = 1;
    sreq->dev.target_win_handle = rreq->dev.target_win_handle;
    sreq->dev.source_win_handle = rreq->dev.source_win_handle;
    sreq->dev.target_disp = rreq->dev.target_disp;

    MPID_Win_get_ptr_valid(sreq->dev.target_win_handle, win_ptr);
    VERIFY(win_ptr != NULL);

    sreq->dev.datatype = rreq->dev.datatype;
    sreq->dev.recv_data_sz = static_cast<MPIDI_msg_sz_t>(sreq->dev.datatype.GetSize());

    void* send_buf = MPIU_Malloc(sreq->dev.recv_data_sz);
    if (send_buf == nullptr)
    {
        sreq->Release();
        return MPIU_ERR_NOMEM();
    }
    sreq->dev.user_buf = send_buf;
    mpi_errno = MPIR_Localcopy(
        static_cast<BYTE*>(win_ptr->base) + sreq->dev.target_disp * win_ptr->disp_unit,
        1,
        sreq->dev.datatype,
        send_buf,
        1,
        sreq->dev.datatype
    );
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = InitRespPacket(sreq, rreq->dev.request_handle);
    ON_ERROR_FAIL(mpi_errno);

    sreq->add_send_iov(sreq->dev.user_buf, sreq->dev.recv_data_sz);

    const BYTE* compare_addr = static_cast<BYTE*>(rreq->dev.user_buf) + extent;
    const BYTE* window_buf = static_cast<BYTE*>(rreq->dev.accum_user_buf);

    bool equal = true;
    for (MPI_Count i = 0; i < extent; i++)
    {
        if (compare_addr[i] ^ window_buf[i])
        {
            equal = false;
            break;
        }
    }
    if (equal)
    {
        mpi_errno = MPIR_Localcopy(rreq->dev.user_buf, 1, rreq->dev.datatype, rreq->dev.accum_user_buf, 1, rreq->dev.datatype);
        ON_ERROR_FAIL(mpi_errno);
    }

    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);

    sreq->Release();

    //
    // free the temporary buffer allocated in Handle_compare_and_swap
    //
    MPIU_Free(rreq->dev.user_buf);

    return rma_op_tail(vc, rreq, complete, false);

fn_fail:
    MPIU_Free(rreq->dev.user_buf);
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_PutRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    MPI_RESULT mpi_errno;
    MPID_Datatype *new_dtp;

    //
    // create derived datatype
    //
    mpi_errno = create_derived_datatype(rreq, &new_dtp);
    ON_ERROR_FAIL(mpi_errno);

    //
    // update receive request to get the data
    //
    rreq->set_type(MPIDI_REQUEST_TYPE_PUT_RESP);
    MPI_RESULT rma_error = set_request_win_buff_for_derived_type(rreq, new_dtp, rreq->dev.target_disp);
    if (rma_error != MPI_SUCCESS)
    {
        *complete = TRUE;
        mpi_errno = process_rma_error(vc, rreq, rma_error);
        ON_ERROR_FAIL(mpi_errno);
    }

    //
    // this will cause the datatype to be freed when the
    // request is freed. free dtype_info here.
    //
    MPIU_Free(rreq->dev.dtype_info);

    rreq->dev.segment_ptr = MPID_Segment_alloc( );
    /* if (!rreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
    MPID_Segment_init(rreq->dev.user_buf,
                      rreq->dev.user_count,
                      rreq->dev.datatype.GetMpiHandle(),
                      rreq->dev.segment_ptr, 0);
    rreq->dev.segment_first = 0;

    if (rma_error == MPI_SUCCESS)
    {
        rreq->dev.segment_size = rreq->dev.recv_data_sz;
    }
    else
    {
        //
        // in case the RMA window doesn't not have enough space 
        // to receive the whole message, opt to not receive it at all
        //
        rreq->status.count = 0;
        rreq->dev.segment_size = 0;
    }
    mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
    ON_ERROR_FAIL(mpi_errno);

    if (!rreq->dev.OnDataAvail)
        rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_PutRespComplete;

    *complete = FALSE;
    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_AccumRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    MPI_RESULT mpi_errno;
    MPID_Datatype *new_dtp;
    MPI_Aint true_lb, true_extent, extent;
    StackGuardPointer<void> tmp_buf;

    /* create derived datatype */
    mpi_errno = create_derived_datatype(rreq, &new_dtp);
    ON_ERROR_FAIL(mpi_errno);

    /* update receive request to get the data */
    rreq->set_type(MPIDI_REQUEST_TYPE_ACCUM_RESP);
    MPI_RESULT rma_error = set_request_win_buff_for_derived_type(rreq, new_dtp, rreq->dev.target_disp);
    if (rma_error != MPI_SUCCESS)
    {
        *complete = TRUE;
        mpi_errno = process_rma_error(vc, rreq, rma_error);
        ON_ERROR_FAIL(mpi_errno);
    }

    rreq->dev.accum_user_buf = rreq->dev.user_buf;

    /* first need to allocate tmp_buf to recv the data into */

    mpi_errno = NMPI_Type_get_true_extent(new_dtp->handle, &true_lb, &true_extent);
    ON_ERROR_FAIL(mpi_errno);

    extent = MPID_Datatype_get_extent(new_dtp->handle);

    tmp_buf =
        MPIU_Malloc( rreq->dev.user_count * max(extent,true_extent) );
    if( tmp_buf == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        ON_ERROR_FAIL(mpi_errno);
    }

    /* adjust for potential negative lower bound in datatype */
    rreq->dev.user_buf = (void *)((char*)tmp_buf.get() - true_lb);

    /* this will cause the datatype to be freed when the
       request is freed. free dtype_info here. */
    MPIU_Free(rreq->dev.dtype_info);

    rreq->dev.segment_ptr = MPID_Segment_alloc( );
    /* if (!rreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
    MPID_Segment_init(rreq->dev.user_buf,
                      rreq->dev.user_count,
                      rreq->dev.datatype.GetMpiHandle(),
                      rreq->dev.segment_ptr, 0);
    rreq->dev.segment_first = 0;

    if (rma_error == MPI_SUCCESS)
    {
        rreq->dev.segment_size = rreq->dev.recv_data_sz;
    }
    else
    {
        //
        // in case the RMA window doesn't not have enough space 
        // to receive the whole message, opt to not receive it at all
        //
        rreq->status.count = 0;
        rreq->dev.segment_size = 0;
    }

    mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
    ON_ERROR_FAIL(mpi_errno);

    if (!rreq->dev.OnDataAvail)
        rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_AccumRespComplete;

    *complete = FALSE;
    tmp_buf.detach();
    return MPI_SUCCESS;

fn_fail:
    new_dtp->Release();
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_GetRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    /* create request for sending data */
    MPI_RESULT mpi_errno;
    MPID_Request* sreq;
    sreq = MPID_Request_create(MPID_REQUEST_SEND);
    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    /* create derived datatype */
    MPID_Datatype *new_dtp;
    mpi_errno = create_derived_datatype(rreq, &new_dtp);
    ON_ERROR_FAIL(mpi_errno);

    MPIU_Free(rreq->dev.dtype_info);

    //
    // Update request with the buffer data.
    //
    sreq->dev.user_count = rreq->dev.user_count;
    sreq->dev.target_win_handle = rreq->dev.target_win_handle;
    sreq->dev.source_win_handle = rreq->dev.source_win_handle;
    if (rreq->is_rma_last_op())
    {
        sreq->set_rma_last_op();
    }
    if (rreq->rma_unlock_needed())
    {
        sreq->set_rma_unlock();
    }

    MPI_RESULT rma_error = set_request_win_buff_for_derived_type(sreq, new_dtp, rreq->dev.target_disp);
    if (rma_error != MPI_SUCCESS)
    {
        *complete = TRUE;
        mpi_errno = process_rma_error(vc, rreq, rma_error);
        ON_ERROR_FAIL(mpi_errno);
    }
    ON_ERROR_FAIL(rreq->status.MPI_ERROR);

    sreq->set_type(MPIDI_REQUEST_TYPE_GET_RESP);
    sreq->dev.OnDataAvail = MPIDI_CH3_SendHandler_GetSendRespComplete;
    sreq->dev.OnFinal     = MPIDI_CH3_SendHandler_GetSendRespComplete;

    mpi_errno = InitRespPacket(sreq, rreq->dev.request_handle);
    ON_ERROR_FAIL(mpi_errno);

    sreq->dev.segment_ptr = MPID_Segment_alloc( );
    /* if (!sreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
    MPID_Segment_init(sreq->dev.user_buf,
                      sreq->dev.user_count,
                      sreq->dev.datatype.GetMpiHandle(),
                      sreq->dev.segment_ptr, 0);
    sreq->dev.segment_first = 0;
    sreq->dev.segment_size = new_dtp->size * sreq->dev.user_count;

    /* Note that the OnFinal handler was set above */
    mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);
    sreq->Release();

fn_exit:
    *complete = TRUE;
    return mpi_errno;

 fn_fail:
    sreq->Release();
    goto fn_exit;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_GetAccumulateRespPredefinedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
)
{
    //
    // create request for sending data
    //
    MPI_RESULT mpi_errno;
    MPID_Request* sreq;
    sreq = MPID_Request_create(MPID_REQUEST_RMA_RESP);
    if (sreq == nullptr)
    {
        *complete = TRUE;
        return MPIU_ERR_NOMEM();
    }
    sreq->set_type(MPIDI_REQUEST_TYPE_GET_ACCUM_RESP);

    sreq->dev.OnDataAvail = nullptr;
    sreq->dev.OnFinal = nullptr;


    MPI_Aint type_size;
    MPI_Aint target_disp;
    MPI_Aint target_size;
    MPID_Win* win_ptr;

    //
    // Update request with the buffer data.
    //
    sreq->dev.user_count = rreq->dev.user_count;
    sreq->dev.target_disp = rreq->dev.target_disp;
    sreq->dev.target_win_handle = rreq->dev.target_win_handle;
    sreq->dev.source_win_handle = rreq->dev.source_win_handle;
    if (rreq->is_rma_last_op())
    {
        sreq->set_rma_last_op();
    }
    if (rreq->rma_unlock_needed())
    {
        sreq->set_rma_unlock();
    }

    MPID_Win_get_ptr_valid(sreq->dev.target_win_handle, win_ptr);
    VERIFY(win_ptr != nullptr);

    sreq->dev.datatype = rreq->dev.datatype;
    type_size = static_cast<MPI_Aint>(sreq->dev.datatype.GetSize());
    target_size = type_size * sreq->dev.user_count;
    target_disp = sreq->dev.target_disp * win_ptr->disp_unit;

    /* FIXME: size mismatch, dev.recv_data_sz is limited to int */
    sreq->dev.recv_data_sz = (MPIDI_msg_sz_t)target_size;
    sreq->dev.user_buf = ((char*)win_ptr->base + target_disp);

    MPI_RESULT rma_error = check_target_window_boundaries_for_request(sreq, win_ptr);
    if (rma_error != MPI_SUCCESS)
    {
        *complete = TRUE;
        mpi_errno = process_rma_error(vc, rreq, rma_error);
        ON_ERROR_FAIL(mpi_errno);
    }
    ON_ERROR_FAIL(rreq->status.MPI_ERROR);

    mpi_errno = InitRespPacket(sreq, rreq->dev.request_handle);
    ON_ERROR_FAIL(mpi_errno);

    void* send_buf = MPIU_Malloc(sreq->dev.recv_data_sz);
    if (send_buf == nullptr)
    {
        sreq->Release();
        return MPIU_ERR_NOMEM();
    }
    mpi_errno = MPIR_Localcopy(sreq->dev.user_buf, sreq->dev.user_count, sreq->dev.datatype,
        send_buf, sreq->dev.user_count, sreq->dev.datatype);
    ON_ERROR_FAIL(mpi_errno);

    sreq->dev.user_buf = send_buf;

    sreq->add_send_iov(sreq->dev.user_buf, sreq->dev.recv_data_sz);

    //
    // accumulate data from tmp_buf into user_buf
    //
    mpi_errno = do_accumulate_op(rreq);
    ON_ERROR_FAIL(mpi_errno);

    //
    // Send GetAccumulate response
    //
    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);

    sreq->Release();

    return rma_op_tail(vc, rreq, complete, false);

fn_fail:
    sreq->Release();
    *complete = TRUE;
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_GetAccumulateRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
)
{
    MPI_RESULT mpi_errno;
    StackGuardPointer<void> tmp_buf;
    //
    // accumulate part
    //

    //
    // create derived datatype
    //
    MPID_Datatype *new_dtp;
    mpi_errno = create_derived_datatype(rreq, &new_dtp);
    ON_ERROR_FAIL(mpi_errno);

    MPI_Aint true_lb, true_extent, extent;
    mpi_errno = NMPI_Type_get_true_extent(new_dtp->handle, &true_lb, &true_extent);
    ON_ERROR_FAIL(mpi_errno);

    extent = MPID_Datatype_get_extent(new_dtp->handle);

    //
    // update receive request to get the data
    //
    MPI_RESULT rma_error = set_request_win_buff_for_derived_type(rreq, new_dtp, rreq->dev.target_disp);
    if (rma_error != MPI_SUCCESS)
    {
        *complete = TRUE;
        mpi_errno = process_rma_error(vc, rreq, rma_error);
        ON_ERROR_FAIL(mpi_errno);
    }

    rreq->dev.accum_user_buf = rreq->dev.user_buf;

    //
    // first need to allocate tmp_buf to recv the data into
    //
    tmp_buf = MPIU_Malloc(rreq->dev.user_count * max(extent, true_extent));
    if (tmp_buf == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        ON_ERROR_FAIL(mpi_errno);
    }

    //
    // adjust for potential negative lower bound in datatype
    //
    rreq->dev.user_buf = (void *)((char*)tmp_buf.get() - true_lb);

    //
    // This will cause the datatype to be freed when the
    // request is freed. free dtype_info here.
    //
    MPIU_Free(rreq->dev.dtype_info);

    rreq->dev.segment_ptr = MPID_Segment_alloc();
    if (rreq->dev.segment_ptr == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        ON_ERROR_FAIL(mpi_errno);
    }

    MPID_Segment_init(rreq->dev.user_buf,
        rreq->dev.user_count,
        rreq->dev.datatype.GetMpiHandle(),
        rreq->dev.segment_ptr, 0);
    rreq->dev.segment_first = 0;

    if (rma_error == MPI_SUCCESS)
    {
        rreq->dev.segment_size = rreq->dev.recv_data_sz;
    }
    else
    {
        //
        // in case the RMA window doesn't not have enough space 
        // to receive the whole message, opt to not receive it at all
        //
        rreq->status.count = 0;
        rreq->dev.segment_size = 0;
    }

    mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
    ON_ERROR_FAIL(mpi_errno);

    if (!rreq->dev.OnDataAvail)
    {
        rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_GetAccumRespComplete;
    }
    rreq->dev.OnFinal = MPIDI_CH3_RecvHandler_GetAccumRespComplete;

    (rreq->dev.datatype.Get())->AddRef();

    *complete = FALSE;
    tmp_buf.detach();
    return MPI_SUCCESS;

fn_fail:
    *complete = TRUE;
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_SinglePutAccumComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    /* received all the data for single lock-put(accum)-unlock
       optimization where the lock was not acquired in
       ch3u_handle_recv_pkt. Try to acquire the lock and do the
       operation. */
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPID_Win *win_ptr;
    MPIDI_Win_lock_queue *lock_queue_entry, *curr_ptr, **curr_ptr_ptr;
    *complete = FALSE;

    /* received all the data for single lock-put(accum)-unlock
       optimization where the lock was not acquired in
       ch3u_handle_recv_pkt. Try to acquire the lock and do the
       operation. */

    MPID_Win_get_ptr_valid(rreq->dev.target_win_handle, win_ptr);
    MPIU_Assert(win_ptr != NULL);
    MPID_Win_valid_ptr(win_ptr, mpi_errno);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Win* win;
    MPID_Win_get_ptr_valid(rreq->dev.target_win_handle, win);
    VERIFY(win != NULL);

    MPI_RESULT rma_error = check_target_window_boundaries_for_request(rreq, win);
    if (rma_error != MPI_SUCCESS)
    {
        *complete = TRUE;
        mpi_errno = process_rma_error(vc, rreq, rma_error);
        ON_ERROR_FAIL(mpi_errno);
    }
    ON_ERROR_FAIL(rreq->status.MPI_ERROR);

    lock_queue_entry = rreq->dev.lock_queue_entry;

    if (MPIDI_CH3I_Try_acquire_win_lock(win_ptr, lock_queue_entry->lock_type))
    {
        /* Single put operation set's the op type to MPI_REPLACE */
        MPIU_Assert(
            rreq->get_type() != MPIDI_REQUEST_TYPE_PT_SINGLE_PUT ||
            lock_queue_entry->pt_single_op->op == MPI_REPLACE
            );

        mpi_errno = do_simple_accumulate(lock_queue_entry->pt_single_op);
        ON_ERROR_FAIL(mpi_errno);

        /* increment counter */
        win_ptr->dev.my_pt_rma_puts_accs++;

        /* send done packet */
        mpi_errno = MPIDI_CH3I_Send_pt_rma_done_pkt(vc,
                             lock_queue_entry->src_win,
                             win_ptr->comm_ptr->rank,
                             true);
        ON_ERROR_FAIL(mpi_errno);

        /* free lock_queue_entry including data buffer and remove
           it from the queue. */
        curr_ptr = win_ptr->dev.lock_queue;
        curr_ptr_ptr = &win_ptr->dev.lock_queue;
        while (curr_ptr != lock_queue_entry)
        {
            curr_ptr_ptr = &(curr_ptr->next);
            curr_ptr = curr_ptr->next;
        }
        *curr_ptr_ptr = curr_ptr->next;

        MPIU_Free(lock_queue_entry->pt_single_op);
        MPIU_Free(lock_queue_entry);

        /* Release lock and grant next lock if there is one. */
        mpi_errno = MPIDI_CH3I_Release_lock(win_ptr);
    }
    else
    {
        /* could not acquire lock. mark data recd as 1 */
        lock_queue_entry->pt_single_op->data_recd = 1;
    }

    *complete = TRUE;

 fn_fail:
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_UnpackUEBufComplete(
    _Inout_ MPIDI_VC_t * /*vc*/,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    int recv_pending;

    recv_pending = rreq->dec_and_get_recv_pending();
    if (!recv_pending)
    {
        if (rreq->dev.recv_data_sz > 0)
        {
            MPIDI_CH3U_Request_unpack_uebuf(rreq);
            MPIU_Free(rreq->dev.tmpbuf);
            rreq->dev.tmpbuf = NULL;
        }
    }
    else
    {
        /* The receive has not been posted yet.  MPID_{Recv/Irecv}()
           is responsible for unpacking the buffer. */
    }

    *complete = TRUE;

    return MPI_SUCCESS;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_UnpackSRBufComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    MPIDI_CH3U_Request_unpack_srbuf(rreq);

    if(rreq->get_type() == MPIDI_REQUEST_TYPE_PUT_RESP)
        return MPIDI_CH3_RecvHandler_PutRespComplete(vc, rreq, complete);

    if(rreq->get_type() == MPIDI_REQUEST_TYPE_ACCUM_RESP)
        return MPIDI_CH3_RecvHandler_AccumRespComplete(vc, rreq, complete);

    if(rreq->get_type() == MPIDI_REQUEST_TYPE_GET_ACCUM_RESP_DERIVED_DT)
        return MPIDI_CH3_RecvHandler_GetAccumRespComplete(vc, rreq, complete);

    *complete = TRUE;
    return MPI_SUCCESS;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_UnpackSRBufReloadIOV(
    _Inout_ MPIDI_VC_t * /*vc*/,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    MPIDI_CH3U_Request_unpack_srbuf(rreq);

    MPI_RESULT mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
    ON_ERROR_FAIL(mpi_errno);

    *complete = FALSE;
    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_RecvHandler_ReloadIOV(
    _Inout_ MPIDI_VC_t * /*vc*/,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    )
{
    MPI_RESULT mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
    ON_ERROR_FAIL(mpi_errno);

    *complete = FALSE;
    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}


/* Release the current lock on the window and grant the next lock in the
   queue if any */
MPI_RESULT MPIDI_CH3I_Release_lock(MPID_Win *win_ptr)
{
    MPIDI_Win_lock_queue *lock_queue, **lock_queue_ptr;
    int requested_lock;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    if (win_ptr->dev.lock_state == MPIDI_LOCKED_SHARED)
    {
        /* decr ref cnt */
        /* FIXME: MT: Must be done atomically */
        win_ptr->dev.shared_lock_ref_cnt--;
    }

    /* If shared lock ref count is 0 (which is also true if the lock is an
       exclusive lock), release the lock. */
    if (win_ptr->dev.shared_lock_ref_cnt == 0)
    {
        /* FIXME: MT: The setting of the lock type must be done atomically */
        win_ptr->dev.lock_state = MPIDI_LOCKED_NOT_LOCKED;

        /* If there is a lock queue, try to satisfy as many lock requests as
           possible. If the first one is a shared lock, grant it and grant all
           other shared locks. If the first one is an exclusive lock, grant
           only that one. */

        /* FIXME: MT: All queue accesses need to be made atomic */
        lock_queue = win_ptr->dev.lock_queue;
        lock_queue_ptr = &win_ptr->dev.lock_queue;
        while (lock_queue)
        {
            /* if it is not a lock-op-unlock type case or if it is a
               lock-op-unlock type case but all the data has been received,
               try to acquire the lock */
            if ((lock_queue->pt_single_op == NULL) ||
                (lock_queue->pt_single_op->data_recd == 1))
            {
                requested_lock = lock_queue->lock_type;
                if (MPIDI_CH3I_Try_acquire_win_lock(win_ptr, requested_lock))
                {
                    if (lock_queue->pt_single_op != NULL)
                    {
                        /* single op. do it here */
                        MPIDI_PT_single_op * single_op;

                        single_op = lock_queue->pt_single_op;
                        if (single_op->type == MPIDI_RMA_OP_PUT)
                        {
                            mpi_errno = MPIR_Localcopy(single_op->data,
                                                       single_op->count,
                                                       single_op->datatype,
                                                       single_op->addr,
                                                       single_op->count,
                                                       single_op->datatype);
                        }
                        else if (single_op->type == MPIDI_RMA_OP_ACCUMULATE)
                        {
                            mpi_errno = do_simple_accumulate(single_op);
                        }
                        else if (single_op->type == MPIDI_RMA_OP_GET)
                        {
                            mpi_errno = do_simple_get(win_ptr, lock_queue);
                        }

                        if( mpi_errno != MPI_SUCCESS )
                        {
                            goto fn_exit;
                        }

                        /* if put or accumulate, send rma done packet and release lock. */
                        if (single_op->type != MPIDI_RMA_OP_GET)
                        {
                            /* increment counter */
                            win_ptr->dev.my_pt_rma_puts_accs++;

                            mpi_errno =
                               MPIDI_CH3I_Send_pt_rma_done_pkt(lock_queue->vc,
                                         lock_queue->src_win,
                                         win_ptr->comm_ptr->rank,
                                         true
                                         );
                            if( mpi_errno != MPI_SUCCESS )
                            {
                                goto fn_exit;
                            }

                            /* release the lock */
                            if (win_ptr->dev.lock_state == MPIDI_LOCKED_SHARED)
                            {
                                /* decr ref cnt */
                                /* FIXME: MT: Must be done atomically */
                                win_ptr->dev.shared_lock_ref_cnt--;
                            }

                            /* If shared lock ref count is 0
                               (which is also true if the lock is an
                               exclusive lock), release the lock. */
                            if (win_ptr->dev.shared_lock_ref_cnt == 0)
                            {
                                /* FIXME: MT: The setting of the lock type
                                   must be done atomically */
                                win_ptr->dev.lock_state = MPIDI_LOCKED_NOT_LOCKED;
                            }

                            /* dequeue entry from lock queue */
                            MPIU_Free(single_op);
                            *lock_queue_ptr = lock_queue->next;
                            MPIU_Free(lock_queue);
                            lock_queue = *lock_queue_ptr;
                        }
                        else
                        {
                            /* it's a get. The operation is not complete. It
                               will be completed in ch3u_handle_send_req.c.
                               Free the single_op structure. If it's an
                               exclusive lock, break. Otherwise continue to the
                               next operation. */

                            MPIU_Free(single_op);
                            *lock_queue_ptr = lock_queue->next;
                            MPIU_Free(lock_queue);
                            lock_queue = *lock_queue_ptr;

                            if (requested_lock == MPI_LOCK_EXCLUSIVE)
                                break;
                        }
                    }
                    else
                    {
                        /* send lock granted packet. */
                        mpi_errno =
                            MPIDI_CH3I_Send_lock_granted_pkt(
                                lock_queue->vc,
                                lock_queue->src_win,
                                win_ptr->comm_ptr->rank,
                                requested_lock
                                );

                        /* dequeue entry from lock queue */
                        *lock_queue_ptr = lock_queue->next;
                        MPIU_Free(lock_queue);
                        lock_queue = *lock_queue_ptr;

                        /* if the granted lock is exclusive,
                           no need to continue */
                        if (requested_lock == MPI_LOCK_EXCLUSIVE)
                            break;
                    }
                }
            }
            else
            {
                lock_queue_ptr = &(lock_queue->next);
                lock_queue = lock_queue->next;
            }
        }
    }

 fn_exit:
    return mpi_errno;
}


MPI_RESULT MPIDI_CH3I_Send_pt_rma_done_pkt(
    MPIDI_VC_t *vc,
    MPI_Win source_win_handle,
    int rank,
    bool unlock
    )
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_pt_rma_done_t *pt_rma_done_pkt = &upkt.pt_rma_done;

    MPIDI_Pkt_init(pt_rma_done_pkt, MPIDI_CH3_PKT_PT_RMA_DONE);
    pt_rma_done_pkt->win_source = source_win_handle;
    pt_rma_done_pkt->rank = rank;
    if (unlock)
    {
        pt_rma_done_pkt->flags |= MPIDI_CH3_PKT_FLAG_RMA_UNLOCK;
    }

    return MPIDI_CH3_ffSend(vc, &upkt);
}


MPI_RESULT MPIDI_CH3I_Send_pt_rma_error_pkt(MPIDI_VC_t *vc, MPI_Win source_win_handle)
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_pt_rma_error_t *pt_rma_error_pkt = &upkt.pt_rma_error;

    MPIDI_Pkt_init(pt_rma_error_pkt, MPIDI_CH3_PKT_PT_RMA_ERROR);
    pt_rma_error_pkt->win_source = source_win_handle;

    return MPIDI_CH3_ffSend(vc, &upkt);
}


MPI_RESULT MPIDI_CH3U_Handle_send_req(MPIDI_VC_t *vc, MPID_Request *sreq, int *complete)
{
    int (*reqFn)(MPIDI_VC_t *, MPID_Request *, int *);

    /* Use the associated function rather than switching on the old ca field */
    /* Routines can call the attached function directly */
    reqFn = sreq->dev.OnDataAvail;
    if (!reqFn)
    {
        MPIU_Assert(sreq->get_type() != MPIDI_REQUEST_TYPE_GET_RESP);
        *complete = TRUE;
    }
    else
    {
        MPI_RESULT mpi_errno = reqFn( vc, sreq, complete );
        if(mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }

    if(*complete)
    {
        //
        // The send message is complete; signal the request completion.
        //
        // N.B. No need to release the request here as there's no ref count ownership
        //      here.  The caller holds the reference to the send request throughout the
        //      send process. (in the vc send queue)
        //
        sreq->signal_completion();
        Mpi.SignalProgress();
    }

    return MPI_SUCCESS;
}

/* ----------------------------------------------------------------------- */
/* Here are the functions that implement the actions that are taken when
 * data is available for a send request (or other completion operations)
 * These include "send" requests that are part of the RMA implementation.
 */
/* ----------------------------------------------------------------------- */

MPI_RESULT
MPIDI_CH3_SendHandler_GetSendRespComplete(
    _Inout_ MPIDI_VC_t * /*vc*/,
    _In_ MPID_Request * sreq,
    _Out_ int * complete
    )
{
    OACR_USE_PTR( sreq );
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    /* FIXME: Should this test be an MPIU_Assert? */
    if (sreq->is_rma_last_op())
    {
        MPID_Win *win_ptr;
        /* Last RMA operation (get) from source. If active target RMA,
           decrement window counter. If passive target RMA,
           release lock on window and grant next lock in the
           lock queue if there is any; no need to send rma done
           packet since the last operation is a get. */

        MPID_Win_get_ptr(sreq->dev.target_win_handle, win_ptr);
        MPID_Win_valid_ptr(win_ptr, mpi_errno);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }

        if (win_ptr->dev.lock_state == MPIDI_LOCKED_NOT_LOCKED)
        {
            /* FIXME: MT: this has to be done atomically */
            win_ptr->dev.my_counter -= 1;
        }
        else if (sreq->rma_unlock_needed())
        {
            mpi_errno = MPIDI_CH3I_Release_lock(win_ptr);
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }
        }
    }

    *complete = TRUE;

fn_fail:
    return mpi_errno;
}


MPI_RESULT
MPIDI_CH3_SendHandler_ReloadIOV(
    _Inout_ MPIDI_VC_t * /*vc*/,
    _In_ MPID_Request * sreq,
    _Out_ int * complete
    )
{
    MPI_RESULT mpi_errno;

    //
    //  Set the iov used count to zero to reloading the *entire* iov.
    //
    sreq->dev.iov_count = 0;
    mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq);
    ON_ERROR_FAIL(mpi_errno);

    *complete = FALSE;

 fn_fail:
    return mpi_errno;
}


/*
 * Send an eager message.  To optimize for the important, short contiguous
 * message case, there are separate routines for the contig and non-contig
 * datatype cases.
 */

/* MPIDI_CH3_SendNoncontig - Sends a message by loading an
   IOV and calling iSendRequest.  The caller must initialize
   sreq->dev.segment as well as segment_first and segment_size. */
MPI_RESULT MPIDI_CH3_SendNoncontig(MPIDI_VC_t *vc, MPID_Request *sreq)
{
    MPIU_Assert(sreq->dev.iov_count == 1);

    /* One the initial load of a send iov req, set the OnFinal action (null for point-to-point) */
    sreq->dev.OnFinal = 0;

    MPI_RESULT mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);

fn_fail:
    return mpi_errno;
}

MPI_RESULT
MPIDI_CH3_SendEager(
    MPID_Request* sreq,
    MPIDI_CH3_Pkt_type_t reqtype,
    MPIDI_msg_sz_t data_sz,
    int dt_contig,
    MPI_Aint dt_true_lb
    )
{
    MPI_RESULT mpi_errno;

    //
    // N.B. This code sends the sreq handle value to support send-cancel.
    //      However, the handle value is referenced only when the send is
    //      actually cancelled, where a reference to this request is taken.
    //      Thus, there is NO need to take a reference to the request here.
    //

    sreq->init_pkt(reqtype);
    MPIDI_CH3_Pkt_eager_send_t* eager_pkt = &sreq->get_pkt()->eager_send;
    eager_pkt->match.rank       = sreq->comm->rank;
    eager_pkt->match.tag        = sreq->dev.match.tag;
    eager_pkt->match.context_id = sreq->dev.match.context_id;
    eager_pkt->sender_req_id    = sreq->handle;
    eager_pkt->data_sz          = data_sz;

    sreq->set_msg_type(MPIDI_REQUEST_EAGER_MSG);
    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(sreq->comm, sreq->dev.match.rank);


    //
    // N.B. Insert code here for sending an eager short message.
    //      The eager-short send should be for contig only data which fits in the fixed
    //      size header leftover buffer (we can accomodate 12 bytes, 16 if we compress
    //      some of the fields).
    //      Use a negative value for size to indiate that the data resides in the header.
    //      Thus, the other side can pick that up easly without a more complicated logic
    //      to find if the data is also contig.
    //

    if (data_sz == 0)
    {
        mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
        ON_ERROR_FAIL(mpi_errno);
    }
    else if (dt_contig)
    {
        sreq->add_send_iov((char *)sreq->dev.user_buf + dt_true_lb, data_sz);

        mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        sreq->dev.segment_ptr = MPID_Segment_alloc( );
        /* if (!sreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
        MPID_Segment_init(
            sreq->dev.user_buf,
            sreq->dev.user_count,
            sreq->dev.datatype.GetMpiHandle(),
            sreq->dev.segment_ptr,
            0   // hetro
            );

        sreq->dev.segment_first = 0;
        sreq->dev.segment_size = data_sz;

        mpi_errno = MPIDI_CH3_SendNoncontig(vc, sreq);
        ON_ERROR_FAIL(mpi_errno);
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


#ifdef USE_EAGER_SHORT

//
// N.B. I left the old receive handler code for eager-short message as a reference for a
//      'correct' eager-short implementation (with every message type).
//


/* This is the matching handler for the EagerShort message defined above */

MPI_RESULT Handle_MPIDI_CH3_PKT_EAGERSHORT_SEND(MPIDI_VC_t *vc, MPIDI_CH3_Pkt_t *pkt, MPID_Request **rreqp)
{
    MPIDI_CH3_Pkt_eagershort_send_t * eagershort_pkt = &pkt->eagershort_send;
    MPID_Request * rreq;
    int found;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    mpi_errno = MPID_Recvq_dq_posted_or_new_unexpected(
                    &eagershort_pkt->match,
                    &found,
                    &req,
                    [&](
                        _In_ MPID_Request*  rreq,
                        _In_ bool           found
                        )
                    {
                        set_request_info(rreq, eagershort_pkt, MPIDI_REQUEST_EAGER_MSG);
                        if (found == false)
                        {
                            if (rreq->dev.recv_data_sz != 0)
                            {
                                MPIDI_msg_sz_t data_sz;
                                /* This is easy; copy the data into a temporary buffer.
                                   To begin with, we use the same temporary location as
                                   is used in receiving eager unexpected data.
                                 */
                                /* FIXME: When eagershort is enabled, provide a preallocated
                                   space for short messages (which is used even if eager short
                                   is not used), since we don't want to have a separate check
                                   to figure out which buffer we're using (or perhaps we should
                                   have a free-buffer-pointer, which can be null if it isn't
                                   a buffer that we've allocated). */
                                /* printf( "Allocating into tmp\n" ); fflush(stdout); */
                                data_sz = rreq->dev.recv_data_sz;
                                rreq->dev.tmpbuf = MPIU_Malloc(data_sz);
                                /* BUGBUG: erezh - one error need to remove rreq from the recv queue */
                                if( rreq->dev.tmpbuf == nullptr )
                                {
                                    return MPIU_ERR_NOMEM();
                                }

                                rreq->dev.tmpbuf_sz = data_sz;
                                /* Copy the payload. We could optimize this if data_sz & 0x3 == 0
                                   (copy (data_sz >> 2) ints, inline that since data size is
                                   currently limited to 4 ints */
                                {
                                    unsigned char const * restrict p = (unsigned char *)eagershort_pkt->data;
                                    unsigned char * restrict bufp = (unsigned char *)rreq->dev.tmpbuf;
                                    int i;
                                    for (i=0; i<data_sz; i++)
                                    {
                                        *bufp++ = *p++;
                                    }
                                }

                                /* These next two indicate that once matched, there is
                                   one more step (the unpack into the user buffer) to perform. */
                                rreq->dev.OnDataAvail = MPIDI_CH3_RecvHandler_UnpackUEBufComplete;
                                rreq->dev.recv_pending_count = 1;
                            }
                            rreq->signal_completion();
                        }
                        return MPI_SUCCESS;
                    });
    if (mpi_errno != MPI_SUCCESS)
    {
        *rreqp  = nullptr;
        return mpi_errno;
    }

    /* Extract the data from the packet */
    /* Note that if the data size is zero, we're already done */
    if (rreq->dev.recv_data_sz != 0 && found)
    {
        bool           dt_contig;
        MPI_Aint       dt_true_lb;
        MPIDI_msg_sz_t userbuf_sz;
        MPIDI_msg_sz_t data_sz;

        /* Make sure that we handle the general (non-contiguous)
           datatypes correctly while optimizing for the
           special case */
        userbuf_sz = fixme_cast<MPIDI_msg_sz_t>(
            rreq->dev.datatype.GetSizeAndInfo(
                rreq->dev.user_count,
                &dt_contig,
                &dt_true_lb
                )
            );

        if (rreq->dev.recv_data_sz <= userbuf_sz)
        {
            data_sz = rreq->dev.recv_data_sz;
        }
        else
        {
            rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_TRUNCATE, "**truncate %d %d %d %d",  rreq->status.MPI_SOURCE, rreq->status.MPI_TAG,  rreq->dev.recv_data_sz, userbuf_sz );
            rreq->status.count = userbuf_sz;
            data_sz = userbuf_sz;
        }

        if (dt_contig && data_sz == rreq->dev.recv_data_sz)
        {
            /* user buffer is contiguous and large enough to store the
               entire message.  We can just copy the code */

            /* Copy the payload. We could optimize this
               if data_sz & 0x3 == 0
               (copy (data_sz >> 2) ints, inline that since data size is
               currently limited to 4 ints */
            {
                unsigned char* restrict p = (unsigned char *)eagershort_pkt->data;
                unsigned char* restrict bufp = (unsigned char *)(char*)(rreq->dev.user_buf) + dt_true_lb;
                int i;
                for (i=0; i<data_sz; i++)
                {
                    *bufp++ = *p++;
                }
            }

            /* FIXME: We want to set the OnDataAvail to the appropriate
               function, which depends on whether this is an RMA
               request or a pt-to-pt request. */
            rreq->dev.OnDataAvail = 0;

            /* The recv_pending_count must be one here (!) because of
               the way the pending count is queried.  We may want
               to fix this, but it will require a sweep of the code */
        }
        else
        {
            MPIDI_msg_sz_t data_sz, last;

            /* user buffer is not contiguous.  Use the segment
               code to unpack it, handling various errors and
               exceptional cases */
            /* FIXME: The MPICH2 tests do not exercise this branch */
            /* printf( "Surprise!\n" ); fflush(stdout);*/
            rreq->dev.segment_ptr = MPID_Segment_alloc( );
            /* if (!rreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
            MPID_Segment_init(rreq->dev.user_buf, rreq->dev.user_count,
                              rreq->dev.datatype, rreq->dev.segment_ptr, 0);

            data_sz = rreq->dev.recv_data_sz;
            last    = data_sz;
            MPID_Segment_unpack( rreq->dev.segment_ptr, 0, &last, eagershort_pkt->data );
            if (last != data_sz)
            {
                /* There are two cases:  a datatype mismatch (could
                   not consume all data) or a too-short buffer. We
                   need to distinguish between these two types. */
                rreq->status.count = static_cast<int>( last );
                if (rreq->dev.recv_data_sz <= userbuf_sz)
                {
                    rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**dtypemismatch");
                }
            }
            rreq->dev.OnDataAvail = 0;
        }

        rreq->signal_completion();
    }

    rreq->Release();
    *rreqp  = nullptr;
    return mpi_errno;
}

#endif


/* FIXME: This is not optimized for short messages, which
   should have the data in the same packet when the data is
   particularly short (e.g., one 8 byte long word) */
MPI_RESULT Handle_MPIDI_CH3_PKT_EAGER_SEND(const MPIDI_CH3_Pkt_t * pkt, MPID_Request ** rreqp)
{
    const MPIDI_CH3_Pkt_eager_send_t * eager_pkt = &pkt->eager_send;
    MPID_Request * rreq;
    int found;
    MPI_RESULT mpi_errno;

    mpi_errno = MPID_Recvq_dq_posted_or_new_unexpected(
                &eager_pkt->match,
                &found,
                &rreq,
                [&](
                    _In_ MPID_Request*  rreq,
                    _In_ bool           /*found*/
                    )
                {
                    set_request_info(rreq, eager_pkt, MPIDI_REQUEST_EAGER_MSG);

                    rreq->dev.pkt.base.flags = eager_pkt->flags;
                    rreq->dev.pCompressionBuffer = reinterpret_cast<CompressionBuffer*>(
                        const_cast<MPIDI_CH3_Pkt_t*>(pkt)+1
                        );

                    /* FIXME: What is the logic here?  On an eager receive, the data
                       should be available already, and we should be optimizing
                       for short messages */
                    MPI_RESULT mpi_errno = MPIDI_CH3U_Post_data_receive(found, rreq);
                    rreq->dev.pCompressionBuffer = NULL;

                    return mpi_errno;
                });
    if( mpi_errno != MPI_SUCCESS )
    {
        *rreqp  = nullptr;
        return mpi_errno;
    }

    if( rreq->is_internal_complete() == true )
    {
        rreq->Release();
        rreq = nullptr;
    }
    *rreqp = rreq;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_READY_SEND(const MPIDI_CH3_Pkt_t * pkt, MPID_Request ** rreqp)
{
    const MPIDI_CH3_Pkt_eager_send_t* ready_pkt = &pkt->eager_send;
    MPID_Request * rreq;
    int found;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    mpi_errno = MPID_Recvq_dq_posted_or_new_unexpected(
                &ready_pkt->match,
                &found,
                &rreq,
                [&](
                    _In_ MPID_Request*  rreq,
                    _In_ bool           /*found*/
                    )
                {
                    MPI_RESULT mpi_errno  = MPI_SUCCESS;
                    set_request_info(rreq, ready_pkt, MPIDI_REQUEST_EAGER_MSG);

                    rreq->dev.pkt.base.flags = ready_pkt->flags;
                    rreq->dev.pCompressionBuffer = reinterpret_cast<CompressionBuffer*>(
                        const_cast<MPIDI_CH3_Pkt_t*>(pkt)+1
                        );

                    if (found == false)
                    {
                        /* FIXME: an error packet should be sent back to the sender
                           indicating that the ready-send failed.  On the send
                           side, the error handler for the communicator can be invoked
                           even if the ready-send request has already
                           completed. */

                        /* We need to consume any outstanding associated data and
                           mark the request with an error. */

                        rreq->status.MPI_ERROR = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rsendnomatch %d %d",  ready_pkt->match.rank, ready_pkt->match.tag);
                        rreq->status.count = 0;
                        if (rreq->dev.recv_data_sz > 0)
                        {
                            /* force read of extra data */
                            rreq->dev.segment_first = 0;
                            rreq->dev.segment_size = 0;
                            mpi_errno = MPIDI_CH3U_Request_load_recv_iov(rreq);
                        }
                        else
                        {
                            rreq->signal_completion();
                            MPIU_Assert( rreq->is_internal_complete() == true );
                        }
                    }
                    else
                    {
                        mpi_errno = MPIDI_CH3U_Post_data_receive(TRUE, rreq);
                    }

                    rreq->dev.pCompressionBuffer = nullptr;

                    return mpi_errno;
                });
    if(mpi_errno != MPI_SUCCESS)
    {
        *rreqp  = nullptr;
        return mpi_errno;
    }

    if( rreq->is_internal_complete() == true )
    {
        rreq->Release();
        rreq = nullptr;
    }

    *rreqp = rreq;
    return MPI_SUCCESS;
}

/*
 * Send a synchronous eager message.  This is an optimization that you
 * may want to use for programs that make extensive use of MPI_Ssend and
 * MPI_Issend for short messages.
 */

/*
 * These routines are called when a receive matches an eager sync send
 */
MPI_RESULT MPIDI_CH3_EagerSyncAck( MPIDI_VC_t *vc, const MPID_Request *rreq )
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_eager_sync_ack_t* esa_pkt = &upkt.eager_sync_ack;
    MPIDI_Pkt_init(esa_pkt, MPIDI_CH3_PKT_EAGER_SYNC_ACK);
    esa_pkt->sender_req_id = rreq->dev.sender_req_id;

    return MPIDI_CH3_ffSend(vc, &upkt);
}


/*
 * Here are the routines that are called by the progress engine to handle
 * the various rendezvous message requests (cancel of sends is in
 * mpid_cancel_send.c).
 */

MPI_RESULT Handle_MPIDI_CH3_PKT_EAGER_SYNC_SEND(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    const MPIDI_CH3_Pkt_eager_send_t * es_pkt = &pkt->eager_send;
    MPID_Request * rreq;
    int found;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    mpi_errno = MPID_Recvq_dq_posted_or_new_unexpected(
                &es_pkt->match,
                &found,
                &rreq,
                [&](
                    _In_ MPID_Request*  rreq,
                    _In_ bool           found
                    )
                {
                    set_request_info(rreq, es_pkt, MPIDI_REQUEST_EAGER_MSG);

                    rreq->dev.pkt.base.flags = es_pkt->flags;
                    rreq->dev.pCompressionBuffer = reinterpret_cast<CompressionBuffer*>(
                        const_cast<MPIDI_CH3_Pkt_t*>(pkt)+1
                        );

                    mpi_errno = MPIDI_CH3U_Post_data_receive(
                                            found ? TRUE : FALSE,
                                            rreq);

                    rreq->dev.pCompressionBuffer = NULL;
                    if (found == false && mpi_errno == MPI_SUCCESS)
                    {
                        rreq->set_sync_ack_needed();
                    }
                    return mpi_errno;
                });
    if( mpi_errno != MPI_SUCCESS )
    {
        *rreqp  = nullptr;
        return mpi_errno;
    }

    if (found)
    {
        MPIDI_CH3_Pkt_t upkt;
        MPIDI_CH3_Pkt_eager_sync_ack_t* esa_pkt = &upkt.eager_sync_ack;
        MPIDI_Pkt_init(esa_pkt, MPIDI_CH3_PKT_EAGER_SYNC_ACK);
        esa_pkt->sender_req_id = rreq->dev.sender_req_id;
        mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
        ON_ERROR_FAIL(mpi_errno);
    }
    if( rreq->is_internal_complete() == true )
    {
        rreq->Release();
        rreq = nullptr;
    }
    *rreqp = rreq;
    return MPI_SUCCESS;

fn_fail:
    rreq->Release();
    *rreqp = nullptr;
    return mpi_errno;
}


MPI_RESULT Handle_MPIDI_CH3_PKT_EAGER_SYNC_ACK(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp)
{
    MPID_Request* sreq;
    const MPIDI_CH3_Pkt_eager_sync_ack_t* esa_pkt = &pkt->eager_sync_ack;
    MPID_Request_get_ptr(esa_pkt->sender_req_id, sreq);

    MPIU_Assert(sreq != nullptr);
    if(sreq != nullptr)
    {
        //
        // The sync-ack came back; signal the request completed and release the
        // reference taken at sync-send time.
        //
        sreq->signal_completion();
        sreq->Release();
    }
    *rreqp = nullptr;
    return MPI_SUCCESS;
}


MPI_RESULT MPIDI_CH3_ffSend(MPIDI_VC_t * vc, const MPIDI_CH3_Pkt_t* pkt)
{
    MPID_Request* sreq = MPID_Request_create(MPID_REQUEST_SEND);
    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    sreq->init_pkt(*pkt);

    MPI_RESULT mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    sreq->Release();

    return mpi_errno;
}


MPI_RESULT MPIDI_CH3_SendRequest(MPIDI_VC_t* vc, MPID_Request* sreq)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPIU_Assert(sreq->dev.iov_offset == 0);
    MPIU_Assert(sreq->dev.iov_count > 0);
    MPIU_Assert(sreq->dev.iov_count <= _countof(sreq->dev.iov));
    MPIU_Assert(sreq->dev.iov[0].len == sizeof(MPIDI_CH3_Pkt_t));

    bool signalWake = false;
    bool defer = false;

    {
        SendQLock lock(vc);

        vc->ch.n_sent++;
        sreq->ch.msg_id = vc->ch.n_sent;

        switch( vc->ch.state )
        {
        case MPIDI_CH3I_VC_STATE_UNCONNECTED:

            //
            // if the SendQ is not empty, then we have already posted a connect and
            //  are currently processing it for another request. We will enqueue this
            //  request and return.
            //
            if (MPIDI_CH3I_SendQ_empty_unsafe(vc))
            {
                /* Form a new connection, queuing the data so it can be sent later. */
                mpi_errno = MPIDI_CH3I_VC_post_connect(vc, sreq);
                if( mpi_errno != MPI_SUCCESS )
                {
                    break;
                }
            }

            if( vc->ch.state != MPIDI_CH3I_VC_STATE_CONNECTED )
            {
                MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);
                signalWake = true;
                break;
            }

            //
            // If we established the connection synchronously somehow, go make progress.
            //
            __fallthrough;

        case MPIDI_CH3I_VC_STATE_CONNECTED:
            /* Connection already formed.  If send queue is empty attempt to send data, queuing any unsent data. */
            if (MPIDI_CH3I_SendQ_empty_unsafe(vc))
            {

                //
                // For ST cases, we will just do the inline send here.
                // For MT cases, we will only do the inline send if we
                //  already have or can take the progress lock.
                //
                // NOTE: the ProgressLock is not recursive, so we must test to
                //  see if we already hold the progress lock before trying to take it.
                //

                bool nestedOrSingle = ((false == Mpi.IsMultiThreaded()) ||
                                       Mpi.IsCurrentThreadMakingProgress());

                if (nestedOrSingle || Mpi.TryAcquireProgressLock())
                {
                    switch( vc->ch.channel )
                    {
                    case MPIDI_CH3I_CH_TYPE_SHM:
                        mpi_errno = MPIDI_CH3I_SHM_start_write( vc, sreq );
                        break;

                    case MPIDI_CH3I_CH_TYPE_ND:
                        mpi_errno = MPIDI_CH3I_Nd_start_write( vc, sreq );
                        break;

                    case MPIDI_CH3I_CH_TYPE_NDv1:
                        mpi_errno = MPIDI_CH3I_Ndv1_start_write( vc, sreq );
                        break;

                    case MPIDI_CH3I_CH_TYPE_SOCK:
                        mpi_errno = MPIDI_CH3I_SOCK_start_write( vc, sreq );
                        break;

                    default:
                        __assume(0);
                    }

                    if (nestedOrSingle == false)
                    {
                        Mpi.ReleaseProgressLock();
                    }

                    break;
                }
                else
                {

                    //
                    // Another thread is currently making progress, so we were not able
                    // to start this send operation, so we need to Q it and defer the write.
                    //
                    // Because SHM channel is handled completely in the progress loop
                    //  and we don't wait for HW notifications or information like ND polling
                    //  for the completion to get hooks for executiuon, SHM doesn't need to
                    //  manually inject a deferred send.  But because the other channels have
                    //  to have some action taken to get the progress loop pumping, we must
                    //  post a deferred send.  If we don't, it is possible that no completion
                    //  will ever fire for this VC (esp in sockets), thus no progress would
                    //  be made
                    //
                    if (vc->ch.channel != MPIDI_CH3I_CH_TYPE_SHM)
                    {
                        defer = true;
                    }
                }

            }
            //
            // A previous send is already in progress - just queue for later processing.
            //
            __fallthrough;

        case MPIDI_CH3I_VC_STATE_CONNECTING:
        case MPIDI_CH3I_VC_STATE_PENDING_ACCEPT:
        case MPIDI_CH3I_VC_STATE_ACCEPTED:
            /* Unable to send data at the moment, so queue it for later */
            MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);
            signalWake = true;
            switch( vc->ch.channel )
            {
            case MPIDI_CH3I_CH_TYPE_SHM:
                TraceSendShm_Queue(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
                break;

            case MPIDI_CH3I_CH_TYPE_ND:
            case MPIDI_CH3I_CH_TYPE_NDv1:
                TraceSendNd_Queue(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
                break;

            case MPIDI_CH3I_CH_TYPE_SOCK:
                TraceSendSock_Queue(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
                break;

            default:
                __assume(0);
            }
            break;

        case MPIDI_CH3I_VC_STATE_FAILED:
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**vcfailedstate %d", vc->pg_rank);
            break;

        default:
            MPIU_Assert( vc->ch.state == MPIDI_CH3I_VC_STATE_FAILED );
        }
    }

    //
    // If we ended up putting this message in the sendq, then we will
    //  ensure that the progress thread is notified if they are
    //  waiting for a completion, but we won't do a full signal
    //  by incrementing the counter, because this would wakeup all the
    //  other threads that can't have made any progress from this.
    //
    if (mpi_errno == MPI_SUCCESS && signalWake && Mpi.IsMultiThreaded())
    {
        //
        // We will send a deferred write if another thread
        //  already owned the progress engine AND we are in
        //  the connected state.  Otherwise, we just wakeup
        //  the progress thread to continue making progress.
        //
        if (defer)
        {
            mpi_errno = MPIDI_CH3I_DeferWrite(vc, sreq);
        }
        else
        {
            Mpi.WakeProgress(false);
        }
    }
    return mpi_errno;
}
