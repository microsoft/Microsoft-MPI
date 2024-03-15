// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
   MPI_Put - Put data into a memory window on a remote process

   Input Parameters:
+ origin_addr -initial address of origin buffer (choice)
. origin_count -number of entries in origin buffer (nonnegative integer)
. origin_datatype -datatype of each entry in origin buffer (handle)
. target_rank -rank of target (nonnegative integer)
. target_disp -displacement from start of window to target buffer (nonnegative integer)
. target_count -number of entries in target buffer (nonnegative integer)
. target_datatype -datatype of each entry in target buffer (handle)

- win - window object used for communication (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Put(
    _In_opt_ const void* origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_range_(>=, MPI_PROC_NULL) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Put(origin_addr,origin_count,origin_datatype,target_rank,target_disp,target_count,target_datatype,win);

    TypeHandle hOriginType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        origin_addr,
        origin_count,
        origin_datatype,
        "origin_datatype",
        &hOriginType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_disp < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp" );
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, target_rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_rank == MPI_PROC_NULL )
    {
        goto fn_exit;
    }

    mpi_errno = MPID_Win_Put(
        origin_addr,
        origin_count,
        hOriginType,
        target_rank,
        target_disp,
        target_count,
        hTargetType,
        win_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

  fn_exit:
    TraceLeave_MPI_Put();
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_put %p %d %D %d %d %d %D %W",
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            win
            )
        );
    TraceError(MPI_Put, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Rput - Put data into a memory window on a remote process

   MPI_Rput is similar to MPI_Put, except that it allocates a communication
   request object and associates it with the request handle.

   Input Parameters:
+ origin_addr -initial address of origin buffer (choice)
. origin_count -number of entries in origin buffer (nonnegative integer)
. origin_datatype -datatype of each entry in origin buffer (handle)
. target_rank -rank of target (nonnegative integer)
. target_disp -displacement from start of window to target buffer (nonnegative integer)
. target_count -number of entries in target buffer (nonnegative integer)
. target_datatype -datatype of each entry in target buffer (handle)
- win - window object used for communication (handle)

   Output Parameters:
. request - RMA request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Rput(
    _In_opt_ const void* origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_range_(>=, MPI_PROC_NULL) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Win win,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Rput(origin_addr,origin_count,origin_datatype,target_rank,target_disp,target_count,target_datatype,win);

    TypeHandle hOriginType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (request == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "request");
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        origin_addr,
        origin_count,
        origin_datatype,
        "origin_datatype",
        &hOriginType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_disp < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp" );
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, target_rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Win_Rput(
        origin_addr,
        origin_count,
        hOriginType,
        target_rank,
        target_disp,
        target_count,
        hTargetType,
        win_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();
    TraceLeave_MPI_Rput(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_rput %p %d %D %d %d %d %D %W %p",
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            win,
            request
            )
        );
    TraceError(MPI_Rput, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Get - Get data from a memory window on a remote process

 Input Parameters:
+ origin_addr - Address of the buffer in which to receive the data
. origin_count - number of entries in origin buffer (nonnegative integer)
. origin_datatype - datatype of each entry in origin buffer (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from window start to the beginning of the
  target buffer (nonnegative integer)
. target_count - number of entries in target buffer (nonnegative integer)
. target_datatype - datatype of each entry in target buffer (handle)
- win - window object used for communication (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Get(
    _In_opt_ void* origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_range_(>=, MPI_PROC_NULL) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Get(origin_addr,origin_count,origin_datatype,target_rank,target_disp,target_count,target_datatype,win);

    TypeHandle hOriginType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        origin_addr,
        origin_count,
        origin_datatype,
        "origin_datatype",
        &hOriginType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_disp < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp" );
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, target_rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_rank == MPI_PROC_NULL )
    {
        goto fn_exit;
    }

    mpi_errno = MPID_Win_Get(
        origin_addr,
        origin_count,
        hOriginType,
        target_rank,
        target_disp,
        target_count,
        hTargetType,
        win_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* ... end of body of routine ... */
  fn_exit:
    TraceLeave_MPI_Get();
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno, "**mpi_get %p %d %D %d %d %d %D %W",
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            win
            )
        );
    TraceError(MPI_Get, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Rget - Get data from a memory window on a remote process

   MPI_Rget is similar to MPI_Get, except that it allocates a communication
   request object and associates it with the request handle (the argument
   request) that can be used to wait or test for completion.

 Input Parameters:
+ origin_addr - Address of the buffer in which to receive the data
. origin_count - number of entries in origin buffer (nonnegative integer)
. origin_datatype - datatype of each entry in origin buffer (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from window start to the beginning of the
  target buffer (nonnegative integer)
. target_count - number of entries in target buffer (nonnegative integer)
. target_datatype - datatype of each entry in target buffer (handle)
- win - window object used for communication (handle)

 Output Parameters:
. request - RMA request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Rget(
    _In_opt_ void* origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_range_(>=, MPI_PROC_NULL) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Win win,
    _Inout_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Rget(origin_addr,origin_count,origin_datatype,target_rank,target_disp,target_count,target_datatype,win);

    TypeHandle hOriginType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (request == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "request");
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        origin_addr,
        origin_count,
        origin_datatype,
        "origin_datatype",
        &hOriginType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_disp < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp" );
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, target_rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Win_Rget(
        origin_addr,
        origin_count,
        hOriginType,
        target_rank,
        target_disp,
        target_count,
        hTargetType,
        win_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();
    TraceLeave_MPI_Rget(*request);

    /* ... end of body of routine ... */
  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno, "**mpi_rget %p %d %D %d %d %d %D %W %p",
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            win,
            request
            )
        );
    TraceError(MPI_Rget, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Accumulate - Accumulate data into the target process using remote
   memory access

   Input Parameters:
+ origin_addr - initial address of buffer (choice)
. origin_count - number of entries in buffer (nonnegative integer)
. origin_datatype - datatype of each buffer entry (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from start of window to beginning of target
  buffer (nonnegative integer)
. target_count - number of entries in target buffer (nonnegative integer)
. target_datatype - datatype of each entry in target buffer (handle)
. op - predefined reduce operation (handle)
- win - window object (handle)

   Notes:
The basic components of both the origin and target datatype must be the same
predefined datatype (e.g., all 'MPI_INT' or all 'MPI_DOUBLE_PRECISION').

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Accumulate(
    _In_opt_ const void* origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_range_(>=, MPI_PROC_NULL) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Op op,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Accumulate(origin_addr,origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, op, win);

    TypeHandle hOriginType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        origin_addr,
        origin_count,
        origin_datatype,
        "origin_datatype",
        &hOriginType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate(
        op,
        hOriginType,
        true,
        &op_ptr
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    if (op_ptr->kind == MPID_OP_NOOP)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**noopnotallowed");
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_disp < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp" );
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, target_rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( target_rank == MPI_PROC_NULL )
    {
        goto fn_exit;
    }

    mpi_errno = MPID_Win_Accumulate(
        origin_addr,
        origin_count,
        hOriginType,
        target_rank,
        target_disp,
        target_count,
        hTargetType,
        op,
        win_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* ... end of body of routine ... */
  fn_exit:
    TraceLeave_MPI_Accumulate();
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_accumulate %p %d %D %d %d %d %D %O %W",
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            op,
            win
            )
        );
    TraceError(MPI_Accumulate, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Raccumulate - Accumulate data into the target process using remote
   memory access

   MPI_Raccumulate is similar to MPI_Accumulate, except that it allocates
   a communication request object and associates it with the request handle.

   Input Parameters:
+ origin_addr - initial address of buffer (choice)
. origin_count - number of entries in buffer (nonnegative integer)
. origin_datatype - datatype of each buffer entry (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from start of window to beginning of target
  buffer (nonnegative integer)
. target_count - number of entries in target buffer (nonnegative integer)
. target_datatype - datatype of each entry in target buffer (handle)
. op - predefined reduce operation (handle)
- win - window object (handle)

 Output Parameters:
. request - RMA request (handle)

   Notes:
The basic components of both the origin and target datatype must be the same
predefined datatype (e.g., all 'MPI_INT' or all 'MPI_DOUBLE_PRECISION').

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Raccumulate(
    _In_opt_ const void* origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_range_(>=, MPI_PROC_NULL) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Op op,
    _In_ MPI_Win win,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Raccumulate(origin_addr,origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, op, win);

    TypeHandle hOriginType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (request == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "request");
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        origin_addr,
        origin_count,
        origin_datatype,
        "origin_datatype",
        &hOriginType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate(
        op,
        hOriginType,
        true,
        &op_ptr
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    if (op_ptr->kind == MPID_OP_NOOP)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**noopnotallowed");
        goto fn_fail;
    }

    if( target_disp < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp" );
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, target_rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *request_ptr;
    mpi_errno = MPID_Win_Raccumulate(
        origin_addr,
        origin_count,
        hOriginType,
        target_rank,
        target_disp,
        target_count,
        hTargetType,
        op,
        win_ptr,
        &request_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* return the handle of the request to the user */
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();
    TraceLeave_MPI_Raccumulate(*request);

    /* ... end of body of routine ... */
  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_raccumulate %p %d %D %d %d %d %D %O %W %p",
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            op,
            win,
            request
            )
        );
    TraceError(MPI_Raccumulate, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Get_accumulate - performs an atomic, one-sided read-and-accumulate operation.

Input Parameters:
+ origin_addr - initial address of buffer (choice)
. origin_count - number of entries in buffer (nonnegative integer)
. origin_datatype - datatype of each buffer entry (handle)
. result_addr - initial address of result buffer (choice)
. result_count - number of entries in result buffer (non-negative integer)
. result_datatype - datatype of each entry in result buffer (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from start of window to beginning of target
buffer (nonnegative integer)
. target_count - number of entries in target buffer (nonnegative integer)
. target_datatype - datatype of each entry in target buffer (handle)
. op - predefined reduce operation (handle)
- win - window object (handle)

Notes:
This operations is atomic with respect to other "accumulate" operations.

The get and accumulate steps are executed atomically for each basic element
in the datatype (see MPI 3.0 Section 11.7 for details). The predefined
operation MPI_REPLACE provides fetch-and-set behavior.

The origin and result buffers (origin_addr and result_addr) must be disjoint.
Each datatype argument must be a predefined datatype or a derived datatype
where all basic components are of the same predefined datatype. All datatype
arguments must be constructed from the same predefined datatype. The operation
op applies to elements of that predefined type. target_datatype must not specify
overlapping entries, and the target buffer must fit in the target window or in
attached memory in a dynamic window.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Get_accumulate(
    _In_opt_ const void* origin_addr,
    _In_range_(>= , 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_opt_ void* result_addr,
    _In_range_(>= , 0) int result_count,
    _In_ MPI_Datatype result_datatype,
    _In_range_(>= , MPI_PROC_NULL) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_range_(>= , 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Op op,
    _In_ MPI_Win win
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Get_accumulate(
        origin_addr,
        origin_count,
        origin_datatype,
        result_addr,
        result_count,
        result_datatype,
        target_rank,
        target_disp,
        target_count,
        target_datatype,
        op,
        win);

    TypeHandle hOriginType;
    TypeHandle hResultType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (op != MPI_NO_OP)
    {
        mpi_errno = MpiaDatatypeValidate(
            origin_addr,
            origin_count,
            origin_datatype,
            "origin_datatype",
            &hOriginType
        );
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
    }

    mpi_errno = MpiaDatatypeValidate(
        result_addr,
        result_count,
        result_datatype,
        "result_datatype",
        &hResultType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (target_disp < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp");
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);
    mpi_errno = MpiaCommValidateSendRank(win_ptr->comm_ptr, target_rank);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (target_rank == MPI_PROC_NULL)
    {
        goto fn_exit;
    }

    if (op == MPI_NO_OP)
    {
        mpi_errno = MPID_Win_Get(
            result_addr,
            result_count,
            hResultType,
            target_rank,
            target_disp,
            target_count,
            hTargetType,
            win_ptr
        );
    }
    else
    {
        mpi_errno = MPID_Win_Get_accumulate(
            origin_addr,
            origin_count,
            hOriginType,
            result_addr,
            result_count,
            hResultType,
            target_rank,
            target_disp,
            target_count,
            hTargetType,
            op,
            win_ptr
        );
    }
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    /* ... end of body of routine ... */
fn_exit:
    TraceLeave_MPI_Get_accumulate();
fn_exit1:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_get_accumulate %p %d %D %p %d %D %d %d %d %D %O %W",
            origin_addr,
            origin_count,
            origin_datatype,
            result_addr,
            result_count,
            result_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            op,
            win
        )
    );
    TraceError(MPI_Get_accumulate, mpi_errno);
    goto fn_exit1;
}


/*@
MPI_Rget_accumulate - Perform an atomic, one-sided read-and-accumulate operation
  and return a request handle for the operation.

Input Parameters:
+ origin_addr - initial address of buffer (choice)
. origin_count - number of entries in buffer (nonnegative integer)
. origin_datatype - datatype of each buffer entry (handle)
. result_addr - initial address of result buffer (choice)
. result_count - number of entries in result buffer (non-negative integer)
. result_datatype - datatype of each entry in result buffer (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from start of window to beginning of target
buffer (nonnegative integer)
. target_count - number of entries in target buffer (nonnegative integer)
. target_datatype - datatype of each entry in target buffer (handle)
. op - predefined reduce operation (handle)
- win - window object (handle)

Output Parameters:
. request - RMA request (handle)

Notes:
MPI_Rget_accumulate is similar to MPI_Get_accumulate, except that it 
allocates a communication request object and associates it with the
request handle (the argument request) that can be used to wait or test
for completion. The completion of an MPI_Rget_accumulate operation
indicates that the data is available in the result buffer and the origin
buffer is free to be updated. It does not indicate that the operation
has been completed at the target window. 

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Rget_accumulate(
    _In_opt_ const void* origin_addr,
    _In_range_(>= , 0) int origin_count,
    _In_ MPI_Datatype origin_datatype,
    _In_opt_ void* result_addr,
    _In_range_(>= , 0) int result_count,
    _In_ MPI_Datatype result_datatype,
    _In_range_(>= , MPI_PROC_NULL) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_range_(>= , 0) int target_count,
    _In_ MPI_Datatype target_datatype,
    _In_ MPI_Op op,
    _In_ MPI_Win win,
    _Out_ MPI_Request* request
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Rget_accumulate(
        origin_addr,
        origin_count,
        origin_datatype,
        result_addr,
        result_count,
        result_datatype,
        target_rank,
        target_disp,
        target_count,
        target_datatype,
        op,
        win);

    TypeHandle hOriginType;
    TypeHandle hResultType;
    TypeHandle hTargetType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (request == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "request");
        goto fn_fail;
    }

    if (op != MPI_NO_OP)
    {
        mpi_errno = MpiaDatatypeValidate(
            origin_addr,
            origin_count,
            origin_datatype,
            "origin_datatype",
            &hOriginType
        );
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
    }

    mpi_errno = MpiaDatatypeValidate(
        result_addr,
        result_count,
        result_datatype,
        "result_datatype",
        &hResultType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    //
    // We don't know the actual address of the destination buffer.
    // use MPI_IN_PLACE so that checks for null/MPI_BOTTOM don't get tripped up.
    //
    mpi_errno = MpiaDatatypeValidate(
        MPI_IN_PLACE,
        target_count,
        target_datatype,
        "target_datatype",
        &hTargetType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (target_disp < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp");
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);
    mpi_errno = MpiaCommValidateSendRank(win_ptr->comm_ptr, target_rank);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (target_rank == MPI_PROC_NULL)
    {
        goto fn_exit;
    }

    MPID_Request *request_ptr;
    if (op == MPI_NO_OP)
    {
        mpi_errno = MPID_Win_Rget(
            result_addr,
            result_count,
            hResultType,
            target_rank,
            target_disp,
            target_count,
            hTargetType,
            win_ptr,
            &request_ptr
        );
    }
    else
    {
        mpi_errno = MPID_Win_Rget_accumulate(
            origin_addr,
            origin_count,
            hOriginType,
            result_addr,
            result_count,
            hResultType,
            target_rank,
            target_disp,
            target_count,
            hTargetType,
            op,
            win_ptr,
            &request_ptr
        );
    }
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    //
    // return the handle of the request to the user
    //
    *request = request_ptr->handle;
    MPID_Request_track_user_alloc();
    TraceLeave_MPI_Rget_accumulate(*request);

    //
    // ... end of body of routine ...
    //
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_rget_accumulate %p %d %D %p %d %D %d %d %d %D %O %W %p",
            origin_addr,
            origin_count,
            origin_datatype,
            result_addr,
            result_count,
            result_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            op,
            win,
            request
        )
    );
    TraceError(MPI_Get_accumulate, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Compare_and_swap - Perform one-sided atomic compare-and-swap.

Input Parameters:
+ origin_addr - initial address of buffer (choice)
. compare_addr - initial address of compare buffer (choice)
. result_addr - initial address of result buffer (choice)
. datatype - datatype of the entry in origin, result, and target buffers (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from start of window to beginning of target
buffer (nonnegative integer)
- win - window object (handle)

Notes:
This function compares one element of type datatype in the compare buffer
compare_addr with the buffer at offset target_disp in the target window
specified by target_rank and win and replaces the value at the target with
the value in the origin buffer origin_addr if the compare buffer and the
target buffer are identical. The original value at the target is returned
in the buffer result_addr.

This operation is atomic with respect to other "accumulate" operations.
The parameter datatype must belong to one of the following categories of
predefined datatypes: C integer, Fortran integer, Logical, Multi-language types,
or Byte as specified in Section 5.9.2 on page 176. The origin and result buffers
(origin_addr and result_addr) must be disjoint.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Compare_and_swap(
    _When_(target_rank != MPI_PROC_NULL, _In_)
    _When_(target_rank == MPI_PROC_NULL, _In_opt_)
        const void* origin_addr,
    _When_(target_rank != MPI_PROC_NULL, _In_)
    _When_(target_rank == MPI_PROC_NULL, _In_opt_)
        const void* compare_addr,
    _When_(target_rank != MPI_PROC_NULL, _In_)
    _When_(target_rank == MPI_PROC_NULL, _In_opt_)
        void* result_addr,
    _In_ MPI_Datatype datatype,
    _In_range_(>= , MPI_PROC_NULL) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Compare_and_swap(
        origin_addr,
        compare_addr,
        result_addr,
        datatype,
        target_rank,
        target_disp,
        win);

    TypeHandle hType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (!MPID_Datatype_is_integer(datatype) &&
        !MPID_Datatype_is_logical(datatype) &&
        !MPID_Datatype_is_multilanguage(datatype) &&
        datatype != MPI_BYTE)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_TYPE, "**rmatype");
        goto fn_fail;
    }

    if (target_disp < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp");
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);
    mpi_errno = MpiaCommValidateSendRank(win_ptr->comm_ptr, target_rank);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (target_rank == MPI_PROC_NULL)
    {
        goto fn_exit;
    }

    mpi_errno = MpiaDatatypeValidate(
        origin_addr,
        1,
        datatype,
        "datatype",
        &hType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        compare_addr,
        1,
        datatype,
        "datatype",
        &hType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        result_addr,
        1,
        datatype,
        "datatype",
        &hType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_Compare_and_swap(
        origin_addr,
        compare_addr,
        result_addr,
        hType,
        target_rank,
        target_disp,
        win_ptr
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    /* ... end of body of routine ... */
fn_exit:
    TraceLeave_MPI_Compare_and_swap();
fn_exit1:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_compare_and_swap %p %p %p %D %d %d %W",
            origin_addr,
            compare_addr,
            result_addr,
            datatype,
            target_rank,
            target_disp,
            win
        )
    );
    TraceError(MPI_Compare_and_swap, mpi_errno);
    goto fn_exit1;
}





/*@
MPI_Fetch_and_op - Perform one-sided read-modify-write.

Input Parameters:
+ origin_addr - initial address of buffer (choice)
. result_addr - initial address of result buffer (choice)
. datatype - datatype of the entry in origin, result, and target buffers (handle)
. target_rank - rank of target (nonnegative integer)
. target_disp - displacement from start of window to beginning of target
buffer (nonnegative integer)
. op - predefined reduce operation (handle)
- win - window object (handle)

Notes:
This operations is atomic with respect to other "accumulate" operations.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_RANK
.N MPI_ERR_TYPE
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Fetch_and_op(
    _In_opt_ const void* origin_addr,
    _When_ (target_rank != MPI_PROC_NULL, _In_)
    _When_ (target_rank == MPI_PROC_NULL, _In_opt_)
        void* result_addr,
    _In_ MPI_Datatype datatype,
    _In_range_(>= , MPI_PROC_NULL) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_ MPI_Op op,
    _In_ MPI_Win win
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Fetch_and_op(origin_addr, result_addr, datatype, target_rank, target_disp, op, win);

    TypeHandle hType;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        result_addr,
        1,
        datatype,
        "datatype",
        &hType
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate(
        op,
        hType,
        true,
        &op_ptr
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (target_disp < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DISP, "**rmadisp");
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);
    mpi_errno = MpiaCommValidateSendRank(win_ptr->comm_ptr, target_rank);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (target_rank == MPI_PROC_NULL)
    {
        goto fn_exit;
    }

    if (op == MPI_NO_OP)
    {
        mpi_errno = MPID_Win_Get(
            result_addr,
            1,
            hType,
            target_rank,
            target_disp,
            1,
            hType,
            win_ptr
        );
    }
    else
    {
        mpi_errno = MPID_Win_Get_accumulate(
            origin_addr,
            1,
            hType,
            result_addr,
            1,
            hType,
            target_rank,
            target_disp,
            1,
            hType,
            op,
            win_ptr
        );
    }
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    /* ... end of body of routine ... */
fn_exit:
    TraceLeave_MPI_Fetch_and_op();
fn_exit1:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_fetch_and_op %p %p %D %d %d %O %W",
            origin_addr,
            result_addr,
            datatype,
            target_rank,
            target_disp,
            op,
            win
        )
    );
    TraceError(MPI_Fetch_and_op, mpi_errno);
    goto fn_exit1;
}
