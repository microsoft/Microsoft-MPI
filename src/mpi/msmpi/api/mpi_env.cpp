// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "ch3_compression.h"
#include "colltuner.h"
#include <Windows.h>
#include <ntverp.h>


/* These are initialized as null (avoids making these into common symbols).
   If the Fortran binding is supported, these can be initialized to
   their Fortran values (MPI only requires that they be valid between
   MPI_Init and MPI_Finalize) */
MPI_Fint *MPI_F_STATUS_IGNORE = 0;
MPI_Fint *MPI_F_STATUSES_IGNORE = 0;


static void MPIR_Init_tracing(void)
{
    EventRegisterMicrosoft_HPC_MPI();
    EventRegisterMicrosoft_MPI_Channel_Provider();
}

static void MPIR_Cleanup_tracing(void)
{
    EventUnregisterMicrosoft_HPC_MPI();
    EventUnregisterMicrosoft_MPI_Channel_Provider();
}


/*@
   MPI_Init_thread - Initialize the MPI execution environment

   Input Parameters:
+  argc - Pointer to the number of arguments
.  argv - Pointer to the argument vector
-  required - Level of desired thread support

   Output Parameter:
.  provided - Level of provided thread support

   Command line arguments:
   MPI specifies no command-line arguments but does allow an MPI
   implementation to make use of them.  See 'MPI_INIT' for a description of
   the command line arguments supported by 'MPI_INIT' and 'MPI_INIT_THREAD'.

   Notes:
   The valid values for the level of thread support are\:
+ MPI_THREAD_SINGLE - Only one thread will execute.
. MPI_THREAD_FUNNELED - The process may be multi-threaded, but only the main
  thread will make MPI calls (all MPI calls are funneled to the
   main thread).
. MPI_THREAD_SERIALIZED - The process may be multi-threaded, and multiple
  threads may make MPI calls, but only one at a time: MPI calls are not
  made concurrently from two distinct threads (all MPI calls are serialized).
- MPI_THREAD_MULTIPLE - Multiple threads may call MPI, with no restrictions.

Notes for Fortran:
   Note that the Fortran binding for this routine does not have the 'argc' and
   'argv' arguments. ('MPI_INIT_THREAD(required, provided, ierror)')


.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER

.seealso: MPI_Init, MPI_Finalize
@*/
EXTERN_C
MPI_METHOD
MPI_Init_thread(
    _In_opt_ const int* argc,
    _Notref_ _In_reads_opt_(*argc) char*** argv,
    _In_ int required,
    _Out_ int* provided
    )
{
    /* Do not call MpiaIsInitializedOrExit(); */
    MpiaEnter();
    MPIR_Init_tracing();
    TraceEnter_MPI_Init_thread(required);

    int mpi_errno;

    mpi_errno = Mpi.Initialize( required, provided );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Init_thread(*provided);

fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_init_thread %p %p %d %p",
            argc,
            argv,
            required,
            provided
            )
        );
    TraceError(MPI_Init_thread, mpi_errno);
    MPIR_Cleanup_tracing();
    goto fn_exit;
}

/*@
   MPI_Init - Initialize the MPI execution environment

   Input Parameters:
+  argc - Pointer to the number of arguments
-  argv - Pointer to the argument vector

Thread and Signal Safety:
This routine must be called by one thread only.  That thread is called
the `main thread` and must be the thread that calls 'MPI_Finalize'.

Notes:
   The MPI standard does not say what a program can do before an 'MPI_INIT' or
   after an 'MPI_FINALIZE'.  In the MPICH implementation, you should do
   as little as possible.  In particular, avoid anything that changes the
   external state of the program, such as opening files, reading standard
   input or writing to standard output.

Notes for Fortran:
The Fortran binding for 'MPI_Init' has only the error return
.vb
    subroutine MPI_INIT( ierr )
    integer ierr
.ve

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INIT

.seealso: MPI_Init_thread, MPI_Finalize
@*/
EXTERN_C
MPI_METHOD
MPI_Init(
    _In_opt_ const int* argc,
    _Notref_ _In_reads_opt_(*argc) char*** argv
    )
{
    /* MpiaIsInitializedOrExit(); */
    MpiaEnter();
    MPIR_Init_tracing();
    TraceEnter_MPI_Init();

    int mpi_errno;

    mpi_errno = Mpi.Initialize( MPI_THREAD_SINGLE, nullptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Init();

    Trace_MPI_Init_info(
        Mpi.CommWorld->rank,
        MSMPI_VER_MAJOR(MSMPI_VER_EX),
        MSMPI_VER_MINOR(MSMPI_VER_EX),
        MSMPI_VER_BUILD(MSMPI_VER_EX)
        );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_init %p %p",
            argc,
            argv
            )
        );

    TraceError(MPI_Init, mpi_errno);
    MPIR_Cleanup_tracing();
    goto fn_exit;
}


/*@
   MPI_Finalize - Terminates MPI execution environment

   Notes:
   All processes must call this routine before exiting.  The number of
   processes running `after` this routine is called is undefined;
   it is best not to perform much more than a 'return rc' after calling
   'MPI_Finalize'.

Thread and Signal Safety:
The MPI standard requires that 'MPI_Finalize' be called `only` by the same
thread that initialized MPI with either 'MPI_Init' or 'MPI_Init_thread'.

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Finalize( void )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Finalize();

    int rank = Mpi.CommWorld->rank;

    /* Remove the attributes, executing the attribute delete routine. */
    /* The standard (MPI-2, section 4.8) says that the attributes on
       MPI_COMM_SELF are deleted before almost anything else happens */
    int mpi_errno = MPIR_Attr_delete_list(
        MPI_COMM_SELF,
        &Mpi.CommSelf->attributes
        );
    MPIU_Assert( mpi_errno == MPI_SUCCESS );

    mpi_errno = MPIR_Attr_delete_list(
        MPI_COMM_WORLD,
        &Mpi.CommWorld->attributes
        );
    MPIU_Assert( mpi_errno == MPI_SUCCESS );

    //
    // Cleanup all attributes on datatypes before we free anything else.
    //
    // Note that we don't use the finalize callback mechanism as it cannot
    // return an error.  Having it be explicit is actually better, anyway.
    //
    mpi_errno = MPIR_Datatype_cleanup();
    //
    // TODO: Under a strict mode, we could return the error here and fail Finalize.
    //
    MPIU_Assert( mpi_errno == MPI_SUCCESS );

    /* Call the high-priority callbacks */
    MPIR_Call_finalize_callbacks( MPIR_FINALIZE_CALLBACK_PRIO + 1);
    ADIO_Finalize();

    mpi_errno = Mpi.Finalize();

    /* delete local and remote groups on comm_world and comm_self if
       they had been created */
    if (Mpi.CommWorld->group)
    {
        MPIR_Group_release(Mpi.CommWorld->group);
    }
    if (Mpi.CommSelf->group)
    {
        MPIR_Group_release(Mpi.CommSelf->group);
    }

    /* Call the low-priority (post Finalize) callbacks */
    MPIR_Call_finalize_callbacks( 0 );

#if DBG
    if( MPID_Keyval_mem.num_user_alloc > 0 )
    {
        printf(
            "WARNING: Application leaked %d keyvals\n",
            MPID_Keyval_mem.num_user_alloc
            );
        fflush( NULL );
    }
    else
    {
        //
        // BUG 23481
        //
        // The keyval_double_free test decrements this so it becomes negative so reset it here.
        //
        MPID_Keyval_mem.num_user_alloc = 0;
    }

    MPIU_Assert( MPID_Keyval_mem.num_alloc == MPID_Keyval_mem.num_user_alloc );
    MPIU_Assert( MPID_Attr_mem.num_alloc == 0 );
#endif

    /* At this point, if there has been a failure, exit before
       completing the finalize */
    ON_ERROR_FAIL(mpi_errno);

    /* At this point, we end the critical section for the Finalize call.
       Since we've set Mpi.Instance.mpi_state value to POST_FINALIZED,
       if the user erroneously calls Finalize from another thread, an
       error message will be issued. */
    Mpi.CompleteFinalize();

    TraceLeave_MPI_Finalize();

    Trace_MPI_Finalize_info(rank);

  fn_exit:
    MPIR_Cleanup_tracing();
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_finalize",
            )
        );

    TraceError(MPI_Finalize, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Initialized - Indicates whether 'MPI_Init' has been called.

Output Argument:
. flag - Flag is true if 'MPI_Init' or 'MPI_Init_thread' has been called and
         false otherwise.

   Notes:

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Initialized(
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    /* MpiaIsInitializedOrExit(); */
    MpiaEnter();

    int mpi_errno = MPI_SUCCESS;

    /* Should check that flag is not null */
    if( flag == nullptr )
    {
        mpi_errno = MPI_ERR_ARG;
        goto fn_fail;
    }

    *flag = Mpi.IsInitialized() ? TRUE : FALSE;

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    if (Mpi.IsReady())
    {
        mpi_errno = MPIR_Err_return_comm(
            NULL,
            FCNAME,
            MPIU_ERR_GET(
                mpi_errno,
                "**mpi_initialized %p",
                flag
                )
            );
    }
    goto fn_exit;
}


/*@
   MPI_Finalized - Indicates whether 'MPI_Finalize' has been called.

Output Parameter:
. flag - Flag is true if 'MPI_Finalize' has been called and false otherwise.
     (logical)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Finalized(
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    /* MpiaIsInitializedOrExit(); */
    MpiaEnter();

    int mpi_errno = MPI_SUCCESS;

    /* Should check that flag is not null */
    if( flag == nullptr )
    {
        mpi_errno = MPI_ERR_ARG;
        goto fn_fail;
    }

    *flag = Mpi.IsFinalized() ? TRUE : FALSE;

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    if (Mpi.IsReady())
    {
        mpi_errno = MPIR_Err_return_comm(
            NULL,
            FCNAME,
            MPIU_ERR_GET(
                mpi_errno,
                "**mpi_finalized %p",
                flag
                )
            );
    }
    goto fn_exit;
}


/*@
   MPI_Is_thread_main - Returns a flag indicating whether this thread called
                        'MPI_Init' or 'MPI_Init_thread'

   Output Parameter:
. flag - Flag is true if 'MPI_Init' or 'MPI_Init_thread' has been called by
         this thread and false otherwise.  (logical)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Is_thread_main(
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Is_thread_main();

    int mpi_errno = MPI_SUCCESS;

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    *flag = Mpi.IsCurrentThreadMaster() ? TRUE : FALSE;

    TraceLeave_MPI_Is_thread_main(*flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_is_thread_main %p",
            flag
            )
        );
    TraceError(MPI_Is_thread_main, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Pcontrol - Controls profiling

  Input Parameters:
+ level - Profiling level
-  ... - other arguments (see notes)

  Notes:
  This routine provides a common interface for profiling control.  The
  interpretation of 'level' and any other arguments is left to the
  profiling library.  The intention is that a profiling library will
  provide a replacement for this routine and define the interpretation
  of the parameters.

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Pcontrol(
    _In_ const int /*level*/,
    ...
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();

    int mpi_errno = MPI_SUCCESS;

    /* This is a dummy routine that does nothing.  It is intended for
       use by the user (or a tool) with the profiling interface */

    MpiaExit();
    return mpi_errno;
}


/*@
   MPI_Query_thread - Return the level of thread support provided by the MPI
    library

   Output Parameter:
.  provided - Level of thread support provided.  This is the same value
   that was returned in the 'provided' argument in 'MPI_Init_thread'.

   Notes:
   The valid values for the level of thread support are\:
+ MPI_THREAD_SINGLE - Only one thread will execute.
. MPI_THREAD_FUNNELED - The process may be multi-threaded, but only the main
  thread will make MPI calls (all MPI calls are funneled to the
   main thread).
. MPI_THREAD_SERIALIZED - The process may be multi-threaded, and multiple
  threads may make MPI calls, but only one at a time: MPI calls are not
  made concurrently from two distinct threads (all MPI calls are serialized).
- MPI_THREAD_MULTIPLE - Multiple threads may call MPI, with no restrictions.

   If 'MPI_Init' was called instead of 'MPI_Init_thread', the level of
   thread support is defined by the implementation.  This routine allows
   you to find out the provided level.  It is also useful for library
   routines that discover that MPI has already been initialized and
   wish to determine what level of thread support is available.

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Query_thread(
    _Out_ int* provided
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Query_thread();

    int mpi_errno = MPI_SUCCESS;

    if( provided == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "provided" );
        goto fn_fail;
    }

    *provided = Mpi.ThreadLevel;

    TraceLeave_MPI_Query_thread(*provided);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_query_thread %p",
            provided
            )
        );
    TraceError(MPI_Query_thread, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Get_version - Return the version number of MPI

   Output Parameters:
+  version - Version of MPI
-  subversion - Subversion of MPI

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Get_version(
    _Out_ int* version,
    _Out_ int* subversion
    )
{
    /* Note that this routine may be called before MPI_Init */
    /* MpiaIsInitializedOrExit(); */
    MpiaEnter();
    TraceEnter_MPI_Get_version();

    int mpi_errno = MPI_SUCCESS;

    /* Validate parameters and objects (post conversion) */
    if( version == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "version" );
        goto fn_fail;
    }
    if( subversion == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "subversion" );
        goto fn_fail;
    }

    *version    = MPI_VERSION;
    *subversion = MPI_SUBVERSION;

    TraceLeave_MPI_Get_version(*version, *subversion);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_get_version %p %p",
            version,
            subversion
            )
        );
    TraceError(MPI_Get_version, mpi_errno);
    goto fn_exit;
}


/*@
   MSMPI_Get_version - Return the version number of the MSMPI feature set.
@*/
EXTERN_C
MPI_METHOD
MSMPI_Get_version()
{
    //
    // This should be defined globally for all MPI code. It should be
    //  set the current version, so this should always return
    //  the intended value.
    //
    return MSMPI_VER;
}


/*@
  MPI_Get_library_version - Gets the version string of msmpi.dll as "MAJOR.MINOR.BUILD"

  Output Parameters:
+ version - String with the MS-MPI specific version. This
  must be an array of size at least 'MPI_MAX_LIBRARY_VERSION_STRING'.
- resultlen - Length (in characters) of the version string

  Notes for Fortran:
  Character argument should be declared as a character string
  of 'MPI_MAX_LIBRARY_VERSION_STRING'
  character*(MPI_MAX_LIBRARY_VERSION_STRING) version
@*/
EXTERN_C
MPI_METHOD
MPI_Get_library_version(
    _Out_writes_z_(MPI_MAX_LIBRARY_VERSION_STRING) char* version,
    _Out_ int* resultlen
    )
{
    int mpi_errno;

    if( version == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "version" );
        goto fn_fail;
    }

    if( resultlen == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "resultlen" );
        goto fn_fail;
    }

    *resultlen = MPIU_Snprintf(
        version,
        MPI_MAX_LIBRARY_VERSION_STRING,
        "Microsoft MPI %u.%u.%u.%u%S",
        MSMPI_VER_MAJOR(MSMPI_VER_EX),
        MSMPI_VER_MINOR(MSMPI_VER_EX),
        _BLDNUMMAJOR,
        _BLDNUMMINOR,
        MSMPI_BUILD_LABEL
        );

    MPIU_Assert( *resultlen > 0 && *resultlen < MPI_MAX_LIBRARY_VERSION_STRING );
    return MPI_SUCCESS;

  fn_fail:
    return MPIU_ERR_GET(
        mpi_errno,
        "**mpi_get_library_version %p %p",
        version,
        resultlen
        );
}


/*@
  MPI_Wtick - Returns the resolution of MPI_Wtime

  Return value:
  Time in seconds of resolution of MPI_Wtime

  Notes for Fortran:
  This is a function, declared as 'DOUBLE PRECISION MPI_WTICK()' in Fortran.

.see also: MPI_Wtime, MPI_Comm_get_attr, MPI_Attr_get
@*/
EXTERN_C
double
MPIAPI
MPI_Wtick( void )
{
    MpiaIsInitializedOrExit();

    return MPID_Wtick();
}


/*@
  MPI_Wtime - Returns an elapsed time on the calling processor

  Return value:
  Time in seconds since an arbitrary time in the past.

  Notes:
  This is intended to be a high-resolution, elapsed (or wall) clock.
  See 'MPI_WTICK' to determine the resolution of 'MPI_WTIME'.
  If the attribute 'MPI_WTIME_IS_GLOBAL' is defined and true, then the
  value is synchronized across all processes in 'MPI_COMM_WORLD'.

  Notes for Fortran:
  This is a function, declared as 'DOUBLE PRECISION MPI_WTIME()' in Fortran.

.see also: MPI_Wtick, MPI_Comm_get_attr, MPI_Attr_get
@*/
EXTERN_C
double
MPIAPI
MPI_Wtime( void )
{
    MpiaIsInitializedOrExit();

    double d;
    MPID_Time_t t;

    MPID_Wtime( &t );
    MPID_Wtime_todouble( &t, &d );

    return d;
}


/*@
   MPI_Alloc_mem - Allocate memory for message passing and RMA

  Input Parameters:
+ size - size of memory segment in bytes (nonnegative integer)
- info - info argument (handle)

  Output Parameter:
. baseptr - pointer to beginning of memory segment allocated

   Notes:
 Using this routine from Fortran requires that the Fortran compiler accept
 a common pointer extension.  See Section 4.11 (Memory Allocation) in the
 MPI-2 standard for more information and examples.

   Also note that while 'baseptr' is a 'void *' type, this is
   simply to allow easy use of any pointer object for this parameter.
   In fact, this argument is really a 'void **' type, that is, a
   pointer to a pointer.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INFO
.N MPI_ERR_ARG
.N MPI_ERR_NO_MEM
@*/
EXTERN_C
MPI_METHOD
MPI_Alloc_mem(
    _In_ MPI_Aint size,
    _In_ MPI_Info info,
    _Out_ void* baseptr
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Alloc_mem(size, info);

    int mpi_errno = MPI_SUCCESS;

    if( size < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "size", size );
        goto fn_fail;
    }

    if( baseptr == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "baseptr" );
        goto fn_fail;
    }

    MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    void *ap = MPID_Alloc_mem(size, info_ptr);
    if (!ap)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_NO_MEM, "**allocmem" );
        goto fn_fail;
    }

    *static_cast<void **>(baseptr) = ap;

    TraceLeave_MPI_Alloc_mem(ap);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_alloc_mem %p %I %p",
            size,
            info,
            baseptr
        )
    );
    TraceError(MPI_Alloc_mem, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Free_mem - Free memory allocated with MPI_Alloc_mem

   Input Parameter:
.  base - initial address of memory segment allocated by 'MPI_ALLOC_MEM'
       (choice)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Free_mem(
    _In_ _Post_invalid_ void* base
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Free_mem(base);

    int mpi_errno;
    if( base == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BASE, "**nullptr %s", "baseptr" );
        goto fn_fail;
    }

    mpi_errno = MPID_Free_mem(base);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Free_mem();
fn_exit:
    MpiaExit();
    return mpi_errno;
fn_fail:
    TraceError(MPI_Free_mem, mpi_errno);
    goto fn_exit;
}


#ifdef HAVE_FORTRAN_BINDING

//
// malloc and free functions to support Fortran static libs.
// Exporting these functions remove the need for the Fortran static libraries
// to link with a specific version of the CRT (e.g., DLL or static lib)
//

EXTERN_C void* MPIAPI MPIR_Malloc(size_t size)
{
    return MPIU_Malloc(size);
}


EXTERN_C void MPIAPI MPIR_Free(void* p)
{
    MPIU_Free(p);
}

#endif // HAVE_FORTRAN_BINDING


/*@
   MPI_Get_address - Get the address of a location in memory

Input Parameter:
. location - location in caller memory (choice)

Output Parameter:
. address - address of location (address)

   Notes:
    This routine is provided for both the Fortran and C programmers.
    On many systems, the address returned by this routine will be the same
    as produced by the C '&' operator, but this is not required in C and
    may not be true of systems with word- rather than byte-oriented
    instructions or systems with segmented address spaces.

    This routine should be used instead of 'MPI_Address'.

.N SignalSafe

.N Fortran

 In Fortran, the integer type is always signed.  This can cause problems
 on systems where the address fits into a four byte unsigned integer but
 the value is larger than the largest signed integer.  For example, a system
 with more than 2 GBytes of memory may have addresses that do not fit within
 a four byte signed integer.  Unfortunately, there is no easy solution to
 this problem, as there is no Fortran datatype that can be used here (using
 a longer integer type will cause other problems, as well as surprising
 users when the size of the integer type is larger that the size of a pointer
 in C).  In this case, it is recommended that you use C to manipulate
 addresses.

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Get_address(
    _In_ const void* location,
    _Out_ MPI_Aint* address
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Get_address(location);

    int mpi_errno = MPI_SUCCESS;

    if( address == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "address" );
        goto fn_fail;
    }

    /* Note that this is the "portable" way to generate an address.
       The difference of two pointers is the number of elements
       between them, so this gives the number of chars between location
       and ptr.  As long as sizeof(char) represents one byte,
       of bytes from 0 to location */
    *address = (INT_PTR)location - (INT_PTR)MPI_BOTTOM;
    /* The same code is used in MPI_Address */

    TraceLeave_MPI_Get_address(*address);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_get_address %p %p",
            location,
            address
            )
        );
    TraceError(MPI_Get_address, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Get_processor_name - Gets the name of the processor

  Output Parameters:
+ name - A unique specifier for the actual (as opposed to virtual) node. This
  must be an array of size at least 'MPI_MAX_PROCESSOR_NAME'.
- resultlen - Length (in characters) of the name

  Notes:
  The name returned should identify a particular piece of hardware;
  the exact format is implementation defined.  This name may or may not
  be the same as might be returned by 'gethostname', 'uname', or 'sysinfo'.

.N ThreadSafe

.N Fortran

 In Fortran, the character argument should be declared as a character string
 of 'MPI_MAX_PROCESSOR_NAME' rather than an array of dimension
 'MPI_MAX_PROCESSOR_NAME'.  That is,
.vb
   character*(MPI_MAX_PROCESSOR_NAME) name
.ve
 rather than
.vb
   character name(MPI_MAX_PROCESSOR_NAME)
.ve
 The two

.N FortranString

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Get_processor_name(
    _Out_writes_z_(MPI_MAX_PROCESSOR_NAME) char* name,
    _Out_ int* resultlen
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Get_processor_name();

    int mpi_errno;

    if( name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "name" );
        goto fn_fail;
    }
    if( resultlen == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "resultlen" );
        goto fn_fail;
    }

    mpi_errno = MPID_Get_processor_name( name, MPI_MAX_PROCESSOR_NAME,
                                         resultlen );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    TraceLeave_MPI_Get_processor_name(*resultlen, name);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_get_processor_name %p %p",
            name,
            resultlen
            )
        );
    TraceError(MPI_Get_processor_name, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Abort - Terminates MPI execution environment

Input Parameters:
+ comm - communicator of tasks to abort
- errorcode - error code to return to invoking environment

Notes:
Terminates all MPI processes associated with the communicator 'comm'; in
most systems (all to date), terminates `all` processes.

.N NotThreadSafe
Because the 'MPI_Abort' routine is intended to ensure that an MPI
process (and possibly an entire job), it cannot wait for a thread to
release a lock or other mechanism for atomic access.

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
#pragma warning(push)
#pragma warning(disable:4702)
EXTERN_C
MPI_METHOD
MPI_Abort(
    _In_ MPI_Comm comm,
    _In_ int errorcode
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Abort(comm, errorcode);

    /* FIXME: 100 is arbitrary and may not be long enough */
    char abort_str[100];

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* FIXME: This is not internationalized */
    MPIU_Snprintf(
        abort_str,
        _countof(abort_str),
        "aborting %s (comm=0x%X), error %d, comm rank %d",
        comm_ptr->name,
        comm,
        errorcode,
        comm_ptr->rank
        );
    MPID_Abort( comm_ptr, FALSE, errorcode, abort_str );
    TraceLeave_MPI_Abort();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    /* It is not clear that doing aborting abort makes sense.  We may
       want to specify that erroneous arguments to MPI_Abort will
       cause an immediate abort. */

    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_abort %C %d",
            comm,
            errorcode
            )
        );
    TraceError(MPI_Abort, mpi_errno);
    goto fn_exit;
}
#pragma warning(pop)


/*@
   MPI_Register_datarep - Register a set of user-provided data conversion
   functions

   Input Parameters:
+ datarep - data representation identifier (string)
. read_conversion_fn - function invoked to convert from file representation to native representation (function)
. write_conversion_fn - function invoked to convert from native representation to file representation (function)
. dtype_file_extent_fn - function invoked to get the extent of a datatype as represented in the file (function)
- extra_state - extra state that is passed to the conversion functions

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Register_datarep(
    _In_z_ const char* datarep,
    _In_opt_ MPI_Datarep_conversion_function* read_conversion_fn,
    _In_opt_ MPI_Datarep_conversion_function* write_conversion_fn,
    _In_ MPI_Datarep_extent_function* dtype_file_extent_fn,
    _In_opt_ void* extra_state
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Register_datarep(datarep,read_conversion_fn,write_conversion_fn,dtype_file_extent_fn,extra_state);

    /* FIXME UNIMPLEMENTED */
    int mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**notimpl");

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Register_datarep();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIO_Err_return_file(
        MPI_FILE_NULL,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_register_datarep %s %p %p %p %p",
            datarep,
            read_conversion_fn,
            write_conversion_fn,
            dtype_file_extent_fn,
            extra_state
            )
        );
    TraceError(MPI_Register_datarep, mpi_errno);
    goto fn_exit;
}
