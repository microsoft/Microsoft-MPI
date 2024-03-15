// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
    MPI_Send - Performs a blocking send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (nonnegative integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Notes:
This routine may block until the message is received by the destination
process.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK

.seealso: MPI_Isend, MPI_Bsend
@*/
EXTERN_C
MPI_METHOD
MPI_Send(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Send(buf,count,datatype,dest,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request * request_ptr;
    mpi_errno = MPID_Send(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (Mpi.IsMultiThreaded() == false)
    {
        /* In the single threaded case, sending to yourself will cause deadlock. */
        if (!request_ptr->is_internal_complete())
        {
            if(request_ptr->get_msg_type() == MPIDI_REQUEST_SELF_MSG)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**dev|selfsenddeadlock");
                goto fn_fail;
            }
        }
    }

    /* If a request was returned, then we need to block until the request is complete */
    mpi_errno = MPIR_Wait(request_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = request_ptr->status.MPI_ERROR;
    request_ptr->Release();

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Send();
  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_send %p %d %D %i %t %C",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm
            )
        );
    TraceError(MPI_Send, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Send_init - Create a persistent request for a standard send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements sent (integer)
. datatype - type of each element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_RANK
.N MPI_ERR_TAG
.N MPI_ERR_COMM
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Start, MPI_Startall, MPI_Request_free
@*/
EXTERN_C
MPI_METHOD
MPI_Send_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request *request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Send_init(buf,count,datatype,dest,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Send_init(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Send_init(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_send_init %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Send_init, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Isend - Begins a nonblocking send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
.N MPI_ERR_EXHAUSTED

@*/
EXTERN_C
MPI_METHOD
MPI_Isend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Isend(buf, count, datatype, dest, tag, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Send(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIR_SENDQ_REMEMBER(request_ptr,dest,tag,comm_ptr->context_id);

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Isend(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_isend %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Isend, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Rsend - Blocking ready send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (nonnegative integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
EXTERN_C
MPI_METHOD
MPI_Rsend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Rsend(buf,count,datatype,dest,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request * request_ptr;
    mpi_errno = MPID_Rsend(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* If a request was returned, then we need to block until the request is complete */
    mpi_errno = MPIR_Wait(request_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = request_ptr->status.MPI_ERROR;
    request_ptr->Release();

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Rsend();
  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_rsend %p %d %D %i %t %C",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm
            )
        );
    TraceError(MPI_Rsend, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Rsend_init - Creates a persistent request for a ready send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements sent (integer)
. datatype - type of each element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_RANK
.N MPI_ERR_TAG
.N MPI_ERR_COMM
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Start, MPI_Request_free, MPI_Send_init
@*/
EXTERN_C
MPI_METHOD
MPI_Rsend_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Rsend_init(buf,count,datatype,dest,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Rsend_init(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Rsend_init(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_rsend_init %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Rsend_init, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Irsend - Starts a nonblocking ready send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
.N MPI_ERR_EXHAUSTED

@*/
EXTERN_C
MPI_METHOD
MPI_Irsend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Irsend(buf, count, datatype, dest, tag, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Rsend(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    MPIR_SENDQ_REMEMBER(request_ptr,dest,tag,comm_ptr->context_id);

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Irsend(*request);

  fn_exit:
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_irsend %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Irsend, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Ssend - Blocking synchronous send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (nonnegative integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
@*/
EXTERN_C
MPI_METHOD
MPI_Ssend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ssend(buf,count,datatype,dest,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request * request_ptr;
    mpi_errno = MPID_Ssend(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (Mpi.IsMultiThreaded() == false)
    {
        /* In the single threaded case, sending to yourself will cause deadlock. */
        if (!request_ptr->is_internal_complete())
        {
            if(request_ptr->get_msg_type() == MPIDI_REQUEST_SELF_MSG)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**dev|selfsenddeadlock");
                goto fn_fail;
            }
        }
    }


    /* If a request was returned, then we need to block until the request is complete */
    mpi_errno = MPIR_Wait(request_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = request_ptr->status.MPI_ERROR;
    request_ptr->Release();

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Ssend();
  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ssend %p %d %D %i %t %C",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm
            )
        );
    TraceError(MPI_Ssend, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Ssend_init - Creates a persistent request for a synchronous send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements sent (integer)
. datatype - type of each element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
@*/
EXTERN_C
MPI_METHOD
MPI_Ssend_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ssend_init(buf,count,datatype,dest,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Ssend_init(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ssend_init(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ssend_init %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Ssend_init, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Issend - Starts a nonblocking synchronous send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
.N MPI_ERR_EXHAUSTED
@*/
EXTERN_C
MPI_METHOD
MPI_Issend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Issend(buf, count,datatype, dest, tag, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Ssend(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    MPIR_SENDQ_REMEMBER(request_ptr,dest,tag,comm_ptr->context_id);

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Issend(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_issend %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Issend, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Buffer_attach - Attaches a user-provided buffer for sending

Input Parameters:
+ buffer - initial buffer address (choice)
- size - buffer size, in bytes (integer)

Notes:
The size given should be the sum of the sizes of all outstanding Bsends that
you intend to have, plus 'MPI_BSEND_OVERHEAD' for each Bsend that you do.
For the purposes of calculating size, you should use 'MPI_Pack_size'.
In other words, in the code
.vb
     MPI_Buffer_attach( buffer, size );
     MPI_Bsend( ..., count=20, datatype=type1,  ... );
     ...
     MPI_Bsend( ..., count=40, datatype=type2, ... );
.ve
the value of 'size' in the 'MPI_Buffer_attach' call should be greater than
the value computed by
.vb
     MPI_Pack_size( 20, type1, comm, &s1 );
     MPI_Pack_size( 40, type2, comm, &s2 );
     size = s1 + s2 + 2 * MPI_BSEND_OVERHEAD;
.ve
The 'MPI_BSEND_OVERHEAD' gives the maximum amount of space that may be used in
the buffer for use by the BSEND routines in using the buffer.  This value
is in 'mpi.h' (for C) and 'mpif.h' (for Fortran).

.N NotThreadSafe
Because the buffer for buffered sends (e.g., 'MPI_Bsend') is shared by all
threads in a process, the user is responsible for ensuring that only
one thread at a time calls this routine or 'MPI_Buffer_detach'.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_BUFFER
.N MPI_ERR_INTERN

.seealso: MPI_Buffer_detach, MPI_Bsend
@*/
EXTERN_C
MPI_METHOD
MPI_Buffer_attach(
    _In_ void* buffer,
    _In_range_(>=, 0) int size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Buffer_attach(buffer, size);

    int mpi_errno;

    if( buffer == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "buffer" );
        goto fn_fail;
    }

    if( size < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "size", size );
        goto fn_fail;
    }

    mpi_errno = MPIR_Bsend_attach( buffer, size );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Buffer_attach();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_buffer_attach %p %d",
            buffer,
            size
            )
        );
    TraceError(MPI_Buffer_attach, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Buffer_detach - Removes an existing buffer (for use in MPI_Bsend etc)

Output Parameters:
+ buffer - initial buffer address (choice)
- size - buffer size, in bytes (integer)

Notes:
    The reason that 'MPI_Buffer_detach' returns the address and size of the
buffer being detached is to allow nested libraries to replace and restore
the buffer.  For example, consider

.vb
    int size, mysize, idummy;
    void *ptr, *myptr, *dummy;
    MPI_Buffer_detach( &ptr, &size );
    MPI_Buffer_attach( myptr, mysize );
    ...
    ... library code ...
    ...
    MPI_Buffer_detach( &dummy, &idummy );
    MPI_Buffer_attach( ptr, size );
.ve

This is much like the action of the Unix signal routine and has the same
strengths (it is simple) and weaknesses (it only works for nested usages).

Note that for this approach to work, MPI_Buffer_detach must return MPI_SUCCESS
even when there is no buffer to detach.  In that case, it returns a size of
zero.  The MPI 1.1 standard for 'MPI_BUFFER_DETACH' contains the text

.vb
   The statements made in this section describe the behavior of MPI for
   buffered-mode sends. When no buffer is currently associated, MPI behaves
   as if a zero-sized buffer is associated with the process.
.ve

This could be read as applying only to the various Bsend routines.  This
implementation takes the position that this applies to 'MPI_BUFFER_DETACH'
as well.

.N NotThreadSafe
Because the buffer for buffered sends (e.g., 'MPI_Bsend') is shared by all
threads in a process, the user is responsible for ensuring that only
one thread at a time calls this routine or 'MPI_Buffer_attach'.

.N Fortran

    The Fortran binding for this routine is different.  Because Fortran
    does not have pointers, it is impossible to provide a way to use the
    output of this routine to exchange buffers.  In this case, only the
    size field is set.

Notes for C:
    Even though the 'bufferptr' argument is declared as 'void *', it is
    really the address of a void pointer.  See the rationale in the
    standard for more details.

.seealso: MPI_Buffer_attach
@*/
EXTERN_C
MPI_METHOD
MPI_Buffer_detach(
    _Out_ void* buffer_addr,
    _Out_ int* size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Buffer_detach();

    int mpi_errno;

    if( buffer_addr == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "buffer_addr" );
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "size" );
        goto fn_fail;
    }

    mpi_errno = MPIR_Bsend_detach( static_cast<void**>(buffer_addr), size );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Buffer_detach(*(void**)buffer_addr, *size);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_buffer_detach %p %p",
            buffer_addr,
            size
            )
        );
    TraceError(MPI_Buffer_detach, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Bsend - Basic send with user-provided buffering

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (nonnegative integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Notes:
This send is provided as a convenience function; it allows the user to
send messages without worring about where they are buffered (because the
user `must` have provided buffer space with 'MPI_Buffer_attach').

In deciding how much buffer space to allocate, remember that the buffer space
is not available for reuse by subsequent 'MPI_Bsend's unless you are certain
that the message
has been received (not just that it should have been received).  For example,
this code does not allocate enough buffer space
.vb
    MPI_Buffer_attach( b, n*sizeof(double) + MPI_BSEND_OVERHEAD );
    for (i=0; i<m; i++)
    {
        MPI_Bsend( buf, n, MPI_DOUBLE, ... );
    }
.ve
because only enough buffer space is provided for a single send, and the
loop may start a second 'MPI_Bsend' before the first is done making use of the
buffer.

In C, you can
force the messages to be delivered by
.vb
    MPI_Buffer_detach( &b, &n );
    MPI_Buffer_attach( b, n );
.ve
(The 'MPI_Buffer_detach' will not complete until all buffered messages are
delivered.)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_RANK
.N MPI_ERR_TAG

.seealso: MPI_Buffer_attach, MPI_Ibsend, MPI_Bsend_init
@*/
EXTERN_C
MPI_METHOD
MPI_Bsend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Bsend(buf, count, datatype, dest, tag, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPIR_Bsend_isend(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    request_ptr->Release();

    TraceLeave_MPI_Bsend();

  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_bsend %p %d %D %i %t %C",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm
            )
        );
    TraceError(MPI_Bsend, mpi_errno);
    goto fn_exit1;
}


/*@
    MSMPI_Get_bsend_overhead - Returns the size of the overhead required for Bsend buffers
@*/
EXTERN_C
MPI_METHOD
MSMPI_Get_bsend_overhead()
{
    return MPIR_Get_bsend_overhead();
}


/*@
    MPI_Bsend_init - Builds a handle for a buffered send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements sent (integer)
. datatype - type of each element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_RANK
.N MPI_ERR_TAG

.seealso: MPI_Buffer_attach
@*/
EXTERN_C
MPI_METHOD
MPI_Bsend_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Bsend_init(buf, count, datatype, dest, tag, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request* request_ptr;
    mpi_errno = MPID_Bsend_init(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Bsend_init(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_bsend_init %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Bsend_init, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Ibsend - Starts a nonblocking buffered send

Input Parameters:
+ buf - initial address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - datatype of each send buffer element (handle)
. dest - rank of destination (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
.N MPI_ERR_BUFFER

@*/
EXTERN_C
MPI_METHOD
MPI_Ibsend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ibsend(buf,count,datatype,dest,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* We don't try tbsend in for MPI_Ibsend because we must create a
       request even if we can send the message */

    MPID_Request *request_ptr;
    mpi_errno = MPIR_Bsend_isend(
        buf,
        count,
        hType,
        dest,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ibsend(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    *request = MPI_REQUEST_NULL; /* FIXME: should we be setting the request at all in the case of an error? */
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ibsend %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Ibsend, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Sendrecv - Sends and receives a message

Input Parameters:
+ sendbuf - initial address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - type of elements in send buffer (handle)
. dest - rank of destination (integer)
. sendtag - send tag (integer)
. recvcount - number of elements in receive buffer (integer)
. recvtype - type of elements in receive buffer (handle)
. source - rank of source (integer)
. recvtag - receive tag (integer)
- comm - communicator (handle)

Output Parameters:
+ recvbuf - initial address of receive buffer (choice)
- status - status object (Status).  This refers to the receive operation.

.N ThreadSafe

.N Fortran

.N FortranStatus

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
EXTERN_C
MPI_METHOD
MPI_Sendrecv(
    _In_opt_ const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int sendtag,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int recvtag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Sendrecv(sendbuf,sendcount,sendtype,dest,sendtag,recvbuf,recvcount,recvtype,source,recvtag,comm);

    TypeHandle hSendType;
    TypeHandle hRecvType;
    MPID_Request * sreq;
    MPID_Request * rreq;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Validate status (status_ignore is not the same as null) */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( sendtag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( recvtag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        sendbuf,
        sendcount,
        sendtype,
        "sendtype",
        &hSendType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        recvbuf,
        recvcount,
        recvtype,
        "recvtype",
        &hRecvType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( sendcount > 0 && recvcount > 0 )
    {
        mpi_errno = MpiaBufferValidateAliasing(
            sendbuf,
            sendtype,
            recvbuf,
            recvtype,
            sendcount + recvcount
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    mpi_errno = MPID_Recv(
        recvbuf,
        recvcount,
        hRecvType,
        source,
        recvtag,
        comm_ptr,
        &rreq
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Send(
        sendbuf,
        sendcount,
        hSendType,
        dest,
        sendtag,
        comm_ptr,
        &sreq
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        /* FIXME: should we cancel the pending (possibly completed) receive request or wait for it to complete? */
        rreq->Release();
        goto fn_fail;
    }

    mpi_errno = MPIR_Wait(sreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIR_Wait(rreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = rreq->status.MPI_ERROR;
    MPIR_Request_extract_status(rreq, status);
    rreq->Release();

    if (mpi_errno == MPI_SUCCESS)
    {
        mpi_errno = sreq->status.MPI_ERROR;
    }
    sreq->Release();

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Sendrecv();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_sendrecv %p %d %D %i %t %p %d %D %i %t %C %p",
            sendbuf,
            sendcount,
            sendtype,
            dest,
            sendtag,
            recvbuf,
            recvcount,
            recvtype,
            source,
            recvtag,
            comm,
            status
            )
        );
    TraceError(MPI_Sendrecv, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Sendrecv_replace - Sends and receives using a single buffer

Input Parameters:
+ count - number of elements in send and receive buffer (integer)
. datatype - type of elements in send and receive buffer (handle)
. dest - rank of destination (integer)
. sendtag - send message tag (integer)
. source - rank of source (integer)
. recvtag - receive message tag (integer)
- comm - communicator (handle)

Output Parameters:
+ buf - initial address of send and receive buffer (choice)
- status - status object (Status)

.N ThreadSafe

.N Fortran

.N FortranStatus

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
.N MPI_ERR_TRUNCATE
.N MPI_ERR_EXHAUSTED

@*/
EXTERN_C
MPI_METHOD
MPI_Sendrecv_replace(
    _Inout_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_PROC_NULL) int dest,
    _In_range_(>=, 0) int sendtag,
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int recvtag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Sendrecv_replace(buf,count,datatype,dest,sendtag,source,recvtag,comm);

    StackGuardArray<BYTE> tmpbuf;
    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Validate status (status_ignore is not the same as null) */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateSendRank( comm_ptr, dest );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaSendTagValidate( sendtag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( recvtag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request * sreq;
    MPID_Request * rreq;
    int tmpbuf_size = 0;
    int tmpbuf_count = 0;

    if (count > 0 && dest != MPI_PROC_NULL)
    {
        mpi_errno = NMPI_Pack_size(count, datatype, comm, &tmpbuf_size);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        tmpbuf = new BYTE[tmpbuf_size];
        if( tmpbuf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        mpi_errno = NMPI_Pack(
            buf,
            count,
            hType.GetMpiHandle(),
            tmpbuf,
            tmpbuf_size,
            &tmpbuf_count,
            comm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    //
    // WARNING!!! We are receiving a packed buffer, but not unpacking it.
    // This assumes that the packed and unpacked representation is identical.
    // It probably is, for now.
    //
    mpi_errno = MPID_Recv(
        buf,
        count,
        hType,
        source,
        recvtag,
        comm_ptr,
        &rreq
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Send(
        tmpbuf,
        tmpbuf_count,
        g_hBuiltinTypes.MPI_Packed,
        dest,
        sendtag,
        comm_ptr,
        &sreq
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        /* FIXME: should we cancel the pending (possibly completed) receive request or wait for it to complete? */
        rreq->Release();
        goto fn_fail;
    }

    mpi_errno = MPIR_Wait(sreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIR_Wait(rreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = rreq->status.MPI_ERROR;
    MPIR_Request_extract_status(rreq, status);
    rreq->Release();

    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = sreq->status.MPI_ERROR;
    }
    sreq->Release();

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Sendrecv_replace(SENTINEL_SAFE_SIZE(status),status);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno =  MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_sendrecv_replace %p %d %D %i %t %i %t %C %p",
            buf,
            count,
            datatype,
            dest,
            sendtag,
            source,
            recvtag,
            comm,
            status
            )
        );
    TraceError(MPI_Sendrecv_replace, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Recv - Blocking receive for a message

Output Parameters:
+ buf - initial address of receive buffer (choice)
- status - status object (Status)

Input Parameters:
+ count - maximum number of elements in receive buffer (integer)
. datatype - datatype of each receive buffer element (handle)
. source - rank of source (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Notes:
The 'count' argument indicates the maximum length of a message; the actual
length of the message can be determined with 'MPI_Get_count'.

.N ThreadSafe

.N Fortran

.N FortranStatus

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TYPE
.N MPI_ERR_COUNT
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
EXTERN_C
MPI_METHOD
MPI_Recv(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Recv(buf, count, datatype, source, tag, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request * request_ptr;
    mpi_errno = MPID_Recv(
        buf,
        count,
        hType,
        source,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* If a request was returned, then we need to block until the request is complete */
    mpi_errno = MPIR_Wait(request_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = request_ptr->status.MPI_ERROR;
    MPIR_Request_extract_status(request_ptr, status);
    request_ptr->Release();

    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    TraceLeave_MPI_Recv(SENTINEL_SAFE_SIZE(status),status);
  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_recv %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            source,
            tag,
            comm,
            status
            )
        );
    TraceError(MPI_Recv, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Recv_init - Create a persistent request for a receive

Input Parameters:
+ buf - initial address of receive buffer (choice)
. count - number of elements received (integer)
. datatype - type of each element (handle)
. source - rank of source or 'MPI_ANY_SOURCE' (integer)
. tag - message tag or 'MPI_ANY_TAG' (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_RANK
.N MPI_ERR_TAG
.N MPI_ERR_COMM
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Start, MPI_Startall, MPI_Request_free
@*/
EXTERN_C
MPI_METHOD
MPI_Recv_init(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Recv_init(buf,count,datatype,source,tag,comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Recv_init(
        buf,
        count,
        hType,
        source,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Recv_init(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_recv_init %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            source,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Recv_init, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Irecv - Begins a nonblocking receive

Input Parameters:
+ buf - initial address of receive buffer (choice)
. count - number of elements in receive buffer (integer)
. datatype - datatype of each receive buffer element (handle)
. source - rank of source (integer)
. tag - message tag (integer)
- comm - communicator (handle)

Output Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK
.N MPI_ERR_EXHAUSTED
@*/
EXTERN_C
MPI_METHOD
MPI_Irecv(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request *request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Irecv(buf, count, datatype, source, tag, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Recv(
        buf,
        count,
        hType,
        source,
        tag,
        comm_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Irecv(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_irecv %p %d %D %i %t %C %p",
            buf,
            count,
            datatype,
            source,
            tag,
            comm,
            request
            )
        );
    TraceError(MPI_Irecv, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Mrecv - Blocking receive for a message matched by MPI_Mprobe or MPI_Improbe.

Input Parameters:
+ count - maximum number of elements in receive buffer (non-negative integer)
- datatype - datatype of each receive buffer element (handle)
- message - message (handle)

Output Parameter:
+ buffer - initial address of receive buffer (choice)
. status - status object (Status)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
EXTERN_C
MPI_METHOD
MPI_Mrecv(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Inout_ MPI_Message* message,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Mrecv( buf, count, datatype, *message );

    MPI_RESULT mpi_errno;
    TypeHandle hType;

    if( message == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "message" );
        goto fn_fail;
    }

    MPID_Request *msg_ptr;
    mpi_errno = MpiaMessageValidate( *message, &msg_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // NOTE: MPI_STATUS_IGNORE != NULL
    //
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Imrecv(
        buf,
        count,
        hType,
        msg_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // If a request was returned, then we need to block until the request is complete
    //
    mpi_errno = MPIR_Wait( request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    mpi_errno = request_ptr->status.MPI_ERROR;

    //
    // Extract the status from the request.
    // Reset message to MPI_MESSAGE_NULL (Refer to MPI 3.0 Section 3.8.2)
    //
    MPIR_Request_extract_status( request_ptr, status );
    *message = MPI_MESSAGE_NULL;
    request_ptr->Release();

    //
    // If message is MPI_MESSAGE_NO_PROC then we have not incremented
    // the message handle tracker in the mprobe/improbe methods.
    //
    if( msg_ptr != nullptr )
    {
        MPID_Message_track_user_free();
    }

    if ( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Mrecv( SENTINEL_SAFE_SIZE(status), status );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_mrecv %p %d %D %p %p",
            buf,
            count,
            datatype,
            message,
            status
            )
        );
    TraceError( MPI_Mrecv, mpi_errno );
    goto fn_exit;
}


/*@
    MPI_Imrecv - Non-Blocking recv for a message matched by MPI_Mprobe/MPI_Improbe.

Input Parameters:
+ count - maximum number of elements in receive buffer (non-negative integer)
- datatype - datatype of each receive buffer element (handle)
- message - message (handle)

Output Parameter:
+ buffer - initial address of receive buffer (choice)
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
EXTERN_C
MPI_METHOD
MPI_Imrecv(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Inout_ MPI_Message* message,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Imrecv( buf, count, datatype, *message );

    MPI_RESULT mpi_errno = MPI_SUCCESS;
    TypeHandle hType;

    if( message == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "message");
        goto fn_fail;
    }

    MPID_Request *msg_ptr;
    mpi_errno = MpiaMessageValidate( *message, &msg_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Imrecv(
        buf,
        count,
        hType,
        msg_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *request = MPI_REQUEST_NULL;
    if ( request_ptr != nullptr )
    {
        *request = request_ptr->handle;
        MPID_Request_track_user_alloc();
    }

    //
    // Reset message to MPI_MESSAGE_NULL (Refer to MPI 3.0 Section 3.8.2)
    //
    *message = MPI_MESSAGE_NULL;
    if( msg_ptr != nullptr )
    {
        MPID_Message_track_user_free();
    }

    TraceLeave_MPI_Imrecv( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_imrecv %p %d %D %p %p",
            buf,
            count,
            datatype,
            message,
            request
            )
        );
    TraceError( MPI_Imrecv, mpi_errno );
    goto fn_exit;
}
