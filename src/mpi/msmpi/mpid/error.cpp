// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "adio.h"
#include "adio_extern.h"


/* Special error handler to call if we are not yet initialized */
void MPIR_Err_preOrPostInit( void )
{
    if (Mpi.TestState(MSMPI_STATE_UNINITIALIZED))
    {
        MPIU_Error_printf("Attempting to use an MPI routine before initializing MPI\n");
    }
    else if (Mpi.TestState(MSMPI_STATE_INITIALIZING))
    {
        MPIU_Error_printf("Attempting to use an MPI routine during initializing MPI\n");
    }
    else if (Mpi.TestState(MSMPI_STATE_FINALIZED))
    {
        MPIU_Error_printf("Attempting to use an MPI routine after finalizing MPI\n");
    }
    else
    {
        MPIU_Assert(0);
    }
    exit(1);
}

/* Check for a valid error code.  If the code is not valid, attempt to
   print out something sensible; resent the error code to have class
   ERR_UNKNOWN */
static inline void
verifyErrcode(
    _In_  int         error_class,
    _In_  PCSTR       location,
    _When_(error_class > MPICH_ERR_LAST_CLASS, _Out_) MPI_RESULT* errcode_p)
{
    if (error_class > MPICH_ERR_LAST_CLASS)
    {
        MPIU_Error_printf("INTERNAL ERROR: Invalid error class (%d) encountered while returning from\n"
                              "%s.  Please file a bug report.\n", error_class, location);
        *errcode_p = MPI_ERR_UNKNOWN;
    }
}

#pragma warning(push)
#pragma warning(disable:4702)
static inline void
handleFatalError(
    _In_opt_ MPID_Comm* comm_ptr,
    _In_ PCSTR      location ,
    _In_ int        errcode
    )
{
    /* Define length of the the maximum error message line (or string with
       newlines?).  This definition is used only within this routine.  */
    /* FIXME: This should really be the same as MPI_MAX_ERROR_STRING, or in the
       worst case, defined in terms of that */
#define MAX_ERRMSG_STRING 4096
    char error_msg[ MAX_ERRMSG_STRING ];
    size_t len;

    /* FIXME: Not internationalized */
    len = MPIU_Snprintf(error_msg, _countof(error_msg), "Fatal error in %s: ", location);
    MPIR_Err_get_string(errcode, &error_msg[len], MAX_ERRMSG_STRING-len);
    MPID_Abort(comm_ptr, TRUE, 0, error_msg);
}
#pragma warning(pop)


/*
 * This is the routine that is invoked by most MPI routines to
 * report an error
 */
_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIR_Err_return_comm(
    _In_opt_ MPID_Comm* comm_ptr,
    _In_ PCSTR      fcname,
    _In_ MPI_RESULT errcode
    )
{
    const int error_class = ERROR_GET_CLASS(errcode);

    verifyErrcode( error_class, fcname, &errcode );

    /* First, check the nesting level */
    if (MPIR_Nested_call())
        return errcode;

    if (comm_ptr == nullptr || comm_ptr->errhandler == nullptr)
    {
        /* Try to replace with the default handler, which is the one on
           MPI_COMM_WORLD.  This gives us correct behavior for the
           case where the error handler on MPI_COMM_WORLD has been changed. */
        if (Mpi.CommWorld != nullptr)
        {
            comm_ptr = Mpi.CommWorld;
        }
    }

    if (ERROR_IS_FATAL(errcode) ||
        comm_ptr == NULL || comm_ptr->errhandler == NULL ||
        comm_ptr->errhandler->handle == MPI_ERRORS_ARE_FATAL)
    {
        /* Calls MPID_Abort */
        handleFatalError( comm_ptr, fcname, errcode );
    }

    /* If the last error in the stack is a user function error, return that error instead of the corresponding mpi error code? */
    errcode = MPIR_Err_get_user_error_code(errcode);

    if (comm_ptr &&
        comm_ptr->errhandler &&
        comm_ptr->errhandler->handle != MPI_ERRORS_RETURN)
    {
        /* Process any user-defined error handling function */
        comm_ptr->errhandler->errfn.comm.proxy(
            comm_ptr->errhandler->errfn.comm.user_function,
            &comm_ptr->handle,
            &errcode
            );
    }

    return errcode;
}


/*
 * MPI routines that detect errors on window objects use this to report errors
 */
_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIR_Err_return_win(
    _In_opt_ MPID_Win*  win_ptr,
    _In_ PCSTR      fcname,
    _In_ MPI_RESULT errcode
    )
{
    const int error_class = ERROR_GET_CLASS(errcode);

    if (win_ptr == NULL || win_ptr->errhandler == NULL)
        return MPIR_Err_return_comm(NULL, fcname, errcode);

    verifyErrcode( error_class, fcname, &errcode );

    /* First, check the nesting level */
    if (MPIR_Nested_call())
        return errcode;

    if (ERROR_IS_FATAL(errcode) ||
        win_ptr == NULL || win_ptr->errhandler == NULL ||
        win_ptr->errhandler->handle == MPI_ERRORS_ARE_FATAL)
    {
        /* Calls MPID_Abort */
        handleFatalError( NULL, fcname, errcode );
    }

    /* If the last error in the stack is a user function error, return that error instead of the corresponding mpi error code? */
    errcode = MPIR_Err_get_user_error_code(errcode);

    if (win_ptr->errhandler->handle != MPI_ERRORS_RETURN)
    {
        /* Now, invoke the error handler for the window */

        /* Process any user-defined error handling function */
        win_ptr->errhandler->errfn.win.proxy(
            win_ptr->errhandler->errfn.win.user_function,
            &win_ptr->handle,
            &errcode
            );
    }

    return errcode;
}


/*
 * MPI routines that detect errors on files use this to report errors
 */
_Post_equals_last_error_
MPI_RESULT MPIR_Err_return_file_helper(MPI_Errhandler eh, MPI_File file, int errcode)
{
    if (ERROR_IS_FATAL(errcode) || eh == MPI_ERRORS_ARE_FATAL)
    {
        /* Calls MPID_Abort */
        handleFatalError( NULL, "MPI IO", errcode );
    }

    if(eh != MPI_ERRORS_RETURN)
    {
        MPID_Errhandler* e;
        MPID_Errhandler_get_ptr_valid( eh, e );
        MPIU_Assert( e != nullptr );

        /* Process any user-defined error handling function */
        e->errfn.file.proxy(
            e->errfn.file.user_function,
            &file,
            &errcode
            );
    }

    return errcode;
}


/*
 * Error handlers.  These are handled just like the other opaque objects
 * in MPICH
 */

#ifndef MPID_ERRHANDLER_PREALLOC
#define MPID_ERRHANDLER_PREALLOC 8
#endif


C_ASSERT( HANDLE_GET_TYPE(MPI_ERRHANDLER_NULL) == HANDLE_TYPE_INVALID );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_ERRHANDLER_NULL) == MPID_ERRHANDLER );

C_ASSERT( HANDLE_GET_TYPE(MPI_ERRORS_ARE_FATAL) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_ERRORS_ARE_FATAL) == MPID_ERRHANDLER );

C_ASSERT( HANDLE_GET_TYPE(MPI_ERRORS_RETURN) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_ERRORS_RETURN) == MPID_ERRHANDLER );

/* Preallocated errorhandler objects */
MPID_Errhandler MPID_Errhandler_builtin[2] =
          { { MPI_ERRORS_ARE_FATAL, 0},
            { MPI_ERRORS_RETURN, 0} };
MPID_Errhandler MPID_Errhandler_direct[MPID_ERRHANDLER_PREALLOC] = { {0} };
MPIU_Object_alloc_t MPID_Errhandler_mem = { 0, 0, 0, 0, MPID_ERRHANDLER,
                                            sizeof(MPID_Errhandler),
                                            MPID_Errhandler_direct,
                                            MPID_ERRHANDLER_PREALLOC, };

void MPID_Errhandler_free(MPID_Errhandler *errhan_ptr)
{
    MPIU_Handle_obj_free(&MPID_Errhandler_mem, errhan_ptr);
}


/*
 * This file contains the routines needed to implement the MPI routines that
 * can add error classes and codes during runtime.  This file is organized
 * so that applications that do not use the MPI-2 routines to create new
 * error codes will not load any of this code.
 *
 * ROMIO has been customized to provide error messages with the same tools
 * as the rest of MPICH2 and will not rely on the dynamically assigned
 * error classes.  This leaves all of the classes and codes for the user.
 *
 * Because we have customized ROMIO, we do not need to implement
 * instance-specific messages for the dynamic error codes.
 */

#define VALID_DYN_CODE(code) \
    ((code & ~(ERROR_CLASS_MASK | ERROR_DINDEX_MASK | ERROR_COD_FLAG)) == ERROR_DYN_FLAG)

/* verify that the ring error index field is a subset of the error code field */
C_ASSERT((ERROR_DINDEX_MASK & ERROR_INDEX_MASK) == ERROR_DINDEX_MASK);


static int  not_initialized = 1;  /* This allows us to use atomic decr */
static const char* user_class_msgs[ERROR_CLASS_SIZE] = { 0 };
static const char* user_code_msgs[ERROR_DINDEX_SIZE] = { 0 };
static int  first_free_class = 0;
static int  first_free_code  = 0;

/* Forward reference */
const char* MPIR_Err_get_dynerr_string( int code );

/* This external allows this package to define the routine that converts
   dynamically assigned codes and classes to their corresponding strings.
   A cleaner implementation could replace this exposed global with a method
   defined in the error_string.c file that allowed this package to set
   the routine. */

static MPI_RESULT MPIR_Dynerrcodes_finalize( void * );

/* Local routine to initialize the data structures for the dynamic
   error classes and codes.

   MPIR_Init_err_dyncodes is called if not_initialized is true.
   Because all of the routines in this file are called by the
   MPI_Add_error_xxx routines, and those routines use the SINGLE_CS
   when the implementation is multithreaded, these routines (until
   we implement finer-grain thread-synchronization) need not worry about
   multiple threads
 */
static void MPIR_Init_err_dyncodes( void )
{
    /* FIXME: Does this need a thread-safe init? */
    not_initialized = 0;

    /* Set the routine to provides access to the dynamically created
       error strings */
    MPIR_Err_set_dynerr_fn(MPIR_Err_get_dynerr_string);

    /* Add a finalize handler to free any allocated space */
    MPIR_Add_finalize( MPIR_Dynerrcodes_finalize, (void*)0, 9 );
}


/*
  MPIR_Err_set_msg - Change the message for an error code or class

  Input Parameter:
+ code - Error code or class
- msg  - New message to use

  Notes:
  This routine is needed to implement 'MPI_Add_error_string'.
*/
MPI_RESULT MPIR_Err_set_msg( _In_ int code, _In_z_ const char *msg_string )
{
    if (not_initialized)
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**argerrcode %d", code );
    }

    if (!VALID_DYN_CODE(code))
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**argerrcode %d", code );
    }

    char* str = MPIU_Strdup( msg_string );
    if( str == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    if (ERROR_IS_CODE(code))
    {
        int errcode = ERROR_GET_INDEX(code);
        if (errcode < first_free_code)
        {
            if (user_code_msgs[errcode])
            {
                MPIU_Free( (void*)(user_code_msgs[errcode]) );
            }
            user_code_msgs[errcode] = str;
        }
        else
        {
            MPIU_Free( str );
            return MPIU_ERR_CREATE(MPI_ERR_ARG, "**argerrcode %d", code );
        }
    }
    else
    {
        int errclass  = ERROR_GET_CLASS(code);
        if (errclass < first_free_class)
        {
            if (user_class_msgs[errclass])
            {
                MPIU_Free( (void*)(user_class_msgs[errclass]) );
            }
            user_class_msgs[errclass] = str;
        }
        else
        {
            MPIU_Free( str );
            return MPIU_ERR_CREATE(MPI_ERR_ARG, "**argerrcode %d", code );
        }
    }

    return MPI_SUCCESS;
}

/*
  MPIR_Err_add_class - Creata a new error class

  Return value:
  An error class.  Returns -1 if no more classes are available.

  Notes:
  This is used to implement 'MPI_Add_error_class'; it may also be used by a
  device to add device-specific error classes.

  Predefined classes are handled directly; this routine is not used to
  initialize the predefined MPI error classes.  This is done to reduce the
  number of steps that must be executed when starting an MPI program.

  This routine should be run within a SINGLE_CS in the multithreaded case.
*/
MPI_RESULT MPIR_Err_add_class()
{
    int new_class;

    if (not_initialized)
    {
        MPIR_Init_err_dyncodes();
    }

    if (first_free_class == _countof(user_class_msgs))
        return -1;

    /* Get new class */
    new_class = first_free_class;
    first_free_class++;

    return (new_class | ERROR_DYN_FLAG);
}

/*
  MPIR_Err_add_code - Create a new error code that is associated with an
  existing error class

  Input Parameters:
. errclass - Error class to which the code belongs.

  Return value:
  An error code.

  Notes:
  This is used to implement 'MPI_Add_error_code'; it may also be used by a
  device to add device-specific error codes.

  */
MPI_RESULT MPIR_Err_add_code( int errclass )
{
    MPI_RESULT new_code;

    /* Note that we can add codes to existing classes, so we may
       need to initialize the dynamic error routines in this function */
    if (not_initialized)
    {
        MPIR_Init_err_dyncodes();
    }

    if (first_free_code == _countof(user_code_msgs))
        return -1;

    /* Get the new code */
    new_code = first_free_code;
    first_free_code++;

    /* Create the full error code */
    new_code = errclass | ERROR_DYN_FLAG | ERROR_COD_FLAG | (new_code << ERROR_DINDEX_SHIFT);

    return new_code;
}

/*
  MPIR_Err_get_dynerr_string - Get the message string that corresponds to a
  dynamically created error class or code

  Input Parameter:
+ code - An error class or code.  If a code, it must have been created by
  'MPIR_Err_create_code'.

  Return value:
  A pointer to a null-terminated text string with the corresponding error
  message.  A null return indicates an error; usually the value of 'code' is
  neither a valid error class or code.

  Notes:
  This routine is used to implement 'MPI_ERROR_STRING'.  It is only called
  for dynamic error codes.
  */
const char* MPIR_Err_get_dynerr_string( int code )
{
    if (!VALID_DYN_CODE(code))
        return NULL;

    if (ERROR_IS_CODE(code))
    {
        int errcode = ERROR_GET_INDEX(code);
        if (errcode < first_free_code)
            return user_code_msgs[errcode];
    }
    else
    {
        int errclass = ERROR_GET_CLASS(code);
        if (errclass < first_free_class)
            return user_class_msgs[errclass];
    }

    return NULL;
}


static MPI_RESULT MPIR_Dynerrcodes_finalize( void* /*p*/ )
{
    int i;

    if (not_initialized == 0)
    {

        for (i = 0; i < first_free_class; i++)
        {
            if (user_class_msgs[i])
            {
                MPIU_Free((char *) user_class_msgs[i]);
            }
        }

        for (i = 0; i < first_free_code; i++)
        {
            if (user_code_msgs[i])
            {
                MPIU_Free((char *) user_code_msgs[i]);
            }
        }
    }

    return MPI_SUCCESS;
}


void MPIAPI MPIR_Comm_errhandler_c_proxy(MPI_Comm_errhandler_fn* fn, MPI_Comm* comm, int* errcode, ...)
{
    /* Pass a final null pointer arg to these routines as MPICH-1 expected that */
    fn(comm, errcode, 0);
}


void MPIAPI MPIR_Win_errhandler_c_proxy(MPI_Win_errhandler_fn* fn, MPI_Win* win, int* errcode, ...)
{
    /* Pass a final null pointer arg to these routines as MPICH-1 expected that */
    fn(win, errcode, 0);
}


void MPIAPI MPIR_File_errhandler_c_proxy(MPI_File_errhandler_fn* fn, MPI_File* file, int* errcode, ...)
{
    /* Pass a final null pointer arg to these routines as MPICH-1 expected that */
    fn(file, errcode, 0);
}


_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIO_Err_return_file(MPI_File fh, int mpi_errno)
{
    MPI_Errhandler e;

    /* If the file pointer is not valid, we use the handler on
       MPI_FILE_NULL (MPI-2, section 9.7).  For now, this code assumes that
       MPI_FILE_NULL has the default handler (return).  FIXME.  See
       below - the set error handler uses ADIOI_DFLT_ERR_HANDLER;
    */

    /* First, get the handler and the corresponding function */
    if (fh == MPI_FILE_NULL)
    {
        e = ADIOI_DFLT_ERR_HANDLER;
    }
    else
    {
        e = fh->err_handler;
    }

    return MPIR_Err_return_file_helper(e, fh, mpi_errno);
}
