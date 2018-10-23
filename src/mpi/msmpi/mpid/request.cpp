// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "ch3_compression.h"


/* Routines and data structures for request allocation and deallocation */

C_ASSERT( HANDLE_GET_TYPE(MPI_REQUEST_NULL) == HANDLE_TYPE_INVALID );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_REQUEST_NULL) == MPID_REQUEST );

MPID_Request MPID_Request_direct[MPID_REQUEST_PREALLOC] = {{0}};
MPIU_Object_alloc_t MPID_Request_mem = {
    0, 0, 0, 0, MPID_REQUEST, sizeof(MPID_Request), MPID_Request_direct,
    MPID_REQUEST_PREALLOC };


/* See the comments above about request creation.  Some routines will
   use macros in mpidimpl.h *instead* of this routine */
MPID_Request* MPID_Request_create(MPID_Request_kind_t kind)
{
    MPID_Request * req;
    req = MPID_Request_create_base(kind);
    if (req == NULL)
        return NULL;

    /* FIXME: This makes request creation expensive.  We need to trim
       this to the basics, with additional setup for special-purpose
       requests (think base class and inheritance).  For example, do we
       *really* want to set the kind to UNDEFINED? And should the RMA
       values be set only for RMA requests? */

    /* FIXME: status fields meaningful only for receive, and even then
       should not need to be set. */
    req->status.MPI_SOURCE     = MPI_UNDEFINED;
    req->status.MPI_TAG        = MPI_UNDEFINED;
    req->status.count          = 0;
    req->comm                  = NULL;

    /* FIXME: RMA ops shouldn't need to be set except when creating a
       request for RMA operations */
    req->dev.target_win_handle = MPI_WIN_NULL;
    req->dev.source_win_handle = MPI_WIN_NULL;
    req->dev.target_rank       = MPI_PROC_NULL;
    req->dev.single_op_opt     = false;
    req->dev.lock_queue_entry  = NULL;
    req->dev.dtype_info        = NULL;
    req->dev.dataloop          = NULL;

    return req;
}

/* FIXME: We need a lighter-weight version of this to avoid all of the
   extra checks.  One posibility would be a single, no special case (no
   comm, datatype, or srbuf to check) and a more general (check everything)
   version.  */
void MPIDI_CH3_Request_destroy(MPID_Request * req)
{
    MPIU_Assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);
    MPIU_Assert(req->ref_count == 0);

    /* FIXME: We need a better way to handle these so that we
       do not always need to initialize these fields and check them
       when we destroy a request */
    /* FIXME: We need a way to call these routines ONLY when the
       related ref count has become zero. */
    if (req->comm != NULL)
    {
        MPIR_Comm_release(req->comm, 0);
    }

    if (req->dev.datatype.IsPredefined() == false)
    {
        req->dev.datatype.Get()->Release();
    }

    if (req->dev.segment_ptr != NULL)
    {
        MPID_Segment_free(req->dev.segment_ptr);
    }

    if (req->using_srbuf())
    {
        MPIDI_CH3U_SRBuf_free(req);
    }

    if (req->dev.pCompressionBuffer != NULL)
    {
        MPIU_Free(req->dev.pCompressionBuffer);
        req->dev.pCompressionBuffer = NULL;
    }

    if( req->kind == MPID_REQUEST_NBC )
    {
        if( req->nbc.tmpBuf != nullptr )
        {
            delete[] req->nbc.tmpBuf;
        }

        if( req->nbc.tasks != nullptr )
        {
            delete[] req->nbc.tasks;
        }
    }

    if (req->kind == MPID_REQUEST_RMA_RESP)
    {
        if (req->dev.user_buf != nullptr)
        {
            MPIU_Free(req->dev.user_buf);
            req->dev.user_buf = nullptr;
        }
    }

    MPIU_Handle_obj_free(&MPID_Request_mem, req);
}


int
MPIAPI
MPIR_Grequest_query_c_proxy(
    MPI_Grequest_query_function* user_function,
    void* extra_state,
    MPI_Status* status
    )
{
    return user_function(extra_state, status);
}


int
MPIAPI
MPIR_Grequest_free_c_proxy(
    MPI_Grequest_free_function* user_function,
    void* extra_state
    )
{
    return user_function(extra_state);
}


int
MPIAPI
MPIR_Grequest_cancel_c_proxy(
    MPI_Grequest_cancel_function* user_function,
    void* extra_state,
    int complete
    )
{
    return user_function(extra_state, complete);
}


/* Complete a request, saving the status data if necessary.
   "active" has meaning only if the request is a persistent request; this
   allows the completion routines to indicate that a persistent request
   was inactive and did not require any extra completion operation.

   If debugger information is being provided for pending (user-initiated)
   send operations, the macros MPIR_SENDQ_FORGET will be defined to
   call the routine MPIR_Sendq_forget; otherwise that macro will be a no-op.
   The implementation of the MPIR_Sendq_xxx is in src/mpi/debugger/dbginit.c .
*/
MPI_RESULT MPIR_Request_complete(MPI_Request * request, MPID_Request * request_ptr,
                          MPI_Status * status, int * active)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    *active = TRUE;
    switch(request_ptr->kind)
    {
        case MPID_BUFFERED_SEND:
        case MPID_REQUEST_SEND:
        case MPID_REQUEST_RMA_PUT:
            if (status != MPI_STATUS_IGNORE)
            {
                MPIR_Status_set_cancelled( status, request_ptr->status.cancelled );
            }
            mpi_errno = request_ptr->status.MPI_ERROR;
            request_ptr->Release();
            /* FIXME: are Ibsend requests added to the send queue? */
            MPIR_SENDQ_FORGET(request_ptr);
            MPID_Request_track_user_free();
            *request = MPI_REQUEST_NULL;
            break;

        case MPID_REQUEST_RECV:
        case MPID_REQUEST_RMA_GET:
            MPIR_Request_extract_status(request_ptr, status);
            mpi_errno = request_ptr->status.MPI_ERROR;
            request_ptr->Release();
            MPID_Request_track_user_free();
            *request = MPI_REQUEST_NULL;
            break;

        case MPID_PERSISTENT_SEND:
            if (request_ptr->partner_request != NULL)
            {
                MPID_Request * prequest_ptr = request_ptr->partner_request;

                /* reset persistent request to inactive state */
                request_ptr->partner_request = NULL;

                if (status != MPI_STATUS_IGNORE)
                {
                    MPIR_Status_set_cancelled( status, prequest_ptr->status.cancelled );
                }
                mpi_errno = prequest_ptr->status.MPI_ERROR;

                prequest_ptr->Release();
            }
            else
            {
                if (request_ptr->status.MPI_ERROR != MPI_SUCCESS)
                {
                    /* if the persistent request failed to start then make the
                       error code available */
                    if (status != MPI_STATUS_IGNORE)
                    {
                        MPIR_Status_set_cancelled( status, FALSE );
                    }
                    mpi_errno = request_ptr->status.MPI_ERROR;
                }
                else
                {
                    MPIR_Status_set_empty(status);
                    *active = FALSE;
                }
            }
            break;

        case MPID_PERSISTENT_RECV:
            if (request_ptr->partner_request != NULL)
            {
                MPID_Request * prequest_ptr = request_ptr->partner_request;

                /* reset persistent request to inactive state */
                request_ptr->partner_request = NULL;

                MPIR_Request_extract_status(prequest_ptr, status);
                mpi_errno = prequest_ptr->status.MPI_ERROR;

                prequest_ptr->Release();
            }
            else
            {
                MPIR_Status_set_empty(status);
                if (request_ptr->status.MPI_ERROR != MPI_SUCCESS)
                {
                    /* if the persistent request failed to start then make the
                       error code available */
                    mpi_errno = request_ptr->status.MPI_ERROR;
                }
                else
                {
                    *active = FALSE;
                }
            }
            break;

        case MPID_GREQUEST:
        {
            mpi_errno = MPIR_Grequest_query(request_ptr);

            MPIR_Request_extract_status(request_ptr, status);

            int rc = MPIR_Grequest_free(request_ptr);
            if (mpi_errno == MPI_SUCCESS)
            {
                mpi_errno = rc;
            }

            request_ptr->Release();
            MPID_Request_track_user_free();
            *request = MPI_REQUEST_NULL;

            break;
        }

        case MPID_REQUEST_NBC:
        case MPID_REQUEST_NOOP:
            MPIR_Request_extract_status(request_ptr, status);
            mpi_errno = request_ptr->status.MPI_ERROR;
            request_ptr->Release();
            MPID_Request_track_user_free();
            *request = MPI_REQUEST_NULL;
            break;

        default:
            /* This should not happen */
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**badcase %d", request_ptr->kind);
            break;
    }

    return mpi_errno;
}


/* FIXME: What is this routine for?
 *
 * [BRT] it is used by testall, although looking at testall now, I think the
 * algorithm can be change slightly and eliminate the need for this routine
 */
MPI_RESULT MPIR_Request_get_error(MPID_Request * request_ptr)
{
    MPI_RESULT mpi_errno;

    switch(request_ptr->kind)
    {
        case MPID_BUFFERED_SEND:
        case MPID_REQUEST_SEND:
        case MPID_REQUEST_RECV:
        case MPID_REQUEST_RMA_PUT:
        case MPID_REQUEST_RMA_GET:
        case MPID_REQUEST_NOOP:
            mpi_errno = request_ptr->status.MPI_ERROR;
            break;

        case MPID_PERSISTENT_SEND:
        case MPID_PERSISTENT_RECV:
            if (request_ptr->partner_request != NULL)
            {
                mpi_errno = request_ptr->partner_request->status.MPI_ERROR;
            }
            else
            {
                mpi_errno = request_ptr->status.MPI_ERROR;
            }

            break;

        case MPID_GREQUEST:
            mpi_errno = MPIR_Grequest_query(request_ptr);
            break;

        default:
            /* This should not happen */
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**badcase %d", request_ptr->kind);
            break;
    }

    return mpi_errno;
}


MPI_RESULT
MPIR_Grequest_cancel(
    _In_ MPID_Request* request
    )
{
    MPID_Grequest_cancel_function* c = &request->greq.cancel;

    MPI_RESULT mpi_errno = c->proxy( c->user_function,
                                     request->greq.extra_state,
                                     request->test_complete() );
    if( mpi_errno != MPI_SUCCESS )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**usercancel %d", mpi_errno);
    }
    return mpi_errno;
}


MPI_RESULT
MPIR_Grequest_query(
    _In_ MPID_Request* request
    )
{
    MPID_Grequest_query_function* q = &request->greq.query;

    MPI_RESULT mpi_errno = q->proxy(
        q->user_function,
        request->greq.extra_state,
        reinterpret_cast<MPI_Status*>( &request->status )
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**userquery %d", mpi_errno);
    }
    return mpi_errno;
}


MPI_RESULT
MPIR_Grequest_free(
    _In_ MPID_Request* request
    )
{
    MPID_Grequest_free_function* f = &request->greq.free;

    MPI_RESULT mpi_errno = f->proxy( f->user_function, request->greq.extra_state );
    if( mpi_errno != MPI_SUCCESS )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**userfree %d", mpi_errno);
    }
    return mpi_errno;
}


/* FIXME: Consider using function pointers for invoking persistent requests;
   if we made those part of the public request structure, the top-level routine
   could implement startall unless the device wanted to study the requests
   and reorder them */

/* FIXME: Where is the memory registration call in the init routines,
   in case the channel wishes to take special action (such as pinning for DMA)
   the memory? This was part of the design. */

static
MPID_Request*
create_psreq(
    MPID_Request_kind_t kind,
    int reqtype,
    MPID_Comm* comm,
    int rank,
    int tag,
    const MPI_CONTEXT_ID& context_id,
    void* buf,
    int count,
    TypeHandle datatype
    )
{
    MPID_Request* req = MPID_Request_create(kind);
    if(req == NULL)
        return NULL;

    req->set_type(reqtype);

    req->cc   = 0;
    req->comm = comm;
    comm->AddRef();
    req->dev.match.rank = rank;
    req->dev.match.tag = tag;
    req->dev.match.context_id = context_id;
    req->dev.user_buf = buf;
    req->dev.user_count = count;
    req->partner_request = NULL;
    req->dev.datatype = datatype;
    if( req->dev.datatype.IsPredefined() == false )
    {
        req->dev.datatype.Get()->AddRef();
    }

    return req;
}

static inline
MPID_Request*
MPIDI_Request_create_send_psreq(
    int reqtype,
    MPID_Comm* comm,
    int rank,
    int tag,
    const void* buf,
    int count,
    TypeHandle datatype
    )
{
    return create_psreq(
        MPID_PERSISTENT_SEND,
        reqtype,
        comm,
        rank,
        tag,
        comm->context_id,
        (void*)buf,
        count,
        datatype
        );
}

inline
MPID_Request*
MPIDI_Request_create_recv_psreq(
    MPID_Comm* comm,
    int rank,
    int tag,
    void* buf,
    int count,
    TypeHandle datatype
    )
{
    return create_psreq(
        MPID_PERSISTENT_RECV,
        MPIDI_REQUEST_TYPE_RECV,
        comm,
        rank,
        tag,
        comm->recvcontext_id,
        buf,
        count,
        datatype
        );
}


/*
 * MPID_Startall()
 */
MPI_RESULT
MPID_Startall(
    _In_ int count,
    _Inout_updates_(count) MPID_Request* requests[]
    )
{
    int i;
    int rc;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    for (i = 0; i < count; i++)
    {
        MPID_Request* preq = requests[i];

        /* FIXME: The odd 7th arg (match.context_id - comm->context_id)
           is probably to get the context offset.  Do we really need the
           context offset? Is there any case where the offset isn't zero? */
        switch (preq->get_type())
        {
            case MPIDI_REQUEST_TYPE_RECV:
            {
                rc = MPID_Recv(preq->dev.user_buf, preq->dev.user_count, preq->dev.datatype, preq->dev.match.rank,
                    preq->dev.match.tag, preq->comm, &preq->partner_request);
                break;
            }

            case MPIDI_REQUEST_TYPE_SEND:
            {
                rc = MPID_Send(preq->dev.user_buf, preq->dev.user_count, preq->dev.datatype, preq->dev.match.rank,
                    preq->dev.match.tag, preq->comm, &preq->partner_request);
                break;
            }

            case MPIDI_REQUEST_TYPE_RSEND:
            {
                rc = MPID_Rsend(preq->dev.user_buf, preq->dev.user_count, preq->dev.datatype, preq->dev.match.rank,
                    preq->dev.match.tag, preq->comm, &preq->partner_request);
                break;
            }

            case MPIDI_REQUEST_TYPE_SSEND:
            {
                rc = MPID_Ssend(preq->dev.user_buf, preq->dev.user_count, preq->dev.datatype, preq->dev.match.rank,
                    preq->dev.match.tag, preq->comm, &preq->partner_request);
                break;
            }

            case MPIDI_REQUEST_TYPE_BSEND:
            {
                MPI_Request sreq_handle;

                rc = NMPI_Ibsend(preq->dev.user_buf, preq->dev.user_count,
                                 preq->dev.datatype.GetMpiHandle(), preq->dev.match.rank,
                                 preq->dev.match.tag, preq->comm->handle,
                                 &sreq_handle);
                if (rc == MPI_SUCCESS)
                {
                    // We used MPI_Ibsend above, but aren't surfacing the handle to the user.
                    MPID_Request_track_user_adjust( -1 );
                    MPID_Request_get_ptr(sreq_handle, preq->partner_request);
                }
                break;
            }

            default:
            {
                rc = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**ch3|badreqtype %d", preq->get_type());
            }
        }

        if (rc == MPI_SUCCESS)
        {
            preq->status.MPI_ERROR = MPI_SUCCESS;
        }
        else
        {
            //
            // If a failure occurs attempting to start the request, then we assume that partner
            // request was *not* created.  Stuff the error code in the persistent request.
            // The wait and test routines will look at the error code in the persistent request
            // if a partner request is not present (see MPIR_Request_complete).
            //
            // Moreover, the request must be in the completed state as persistent requests
            // are started as completed, and the Start functions must be called with inactive
            // persistent requests.
            //
            preq->partner_request = NULL;
            preq->status.MPI_ERROR = rc;
            MPIU_Assert(preq->is_internal_complete());
        }
    }

    return mpi_errno;
}


/*
 * MPID_Send_init()
 */
MPI_RESULT
MPID_Send_init(
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
    sreq = MPIDI_Request_create_send_psreq(
                MPIDI_REQUEST_TYPE_SEND,
                comm,
                rank ,
                tag,
                buf, count,
                datatype
                );
    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    *request = sreq;

    return MPI_SUCCESS;
}

/*
 * MPID_Ssend_init()
 */
MPI_RESULT
MPID_Ssend_init(
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
    sreq = MPIDI_Request_create_send_psreq(
                MPIDI_REQUEST_TYPE_SSEND,
                comm,
                rank,
                tag,
                buf,
                count,
                datatype
                );
    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    *request = sreq;
    return MPI_SUCCESS;
}

/*
 * MPID_Rsend_init()
 */
MPI_RESULT
MPID_Rsend_init(
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
    sreq = MPIDI_Request_create_send_psreq(
                MPIDI_REQUEST_TYPE_RSEND,
                comm,
                rank,
                tag,
                buf,
                count,
                datatype
                );

    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    *request = sreq;
    return MPI_SUCCESS;
}

/*
 * MPID_Bsend_init()
 */
MPI_RESULT
MPID_Bsend_init(
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
    sreq = MPIDI_Request_create_send_psreq(
                MPIDI_REQUEST_TYPE_BSEND,
                comm,
                rank,
                tag,
                buf,
                count,
                datatype
                );

    if( sreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    *request = sreq;
    return MPI_SUCCESS;
}

/*
 * FIXME: The ch3 implmentation of the persistent routines should
 * be very simple and use common code as much as possible.  All
 * persistent routine should be in the same file, along with
 * startall.  Consider using function pointers to specify the
 * start functions, as if these were generalized requests,
 * rather than having MPID_Startall look at the request type.
 */
/*
 * MPID_Recv_init()
 */
MPI_RESULT
MPID_Recv_init(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    )
{
    MPID_Request* rreq;
    rreq = MPIDI_Request_create_recv_psreq(
                comm,
                rank,
                tag,
                buf,
                count,
                datatype
                );

    if( rreq == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    *request = rreq;
    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Request::execute_nbc_tasklist(
    _In_range_(<, ULONG_MAX) ULONG nextTask
    )
{
    MPIU_Assert( nbc.tasks != nullptr && nbc.nTasks > 0 );
    MPI_RESULT mpi_errno;

    do
    {
        NbcTask* curTask = &nbc.tasks[nextTask];
        mpi_errno = curTask->Execute( comm, nbc.tag, &nextTask );
        if( mpi_errno != MPI_SUCCESS )
        {
            //
            // Clean up outstanding NBC tasks.
            //
            cancel_nbc_tasklist();
            return mpi_errno;
        }
    } while( nextTask != NBC_TASK_NONE );

    return MPI_SUCCESS;
}


bool
MPID_Request::nbcreq_test_complete()
{
    MPI_RESULT mpi_errno;
    ULONG nextTask = NBC_TASK_NONE;

    for( unsigned i = nbc.nCompletedTasks; i < nbc.nTasks; i++ )
    {
        mpi_errno = nbc.tasks[i].TestProgress( &nextTask );
        if( mpi_errno == MPI_ERR_PENDING )
        {
            //
            // Task is not finished yet.
            //
            // We have a chicken-egg situation when dealing with eager limits - 
            // before we actually make connection, we don't know which channel
            // will be used, so we don't know which eager limit to use (SHM, ND
            // or SOCK) for the first message. Thus we use the default value
            // MPIDI_CH3_EAGER_LIMIT_DEFAULT. The user could have set the eager
            // limit for the channel they want to use (for example,
            // MSMPI_ND_EAGER_LIMIT) to something larger. If the message size is
            // larger than the default value and smaller than the user defined
            // value, the messages before the channel is connected will all be
            // sent as rendezvous messages. After the channel is connected,
            // the following messages will be sent as eager messages. On the
            // receiving side, while it's still processing the rendezvous protocol
            // for the rendezvous messages, the eager message could have finished.
            // We will then start the OnComplete task of that eager receive.
            // This breaks the assumption that all the messages from the same rank
            // should be received in order, thus their OnComplete tasks should be
            // executed in order too.
            //
            // We can also repro this issue by letting the average chunk size
            // greater than the default eager limit, but letting the last chunk
            // size less than the default eager limit. This will force the last
            // chunk to be sent as eager and cause this issue to happen.
            //
            // To summary, MPI implementation guarantees in-order matching, not
            // in-order completion. Our logic depends on in-order completion.
            // To guarantee it, we quit processing the task list if the current
            // task is pending. Besides this change could improve performance
            // because we will not be going through the whole task list every
            // time and this means we will check the top unfinished task more
            // frequently.
            //
            break;
        }
        else if( mpi_errno != MPI_SUCCESS )
        {
            //
            // Clean up outstanding NBC tasks.
            //
            cancel_nbc_tasklist();
            goto fn_fail;
        }

        //
        // Task is finished.
        //
        nbc.nCompletedTasks++;

        if( nextTask != NBC_TASK_NONE )
        {
            mpi_errno = execute_nbc_tasklist( nextTask );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
    }

    return nbc.nCompletedTasks == nbc.nTasks;

fn_fail:
    //
    // Clean up outstanding NBC tasks.
    //
    status.MPI_ERROR = mpi_errno;
    signal_completion();
    return true;
}


void
MPID_Request::cancel_nbc_tasklist()
{
    for( unsigned i = 0; i < nbc.nTasks; i++ )
    {
        nbc.tasks[i].Cancel();
    }
}


void
MPID_Request::print_nbc_tasklist()
{
    if( kind == MPID_REQUEST_NOOP )
    {
        return;
    }

    if( comm->comm_kind == MPID_INTERCOMM )
    {
        return;
    }

    for( int r = 0; r < comm->remote_size; r++ )
    {
        MPIR_Barrier_intra( comm );
        if( r == comm->rank )
        {
            if( r == 0 )
            {
                printf( "task kind target init complete\n" );
            }
            printf( "Rank %d:\n", r );
            for( unsigned i = 0; i < nbc.nTasks; i++ )
            {
                printf( "%u ", i );
                nbc.tasks[i].Print();
                printf( "\n" );
            }
            printf( "\n" );
        }
        fflush( nullptr );
    }
}
