// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#if !defined(MPIUTIL_H_INCLUDED)
#define MPIUTIL_H_INCLUDED
#include <oacr.h>
#include <strsafe.h>

/*
 * Debuging function and macros
 */
void MPIU_dbg_preinit(void);

_Success_(return==MPI_SUCCESS)
int MPIU_dbg_init(
    _In_ unsigned int rank,
    _In_ unsigned int world_size
    );

void
MPIU_dbg_printf(
    _Printf_format_string_ const char *str,
    ...
    );


_Success_(return==MPI_SUCCESS)
int
MPIU_Parse_rank_range(
    _In_                  unsigned int  rank,
    _In_z_                const char*   range,
    _In_                  unsigned int  world_size,
    _Out_                 bool*         isWithinRange,
    _Out_writes_(world_size) unsigned int* total_unique_ranks
    );

struct MPID_Comm;


/*@
  MPID_Abort - Abort at least the processes in the specified communicator.

  Input Parameters:
+ comm        - Communicator of processes to abort
. intern      - indicates if the abort is internal or by the application.
. exit_code   - Exit code to return to the calling environment.  See notes.
- error_msg   - error message (not optional)

  Return value:
  'MPI_SUCCESS' or an MPI error code.  Normally, this routine should not
  return, since the calling process must be a member of the communicator.
  However, under some circumstances, the 'MPID_Abort' might fail; in this
  case, returning an error indication is appropriate.

  Notes:

  In a fault-tolerant MPI implementation, this operation should abort `only`
  the processes in the specified communicator.  Any communicator that shares
  processes with the aborted communicator becomes invalid.  For more
  details, see (paper not yet written on fault-tolerant MPI).

  In particular, if the communicator is 'MPI_COMM_SELF', only the calling
  process should be aborted.

  The 'exit_code' is the exit code that this particular process will
  attempt to provide to the 'mpiexec' or other program invocation
  environment.  See 'mpiexec' for a discussion of how exit codes from
  many processes may be combined.

  If the error_msg field is non-nullptr this string will be used as the message
  with the abort output.  Otherwise, the output message will be base on the
  error message associated with the mpi_errno.

  An external agent that is aborting processes can invoke this with either
  'MPI_COMM_WORLD' or 'MPI_COMM_SELF'.  For example, if the process manager
  wishes to abort a group of processes, it should cause 'MPID_Abort' to
  be invoked with 'MPI_COMM_SELF' on each process in the group.

  Question:
  An alternative design is to provide an 'MPID_Group' instead of a
  communicator.  This would allow a process manager to ask the ADI
  to kill an entire group of processes without needing a communicator.
  However, the implementation of 'MPID_Abort' will either do this by
  communicating with other processes or by requesting the process manager
  to kill the processes.  That brings up this question: should
  'MPID_Abort' use 'PMI' to kill processes?  Should it be required to
  notify the process manager?  What about persistent resources (such
  as SYSV segments or forked processes)?

  This suggests that for any persistent resource, an exit handler be
  defined.  These would be executed by 'MPID_Abort' or 'MPID_Finalize'.
  See the implementation of 'MPI_Finalize' for an example of exit callbacks.
  In addition, code that registered persistent resources could use persistent
  storage (i.e., a file) to record that information, allowing cleanup
  utilities (such as 'mpiexec') to remove any resources left after the
  process exits.

  'MPI_Finalize' requires that attributes on 'MPI_COMM_SELF' be deleted
  before anything else happens; this allows libraries to attach end-of-job
  actions to 'MPI_Finalize'.  It is valuable to have a similar
  capability on 'MPI_Abort', with the caveat that 'MPI_Abort' may not
  guarantee that the run-on-abort routines were called.  This provides a
  consistent way for the MPICH implementation to handle freeing any
  persistent resources.  However, such callbacks must be limited since
  communication may not be possible once 'MPI_Abort' is called.  Further,
  any callbacks must guarantee that they have finite termination.

  One possible extension would be to allow `users` to add actions to be
  run when 'MPI_Abort' is called, perhaps through a special attribute value
  applied to 'MPI_COMM_SELF'.  Note that is is incorrect to call the delete
  functions for the normal attributes on 'MPI_COMM_SELF' because MPI
  only specifies that those are run on 'MPI_Finalize' (i.e., normal
  termination).

  Module:
  MPID_CORE
  @*/
_Analysis_noreturn_
DECLSPEC_NORETURN
int MPID_Abort(
    _Inout_opt_ MPID_Comm* comm,
    _In_ BOOL intern,
    _In_ int exit_code,
    _In_z_ const char* error_msg
    );

_Success_(return>=0)
int
MPIU_Internal_error_printf(
    _Printf_format_string_ const char *str,
    ...
    );

_Success_(return>=0)
int
MPIU_Error_printf(
    _Printf_format_string_ const char *str,
    ...
    );


static inline void MPIU_Debug_break(void)
{
    //
    // Debug break without giving a chance to any exception handler to ignore the break
    //
    __try
    {
        __debugbreak();
    }
    __except(UnhandledExceptionFilter(GetExceptionInformation()))
    OACR_WARNING_SUPPRESS(EXCEPT_BLOCK_EMPTY,"lucasm: debug break handler")
    {
    }
}


#define ASSERT(a_) MPIU_Assert(a_)
#define VERIFY(a_) MPIU_Assertp(a_)

/*
 * MPIU_Assert()
 *
 * Similar to assert() except that it performs an MPID_Abort() when the
 * assertion fails.  Also, for Windows, it doesn't popup a
 * mesage box on a remote machine.
 *
 * MPIU_AssertDecl may be used to include declarations only needed
 * when MPIU_Assert is non-null (e.g., when assertions are enabled)
 */

#if DBG

#define MPIU_DebugBuildCode(a_) a_
#define MPIU_Assert(a_) \
    (void) ((!!(a_)) || \
            (MPIU_Internal_error_printf("Assertion failed in %s(%d): %s\n", __FILE__, __LINE__, #a_), 0) || \
            (MPIU_Debug_break(), 0) || \
            (MPID_Abort(nullptr, TRUE, 0, "assertion failed")) \
            ); __analysis_assume(a_)

#define MPIU_Assertp(a_) MPIU_Assert(a_)

#else

#define MPIU_Assert(a_) __analysis_assume(a_)
#define MPIU_DebugBuildCode(a_)

/*
 * MPIU_Assertp()
 *
 * Similar to MPIU_Assert() except that these assertions persist regardless of
 * DBG.  MPIU_Assertp() may be used for error checking in prototype code, although
 * it should be converted real error checking and reporting once the prototype
 * becomes part of the official and supported code base.
 */
#define MPIU_Assertp(a_) \
    (void) ((!!(a_)) || \
            (MPIU_Internal_error_printf("Assertion failed in %s(%d): %s\n", __FILE__, __LINE__, #a_), 0) || \
            (MPID_Abort(nullptr, TRUE, 0, "assertion failed")) \
           ); __analysis_assume(a_)

#endif


/*@ env_is_on - Check if an environment variable is in the 'on' state

    Return value:
    'def' if the env var is not set
    1 if the env var is set to 'on'
    0 if the env var is set not to to 'on'

 @*/
BOOL
env_is_on_ex(
    _In_z_ const wchar_t* name,
    _In_opt_z_ const wchar_t* deprecatedName,
    _In_ BOOL defval
    );


int
env_to_int_ex(
    _In_z_ const wchar_t* name,
    _In_opt_z_ const wchar_t* deprecatedName,
    _In_ int defval,
    _In_ int minval
    );


_Success_(return == TRUE)
BOOL
env_to_range_ex(
    _In_z_ const wchar_t* name,
    _In_opt_z_ const wchar_t* deprecatedName,
    _In_ int minval,
    _In_ int maxval,
    _In_ bool allowSingleValue,
    _Out_ int* low,
    _Out_ int* high
    );


inline BOOL
env_is_on(
    _In_z_ const wchar_t* name,
    _In_ BOOL defval
    )
{
    return env_is_on_ex(name, nullptr, defval);
}


inline int
env_to_int(
    _In_z_ const wchar_t *name,
    _In_ int defval,
    _In_ int minval
    )
{
    return env_to_int_ex(name, nullptr, defval, minval);
}


_Success_(return == TRUE)
inline BOOL
env_to_range(
    _In_z_ const wchar_t* name,
    _In_ int minval,
    _In_ int maxval,
    _In_ bool allowSingleValue,
    _Out_ int* low,
    _Out_ int* high
    )
{
    return env_to_range_ex( name, nullptr, minval, maxval, allowSingleValue, low, high );
}


_Success_(return == NO_ERROR)
DWORD
MPIU_Getenv(
    _In_z_                 PCSTR name,
    _Out_writes_z_(cchBuffer)PSTR  buffer,
    _In_                   DWORD cchBuffer
    );


_Success_(return == NO_ERROR)
DWORD
MPIU_Getenv(
    _In_z_                 PCWSTR name,
    _Out_writes_z_(cchBuffer)PWSTR  buffer,
    _In_                   DWORD  cchBuffer
    );


//
// Max string representation of GUID with hyphens, no braces
//
#define GUID_STRING_LENGTH 36

static inline void
GuidToStr(
    _In_ const GUID& guid,
    _Out_writes_z_(cchBuffer) char* buffer,
    _In_range_(>,GUID_STRING_LENGTH) size_t cchBuffer
    )
{
    MPIU_Assert( cchBuffer > GUID_STRING_LENGTH );

    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return value, we have an existing assert for buffer size");
    OACR_WARNING_SUPPRESS(USE_WIDE_API, "MS MPI uses ANSI character set.");
    (void) StringCchPrintfA(
        buffer,
        cchBuffer,
        "%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
        );
}


static inline void
GuidToStr(
    _In_ const GUID& guid,
    _Out_writes_z_(cchBuffer) wchar_t* buffer,
    _In_range_(>,GUID_STRING_LENGTH) size_t cchBuffer
    )
{
    MPIU_Assert( cchBuffer > GUID_STRING_LENGTH );

    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return value, we have an existing assert for buffer size");
    (void) StringCchPrintfW(
        buffer,
        cchBuffer,
        L"%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
        );
}


//
// Summary: Given a starting port, find an open TCP port that can be
// used for listening
//
int
FindNextOpenPort(
    int startPort
    );

#endif /* !defined(MPIUTIL_H_INCLUDED) */
