// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
    MPI_Probe - Blocking test for a message

Input Parameters:
+ source - source rank, or 'MPI_ANY_SOURCE' (integer)
. tag - tag value or 'MPI_ANY_TAG' (integer)
- comm - communicator (handle)

Output Parameter:
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
MPI_Probe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Probe(source, tag, comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( tag );
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

    mpi_errno = MPID_Probe(source, tag, comm_ptr, status);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Probe(SENTINEL_SAFE_SIZE(status),status);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_probe %i %t %C %p",
            source,
            tag,
            comm,
            status
            )
        );
    TraceError(MPI_Probe,  mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Iprobe - Nonblocking test for a message

Input Parameters:
+ source - source rank, or  'MPI_ANY_SOURCE' (integer)
. tag - tag value or  'MPI_ANY_TAG' (integer)
- comm - communicator (handle)

Output Parameters:
+ flag - True if a message with the specified source, tag, and communicator
    is available (logical)
- status - status object (Status)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
EXTERN_C
_Success_(return == MPI_SUCCESS && *flag != 0)
int
MPIAPI
MPI_Iprobe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPI_Comm comm,
    _Out_ _Deref_out_range_(0, 1) int* flag,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iprobe(source, tag, comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
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

    mpi_errno = MPID_Iprobe(source, tag, comm_ptr, flag, status);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Iprobe( *flag, SENTINEL_SAFE_SIZE(status), status );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iprobe %i %t %C %p %p",
            source,
            tag,
            comm,
            flag,
            status
            )
        );
    TraceError(MPI_Iprobe, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Improbe - Non-blocking probe for a message.  Provides a mechanism to receive the
    specific message that was matched regardless of intervening probe/recv operations.
    The matched message is dequeued off the receive queue, giving the application an
    opportunity to decide how to receive the message based on the info returned by the
    improbe operation.  The matched message is then received using mrecv/imrecv methods.

Input Parameters:
+ source - source rank, or  'MPI_ANY_SOURCE' (integer)
- tag - tag value or  'MPI_ANY_TAG' (integer)
- comm - communicator (handle)

Output Parameters:
+ flag - True if a message with the specified source, tag, and communicator
    is matched (logical)
- message - handle to the matched message (Message)
- status - status object (Status)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
EXTERN_C
_Success_(return == MPI_SUCCESS && *flag != 0)
int
MPIAPI
MPI_Improbe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPI_Comm comm,
    _Out_ _Deref_out_range_(0, 1) int* flag,
    _Out_ MPI_Message* message,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Improbe( source, tag, comm );

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( message == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "message" );
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

    if( source == MPI_PROC_NULL )
    {
        MPIR_Status_set_procnull( status );
        *message = MPI_MESSAGE_NO_PROC;
        //
        // We set the flag to true because an MPI_Mrecv/MPI_Imrecv with this rank will
        // return immediately.
        //
        *flag = TRUE;
        return MPI_SUCCESS;
    }

    MPID_Request *msg_ptr;
    mpi_errno = MPID_Improbe( source, tag, comm_ptr, flag, &msg_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( *flag != 0 && msg_ptr != nullptr)
    {
        MPIR_Request_extract_status( msg_ptr, status );
        *message = HANDLE_SET_MPI_KIND( msg_ptr->handle, MPID_MESSAGE );
        MPID_Message_track_user_alloc();
    }

    TraceLeave_MPI_Improbe( *flag, *message, SENTINEL_SAFE_SIZE(status), status );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_improbe %i %t %C %p %p %p",
            source,
            tag,
            comm,
            flag,
            message,
            status
            )
        );
    TraceError( MPI_Improbe, mpi_errno );
    goto fn_exit;
}


/*@
    MPI_Mprobe - Blocking probe for a message.  Provides a mechanism to receive the
    specific message that was matched regardless of intervening probe/recv operations.
    The matched message is dequeued off the receive queue, giving the application an
    opportunity to decide how to receive the message based on the info returned by the
    mprobe operation.  The matched message is then received using mrecv/imrecv methods.

Input Parameters:
+ source - source rank, or 'MPI_ANY_SOURCE' (integer)
- tag - tag value or 'MPI_ANY_TAG' (integer)
- comm - communicator (handle)

Output Parameter:
+ message - matched message (handle)
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
MPI_Mprobe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPI_Comm comm,
    _Out_ MPI_Message* message,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Mprobe( source, tag, comm );

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaRecvTagValidate( tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( message == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "message" );
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

    mpi_errno = MpiaCommValidateRecvRank( comm_ptr, source );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( source == MPI_PROC_NULL )
    {
        MPIR_Status_set_procnull( status );
        *message = MPI_MESSAGE_NO_PROC;
        return MPI_SUCCESS;
    }

    MPID_Request *msg_ptr;
    mpi_errno = MPID_Mprobe( source, tag, comm_ptr, &msg_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIR_Request_extract_status( msg_ptr, status );
    *message = HANDLE_SET_MPI_KIND( msg_ptr->handle, MPID_MESSAGE );
    MPID_Message_track_user_alloc();

    TraceLeave_MPI_Mprobe( *message, SENTINEL_SAFE_SIZE(status), status );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_mprobe %i %t %C %p %p",
            source,
            tag,
            comm,
            message,
            status
            )
        );
    TraceError( MPI_Mprobe,  mpi_errno );
    goto fn_exit;
}
