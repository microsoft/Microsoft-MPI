// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
   MPI_Win_complete - Completes an RMA operations begun after an MPI_Win_start.

   Input Parameter:
. win - window object (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_complete(
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_complete(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    mpi_errno = MPID_Win_complete(win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_complete();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_complete %W",
            win
            )
        );
    TraceError(MPI_Win_complete, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_create - Create an MPI Window object for one-sided communication

   Input Parameters:
+ base - initial address of window (choice)
. size - size of window in bytes (nonnegative integer)
. disp_unit - local unit size for displacements, in bytes (positive integer)
. info - info argument (handle)
- comm - communicator (handle)

  Output Parameter:
. win - window object returned by the call (handle)

.N ThreadSafe
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_create(
    _In_ void* base,
    _In_range_(>=, 0)  MPI_Aint size,
    _In_range_(>, 0)  int disp_unit,
    _In_ MPI_Info info,
    _In_ MPI_Comm comm,
    _Out_ MPI_Win* win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_create(base,size,disp_unit,info,comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm(comm, &comm_ptr);
    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    const MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull(info, const_cast<MPID_Info**>(&info_ptr));
    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if(size < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_SIZE, "**rmasize %d", size);
        goto fn_fail;
    }

    if(disp_unit <= 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s",  "disp_unit must be positive");
        goto fn_fail;
    }

    if(win == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "win");
        goto fn_fail;
    }

    MPID_Win *win_ptr;

    mpi_errno = MPID_Win_create(base, size, disp_unit, info_ptr, comm_ptr, false, false, &win_ptr);
    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    win_ptr->createFlavor = MPI_WIN_FLAVOR_CREATE;

    /* return the handle of the window object to the user */
    *win = win_ptr->handle;

    TraceLeave_MPI_Win_create(*win);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_create %p %d %d %I %C %p",
            base,
            size,
            disp_unit,
            info,
            comm,
            win
            )
        );
    TraceError(MPI_Win_create, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Win_create_dynamic - Create an MPI Window object for one-sided communication. This window allows
memory to be dynamically exposed and un-exposed for RMA operations.

Input Parameters:
+ info - info argument (handle)
- comm - communicator (handle)

Output Parameter:
. win - window object returned by the call (handle)

.N ThreadSafe
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_create_dynamic(
    _In_ MPI_Info info,
    _In_ MPI_Comm comm,
    _Out_ MPI_Win* win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_create_dynamic(info, comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm(comm, &comm_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    const MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull(info, const_cast<MPID_Info**>(&info_ptr));
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (win == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "win");
        goto fn_fail;
    }

    MPID_Win *win_ptr;

    mpi_errno = MPID_Win_create(MPI_BOTTOM, 0, 1, info_ptr, comm_ptr, false, false, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    win_ptr->createFlavor = MPI_WIN_FLAVOR_DYNAMIC;

    /* return the handle of the window object to the user */
    *win = win_ptr->handle;

    TraceLeave_MPI_Win_create_dynamic(*win);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
        mpi_errno,
        "**mpi_win_create_dynamic %I %C %p",
        info,
        comm,
        win
        )
        );
    TraceError(MPI_Win_create_dynamic, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Win_allocate - allocates memory and creates a MPI Window object that can be
used by all processes in comm to perform RMA operations

Input Parameters:
. size - size of window in bytes (nonnegative integer)
. disp_unit - local unit size for displacements, in bytes (positive integer)
. info - info argument (handle)
- comm - communicator (handle)

Output Parameters:
. baseptr - base address of the window in local memory
. win - window object returned by the call (handle)

Info keys:
    same_size: if set to true, the implementation may assume that the argument size
    is identical on all processes.

.N ThreadSafe
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_allocate(
    _In_range_(>= , 0) MPI_Aint size,
    _In_range_(>, 0) int disp_unit,
    _In_ MPI_Info info,
    _In_ MPI_Comm comm,
    _Out_ void *baseptr,
    _Out_ MPI_Win *win
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_allocate( size, disp_unit, info, comm );

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm(comm, &comm_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull(info, &info_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (size < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_SIZE, "**rmasize %d", size);
        goto fn_fail;
    }

    if (disp_unit <= 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "disp_unit must be positive");
        goto fn_fail;
    }

    if (win == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "win");
        goto fn_fail;
    }

    MPID_Win *win_ptr;

    mpi_errno = MPID_Win_allocate(size, disp_unit, info_ptr, comm_ptr, baseptr, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    win_ptr->createFlavor = MPI_WIN_FLAVOR_ALLOCATE;

    /* return the handle of the window object to the user */
    *win = win_ptr->handle;

    TraceLeave_MPI_Win_allocate( baseptr, *win );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_allocate %d %d %I %C %p %p",
            size,
            disp_unit,
            info,
            comm,
            baseptr,
            win
            )
        );
    TraceError( MPI_Win_allocate, mpi_errno );
    goto fn_exit;
}


/*@
   MPI_Win_allocate_shared - Create and allocate an MPI Window object for one-sided
   communication.

   Input Parameters:
. size - size of window in bytes (nonnegative integer)
. disp_unit - local unit size for displacements, in bytes (positive integer)
. info - info argument (handle)
- comm - communicator (handle)

   Output Parameters:
. baseptr - base address of the window in local memory
. win - window object returned by the call (handle)

   Info keys:
        same_size: if set to true, the implementation may assume that the argument size
        is identical on all processes.

        alloc_shared_noncontig: The allocated memory is contiguous across process ranks
        unless the info key alloc_shared_noncontig is specified.

It is the user's responsibility to ensure that the communicator comm represents a group
of processes that can create a shared memory segment that can be accessed by all
processes in the group

.N ThreadSafe
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_allocate_shared(
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int disp_unit,
    _In_ MPI_Info info,
    _In_ MPI_Comm comm,
    _Out_ void *baseptr,
    _Out_ MPI_Win *win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_allocate_shared( size, disp_unit, info, comm );

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull( info, const_cast<MPID_Info**>(&info_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if ( size < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_SIZE, "**rmasize %d", size );
        goto fn_fail;
    }

    if ( disp_unit <= 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_DISP, "**arg %s",  "disp_unit must be positive" );
        goto fn_fail;
    }

    if( baseptr == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BASE, "**nullptr %s", "baseptr" );
        goto fn_fail;
    }

    if( win == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_WIN, "**nullptr %s", "Window" );
        goto fn_fail;
    }

    MPID_Win *win_ptr;
    mpi_errno = MPID_Win_allocate_shared(
        size,
        disp_unit,
        info_ptr,
        comm_ptr,
        static_cast<void **>( baseptr ),
        &win_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    win_ptr->createFlavor = MPI_WIN_FLAVOR_SHARED;
    *win = win_ptr->handle;

    TraceLeave_MPI_Win_allocate_shared( baseptr, *win );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_allocate_shared %d %d %I %C %p %p",
            baseptr,
            size,
            disp_unit,
            info,
            comm,
            win
            )
        );
    TraceError( MPI_Win_allocate_shared, mpi_errno );
    goto fn_exit;
}


/*@
   MPI_Win_shared_query - Query the size and base pointer for a patch of a shared memory
   window.
   This function queries the process-local address for remote memory segments created
   with MPI_Win_allocate_shared.

   Input Parameters:
. win - window object used for communication (handle)
. rank - rank in the group of window win (non-negative integer) or MPI_PROC_NULL

   Output Parameters
. size - size of the segment at the given rank
. disp_unit - local unit size for displacements, in bytes (positive integer)
. baseptr - base pointer in the calling process' address space of the shared segment
   belonging to the target rank.

   When rank is MPI_PROC_NULL, the pointer, disp_unit, and size returned are the
   pointer, disp_unit, and size of the memory segment belonging the lowest rank
   that specified size > 0. If all processes in the group attached to the window
   specified size = 0, then the call returns size = 0 and a baseptr as if
   MPI_ALLOC_MEM was called with size = 0.
@*/
EXTERN_C
MPI_METHOD
MPI_Win_shared_query(
    _In_ MPI_Win win,
    _In_range_(>=, MPI_PROC_NULL) int rank,
    _Out_ MPI_Aint *size,
    _Out_ int *disp_unit,
    _Out_ void *baseptr
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_shared_query( win, rank );

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( !win_ptr->dev.isShm )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_WIN, "**winnotshm" );
        goto fn_fail;
    }

    if ( rank < MPI_PROC_NULL || rank >= win_ptr->dev.shmInfo.m_processCount )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_RANK,
            "**rank %d %d",
            rank,
            win_ptr->dev.shmInfo.m_processCount
            );
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_SIZE, "**nullptrtype %s", "size" );
        goto fn_fail;
    }

    if( disp_unit == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_DISP, "**nullptrtype %s", "disp_unit" );
        goto fn_fail;
    }

    if( baseptr == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BASE, "**nullptrtype %s", "baseptr" );
        goto fn_fail;
    }

    mpi_errno = MPID_Win_shared_query(
        win_ptr,
        rank,
        size,
        disp_unit,
        static_cast<void **>(baseptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_shared_query( *size, *disp_unit, baseptr );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_shared_query %p %d %d %d %p",
            win,
            rank,
            *size,
            *disp_unit,
            baseptr
            )
        );
    TraceError( MPI_Win_shared_query, mpi_errno );
    goto fn_exit;
}


/*@
MPI_Win_attach - Attach memory to a dynamic window.

Input Parameters:
+ win - window object (handle)
. base - initial address of memory to be attached
- size - size of memory to be attached in bytes

.N ThreadSafe
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_attach(
    _In_ MPI_Win win,
    _In_ void* base,
    _In_range_(>=, 0) MPI_Aint size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_attach(win, base, size);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    if (win_ptr->createFlavor != MPI_WIN_FLAVOR_DYNAMIC)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "memory region can only be attached to dynamic window");
        goto fn_fail;
    }

    if (base == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "base");
        goto fn_fail;
    }

    if (size <= 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_SIZE, "**arg %s", "size must be positive");
        goto fn_fail;
    }

    MPI_Aint address = reinterpret_cast<MPI_Aint>(base) - reinterpret_cast<MPI_Aint>(MPI_BOTTOM);
    /* The same code is used in MPI_Address */

    mpi_errno = MPID_Win_attach(win_ptr, address, size);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_attach();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
        mpi_errno,
        "**mpi_win_attach %W %p %d",
        win,
        base,
        size
        )
        );
    TraceError(MPI_Win_attach, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Win_detach - Detach memory from a dynamic window.

Input Parameters:
+ win - window object (handle)
- base - initial address of memory to be detached

.N ThreadSafe
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_detach(
    _In_ MPI_Win win,
    _In_ void* base
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_detach(win, base);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    if (win_ptr->createFlavor != MPI_WIN_FLAVOR_DYNAMIC)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "memory region can only be detached from dynamic window");
        goto fn_fail;
    }

    if (base == nullptr)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**nullptr %s", "base");
        goto fn_fail;
    }

    MPI_Aint address = reinterpret_cast<MPI_Aint>(base) - reinterpret_cast<MPI_Aint>(MPI_BOTTOM);
    /* The same code is used in MPI_Address */

    mpi_errno = MPID_Win_detach(win_ptr, address);

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_detach();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
        mpi_errno,
        "**mpi_win_detach %W %p",
        win,
        base
        )
        );
    TraceError(MPI_Win_detach, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_fence - Perform an MPI fence synchronization on a MPI window

   Input Parameters:
+ assert - program assertion (integer)
- win - window object (handle)

   Notes:
   The 'assert' argument is used to indicate special conditions for the
   fence that an implementation may use to optimize the 'MPI_Win_fence'
   operation.  The value zero is always correct.  Other assertion values
   may be or''ed together.  Assertions that are valid for 'MPI_Win_fence' are\:

+ MPI_MODE_NOSTORE - the local window was not updated by local stores
  (or local get or receive calls) since last synchronization.
. MPI_MODE_NOPUT - the local window will not be updated by put or accumulate
  calls after the fence call, until the ensuing (fence) synchronization.
. MPI_MODE_NOPRECEDE - the fence does not complete any sequence of locally
  issued RMA calls. If this assertion is given by any process in the window
  group, then it must be given by all processes in the group.
- MPI_MODE_NOSUCCEED - the fence does not start any sequence of locally
  issued RMA calls. If the assertion is given by any process in the window
  group, then it must be given by all processes in the group.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_fence(
    _In_ int assert,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_fence(assert,win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    mpi_errno = MPID_Win_fence(assert, win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_fence();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_fence %A %W",
            assert,
            win
            )
        );
    TraceError(MPI_Win_fence, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_free - Free an MPI RMA window

   Input Parameter:
. win - window object (handle)

   Notes:
   If successfully freed, 'win' is set to 'MPI_WIN_NULL'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_free(
    _Inout_ MPI_Win* win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_free(*win);

    MPID_Win *win_ptr;

    int mpi_errno;
    if( win == nullptr )
    {
        win_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "win" );
        goto fn_fail;
    }

    mpi_errno = MpiaWinValidateHandle( *win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    if (win_ptr->attributes != NULL)
    {
        mpi_errno = MPIR_Attr_delete_list( win_ptr->handle, &win_ptr->attributes );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    mpi_errno = MPID_Win_free(win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *win = MPI_WIN_NULL;

    TraceLeave_MPI_Win_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_free %p",
            win
            )
        );
    TraceError(MPI_Win_free, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_get_group - Get the MPI Group of the window object

   Input Parameter:
. win - window object (handle)

   Output Parameter:
. group - group of processes which share access to the window (handle)

   Notes:
   The group is a duplicate of the group from the communicator used to
   create the MPI window, and should be freed with 'MPI_Group_free' when
   it is no longer needed.  This group can be used to form the group of
   neighbors for the routines 'MPI_Win_post' and 'MPI_Win_start'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_ARG
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_get_group(
    _In_ MPI_Win win,
    _Out_ MPI_Group* group
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_get_group(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    if( group == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "group" );
        goto fn_fail;
    }

    mpi_errno = NMPI_Comm_group(win_ptr->comm_ptr->handle, group);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_get_group(*group);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_get_group %W %p",
            win,
            group
            )
        );
    TraceError(MPI_Win_get_group, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_get_name - Get the print name associated with the MPI RMA window

   Input Parameter:
. win - window whose name is to be returned (handle)

   Output Parameters:
+ win_name - the name previously stored on the window, or a empty string if
  no such name exists (string)
- resultlen - length of returned name (integer)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Win_get_name(
    _In_ MPI_Win win,
    _Out_writes_z_(MPI_MAX_OBJECT_NAME) char* win_name,
    _Out_ int* resultlen
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_get_name(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( win_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "win_name" );
        goto fn_fail;
    }

    if( resultlen == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "resultlen" );
        goto fn_fail;
    }

    MPIU_Strncpy( win_name, win_ptr->name, MPI_MAX_OBJECT_NAME );
    *resultlen = static_cast<int>( MPIU_Strlen( win_name, MPI_MAX_OBJECT_NAME ) );

    TraceLeave_MPI_Win_get_name(*resultlen, win_name);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_get_name %W %p %p",
            win,
            win_name,
            resultlen
            )
        );
    TraceError(MPI_Win_get_name, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_lock - Begin an RMA access epoch at the target process.

   Input Parameters:
+ lock_type - Indicates whether other processes may access the target
   window at the same time (if 'MPI_LOCK_SHARED') or not ('MPI_LOCK_EXCLUSIVE')
. rank - rank of locked window (nonnegative integer)
. assert - Used to optimize this call; zero may be used as a default.
  See notes. (integer)
- win - window object (handle)

   Notes:

   The name of this routine is misleading.  In particular, this
   routine need not block, except when the target process is the calling
   process.

   Implementations may restrict the use of RMA communication that is
   synchronized
   by lock calls to windows in memory allocated by 'MPI_Alloc_mem'. Locks can
   be used portably only in such memory.

   The 'assert' argument is used to indicate special conditions for the
   fence that an implementation may use to optimize the 'MPI_Win_fence'
   operation.  The value zero is always correct.  Other assertion values
   may be or''ed together.  Assertions that are valid for 'MPI_Win_fence' are\:

. MPI_MODE_NOCHECK - no other process holds, or will attempt to acquire a
  conflicting lock, while the caller holds the window lock. This is useful
  when mutual exclusion is achieved by other means, but the coherence
  operations that may be attached to the lock and unlock calls are still
  required.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_lock(
    _In_ int lock_type,
    _In_range_(>=, MPI_PROC_NULL) int rank,
    _In_ int assert,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_lock(lock_type, rank, assert, win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( lock_type != MPI_LOCK_SHARED && lock_type != MPI_LOCK_EXCLUSIVE )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER,  "**locktype" );
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_lock(lock_type, rank, assert, win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_lock();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_lock %d %d %A %W",
            lock_type,
            rank,
            assert,
            win
            )
        );
    TraceError(MPI_Win_lock, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Win_lock_all - Begin an RMA access epoch at all processes on the given window.

Input Parameters:
+ assert - Used to optimize this call; zero may be used as a default.
See notes. (integer)
- win - window object (handle)

Notes:

Starts an RMA access epoch to all processes in win, with a lock type of
MPI_Lock_shared. During the epoch, the calling process can access the window
memory on all processes in win by using RMA operations. A window locked with
MPI_Win_lock_all must be unlocked with MPI_Win_unlock_all. This routine is not
collective -- the ALL refers to a lock on all members of the group of the window.

The 'assert' argument is used to indicate special conditions for the fence that
an implementation may use to optimize the MPI_Win_lock_all operation. The value
zero is always correct. Other assertion values may be or'ed together.
Assertions that are valid for MPI_Win_lock_all are:

. MPI_MODE_NOCHECK - no other process holds, or will attempt to acquire a
conflicting lock, while the caller holds the window lock. This is useful
when mutual exclusion is achieved by other means, but the coherence
operations that may be attached to the lock and unlock calls are still
required.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_lock_all(
    _In_ int assert,
    _In_ MPI_Win win
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_lock_all(assert, win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);

    mpi_errno = MPID_Win_lock_all(assert, win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_lock_all();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_lock_all %A %W",
            assert,
            win
        )
    );
    TraceError(MPI_Win_lock_all, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_post - Start an RMA exposure epoch

   Input parameters:
+ group - group of origin processes (handle)
. assert - Used to optimize this call; zero may be used as a default.
  See notes. (integer)
- win - window object (handle)

   Notes:
   The 'assert' argument is used to indicate special conditions for the
   fence that an implementation may use to optimize the 'MPI_Win_post'
   operation.  The value zero is always correct.  Other assertion values
   may be or''ed together.  Assertions that are valid for 'MPI_Win_post' are\:

+ MPI_MODE_NOCHECK - the matching calls to 'MPI_WIN_START' have not yet
  occurred on any origin processes when the call to 'MPI_WIN_POST' is made.
  The nocheck option can be specified by a post call if and only if it is
  specified by each matching start call.
. MPI_MODE_NOSTORE - the local window was not updated by local stores (or
  local get or receive calls) since last synchronization. This may avoid
  the need for cache synchronization at the post call.
- MPI_MODE_NOPUT - the local window will not be updated by put or accumulate
  calls after the post call, until the ensuing (wait) synchronization. This
  may avoid the need for cache synchronization at the wait call.

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Win_post(
    _In_ MPI_Group group,
    _In_ int assert,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_post(group, assert, win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Group *group_ptr;
    mpi_errno = MpiaGroupValidateHandle( group, const_cast<MPID_Group**>(&group_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_post(group_ptr, assert, win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_post();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_post %G %A %W",
            group,
            assert,
            win
            )
        );
    TraceError(MPI_Win_post, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_set_name - Set the print name for an MPI RMA window

   Input Parameters:
+ win - window whose identifier is to be set (handle)
- win_name - the character string which is remembered as the name (string)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Win_set_name(
    _In_ MPI_Win win,
    _In_z_ const char* win_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_set_name(win, win_name);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( win_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "win_name" );
        goto fn_fail;
    }

    if( SetName<MPID_Win>( win_ptr, win_name ) == false )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    TraceLeave_MPI_Win_set_name();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_set_name %W %s",
            win,
            win_name
            )
        );
    TraceError(MPI_Win_set_name, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_start - Start an RMA access epoch for MPI

  Input Parameters:
+ group - group of target processes (handle)
. assert - Used to optimize this call; zero may be used as a default.
  See notes. (integer)
- win - window object (handle)

   Notes:
   The 'assert' argument is used to indicate special conditions for the
   fence that an implementation may use to optimize the 'MPI_Win_start'
   operation.  The value zero is always correct.  Other assertion values
   may be or''ed together.  Assertions tha are valid for 'MPI_Win_start' are\:

. MPI_MODE_NOCHECK - the matching calls to 'MPI_WIN_POST' have already
  completed on all target processes when the call to 'MPI_WIN_START' is made.
  The nocheck option can be specified in a start call if and only if it is
  specified in each matching post call. This is similar to the optimization
  of ready-send that may save a handshake when the handshake is implicit in
  the code. (However, ready-send is matched by a regular receive, whereas
  both start and post must specify the nocheck option.)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_start(
    _In_ MPI_Group group,
    _In_ int assert,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_start(win, group, assert);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Group *group_ptr;
    mpi_errno = MpiaGroupValidateHandle( group, &group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_start(group_ptr, assert, win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_start();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_start %G %A %W",
            group,
            assert,
            win
            )
        );
    TraceError(MPI_Win_start, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_test - Test whether an RMA exposure epoch has completed

   Input Parameter:
. win - window object (handle)

   Output Parameter:
. flag - success flag (logical)

   Notes:
   This is the nonblocking version of 'MPI_Win_wait'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
.N MPI_ERR_ARG

.seealso: MPI_Win_wait, MPI_Win_post
@*/
EXTERN_C
MPI_METHOD
MPI_Win_test(
    _In_ MPI_Win win,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_test(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    mpi_errno = MPID_Win_test(win_ptr, flag);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_test(*flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_test %W %p",
            win,
            flag
            )
        );
    TraceError(MPI_Win_test, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_unlock - Completes an RMA access epoch at the target process

   Input Parameters:
+ rank - rank of window (nonnegative integer)
- win - window object (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
.N MPI_ERR_OTHER

.seealso: MPI_Win_lock
@*/
EXTERN_C
MPI_METHOD
MPI_Win_unlock(
    _In_range_(>=, MPI_PROC_NULL) int rank,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_unlock(rank, win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_unlock(rank, win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_unlock();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_unlock %d %W",
            rank,
            win
            )
        );
    TraceError(MPI_Win_unlock, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_unlock_all - Completes an RMA access epoch at all processes on the given window.

   Input Parameters:
+ win - window object (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
.N MPI_ERR_OTHER

.seealso: MPI_Win_lock
@*/
EXTERN_C
MPI_METHOD
MPI_Win_unlock_all(
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_unlock_all(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    mpi_errno = MPID_Win_unlock_all(win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_unlock_all();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_unlock_all %W",
            win
            )
        );
    TraceError(MPI_Win_unlock_all, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_wait - Completes an RMA exposure epoch begun with MPI_Win_post

   Input Parameter:
. win - window object (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_wait(
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_wait(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_wait(win_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_wait();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_wait %W",
            win
            )
        );
    TraceError(MPI_Win_wait, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_flush - Complete all outstanding RMA operations at the given target.

   Input Parameters:
+ rank - rank of window (nonnegative integer)
- win - window object (handle)

   Notes:
   MPI_Win_flush completes all outstanding RMA operations initiated by the calling
   process to the target rank on the specified window. The operations are completed
   both at the origin and at the target.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
.N MPI_ERR_OTHER

@*/
EXTERN_C
MPI_METHOD
MPI_Win_flush(
    _In_range_(>=, MPI_PROC_NULL) int rank,
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_flush(rank, win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );
    mpi_errno = MpiaCommValidateSendRank( win_ptr->comm_ptr, rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_flush(rank, win_ptr, false);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_flush();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_flush %d %W",
            rank,
            win
            )
        );
    TraceError(MPI_Win_flush, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_flush_all - Complete all outstanding RMA operations at all targets

   Input Parameters:
+ win - window object (handle)

   Notes:
   All RMA operations issued by the calling process to any target on the specified
   window prior to this call and in the specified window will have completed both
   at the origin and at the target when this call returns.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
.N MPI_ERR_OTHER

@*/
EXTERN_C
MPI_METHOD
MPI_Win_flush_all(
    _In_ MPI_Win win
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_flush_all(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Assert( win_ptr->comm_ptr != nullptr );

    mpi_errno = MPID_Win_flush_all(win_ptr, false);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_flush_all();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_flush_all %W",
            win
            )
        );
    TraceError(MPI_Win_flush_all, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Win_flush_local - Complete locally all outstanding RMA operations at the given target.

Input Parameters:
+ rank - rank of window (nonnegative integer)
- win - window object (handle)

Notes:
Locally completes at the origin all outstanding RMA operations initiated by the
calling process to the target process specified by rank on the specified window.
For example, after this routine completes, the user may reuse any buffers provided
to put, get, or accumulate operations.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
.N MPI_ERR_OTHER

@*/
EXTERN_C
MPI_METHOD
MPI_Win_flush_local(
    _In_range_(>= , MPI_PROC_NULL) int rank,
    _In_ MPI_Win win
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_flush_local(rank, win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);
    mpi_errno = MpiaCommValidateSendRank(win_ptr->comm_ptr, rank);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Win_flush(rank, win_ptr, true);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_flush_local();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_flush_local %d %W",
            rank,
            win
        )
    );
    TraceError(MPI_Win_flush_local, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Win_flush_local_all - Complete locally all outstanding RMA operations at all targets

Input Parameters:
+ win - window object (handle)

Notes:
All RMA operations issued to any target prior to this call in this window will
have completed at the origin when MPI_Win_flush_local_all returns.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
.N MPI_ERR_OTHER

@*/
EXTERN_C
MPI_METHOD
MPI_Win_flush_local_all(
    _In_ MPI_Win win
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_flush_local_all(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);

    mpi_errno = MPID_Win_flush_all(win_ptr, true);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_flush_local_all();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_flush_local_all %W",
            win
        )
    );
    TraceError(MPI_Win_flush_local_all, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Win_sync - Synchronize public and private copies of the given window.

Input Parameters:
+ win - window object (handle)

Notes:
The call MPI_Win_sync synchronizes the private and public window copies of win.
For the purposes of synchronizing the private and public window, MPI_Win_sync has
the effect of ending and reopening an access and exposure epoch on the window (note
that it does not actually end an epoch or complete any pending MPI RMA operations).

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_RANK
.N MPI_ERR_WIN
.N MPI_ERR_OTHER

@*/
EXTERN_C
MPI_METHOD
MPI_Win_sync(
    _In_ MPI_Win win
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_sync(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle(win, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    MPIU_Assert(win_ptr->comm_ptr != nullptr);
    TraceLeave_MPI_Win_sync();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_sync %W",
            win
        )
    );
    TraceError(MPI_Win_sync, mpi_errno);
    goto fn_exit;
}
