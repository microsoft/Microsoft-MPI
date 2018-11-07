// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


//
// IMPORTANT NOTE: All MPI*_Err_* functions must have return value and function name
// on the SAME LINE or the error string parser will FAIL.
//

#ifndef MPIERROR_H_INCLUDED
#define MPIERROR_H_INCLUDED

/* Error severity */
#define MPIR_ERR_FATAL 1
#define MPIR_ERR_RECOVERABLE 0

/*
    This file contains the definitions of the error code fields

    An error code is organized as:

     3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
     1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    +-+-+-----------------------------------------+-+-+-------------+
    |0|D|             Error Code Index            |C|F|    Class    |
    +-+-+-----------------------------------------+-+-+-------------+

    Class: [0-6]
    The MPI error class (including dynamically defined classes).

    Fatal: [7]
    Set if the error is fatal and should not be returned to the user.

    Code: [8]
    Set for error Codes (vs error class). Note that 0 is a valid value for the Error Code Index.

    Index: [9-28]
    The Error Code Index assigned at error code creation. The lower 7 bits are used as an index to
    the ErrorRing to find the error message. The rest of this field bits help verifing that the
    ErrorRing entry is assinged to that error.

    Dyanmic: [30]
    Set if this is a dynamically created error code (using the routines to add error classes and
    codes at runtime).  This *must* be the top bit so that MPI_ERR_LASTCODE and MPI_LASTUSEDCODE
    can be set properly.  (MPI_ERR_LASTCODE must be the largest valid error *code* from the
    predefined codes.
    The standard text is poorly worded here, but users will expect to be able to perform
    (errcode <= MPI_ERR_LASTCODE). See Section 8.5 in the MPI-2 standard for MPI_LASTUSEDCODE.

    0: [31]
    Error codes must be positive integers, so we lose one bit (if they aren't positive, the
    comparisons agains MPI_ERR_LASTCODE and the value of the attribute MPI_LASTUSEDCODE will fail).
 */


#define ERROR_CLASS_MASK        0x0000007f
#define ERROR_CLASS_SIZE        128

#define ERROR_FATAL_FLAG        0x00000080
#define ERROR_COD_FLAG          0x00000100

#define ERROR_INDEX_MASK        0x3FFFFE00
#define ERROR_INDEX_SHIFT       9

#define ERROR_DYN_FLAG          0x40000000

#define ERROR_DINDEX_MASK       0x003FFE00
#define ERROR_DINDEX_SHIFT      9
#define ERROR_DINDEX_SIZE       8192

/* shorthand macros */
#define ERROR_GET_CLASS(code)   (code & ERROR_CLASS_MASK)
#define ERROR_GET_INDEX(code)   ((code & ERROR_INDEX_MASK) >> ERROR_INDEX_SHIFT)

#define ERROR_IS_FATAL(code)    ((code & ERROR_FATAL_FLAG) != 0)
#define ERROR_IS_CODE(code)     ((code & ERROR_COD_FLAG) != 0)
#define ERROR_IS_DYN(code)      ((code & ERROR_DYN_FLAG) != 0)


/* FIXME:
 * The following description is out of date and should not be used
 */
/*@
  MPIR_Err_create_code - Create an error code and associated message
  to report an error

  Input Parameters:
+ lastcode - Previous error code (see notes)
. severity  - Indicates severity of error
. class - Error class
. instance_msg - A message containing printf-style formatting commands
  that, when combined with the instance_parameters, specify an error
  message containing instance-specific data.
- instance_parameters - The remaining parameters.  These must match
 the formatting commands in 'instance_msg'.

 Notes:
 A typical use is:
.vb
   mpi_errno = MPIR_Err_create_code( mpi_errno, MPIR_ERR_RECOVERABLE, MPI_ERR_RANK, "Invalid rank %d", rank );
.ve

  Predefined message may also be used.  Any message that uses the
  prefix '"**"' will be looked up in a table.  This allows standardized
  messages to be used for a message that is used in several different locations
  in the code.  For example, the name '"**rank"' might be used instead of
  '"Invalid Rank"'; this would also allow the message to be made more
  specific and useful, such as
.vb
   Invalid rank provided.  The rank must be between 0 and the 1 less than
   the size of the communicator in this call.
.ve
  This interface is compatible with the 'gettext' interface for
  internationalization, in the sense that the 'generic_msg' and 'instance_msg'
  may be used as arguments to 'gettext' to return a string in the appropriate
  language; the implementation of 'MPID_Err_create_code' can then convert
  this text into the appropriate code value.

  The current set of formatting commands is undocumented and will change.
  You may safely use '%d' and '%s' (though only use '%s' for names of
  objects, not text messages, as using '%s' for a message breaks support for
  internationalization.

  This interface allows error messages to be chained together.  The first
  argument is the last error code; if there is no previous error code,
  use 'MPI_SUCCESS'.

  Module:
  Error

  @*/
_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT MPIR_Err_create_code(
    _In_ int lastcode,
    _In_ int fatal,
    _In_ int error_class,
    _In_z_ const char specific_msg[],
    ...
    );

_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT MPIR_Err_create_code_valist(
    _In_ int lastcode,
    _In_ int fatal,
    _In_ int error_class,
    _In_z_ const char specific_msg[],
    _In_ va_list Argp
    );

void MPIR_Err_preOrPostInit( void );


/*@
  MPID_Err_get_string - Get the message string that corresponds to an error
  class or code

  Input Parameter:
+ code - An error class or code.  If a code, it must have been created by
  'MPID_Err_create_code'.
- msg_len - Length of 'msg'.

  Output Parameter:
. msg - A null-terminated text string of length (including the null) of no
  more than 'msg_len'.

  Return value:
  Zero on success.  Non-zero returns indicate either (a) 'msg_len' is too
  small for the message or (b) the value of 'code' is neither a valid
  error class or code.

  Notes:
  This routine is used to implement 'MPI_ERROR_STRING'.

  Module:
  Error

  Question:
  What values should be used for the error returns?  Should they be
  valid error codes?

  How do we get a good value for 'MPI_MAX_ERROR_STRING' for 'mpi.h'?
  See 'errgetmsg' for one idea.

  @*/

void MPIR_Err_get_string(
    _In_ int errorcode,
    _Out_writes_z_(length) char * msg,
    _In_ size_t length
    );


/* Prototypes for internal routines for the errhandling module */
MPI_RESULT
MPIR_Err_set_msg(
    _In_ int code,
    _In_z_ const char * msg_string
    );

int MPIR_Err_add_class( void );
int MPIR_Err_add_code( int );

_Success_(return==MPI_SUCCESS)
int MPIR_Err_vsnprintf_mpi(
    _Out_writes_z_(maxlen) char* str,
    _In_ size_t maxlen,
    _Printf_format_string_ const char* fmt,
    _In_ va_list list
    );

_Post_satisfies_( return != MPI_SUCCESS )
int MPIR_Err_get_user_error_code(
    _In_ int errcode
    );

typedef _Ret_z_ const char* (*MPIR_Err_to_string_fn)(int code);

void MPIR_Err_set_dynerr_fn(
    _In_ MPIR_Err_to_string_fn fn
    );



/*
 *  Standardized error checking macros.  These provide the correct tests for
 *  common tests.  These set err with the encoded error value.
 */

/* The following are placeholders.  We haven't decided yet whether these
   should take a handle or pointer, or if they should take a handle and return
   a pointer if the handle is valid.  These need to be rationalized with the
   MPID_xxx_valid_ptr and MPID_xxx_get_ptr.

*/


#define MPIU_ERR_FAIL(err_) \
    err_

#define ON_ERROR_FAIL(err_) \
    if((err_) != MPI_SUCCESS) { goto fn_fail; }

#define MPIU_ERR_NOMEM() \
    MPIU_ERR_CREATE(MPI_ERR_OTHER, "**nomem")

/*
 * Standardized error setting and checking macros
 * These are intended to simplify the insertion of standardized error
 * checks
 *
 */
/* --BEGIN ERROR MACROS-- */

/* If you add any macros to this list, make sure that you update
 maint/extracterrmsgs to handle the additional macros (see the hash
 KnownErrRoutines in that script) */
#define MPIU_ERR_TYPE_GET(err_, fatal_, class_, fmt_, ...) \
    MPIR_Err_create_code(err_, fatal_, class_, fmt_, __VA_ARGS__)

/* Get fatal error code */
#define MPIU_ERR_FATAL_GET(err_, class_, fmt_, ...) \
    MPIU_ERR_TYPE_GET(err_, MPIR_ERR_FATAL, class_, fmt_, __VA_ARGS__)

/* Get recoverable error code */
#define MPIU_ERR_GET(err_, fmt_, ...) \
    MPIU_ERR_TYPE_GET(*&err_, MPIR_ERR_RECOVERABLE, MPI_ERR_OTHER, fmt_, __VA_ARGS__)

/* Get recov error code with class */
#define MPIU_ERR_CLASS_GET(err_, class_, fmt_, ...) \
    MPIU_ERR_TYPE_GET(*&err_, MPIR_ERR_RECOVERABLE, class_, fmt_, __VA_ARGS__)

/* Create a new recoverable error code */
#define MPIU_ERR_CREATE(class_, fmt_, ...) \
    MPIU_ERR_TYPE_GET( MPI_SUCCESS, MPIR_ERR_RECOVERABLE, class_, fmt_, __VA_ARGS__ )

/* --END ERROR MACROS-- */

_Ret_z_
const char*
get_error_string(
    _In_ int error
    );

void CreateDumpFileIfConfigured(
    _In_ EXCEPTION_POINTERS* exp
    );

#endif
