// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once


inline void MPID_Request::AddRef()
{
    MPIU_Assert( ref_count != 0 );
    ::InterlockedIncrement(&ref_count);
}


inline void MPID_Request::Release()
{
    MPIU_Assert( ref_count != 0 );
    long value = ::InterlockedDecrement(&ref_count);
    if(value == 0)
    {
        MPIDI_CH3_Request_destroy(this);
    }
}


inline MPIDI_CH3_Pkt_t* MPID_Request::get_pkt()
{
    MPIU_Assert(dev.iov_count > 0);
    MPIU_Assert(dev.iov[0].buf == (void*)&dev.pkt);
    return &dev.pkt;
}


inline void MPID_Request::init_pkt(MPIDI_CH3_Pkt_type_t type)
{
    dev.pkt.type = type;
    dev.iov[0].buf = (iovsendbuf_t*)&dev.pkt;
    dev.iov[0].len = sizeof(dev.pkt);
    dev.iov_count = 1;
    dev.iov_offset = 0;
}


inline void MPID_Request::init_pkt(const MPIDI_CH3_Pkt_t& pkt)
{
    dev.pkt = pkt;
    dev.iov[0].buf = (iovsendbuf_t*)&dev.pkt;
    dev.iov[0].len = sizeof(dev.pkt);
    dev.iov_count = 1;
    dev.iov_offset = 0;
}


inline MPID_IOV* MPID_Request::iov()
{
    return dev.iov;
}


inline void MPID_Request::add_recv_iov(void* buf, int len)
{
    MPIU_Assert(dev.iov_count < _countof(dev.iov));
    dev.iov[dev.iov_count].buf = static_cast<iovsendbuf_t*>(buf);
    dev.iov[dev.iov_count].len = len;
    dev.iov_count++;
}


inline void MPID_Request::add_send_iov(const void* buf, int len)
{
    add_recv_iov(const_cast<void*>(buf), len);
}


inline int MPID_Request::iov_size() const
{
    return ::iov_size(dev.iov, dev.iov_count);
}


//
// Adjust the iovec in the request by the supplied number of bytes.
// If the iovec has been consumed, return true; otherwise return false.
//
inline bool MPID_Request::adjust_iov( MPIDI_msg_sz_t nb )
{
    while( dev.iov_offset < dev.iov_count )
    {
        if( dev.iov[dev.iov_offset].len <= nb )
        {
            nb -= dev.iov[dev.iov_offset].len;
            dev.iov_offset++;
        }
        else
        {
            dev.iov[dev.iov_offset].buf = (iovsendbuf_t*)((char *) dev.iov[dev.iov_offset].buf + nb);
            dev.iov[dev.iov_offset].len -= nb;
            return false;
        }
    }

    return true;
}


inline MPI_RESULT
MPID_Request::set_apc(
    MSMPI_Request_callback* callback_fn,
    MPI_Status* callback_status
    )
{
    int mpierrno = Mpi.CallState->OpenThreadHandle(&threadInfo);
    if( mpierrno != MPI_SUCCESS )
    {
        return mpierrno;
    }

    pfnCallback = callback_fn;
    pStatus = reinterpret_cast<MPIR_Status*>( callback_status );

    //
    // The request could already be complete.  In that case, we need to trigger the APC manually.
    //
    if( cc == 0 )
    {
        signal_apc();
    }
    return MPI_SUCCESS;
}


inline void MPID_Request::signal_apc()
{
    MPIU_Assert( threadInfo != NULL );
    //
    // Copy request status to the user's status entry.  Note that if
    // the user freed the entry, we'll AV.  Bad dog, no cookie.
    //
    *pStatus = status;

    threadInfo->QueueApc( reinterpret_cast<PAPCFUNC>(pfnCallback), reinterpret_cast<ULONG_PTR>(pStatus) );
    threadInfo = NULL;
}


inline void MPID_Request::signal_completion()
{
    MPIU_Assert(kind != MPID_PERSISTENT_SEND);
    MPIU_Assert(kind != MPID_PERSISTENT_RECV);
    long value = ::InterlockedDecrement(&cc);
    if( threadInfo != NULL && value == 0 )
    {
        signal_apc();
    }
}


inline void MPID_Request::postpone_completion()
{
    MPIU_Assert(kind != MPID_PERSISTENT_SEND);
    MPIU_Assert(kind != MPID_PERSISTENT_RECV);
    ::InterlockedIncrement(&cc);
}


inline bool MPID_Request::is_internal_complete() const
{
    return (cc == 0);
}


inline bool MPID_Request::test_complete()
{
    switch( kind )
    {
    case MPID_BUFFERED_SEND:
        return bsend_is_complete();

    case MPID_PERSISTENT_SEND:
    case MPID_PERSISTENT_RECV:
        return preq_is_complete();

    case MPID_REQUEST_MPROBE:
        return false;

    case MPID_REQUEST_NBC:
        return nbcreq_test_complete();

    default:
        return is_internal_complete();
    }
}


inline bool MPID_Request::preq_is_complete() const
{
    if(partner_request == NULL)
        return true;

    return partner_request->test_complete();
}


inline bool MPID_Request::bsend_is_complete() const
{
    if(!dev.cancel_pending)
        return true;

    return is_internal_complete();
}


inline MPI_RESULT MPID_Request::execute()
{
    //
    // Receive requests get executed when they are initiated so there is
    // nothing to do here.  However, they can complete when they are first
    // initiated in case of a late receiver situation, and thus cc may be 0.
    //
    MPIU_Assert( cc != 0 || kind == MPID_REQUEST_RECV );

    switch( kind )
    {
    case MPID_REQUEST_SEND:
        switch( get_type() )
        {
        case MPIDI_REQUEST_TYPE_SEND:
            return execute_send();

        case MPIDI_REQUEST_TYPE_SSEND:
            return execute_ssend();

        case MPIDI_REQUEST_TYPE_RSEND:
            return execute_rsend();
        }

    case MPID_REQUEST_RECV:
        return MPI_SUCCESS;

    case MPID_REQUEST_NBC:
        return execute_nbc_tasklist();

    case MPID_REQUEST_NOOP:
        signal_completion();
        return MPI_SUCCESS;

    default:
        MPIU_Assert(
            kind == MPID_REQUEST_SEND ||
            kind == MPID_REQUEST_RECV ||
            kind == MPID_REQUEST_NBC ||
            kind == MPID_REQUEST_NOOP
            );
        return MPI_SUCCESS;
    }
}


inline int MPID_Request::get_type() const
{
    return dev.flags.req_type;
}


inline void MPID_Request::set_type(int type)
{
    dev.flags.req_type = type;
}


inline int MPID_Request::get_msg_type() const
{
    return dev.flags.msg_type;
}


inline void MPID_Request::set_msg_type(int msgtype)
{
    dev.flags.msg_type = msgtype;
}


inline int MPID_Request::get_and_set_cancel_pending()
{
    return InterlockedExchange(&dev.cancel_pending, TRUE);
}


inline int MPID_Request::dec_and_get_recv_pending()
{
    return InterlockedDecrement(&dev.recv_pending_count);
}


inline bool MPID_Request::using_srbuf() const
{
    return !!dev.flags.use_srbuf;
}


inline void MPID_Request::set_using_srbuf()
{
    dev.flags.use_srbuf = 1;
}


inline void MPID_Request::clear_using_srbuf()
{
    dev.flags.use_srbuf = 0;
}


inline bool MPID_Request::needs_sync_ack() const
{
    return !!dev.flags.sync_ack;
}


inline void MPID_Request::set_sync_ack_needed()
{
    dev.flags.sync_ack = 1;
}


inline bool MPID_Request::is_rma_last_op() const
{
    return !!dev.flags.rma_last_op;
}


inline void MPID_Request::set_rma_last_op()
{
    dev.flags.rma_last_op = 1;
}


inline void MPID_Request::clear_rma_last_op()
{
    dev.flags.rma_last_op = 0;
}


inline bool MPID_Request::rma_unlock_needed() const
{
    return !!dev.flags.rma_unlock;
}


inline void MPID_Request::set_rma_unlock()
{
    dev.flags.rma_unlock = 1;
}


inline void MPID_Request::clear_rma_unlock()
{
    dev.flags.rma_unlock = 0;
}


static inline MPID_Request* MPID_Request_create_base(MPID_Request_kind_t kind)
{
    MPID_Request * req;
    req = (MPID_Request*)MPIU_Handle_obj_alloc(&MPID_Request_mem);
    if (req == NULL)
        return NULL;

    MPIU_Assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);

    req->kind = kind;
    req->cc = 1;
    req->status.MPI_ERROR = MPI_SUCCESS;
    req->status.cancelled = FALSE;
    req->dev.state = 0;
    req->dev.iov_offset = 0;
    req->dev.datatype = g_hBuiltinTypes.MPI_Datatype_null;
    req->dev.segment_ptr = NULL;
    req->dev.cancel_pending = FALSE;
    req->dev.OnDataAvail = NULL;
    req->threadInfo = NULL;

    req->dev.pCompressionBuffer = NULL;
    req->dev.pkt.base.flags = MPIDI_CH3_PKT_FLAGS_CLEAR;

#ifdef HAVE_DEBUGGER_SUPPORT
    req->sendq_req_ptr         = nullptr;
#endif

    if( kind == MPID_REQUEST_NBC )
    {
        req->nbc.tmpBuf = nullptr;
        req->nbc.tasks = nullptr;
        req->nbc.nTasks = 0;
        req->nbc.nCompletedTasks = 0;
    }

    return req;
}


/* FIXME: Why does a send request need the match information?
   Is that for debugging information?  In case the initial envelope
   cannot be sent? Ditto for the dev.user_buf, count, and datatype
   fields when the data is sent eagerly.

   The following fields needed to be set:
   datatype_ptr
   status.MPI_ERROR

   Note that this macro requires that rank, tag, context_offset,
   comm, buf, datatype, and count all be available with those names
   (they are not arguments to the routine)
*/
inline
MPID_Request*
MPIDI_Request_create_sreq(
    _In_opt_ int reqtype,
    _In_ const void* buf,
    _In_ int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm
)
{
    MPID_Request* pReq = MPID_Request_create_base(MPID_REQUEST_SEND);
    if( pReq == NULL )
        return NULL;

    pReq->set_type(reqtype);

    pReq->comm = comm;
    comm->AddRef();

    //
    // The match data is stored with the send request to be used for
    // cancelling nonblocking send reuqests.
    //
    pReq->dev.match.rank = rank;
    pReq->dev.match.tag = tag;
    pReq->dev.match.context_id = comm->context_id;

    pReq->dev.user_buf = (void *)(buf);
    pReq->dev.user_count = count;
    pReq->dev.datatype = datatype;
    if( datatype.IsPredefined() == false )
    {
        // We take a ref on non-builtin types to keep them
        // from being freed while still in use internally.
        pReq->dev.datatype.Get()->AddRef();
    }

    return pReq;
}


/* This is the receive request version of MPIDI_Request_create_sreq */
inline MPID_Request* MPIDI_Request_create_rreq()
{
    MPID_Request* pReq = MPID_Request_create_base(MPID_REQUEST_RECV);
    if (pReq == NULL)
        return NULL;

    pReq->comm = NULL;

    return pReq;
}

inline MPID_Request* MPIDI_Request_create_completed_req()
{
    MPID_Request* preq = MPID_Request_create(MPID_REQUEST_NOOP);
    if (preq == nullptr)
    {
        return nullptr;
    }
    preq->status.MPI_ERROR = MPI_SUCCESS;
    preq->signal_completion();
    return preq;
}

static inline int MPIR_Wait(MPID_Request* req)
{
    while(req->test_complete() == false)
    {
        int mpi_errno = MPID_Progress_wait();
        if(mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }

    return MPI_SUCCESS;
}


static inline
MPI_RESULT
MPIR_Waitall(
    _In_ int count,
    _In_reads_(count) MPID_Request* req[]
    )
{
    MPI_RESULT reqErr = MPI_SUCCESS;
    for( int i = 0; i < count; ++i )
    {
        MPI_RESULT mpi_errno = MPIR_Wait( req[i] );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        if( req[i]->status.MPI_ERROR != MPI_SUCCESS )
        {
            reqErr = req[i]->status.MPI_ERROR;
        }
    }

    return reqErr;
}


static inline
MPI_RESULT
MPIR_Waitany(
    _In_ int count,
    _In_reads_(count) MPID_Request* req[],
    _Out_ unsigned int* index
    )
{
    MPI_RESULT reqErr = MPI_SUCCESS;
    for(;;)
    {
        for( unsigned int i = 0; i < static_cast<unsigned int>( count ); ++i )
        {
            if( req[i] == nullptr ||
                req[i]->test_complete() == false )
            {
                continue;
            }

            //
            // Found a complete request
            //
            if( req[i]->status.MPI_ERROR != MPI_SUCCESS )
            {
                reqErr = req[i]->status.MPI_ERROR;
            }
            *index = i;
            return reqErr;
        }

        reqErr = MPID_Progress_wait();
        if( reqErr != MPI_SUCCESS )
        {
            return reqErr;
        }
    }
}


static inline int MPIR_WaitInternal(MPID_Request* req)
{
    while(req->is_internal_complete() == false)
    {
        int mpi_errno = MPID_Progress_wait();
        if(mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }

    return MPI_SUCCESS;
}


static inline void
MPIR_Request_extract_status(
    _In_ const MPID_Request *req,
    _Inout_ MPI_Status *status
    )
{
    if( status != MPI_STATUS_IGNORE )
    {
        MPIR_Status_set_count( status, req->status.count );
        MPIR_Status_set_cancelled( status, req->status.cancelled );
        status->MPI_SOURCE  = req->status.MPI_SOURCE;
        status->MPI_TAG     = req->status.MPI_TAG;
    }
}


MPI_RESULT
MPID_Request::execute_send()
{
    TraceSendMsg(
        comm->handle,
        dev.match.rank,
        comm->rank,
        dev.match.tag,
        dev.datatype.GetMpiHandle(),
        dev.user_buf,
        dev.user_count
        );

    if (dev.match.rank == MPI_PROC_NULL)
    {
        signal_completion();
        return MPI_SUCCESS;
    }

    if (dev.match.rank == comm->rank && comm->comm_kind != MPID_INTERCOMM)
    {
        return MPIDI_Isend_self(this);
    }

    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = static_cast<MPIDI_msg_sz_t>(
        dev.datatype.GetSizeAndInfo( dev.user_count, &dt_contig, &dt_true_lb )
        );

    const MPIDI_VC_t* vc = MPIDI_Comm_get_vc(comm, dev.match.rank);

    if (data_sz <= vc->eager_max_msg_sz)
    {
        return MPIDI_CH3_SendEager(
                        this,
                        MPIDI_CH3_PKT_EAGER_SEND,
                        data_sz,
                        dt_contig,
                        dt_true_lb
                        );
    }
    else
    {
        return MPIDI_CH3_SendRndv(this, data_sz);
    }
}


MPI_RESULT
MPID_Request::execute_ssend()
{
    TraceSsendMsg(
        comm->handle,
        dev.match.rank,
        comm->rank,
        dev.match.tag,
        dev.datatype.GetMpiHandle(),
        dev.user_buf,
        dev.user_count
        );

    if (dev.match.rank == MPI_PROC_NULL)
    {
        signal_completion();
        return MPI_SUCCESS;
    }

    if (dev.match.rank == comm->rank && comm->comm_kind != MPID_INTERCOMM)
    {
        return MPIDI_Isend_self(this);
    }

    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = static_cast<MPIDI_msg_sz_t>(
        dev.datatype.GetSizeAndInfo( dev.user_count, &dt_contig, &dt_true_lb )
        );

    const MPIDI_VC_t* vc = MPIDI_Comm_get_vc(comm, dev.match.rank);

    if (data_sz > vc->eager_max_msg_sz)
    {
        return MPIDI_CH3_SendRndv(this, data_sz);
    }

    MPI_RESULT mpi_errno = MPIDI_CH3_SendEager(
        this,
        MPIDI_CH3_PKT_EAGER_SYNC_SEND,
        data_sz,
        dt_contig,
        dt_true_lb
        );

    if( mpi_errno == MPI_SUCCESS )
    {
        //
        // N.B. The handle value is sent to support sync-ack.  Take a reference on
        //      the send request to keep it alive until the sync-ack comes back.
        //      The code adds the reference here, after the actual send, to avoid
        //      the need to release the request in case of an error.
        //      The request is complete only when the sync-ack comes back, hence
        //      postpone the completion.
        //
        AddRef();
        postpone_completion();
    }
    return mpi_errno;
}


MPI_RESULT
MPID_Request::execute_rsend()
{
    TraceRsendMsg(
        comm->handle,
        dev.match.rank,
        comm->rank,
        dev.match.tag,
        dev.datatype.GetMpiHandle(),
        dev.user_buf,
        dev.user_count
        );

    if (dev.match.rank == MPI_PROC_NULL)
    {
        signal_completion();
        return MPI_SUCCESS;
    }

    if (dev.match.rank == comm->rank && comm->comm_kind != MPID_INTERCOMM)
    {
        return MPIDI_Isend_self(this);
    }

    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = static_cast<MPIDI_msg_sz_t>(
        dev.datatype.GetSizeAndInfo( dev.user_count, &dt_contig, &dt_true_lb )
        );

    return MPIDI_CH3_SendEager(
        this,
        MPIDI_CH3_PKT_READY_SEND,
        data_sz,
        dt_contig,
        dt_true_lb
        );
}


inline void MPID_Request::cancel()
{
    switch( kind )
    {
    case MPID_REQUEST_SEND:
        MPID_Cancel_send( this );
        break;

    case MPID_REQUEST_RECV:
        MPID_Cancel_recv( this );
        break;

    case MPID_REQUEST_NBC:
        cancel_nbc_tasklist();
        break;

    case MPID_REQUEST_NOOP:
        break;

    default:
        MPIU_Assert(
            kind == MPID_REQUEST_SEND ||
            kind == MPID_REQUEST_NBC ||
            kind == MPID_REQUEST_RECV ||
            kind == MPID_REQUEST_NOOP
            );
    }
}
