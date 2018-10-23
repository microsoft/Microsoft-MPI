// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "ch3i_overlapped.h"
#include "ch3_compression.h"

#define IPV4_MAX_IP_ADDRESS_STRING_LENGTH 16

static int SendMessageSucceeded_cb(EXOVERLAPPED* pexov);
static int RecvMessageBodySucceeded_cb(EXOVERLAPPED* pexov);
static int MPIDI_CH3I_Post_recv_pkt(MPIDI_CH3I_Connection_t* conn);

//
// Channel private packets
//
#define SC_OPEN_REQ  1234
#define SC_OPEN_RESP 4321

struct Sock_Pkt_open_req_t
{
    //
    // Version comes first, so that we can change things after it without potentially breaking things.
    //
    UINT32 ver;
    int sig;
    int pg_rank;
};
C_ASSERT(sizeof(Sock_Pkt_open_req_t)  <= sizeof(MPIDI_CH3_Pkt_t));

#define Sock_open_req_pkt(conn) ((Sock_Pkt_open_req_t*)&conn->pkt)

struct Sock_Pkt_open_resp_t
{
    int sig;
    int ack;
};
C_ASSERT(sizeof(Sock_Pkt_open_resp_t) <= sizeof(MPIDI_CH3_Pkt_t));

#define Sock_open_resp_pkt(conn) ((Sock_Pkt_open_resp_t*)&conn->pkt)

static int CloseConnectionComplete_cb(EXOVERLAPPED* pexov)
{
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;

    /* If the conn pointer is NULL then the close was intentional */
    MPIU_Assert(conn->disconnect);
    MPIU_Assert(conn->recv_active == NULL);

    if(conn->vc != NULL)
    {
        Trace_SOCKETS_Info_CloseConnectionComplete_cb_Terminated(conn->vc->pg_rank);
        MPIU_Assert(MPIDI_CH3I_SendQ_empty(conn->vc));
        conn->vc->ch.conn = NULL;
        conn->vc->ch.state = MPIDI_CH3I_VC_STATE_UNCONNECTED;
        MPIDI_CH3U_Handle_connection(conn->vc, MPIDI_VC_EVENT_TERMINATED);
    }
    else
    {
        Trace_SOCKETS_Info_CloseConnectionComplete_cb_Intentional();
    }

    MPIDI_CH3I_Connection_free(conn);
    ch3i_free_overlapped(pov);
    return MPI_SUCCESS;
}


int MPIDI_CH3I_Post_close_connection(MPIDI_CH3I_Connection_t* conn)
{
    conn->disconnect = TRUE;
    MPIU_Assert( conn->vc != NULL );

    if(!MPIDI_CH3I_SendQ_empty(conn->vc))
    {
        Trace_SOCKETS_Info_MPIDI_CH3I_Post_close_connection_Ignored(conn->vc->pg_rank);
        return MPI_SUCCESS;
    }

    Trace_SOCKETS_Info_MPIDI_CH3I_Post_close_connection_Honored(conn->vc->pg_rank);
    return ch3i_post_close(conn, CloseConnectionComplete_cb);
}


static int SendFailed_cb(EXOVERLAPPED* pexov)
{
    int mpi_errno = ExGetStatus(pexov);
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);

    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;

    get_sock_peer_address(pov->conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);

    ch3i_free_overlapped(pov);
    Trace_SOCKETS_Error_SendFailed_cb(mpi_errno, pPeerAddress, peerPort);
    return MPIU_ERR_FAIL(mpi_errno);
}


static int RecvFailed_cb(EXOVERLAPPED* pexov)
{
    int mpi_errno = ExGetStatus(pexov);
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    const MPIDI_CH3I_Connection_t* conn = pov->conn;

    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;
    get_sock_peer_address(conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);

    ch3i_free_overlapped(pov);

    //
    // Connection was closed gracefully (indicated by MPI_SUCCESS error *class*)
    //
    if(ERROR_GET_CLASS(mpi_errno) == MPI_SUCCESS)
    {
        Trace_SOCKETS_Info_RecvFailed_cb_SocketClosed(pPeerAddress, peerPort);
        return MPI_SUCCESS;
    }

    //
    // Workaround: TCP bug aborts the connection even though we're doing graceful
    // shutdown (race condition in tcp stack). Ignore errors if we are closing.
    //
    if(conn->disconnect)
    {
        Trace_SOCKETS_Info_RecvFailed_cb_SocketAborted(mpi_errno, pPeerAddress, peerPort);
        return MPI_SUCCESS;
    }

    Trace_SOCKETS_Error_RecvFailed_cb_Failure(mpi_errno, pPeerAddress, peerPort);
    return MPIU_ERR_FAIL(mpi_errno);

}


static MPIU_Bsize_t copy_to_iov(const char* buffer, MPIU_Bsize_t size, MPID_IOV* iov, int iov_count, int* piov_offset)
{
    MPIU_Bsize_t nb = size;
    MPID_IOV* p = &iov[*piov_offset];
    const MPID_IOV* end = &iov[iov_count];

    while(p < end && nb > 0)
    {
        MPIU_Bsize_t iov_len = p->len;

        if (iov_len <= nb)
        {
            memcpy(p->buf, buffer, iov_len);
            nb -= iov_len;
            buffer += iov_len;
            p++;
        }
        else
        {
            memcpy(p->buf, buffer, nb);
            p->buf = (char*)p->buf + nb;
            p->len -= nb;
            nb = 0;
        }
    }

    //
    // It's safe to cast to int here because p is bound by iov + iov_count
    // and iov_count is an int
    //
    *piov_offset = static_cast<int>(p - iov); // next offset to process
    return (size - nb); // number of bytes copied
}


static
int
post_recv_pkt_ex(
    MPIDI_CH3I_Connection_t* conn,
    MPIU_Bsize_t max_size,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    int mpi_errno;

    MPIU_Assert(conn->recv_active == NULL);
    MPIU_Assert(max_size <= sizeof(conn->recv_buffer));
    MPIU_Assert(conn->recv_buff_end < max_size);
    MPIU_Assert(conn->recv_buff_end < sizeof(conn->pkt));

    mpi_errno = ch3i_post_read_min(
                        conn,
                        &conn->recv_buffer[conn->recv_buff_end],
                        max_size - conn->recv_buff_end,
                        sizeof(conn->pkt) - conn->recv_buff_end,
                        pfnSuccess,
                        pfnFailure
                        );
    if (mpi_errno != MPI_SUCCESS)
    {
        return MPIU_ERR_FAIL(mpi_errno);
    }
    return mpi_errno;
}


static
int
send_head_of_queue_ex_unsafe(
    _In_ MPIDI_CH3I_Connection_t* conn,
    _In_ ch3i_overlapped_t* pov
    )
{
    int mpi_errno;

    MPIU_Assert( conn->vc != NULL );

    MPID_Request* sreq = MPIDI_CH3I_SendQ_head_unsafe(conn->vc);
    if(sreq == NULL)
    {
        ch3i_free_overlapped(pov);
        return MPI_SUCCESS;
    }

    //
    // TODO: Need to mark the connection/vc with some state saying
    //  that we should not dequeue more send messages.  This would allow
    //  us to release the exclusive lock we have while compression is taking
    //  place.  This doesn't allow more people to send at a time, but does all
    //  people to add stuff to the queue while we compress.
    //

    if(g_CompressionThreshold != MSMPI_COMPRESSION_OFF)
    {
        mpi_errno = CompressRequest(sreq);
        if(mpi_errno != MPI_SUCCESS)
        {
            return MPIU_ERR_FAIL(mpi_errno);
        }
    }

    MPIU_Assert(sreq->dev.iov_offset == 0);
    TraceSendSock_Head(MPIDI_Process.my_pg_rank, conn->vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
    mpi_errno = ch3i_post_writev_ex(
                    conn,
                    sreq->dev.iov,
                    sreq->dev.iov_count,
                    pov,
                    SendMessageSucceeded_cb,
                    SendFailed_cb
                    );

    if (mpi_errno != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
        return MPIU_ERR_FAIL(mpi_errno);
    }
    return mpi_errno;
}


static
int
send_head_of_queue_ex(
    _In_ MPIDI_CH3I_Connection_t* conn,
    _In_ ch3i_overlapped_t* pov
    )
{
    MPIU_Assert( conn->vc != NULL );
    SendQLock lock(conn->vc);
    return send_head_of_queue_ex_unsafe(conn, pov);
}


static void connection_accept(MPIDI_CH3I_Connection_t* conn, MPIDI_VC_t* vc)
{
    Trace_SOCKETS_Info_connection_accept(vc->pg_rank);

    MPIDI_CH3I_Connection_t* old_conn = vc->ch.conn;

    if(old_conn != NULL)
    {
        //
        // The old connection will get refused by the other side. Copy the
        // send queue to the new established connection.
        //
        MPIU_Assert( conn->vc == NULL );

        old_conn->vc = NULL;
        old_conn->disconnect = TRUE;
        Trace_SOCKETS_Info_connection_accept_CloseOldConnection(vc->pg_rank);
        ch3i_post_close(old_conn, CloseConnectionComplete_cb);
    }

    vc->ch.conn = conn;
    conn->vc = vc;
    vc->eager_max_msg_sz = MPIDI_CH3I_Process.sock_eager_limit;

    Sock_open_resp_pkt(conn)->sig = SC_OPEN_RESP;
    Sock_open_resp_pkt(conn)->ack = TRUE;
}


static inline void connection_reject(MPIDI_CH3I_Connection_t* conn)
{
    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;
    get_sock_peer_address(conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);

    Trace_SOCKETS_Info_connection_reject(pPeerAddress, peerPort);

    Sock_open_resp_pkt(conn)->sig = SC_OPEN_RESP;
    Sock_open_resp_pkt(conn)->ack = FALSE;
}


static int connection_complete_ex(MPIDI_CH3I_Connection_t* conn, ch3i_overlapped_t* pov)
{
    MPIU_Assert(conn->vc->ch.conn == conn);

    conn->vc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTED;

    int mpi_errno;
    mpi_errno = MPIDI_CH3I_Post_recv_pkt(conn);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = send_head_of_queue_ex(conn, pov);
    ON_ERROR_FAIL(mpi_errno);

    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}


static int read_message_data(MPIDI_CH3I_Connection_t* conn, MPID_Request* rreq, ch3i_overlapped_t* pov)
{
    int mpi_errno;

    conn->recv_active = rreq;
    mpi_errno = ch3i_post_readv_ex(
                    conn,
                    &rreq->dev.iov[rreq->dev.iov_offset],
                    rreq->dev.iov_count - rreq->dev.iov_offset,
                    pov,
                    RecvMessageBodySucceeded_cb,
                    RecvFailed_cb
                    );
    if (mpi_errno != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
        Trace_SOCKETS_Error_read_message_data(mpi_errno, conn->vc->pg_rank);
        mpi_errno = MPIU_ERR_FATAL_GET(mpi_errno, MPI_ERR_OTHER, "**ch3|sock|postread %p %p %p", rreq, conn, conn->vc);
        return mpi_errno;
    }

    return MPI_SUCCESS;
}


static int RecvMessageBodySucceeded_cb(EXOVERLAPPED* pexov)
{
    int complete;
    int mpi_errno;
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);

    MPIDI_CH3I_Connection_t* conn = pov->conn;
    MPID_Request* rreq = conn->recv_active;

    MPIU_Assert(ExGetBytesTransferred(pexov) > 0);

    /* handle incoming data */
    TraceRecvSock_Data(conn->vc->pg_rank, MPIDI_Process.my_pg_rank, conn->vc->ch.n_recv);
    mpi_errno = MPIDI_CH3U_Handle_recv_req(conn->vc, rreq, &complete);
    if (mpi_errno != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    if (complete)
    {
        TraceRecvSock_Done(conn->vc->pg_rank, MPIDI_Process.my_pg_rank, conn->vc->ch.n_recv);
        ch3i_free_overlapped(pov);

        conn->recv_active = NULL;
        mpi_errno = MPIDI_CH3I_Post_recv_pkt(conn);
        if (mpi_errno != MPI_SUCCESS)
        {
            return MPIU_ERR_FAIL(mpi_errno);
        }
        return MPI_SUCCESS;
    }

    /* more data to be read */
    rreq->dev.iov_offset = 0;
    return read_message_data(conn, rreq, pov);
}


static int RecvNewMessageSucceeded_cb(EXOVERLAPPED* pexov)
{
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;
    const char* buff_head = conn->recv_buffer;
    MPIU_Bsize_t num_bytes = ExGetBytesTransferred(pexov);
    MPIU_Bsize_t buff_size = conn->recv_buff_end + num_bytes;

    MPIU_Assert(conn->recv_active == NULL);
    MPIU_Assert(conn->recv_buff_end < sizeof(conn->pkt));

    MPIU_Assert(num_bytes > 0);
    MPIU_Assert(num_bytes <= sizeof(conn->recv_buffer));
    MPIU_Assert(buff_size >= sizeof(conn->pkt));
    MPIU_Assert(buff_size <= sizeof(conn->recv_buffer));
    MPIU_Assert(conn->pkt.type < MPIDI_CH3_PKT_END_CH3);

    for(;;)
    {
        int mpi_errno;
        MPID_Request* rreq;
        const MPIDI_CH3_Pkt_t* pkt = reinterpret_cast<const MPIDI_CH3_Pkt_t*>(const_cast<char*>(buff_head));

        conn->vc->ch.n_recv++;
        TraceRecvSock_Packet(conn->vc->pg_rank, MPIDI_Process.my_pg_rank, conn->vc->ch.n_recv, pkt->type);
        mpi_errno = MPIDI_CH3U_Handle_recv_pkt(conn->vc, pkt, &rreq);

        if (mpi_errno != MPI_SUCCESS)
        {
            ch3i_free_overlapped(pov);
            return MPIU_ERR_FAIL(mpi_errno);
        }

        buff_size -= sizeof(conn->pkt);
        buff_head += sizeof(conn->pkt);

        //
        // Process received message data
        //
        if (rreq != NULL)
        {
            rreq->dev.iov_offset = 0;

            for(;;)
            {
               int complete;
               int bytes_copied;

                if(buff_size == 0)
                {
                    conn->recv_buff_end = 0;
                    return read_message_data(conn, rreq, pov);
                }

                bytes_copied = copy_to_iov(
                                    buff_head,
                                    buff_size,
                                    rreq->dev.iov,
                                    rreq->dev.iov_count,
                                    &rreq->dev.iov_offset
                                    );

                buff_size -= bytes_copied;
                buff_head += bytes_copied;

                if(rreq->dev.iov_offset != rreq->dev.iov_count)
                    continue;

                mpi_errno = MPIDI_CH3U_Handle_recv_req(conn->vc, rreq, &complete);
                if (mpi_errno != MPI_SUCCESS)
                {
                    ch3i_free_overlapped(pov);
                    return MPIU_ERR_FAIL(mpi_errno);
                }

                if(complete)
                    break;

                rreq->dev.iov_offset = 0;
            }
        }

        TraceRecvSock_Done(conn->vc->pg_rank, MPIDI_Process.my_pg_rank, conn->vc->ch.n_recv);

        //
        // Process next message in buffer
        //

        if (conn->disconnect)
        {
            ch3i_free_overlapped(pov);
            return MPI_SUCCESS;
        }

        if(buff_size == 0)
        {
            //
            // The buffer was completely consumed without any partial message
            // header leftover. Go and read the next message if a receiver is
            // pending.
            //
            conn->recv_buff_end = 0;
            ch3i_free_overlapped(pov);
            return MPIDI_CH3I_Post_recv_pkt(conn);
        }

        if(buff_size < sizeof(conn->pkt))
        {
            //
            // There is a partial message header in the buffer go ahead and read
            // the rest of the message regardless if there is a receiver pending.
            //
            memcpy(conn->recv_buffer, buff_head, buff_size);
            conn->recv_buff_end = buff_size;
            ch3i_free_overlapped(pov);
            return MPIDI_CH3I_Post_recv_pkt(conn);
        }
    }
}


static int MPIDI_CH3I_Post_recv_pkt(MPIDI_CH3I_Connection_t* conn)
{
    MPIU_Assert(conn->recv_buff_end < sizeof(conn->recv_buffer));
    return post_recv_pkt_ex(
                conn,
                sizeof(conn->recv_buffer),
                RecvNewMessageSucceeded_cb,
                RecvFailed_cb
                );
}


static int SendMessageSucceeded_cb(EXOVERLAPPED* pexov)
{
    int complete;
    int mpi_errno;
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);

    MPIDI_CH3I_Connection_t* conn = pov->conn;
    MPIDI_VC_t* vc = conn->vc;

    {
        SendQLock lock(vc);

        MPID_Request* sreq = MPIDI_CH3I_SendQ_head_unsafe(vc);
        MPIU_Assert(sreq != NULL);


        mpi_errno = MPIDI_CH3U_Handle_send_req(vc, sreq, &complete);
        if (mpi_errno != MPI_SUCCESS)
        {
            ch3i_free_overlapped(pov);
            return MPIU_ERR_FAIL(mpi_errno);
        }

        if (complete)
        {
            TraceSendSock_Done(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);

            MPIDI_CH3I_SendQ_dequeue_unsafe(vc);

            if(conn->disconnect && MPIDI_CH3I_SendQ_empty_unsafe(vc))
            {
                ch3i_post_close_ex(conn, pov, CloseConnectionComplete_cb);
                return MPI_SUCCESS;
            }

            mpi_errno = send_head_of_queue_ex_unsafe(conn, pov);
            if(mpi_errno != MPI_SUCCESS)
            {
                return MPIU_ERR_FAIL(mpi_errno);
            }
            return mpi_errno;
        }

        /* more data to send */
        TraceSendSock_Continue(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size());
        mpi_errno = ch3i_post_writev_ex(
                        conn,
                        &sreq->dev.iov[sreq->dev.iov_offset],
                        sreq->dev.iov_count - sreq->dev.iov_offset,
                        pov,
                        SendMessageSucceeded_cb,
                        SendFailed_cb
                        );
        if (mpi_errno != MPI_SUCCESS)
        {
            ch3i_free_overlapped(pov);
            mpi_errno = MPIU_ERR_FATAL_GET(mpi_errno, MPI_ERR_OTHER, "**ch3|sock|postwrite %p %p %p", sreq, conn, vc);
            return mpi_errno;
        }
    }
    return mpi_errno;
}


static int MPIDI_CH3I_SOCK_start_write_ex(
    _In_ MPIDI_VC_t*    vc,
    _In_ MPID_Request*  sreq,
    _In_ bool queued,
    _Out_ int* complete
    )
{
    /* FIXME: the current code only agressively writes the first IOV.  Eventually it should be changed to agressively write
    as much as possible.  Ideally, the code would be shared between the send routines and the progress engine. */
    TraceSendSock_Inline(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
    MPIU_Bsize_t nb = 0;
    int mpi_errno;

    *complete = FALSE;

    if(g_CompressionThreshold != MSMPI_COMPRESSION_OFF)
    {
        mpi_errno = CompressRequest(sreq);
        ON_ERROR_FAIL(mpi_errno);
    }

    mpi_errno = MPIDU_Sock_writev(vc->ch.conn->sock, sreq->iov(), sreq->dev.iov_count, &nb);
    ON_ERROR_FAIL(mpi_errno);

    MPIU_Assert( sreq->dev.iov_offset == 0 );
    if( !sreq->adjust_iov( nb ) )
    {
        if (queued == false)
        {
            MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);
        }

        mpi_errno = MPIDI_CH3I_Post_sendv(
                        vc->ch.conn,
                        sreq->dev.iov + sreq->dev.iov_offset,
                        sreq->dev.iov_count - sreq->dev.iov_offset
                        );
        ON_ERROR_FAIL(mpi_errno);
        goto fn_exit;
    }

    mpi_errno = MPIDI_CH3U_Handle_send_req(vc, sreq, complete);
    ON_ERROR_FAIL(mpi_errno);

    if (*complete == FALSE)
    {
        sreq->dev.iov_offset = 0;
        if (queued == false)
        {
            MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);
        }

        TraceSendSock_Continue(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size());
        mpi_errno = MPIDI_CH3I_Post_sendv(
                        vc->ch.conn,
                        sreq->dev.iov,
                        sreq->dev.iov_count
                        );
        if (mpi_errno != MPI_SUCCESS)
        {
            mpi_errno = MPIU_ERR_FATAL_GET(mpi_errno, MPI_ERR_OTHER, "**ch3|sock|postwrite %p %p %p", sreq, vc->ch.conn, vc);
        }
    }
    else
    {
        TraceSendSock_Done(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
    }

fn_exit:
    return mpi_errno;
fn_fail:
    Trace_SOCKETS_Error_MPIDI_CH3I_SOCK_start_write_PostSendVFailed(mpi_errno, vc->pg_rank);
    goto fn_exit;
}


int MPIDI_CH3I_SOCK_start_write(
    _In_ MPIDI_VC_t*    vc,
    _In_ MPID_Request*  sreq
    )
{
    int completed;
    return MPIDI_CH3I_SOCK_start_write_ex(vc, sreq, false, &completed);
}


int MPIDI_CH3I_SOCK_write_progress(
        _In_ MPIDI_VC_t*    vc
        )
{
    int mpi_errno = MPI_SUCCESS;
    int complete;
    MPID_Request* sreq = MPIDI_CH3I_SendQ_head_unsafe(vc);

    while (sreq != nullptr)
    {

        mpi_errno = MPIDI_CH3I_SOCK_start_write_ex( vc, sreq, true, &complete);
        if (mpi_errno != MPI_SUCCESS)
        {
            MPIDI_CH3I_SendQ_dequeue_unsafe(vc);
            break;
        }

        //
        // if the request is not complete leave it in the queue and exit
        //  the handler to continue the progress pump
        //
        //
        if (complete == FALSE)
        {
            break;
        }

        MPIDI_CH3I_SendQ_dequeue_unsafe(vc);
        sreq = MPIDI_CH3I_SendQ_head_unsafe(vc);
    }

    return mpi_errno;
}


int MPIDI_CH3I_Post_sendv(MPIDI_CH3I_Connection_t* conn, MPID_IOV* iov, int iov_n)
{
    return ch3i_post_writev( conn,
                             iov,
                             iov_n,
                             SendMessageSucceeded_cb,
                             SendFailed_cb);
}


static int SendOpenResponseSucceeded_cb(EXOVERLAPPED* pexov)
{
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;

    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;
    get_sock_peer_address(conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);

    /* finished sending open response packet */
    if (Sock_open_resp_pkt(conn)->ack == TRUE)
    {
        Trace_SOCKETS_Info_SendOpenResponseSucceeded_cb_Success(pPeerAddress, peerPort);
        return connection_complete_ex(conn, pov);
    }

    Trace_SOCKETS_Info_SendOpenResponseSucceeded_cb_HeadToHead(pPeerAddress, peerPort);
    /* head-to-head connections - close this connection */
    conn->disconnect = TRUE;
    ch3i_post_close_ex(conn, pov, CloseConnectionComplete_cb);

    return MPI_SUCCESS;
}


static int RecvOpenRequestDataSucceeded_cb(EXOVERLAPPED* pexov)
{
    int mpi_errno;
    int pg_rank;
    MPIDI_VC_t* vc = NULL;
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;

    MPIU_Assert(ExGetBytesTransferred(pexov));

    MPIDI_PG_t* pg = MPIDI_PG_Find( conn->pg_id );
    if (pg == NULL)
    {
        Trace_SOCKETS_Error_RecvOpenRequestDataSucceeded_cb_PGFail();
        ch3i_free_overlapped(pov);
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**pglookup %g", conn->pg_id);
        return mpi_errno;
    }

    pg_rank = Sock_open_req_pkt(conn)->pg_rank;
    if((pg_rank >= 0) && (pg_rank < pg->size))
    {
        vc = MPIDI_PG_Get_vc(pg, pg_rank);
        MPIU_Assert(vc->pg_rank == pg_rank);
    }

    if(vc == NULL)
    {
        /* bad rank, reject connection */
        connection_reject(conn);
    }
    else if (vc->ch.conn == NULL)
    {
        /* no head-to-head connects, accept the connection */
        connection_accept(conn, vc);

        //
        // No connection object exist, the vc should not be in the
        // connecting state.
        //
        MPIU_Assert(vc->ch.state != MPIDI_CH3I_VC_STATE_CONNECTING);
        vc->ch.channel = MPIDI_CH3I_CH_TYPE_SOCK;
        vc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTING;
    }
    else
    {
        /* head to head situation */
        if (pg == MPIDI_Process.my_pg)
        {
            /* the other process is in the same comm_world; just compare the ranks */
            if (Mpi.CommWorld->rank < pg_rank)
            {
                /* accept connection */
                connection_accept(conn, vc);
            }
            else
            {
                /* refuse connection */
                connection_reject(conn);
            }
        }
        else
        {
            RPC_STATUS status;
            /* the two processes are in different comm_worlds; compare their unique pg_ids. */
            if ( UuidCompare( &MPIDI_Process.my_pg->id, &pg->id, &status ) < 0 )
            {
                /* accept connection */
                connection_accept(conn, vc);
            }
            else
            {
                /* refuse connection */
                connection_reject(conn);
            }
        }
    }

    mpi_errno = ch3i_post_write_ex(
                    conn,
                    &conn->pkt,
                    sizeof(conn->pkt),
                    pov,
                    SendOpenResponseSucceeded_cb,
                    SendFailed_cb
                    );
    if (mpi_errno != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
        Trace_SOCKETS_Error_RecvOpenRequestDataSucceeded_cb_SendResponseFailed(mpi_errno, conn->vc->pg_rank);
        mpi_errno = MPIU_ERR_FATAL_GET(mpi_errno, MPI_ERR_INTERN, "**ch3|sock|open_lrecv_data");
        return mpi_errno;
    }

    return MPI_SUCCESS;
}


static int unexpected_control_pkt(const MPIDI_CH3I_Connection_t* conn)
{
    return MPIU_ERR_CREATE(MPI_ERR_INTERN, "**ch3|sock|badpacket %d", conn->pkt.type);
}


static int RecvOpenRequestSucceeded_cb(EXOVERLAPPED* pexov)
{
    int mpi_errno;
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;

    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;
    get_sock_peer_address(conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);

    MPIU_Assert(ExGetBytesTransferred(pexov) > 0);
    MPIU_Assert(conn->recv_buff_end == 0);
    MPIU_Assert(ExGetBytesTransferred(pexov) == sizeof(conn->pkt));

    if(Sock_open_req_pkt(conn)->sig != SC_OPEN_REQ)
    {
        Trace_SOCKETS_Error_RecvOpenRequestSucceeded_cb(RecvOpenRequestSucceededUnexpectedControl, pPeerAddress, peerPort);
        mpi_errno = unexpected_control_pkt(conn);
    }
    else if(Sock_open_req_pkt(conn)->ver != MSMPI_VER_EX)
    {
        Trace_SOCKETS_Error_RecvOpenRequestSucceeded_cb(RecvOpenRequestSucceededMismatchedVersion, pPeerAddress, peerPort);
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**version %d %d %d %d %d %d",
            MSMPI_VER_MAJOR( Sock_open_req_pkt(conn)->ver ),
            MSMPI_VER_MINOR( Sock_open_req_pkt(conn)->ver ),
            MSMPI_VER_BUILD( Sock_open_req_pkt(conn)->ver ),
            MSMPI_VER_MAJOR( MSMPI_VER_EX ),
            MSMPI_VER_MINOR( MSMPI_VER_EX ),
            MSMPI_VER_BUILD( MSMPI_VER_EX )
            );
    }
    else
    {
        Trace_SOCKETS_Info_RecvOpenRequestSucceeded_cb(RecvOpenRequestSucceededSuccess, pPeerAddress, peerPort);
        mpi_errno = ch3i_post_read_ex(
                        conn,
                        &conn->pg_id,
                        sizeof( conn->pg_id ),
                        pov,
                        RecvOpenRequestDataSucceeded_cb,
                        RecvFailed_cb
                        );
    }

    if (mpi_errno != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    return MPI_SUCCESS;
}


static int RecvOpenRequestFailed_cb(EXOVERLAPPED* pexov)
{
    //
    // Receiving the open request message failed. This might happen in a
    // head-to-head race when the other side wins, and thus close the connection
    // before this side is able to read from it. Close the connection and free
    // the resources.
    //


    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;

    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;
    get_sock_peer_address(conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);
    Trace_SOCKETS_Info_RecvOpenRequestFailed_cb(pPeerAddress, peerPort);

    ch3i_free_overlapped(pov);
    MPIDU_Sock_close(conn->sock);
    MPIDI_CH3I_Connection_free(conn);
    return MPI_SUCCESS;
}


static int AcceptNewConnectionFailed_cb(EXOVERLAPPED* pexov)
{
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    int gle = ExGetStatus( pexov );

    ch3i_free_overlapped(pov);

    //
    // We should not handle WSA_OPERATION_ABORTED.
    // It usually means the socket had died unexpectedly.
    // (Corrupted user code closed it accidentally, for example).
    // In that case it is safer and cleaner to just terminate here.
    //

    if( gle == WSAECANCELLED )
    {
        Trace_SOCKETS_Info_AcceptNewConnectionFailed_cb_Canceled();
        return MPI_SUCCESS;
    }

    Trace_SOCKETS_Error_AcceptNewConnectionFailed_cb_Failed(gle, get_error_string(gle));
    return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**fail %s %d", get_error_string(gle), gle);
}


static int AcceptNewConnectionSucceeded_cb(EXOVERLAPPED* pexov)
{
    int mpi_errno;
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* new_conn;
    MPIDI_CH3I_Connection_t* listener = pov->conn;

    mpi_errno = MPIDI_CH3I_Connection_alloc(&new_conn);
    if (mpi_errno != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    new_conn->sock = listener->aux_sock;

    /* renew the accept on the listener socket */
    mpi_errno = ch3i_post_accept_ex(
                    listener,
                    pov,
                    AcceptNewConnectionSucceeded_cb,
                    AcceptNewConnectionFailed_cb
                    );
    if (mpi_errno != MPI_SUCCESS)
    {
        Trace_SOCKETS_Error_AcceptNewConnectionSucceeded_cb_PostListener(mpi_errno);
        ch3i_free_overlapped(pov);
        MPIDU_Sock_close(new_conn->sock);
        MPIDI_CH3I_Connection_free(new_conn);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;
    get_sock_peer_address(new_conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);

    mpi_errno = post_recv_pkt_ex(
                    new_conn,
                    sizeof(new_conn->pkt),
                    RecvOpenRequestSucceeded_cb,
                    RecvOpenRequestFailed_cb
                    );
    if (mpi_errno != MPI_SUCCESS)
    {
        //
        // Don't fail the call if failed to read from the connection.
        // This might happen in a head-to-head race when the other side wins,
        // and thus close the connection before this side is able to read from
        // it.
        //
        Trace_SOCKETS_Info_AcceptNewConnectionSucceeded_cb_HeadToHead(mpi_errno, pPeerAddress, peerPort);
        MPIDU_Sock_close(new_conn->sock);
        MPIDI_CH3I_Connection_free(new_conn);
    }

    Trace_SOCKETS_Info_AcceptNewConnectionSucceeded_cb_Succeeded(pPeerAddress, peerPort);
    return MPI_SUCCESS;
}


int MPIDI_CH3I_Post_accept(MPIDI_CH3I_Connection_t* listener)
{
    Trace_SOCKETS_Info_MPIDI_CH3I_Post_accept();
    return ch3i_post_accept( listener,
                             AcceptNewConnectionSucceeded_cb,
                             AcceptNewConnectionFailed_cb );
}


static int RecvOpenResponseSucceeded_cb(EXOVERLAPPED* pexov)
{
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;

    MPIU_Assert(ExGetBytesTransferred(pexov) > 0);
    MPIU_Assert(conn->recv_buff_end == 0);
    MPIU_Assert(ExGetBytesTransferred(pexov) == sizeof(conn->pkt));

    char pPeerAddress[IPV4_MAX_IP_ADDRESS_STRING_LENGTH];
    int peerPort;
    get_sock_peer_address(conn->sock, pPeerAddress, IPV4_MAX_IP_ADDRESS_STRING_LENGTH, &peerPort);

    if(conn->disconnect)
    {
        Trace_SOCKETS_Info_RecvOpenResponseSucceeded_cb_Disconnect(pPeerAddress, peerPort);
        ch3i_free_overlapped(pov);
        return MPI_SUCCESS;
    }

    if(Sock_open_resp_pkt(conn)->sig != SC_OPEN_RESP)
    {
        Trace_SOCKETS_Error_RecvOpenResponseSucceeded_cb_UnexpectedControl(pPeerAddress, peerPort);
        ch3i_free_overlapped(pov);
        return unexpected_control_pkt(conn);
    }

    if (Sock_open_resp_pkt(conn)->ack)
    {
        Trace_SOCKETS_Info_RecvOpenResponseSucceeded_cb_ConnectionComplete(pPeerAddress, peerPort);
        return connection_complete_ex(conn, pov);
    }
    //
    // Our connection was rejected by the other side. set this
    // connection state to closing and wait for the other side
    // connection to arrive. (or if it arrived already close
    // was posted). To allow copying the sendq to the new
    // connection.
    //
    Trace_SOCKETS_Info_RecvOpenResponseSucceeded_cb_HeadToHeadRejected(pPeerAddress, peerPort);
    conn->disconnect = TRUE;
    ch3i_free_overlapped(pov);
    return MPI_SUCCESS;
}


static int SendOpenRequestSucceeded_cb(EXOVERLAPPED* pexov)
{
    int mpi_errno;
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;
    ch3i_free_overlapped(pov);

    if(conn->disconnect)
    {
        Trace_SOCKETS_Info_SendOpenRequestSucceeded_cb_Disconnected(conn->vc->pg_rank);
        return MPI_SUCCESS;
    }

    /* finished sending open request packet */
    /* post receive for open response packet */
    mpi_errno = post_recv_pkt_ex(
                    conn,
                    sizeof(conn->pkt),
                    RecvOpenResponseSucceeded_cb,
                    RecvFailed_cb
                    );
    if (mpi_errno != MPI_SUCCESS)
    {
        Trace_SOCKETS_Error_SendOpenRequestSucceeded_cb_PostRecvPktFailed(conn->vc->pg_rank);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    Trace_SOCKETS_Info_SendOpenRequestSucceeded_cb_Succeeded(conn->vc->pg_rank);
    return MPI_SUCCESS;
}


static int send_open_request(MPIDI_CH3I_Connection_t* conn, ch3i_overlapped_t* pov)
{
    int mpi_errno;

    Sock_open_req_pkt(conn)->sig = SC_OPEN_REQ;
    Sock_open_req_pkt(conn)->ver = MSMPI_VER_EX;

    Sock_open_req_pkt(conn)->pg_rank = Mpi.CommWorld->rank;

    conn->iov[0].buf = reinterpret_cast<iovsendbuf_t*>( &conn->pkt );
    conn->iov[0].len = sizeof(conn->pkt);

    conn->iov[1].buf = reinterpret_cast<iovsendbuf_t*>( &MPIDI_Process.my_pg->id );
    conn->iov[1].len = sizeof( MPIDI_Process.my_pg->id);

    mpi_errno = ch3i_post_writev_ex(
                    conn,
                    conn->iov,
                    2,
                    pov,
                    SendOpenRequestSucceeded_cb,
                    SendFailed_cb
                    );
    if (mpi_errno != MPI_SUCCESS)
    {
        Trace_SOCKETS_Error_send_open_request_Failed(mpi_errno, conn->vc->pg_rank);
        ch3i_free_overlapped(pov);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    Trace_SOCKETS_Info_send_open_request_Succeeded(conn->vc->pg_rank);
    return MPI_SUCCESS;
}


static int ConnectFailed_cb(EXOVERLAPPED* pexov)
{
    int mpi_errno = ExGetStatus(pexov);
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    const MPIDI_CH3I_Connection_t* conn = pov->conn;

    ch3i_free_overlapped(pov);

    if(conn->disconnect)
    {
        Trace_SOCKETS_Info_ConnectFailed_cb_Disconnect(conn->vc->pg_rank);
        return MPI_SUCCESS;
    }

    Trace_SOCKETS_Error_ConnectFailed_cb_Failed(mpi_errno, conn->vc->pg_rank);
    return MPIU_ERR_GET(mpi_errno, "**ch3|sock|connfailed %g %d", conn->vc->pg->id, conn->vc->pg_rank);
}


static int ConnectSucceeded_cb(EXOVERLAPPED* pexov)
{
    ch3i_overlapped_t* pov = ch3i_ov_from_exov(pexov);
    MPIDI_CH3I_Connection_t* conn = pov->conn;

    if(conn->disconnect)
    {
        Trace_SOCKETS_Info_ConnectSucceeded_cb_Disconnect(conn->vc->pg_rank);
        ch3i_free_overlapped(pov);
        return MPI_SUCCESS;
    }

    Trace_SOCKETS_Info_ConnectSucceeded_cb_Succeeded(conn->vc->pg_rank);
    return send_open_request(conn, pov);
}


int
MPIDI_CH3I_Post_connect(
    _In_   MPIDI_CH3I_Connection_t* conn,
    _In_z_ const char*              host_description,
    _In_   int                      port )
{
    Trace_SOCKETS_Info_MPIDI_CH3I_Post_connect(conn->vc->pg_rank, host_description, port);
    return ch3i_post_connect(conn, host_description, port, ConnectSucceeded_cb, ConnectFailed_cb);
}
