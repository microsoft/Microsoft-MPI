// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"

/* defmsg is generated automatically from the source files and contains
   all of the error messages */
#include "defmsg.h"

#include "mpidump.h"
#include <excpt.h>
#include <evntprov.h>


/*
 * Instance-specific error messages are stored in a ring.  The elements of this
 * ring are MPIR_Err_msg_t structures, which contain the following fields:
 *    id - this is used to check that the entry is valid; it is computed from
 *         the error code and location in the ring.  The routine
 *           ErrcodeToId( errcode, &id ) is used to extract the id from an
 *         error code and
 *           ErrcodeCreateID( class, generic, msg, &id, &seq ) is used to
 *         create the id from an error class, generic index, and message
 *         string.  The "seq" field is inserted into the error code as a
 *         check.
 *
 *    prev_error - The full MPI error code of the previous error attached
 *         to this list of errors, or MPI_SUCCESSS (which has value 0).
 *         This is the last error code, not the index in the ring of the last
 *         error code.  That's the right choice, because we want to ensure
 *         that the value is valid if the ring overflows.  In addition,
 *         we allow this to be an error CLASS (one of the predefined MPI
 *         error classes).  This is particularly important for
 *         MPI_ERR_IN_STATUS, which may be returned as a valid error code.
 *         (classes are valid error codes).
 *
 *    use_user_error_code and user_error_code - Used to handle a few cases
 *         in MPI where a user-provided routine returns an error code;
 *         this allows us to provide information about the chain of
 *         routines that were involved, while returning the users prefered
 *         error value to the users environment.
 *
 *    location - A string that indicates what function and line number
 *         where the error code was set.
 *
 *    msg - A message about the error.  This may be instance-specific (e.g.,
 *         it may have been created at the time the error was detected with
 *         information about the parameters that caused the error).
 *
 * Note that both location and msg are defined as length MAX_xxx+1.  This
 * isn't really necessary (at least for msg), since the MPI standard
 * requires that MAX_MPI_ERROR_STRING include the space for the trailing null,
 * but using the extra byte makes the code a little simpler.
 *
 * The "id" value is used to keep a sort of "checkvalue" to ensure that the

 * error code that points at this message is in fact for this particular
 * message.  This is used to handle the unlikely but possible situation where
 * so many error messages are generated that the ring is overlapped.
 *
 * The message arrays are preallocated to ensure that there is space for these
 * messages when an error occurs.  One variation would be to allow these
 * to be dynamically allocated, but it is probably better to either preallocate
 * these or turn off all error message generation (which will eliminate these
 * arrays).
 *
 * One possible alternative is to use the message ring *only* for instance
 * messages and use the predefined messages in-place for the generic
 * messages.  This approach is used to provide uniform handling of all
 * error messages.
 */

#define MAX_LOCATION_LEN 63

/* The maximum error string in this case may be a multi-line message,
   constructed from multiple entries in the error message ring.  The
   individual ring messages should be shorter than MPI_MAX_ERROR_STRING,
   perhaps as small a 256. We define a separate value for the error lines.
 */
#define MPIR_MAX_ERROR_LINE 512

/* See the description above for the fields in this structure */
struct MPIR_Err_msg_t
{
    int  id;
    int  prev_error;

    int use_user_error_code;
    int user_error_code;

    char location[MAX_LOCATION_LEN+1];
    char msg[MPIR_MAX_ERROR_LINE+1];
};

static MPIR_Err_msg_t ErrorRing[128];
static volatile long error_ring_loc = 0;

#define GET_RING_INDEX(code) \
    (ERROR_GET_INDEX(code) % _countof(ErrorRing))

static MPIR_Err_to_string_fn MPIR_Err_code_to_string;

void
MPIR_Err_set_dynerr_fn(
    _In_ MPIR_Err_to_string_fn fn
    )
{
    MPIU_Assert(MPIR_Err_code_to_string == nullptr);
    MPIR_Err_code_to_string = fn;
}


/* ------------------------------------------------------------------------- */
/* The following block of code manages the instance-specific error messages  */
/* ------------------------------------------------------------------------- */

#if DBG

static int
ErrcodeIsValid(
    _In_ int errcode
    )
{
    int idx;

    /* If the errcode is a class, then it is valid */
    if (errcode >= 0 && errcode <= MPICH_ERR_LAST_CLASS)
        return TRUE;

    /* check for extra bits set. note: dynamic error codes are not valid here */
    if((errcode & ~(ERROR_CLASS_MASK | ERROR_INDEX_MASK | ERROR_FATAL_FLAG)) != ERROR_COD_FLAG)
        return FALSE;

    idx = GET_RING_INDEX(errcode);
    if (ErrorRing[idx].id != errcode)
        return FALSE;

    return TRUE;
}

#endif // DBG


_Post_satisfies_( return != MPI_SUCCESS )
int
MPIR_Err_get_user_error_code(
    _In_ int errcode
    )
{
    int idx;

    /* check for class only error code */
    if(!ERROR_IS_CODE(errcode))
        return errcode;

    /* Can we get a more specific error message */
    idx = GET_RING_INDEX(errcode);
    if (ErrorRing[idx].id == errcode && ErrorRing[idx].use_user_error_code)
        return ErrorRing[idx].user_error_code;

    return errcode;
}


/*
 * Given a message string abbreviation (e.g., one that starts "**"),
 * return the corresponding index.
 */
static int __cdecl
compare_key_map(
    _In_ const void* pv1,
    _In_ const void* pv2
    )
{
    const char* key = (const char*)pv1;
    const msgpair* pair = (const msgpair*)pv2;
    return strcmp( key, pair->key );
} 

static int
FindMessageIndex(
    _In_z_ const char *msg
    )
{
    const void* p;
    p = bsearch(
            msg,
            errors_map,
            _countof(errors_map),
            sizeof(errors_map[0]),
            compare_key_map
            );

    MPIU_Assert(p != nullptr);

    return (int)((const msgpair*)p - errors_map);
}


/* ------------------------------------------------------------------------ */
/* The following routines create an MPI error code, handling optional,      */
/* instance-specific error message information.  There are two key routines:*/
/*    MPIR_Err_create_code - Create the error code; this is the routine used*/
/*                           by most routines                               */
/*    MPIR_Err_create_code_valist - Create the error code; accept a valist  */
/*                           instead of a variable argument list (this is   */
/*                           used to allow this routine to be used from     */
/*                           within another varargs routine)                */
/* ------------------------------------------------------------------------ */
/* --BEGIN ERROR MACROS-- */

_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIR_Err_create_code(
    _In_ int lastcode,
    _In_ int fatal,
    _In_ int error_class,
    _Printf_format_string_ const char specific_msg[],
    ...
    )
{
    int rc;
    va_list Argp;
    va_start(Argp, specific_msg);
    __analysis_assert(error_class != 0);
    rc = MPIR_Err_create_code_valist(
            lastcode,
            fatal,
            error_class,
            specific_msg,
            Argp
            );

    __analysis_assume(rc != 0);
    va_end(Argp);
    return rc;
}

/* --END ERROR MACROS-- */

static int
TakeDump(
    _In_ EXCEPTION_POINTERS* exp,
    _In_ int dumpMode
    )
{
    MINIDUMP_EXCEPTION_INFORMATION exrParam;
    exrParam.ExceptionPointers = exp;
    exrParam.ThreadId = GetCurrentThreadId();
    exrParam.ClientPointers = FALSE;

    MINIDUMP_TYPE dumpType;
    if( dumpMode >= MsmpiDumpFull  )
    {
        dumpType = MiniDumpWithFullMemory;
    }
    else
    {
        dumpType = MiniDumpNormal;
    }

    wchar_t dumpPath[MAX_PATH];
    DWORD err = MPIU_Getenv( L"MSMPI_DUMP_PATH",
                             dumpPath,
                             _countof( dumpPath ) );
    if( err != NOERROR )
    {
        dumpPath[0] = '\0';
    }

    HANDLE tempDumpFile = CreateTempDumpFile(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        dumpType,
        dumpPath,
        &exrParam
        );

    if( tempDumpFile != INVALID_HANDLE_VALUE )
    {
        CreateFinalDumpFile(
            tempDumpFile,
            env_to_int( L"PMI_RANK", -1, -1 ),
            dumpPath,
            env_to_int( L"CCP_JOBID", 0, 0 ),
            env_to_int( L"CCP_TASKID", 0, 0 ),
            env_to_int( L"CCP_TASKINSTANCEID", 0, 0 )
            );
        CloseHandle( tempDumpFile );
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}


void CreateDumpFileIfConfigured(
    _In_ EXCEPTION_POINTERS* exp
    )
{
    enum MSMPI_DUMP_MODE dumpMode = GetDumpMode();
    if (dumpMode != MsmpiDumpNone)
    {
        TakeDump(exp, dumpMode);
    }
}


/*
 * This is the real routine for generating an error code.  It takes
 * a va_list so that it can be called by any routine that accepts a
 * variable number of arguments.
 */
_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIR_Err_create_code_valist(
    _In_ int lastcode,
    _In_ int fatal,
    _In_ int error_class,
    _Printf_format_string_ const char specific_msg[],
    _In_ va_list Argp
    )
{
    int user_error_code = -1;
    int specific_idx;
    int ring_idx;
    long ring_idx_base;
    const char* specific_fmt;
    char* ring_msg;

    /* Check that lastcode is valid */
#if DBG
    MPIU_Assert(ErrcodeIsValid(lastcode));
#endif //DBG
    MPIU_Assert(specific_msg != nullptr);

    if( IsDebuggerPresent() )
    {
        DebugBreak();
    }

    enum MSMPI_DUMP_MODE dumpMode = GetDumpMode();
    if( dumpMode != MsmpiDumpNone && lastcode == MPI_SUCCESS )
    {
        __try
        {
            RaiseException( 0, 0, 0, nullptr );
        }
        __except( TakeDump( GetExceptionInformation(), dumpMode ) )
        OACR_WARNING_SUPPRESS(EXCEPT_BLOCK_EMPTY,"mpicr: dump handler")
        {
        }
    }

    if (error_class == MPI_ERR_OTHER)
    {
        if (ERROR_GET_CLASS(lastcode) != MPI_SUCCESS)
        {
            /* If the last class is more specific (and is valid), then pass it through */
            error_class = ERROR_GET_CLASS(lastcode);
        }
    }

    /* Handle special case of MPI_ERR_IN_STATUS.  According to the standard,
       the code must be equal to the class. See section 3.7.5.
       Information on the particular error is in the MPI_ERROR field
       of the status. */
    if (error_class == MPI_ERR_IN_STATUS)
        return MPI_ERR_IN_STATUS;

    ring_idx_base = ::InterlockedIncrement(&error_ring_loc);

    /* Get the next entry in the ring */
    ring_idx = ring_idx_base % _countof(ErrorRing);

    ring_msg = ErrorRing[ring_idx].msg;

    specific_idx = FindMessageIndex(specific_msg);
    specific_fmt = errors_map[specific_idx].fmt;

    //
    // CompareString does not check for null terminating character
    // when length of string is given explicitly
    //
    int len = _countof("**user") - 1;
    if( MPIU_Strlen( errors_map[specific_idx].key ) >= static_cast<size_t>(len) &&
        CompareStringA( LOCALE_INVARIANT,
                        0,
                        errors_map[specific_idx].key,
                        len,
                        "**user",
                        len ) == CSTR_EQUAL )
    {
        /* This is a special case.  The format is ..., "**userxxx %d", intval);
           In this case we must save the user value because we store it explicitly in the ring.
           We do this here because we cannot both access the user error code and pass the argp
           to vsnprintf_mpi. */
        user_error_code = va_arg(Argp,int);
        ErrorRing[ring_idx].use_user_error_code = 1;
        ErrorRing[ring_idx].user_error_code = user_error_code;

        // errors_map is generated by a perl script but since its test code isn't compiled
        // there is no opportunity to do specifier to type checking. Ideally, testerr.c would
        // be included in the project build so oacr could potentially find mismatches
        OACR_WARNING_SUPPRESS(PRINTF_FORMAT_STRING_PARAM_NEEDS_REVIEW, "format is determined at runtime");
        MPIU_Snprintf( ring_msg, MPIR_MAX_ERROR_LINE, specific_fmt, user_error_code );
    }
    else
    {
        OACR_WARNING_SUPPRESS(PRINTF_FORMAT_STRING_PARAM_NEEDS_REVIEW, "format is determined at runtime");
        MPIR_Err_vsnprintf_mpi( ring_msg, MPIR_MAX_ERROR_LINE, specific_fmt, Argp );

        if (ERROR_IS_CODE(lastcode))
        {
            int last_ring_idx;

            last_ring_idx = GET_RING_INDEX(lastcode);
            if (ErrorRing[last_ring_idx].id == lastcode)
            {
                if (ErrorRing[last_ring_idx].use_user_error_code)
                {
                    ErrorRing[ring_idx].use_user_error_code = 1;
                    ErrorRing[ring_idx].user_error_code = ErrorRing[last_ring_idx].user_error_code;
                }
            }
        }
    }

    ring_msg[MPIR_MAX_ERROR_LINE] = '\0';

    /* Set the previous code. */
    ErrorRing[ring_idx].prev_error = lastcode;
    ErrorRing[ring_idx].location[0] = '\0';

    /* Make sure error_index doesn't get so large that it sets the dynamic bit. */
    int error_index = (ring_idx_base << ERROR_INDEX_SHIFT) & ERROR_INDEX_MASK;

    int err_code = error_class | ERROR_COD_FLAG | error_index;
    if (fatal || ERROR_IS_FATAL(lastcode))
    {
        err_code |= ERROR_FATAL_FLAG;
    }

    ErrorRing[ring_idx].id = err_code;
    __analysis_assume( err_code != MPI_SUCCESS );
    return err_code;
}


/*
 * Accessor routines for the predefined messages.  These can be
 * used by the other routines (such as MPI_Error_string) to
 * access the messages in this file, or the messages that may be
 * available through any message catalog facility
 */
C_ASSERT(_countof(class_to_index) == MPICH_ERR_LAST_CLASS + 1);

_Ret_z_
static const char*
get_class_msg(
    _In_ int error_class
    )
{
    if (error_class >= 0 && error_class < _countof(class_to_index))
    {
        return errors_map[class_to_index[error_class]].fmt;
    }
    else
    {
        return "Unknown error class";
    }
}

/* Given an error code, print the stack of messages corresponding to this
   error code. */
static void
MPIR_Err_print_stack_string(
    _In_ int errcode,
    _Out_writes_z_(maxlen) char *str,
    _In_ size_t maxlen
    )
{
    size_t len;
    const char *str_orig = str;
    size_t max_location_len = 0;
    int tmp_errcode = errcode;
    int error_class;

    /* make sure is not a dynamic error code or a simple error class */
    MPIU_Assert(!ERROR_IS_DYN(errcode));
    MPIU_Assert(ERROR_GET_CLASS(errcode) != errcode);
    MPIU_Assert(maxlen > 1);
    *str = '\0';


    /* Find the longest location string in the stack */
    while (ERROR_IS_CODE(tmp_errcode))
    {
        int ring_idx = GET_RING_INDEX(tmp_errcode);

        if (ErrorRing[ring_idx].id != tmp_errcode)
            break;

        len = MPIU_Strlen( ErrorRing[ring_idx].location,
                           _countof(ErrorRing[ring_idx].location) );
        max_location_len = max(max_location_len, len);
        tmp_errcode = ErrorRing[ring_idx].prev_error;
    }

    max_location_len += 2; /* add space for the ": " */

    /* print the error stack */
    while (ERROR_IS_CODE(errcode))
    {
        size_t nchrs;

        int ring_idx = GET_RING_INDEX(errcode);

        if (ErrorRing[ring_idx].id != errcode)
            break;

        len = MPIU_Snprintf(str, maxlen, "%s", ErrorRing[ring_idx].location);
        maxlen -= len;
        str += len;

        nchrs = max_location_len -
            MPIU_Strlen( ErrorRing[ring_idx].location,
                         _countof(ErrorRing[ring_idx].location) ) - 2;
        while (nchrs > 0 && maxlen > 0)
        {
            *str++ = '.';
            nchrs--;
            maxlen--;
        }

        len = MPIU_Snprintf(str, maxlen, "%s\n", ErrorRing[ring_idx].msg);
        maxlen -= len;
        str += len;

        errcode = ErrorRing[ring_idx].prev_error;
    }

    /* FIXME: This is wrong.  The only way that you can get here without
       errcode beign MPI_SUCCESS is if there is an error in the
       processing of the error codes.  Dropping through into the next
       level of code (particularly when that code doesn't check for
       valid error codes!) is erroneous */
    if (errcode == MPI_SUCCESS)
    {
        goto fn_exit;
    }

    error_class = ERROR_GET_CLASS(errcode);

    if (error_class <= MPICH_ERR_LAST_CLASS)
    {
        len = MPIU_Snprintf(str, maxlen, "(unknown)(): %s\n", get_class_msg(error_class));
        maxlen -= len;
        str += len;
    }
    else
    {
        len = MPIU_Snprintf(str, maxlen,
                            "Error code contains an invalid class (%d)\n",
                            error_class);
        maxlen -= len;
        str += len;
    }

fn_exit:
    if (str_orig != str)
    {
        str--;
        *str = '\0'; /* erase the last \n */
    }
}


void
MPIR_Err_get_string(
    _In_ int errorcode,
    _Out_writes_z_(length) char * msg,
    _In_ size_t length
    )
{
    size_t len;
    size_t num_remaining = length;

    MPIU_Assert(num_remaining > 0);

    /* Convert the code to a string.  The cases are:
       simple class.  Find the corresponding string.
       <not done>
       if (user code)
       {
           go to code that extracts user error messages
       }
       else
       {
           is specific message code set and available?  if so, use it
           else use generic code (lookup index in table of messages)
       }
     */
    if (ERROR_IS_DYN(errorcode))
    {
        /* This is a dynamically created error code (e.g., with MPI_Err_add_class) */

        /* If a dynamic error code was created, the function to convert
           them into strings has been set.  Check to see that it was; this
           is a safeguard against a bogus error code */
        const char* s = nullptr;
        if (MPIR_Err_code_to_string)
        {
            /* FIXME: not internationalized */
            s = MPIR_Err_code_to_string(errorcode);
            if(s != nullptr)
            {
                MPIU_Strncpy(msg, s, num_remaining);
            }
        }

        if(s == nullptr)
        {
            len = MPIU_Snprintf(msg, num_remaining, "Undefined dynamic error code (%d)", errorcode);
            msg[num_remaining-1] = '\0';
        }
    }
    else if (ERROR_GET_CLASS(errorcode) == errorcode)
    {
        MPIU_Strncpy(msg, get_class_msg( errorcode ), num_remaining);
    }
    else
    {
        /* print the class message first */
        MPIU_Strncpy(msg, get_class_msg(ERROR_GET_CLASS(errorcode)), num_remaining);

        msg[num_remaining - 1] = '\0';
        len = MPIU_Strlen( msg, num_remaining );
        msg += len;
        num_remaining -= len;

        /* then print the stack or the last specific error message */
        MPIU_Strncpy(msg, ", error stack:\n", num_remaining);
        len = MPIU_Strlen( msg, num_remaining );
        msg += len;
        num_remaining -= len;
        MPIR_Err_print_stack_string(errorcode, msg, num_remaining);
    }
}

_Ret_z_
const char*
get_error_string(
    _In_ int error
    )
{
    wchar_t* wmsg;
    static char msg[1024];

    int n;
    OACR_REVIEWED_CALL(
        mpicr,
        n = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS |
        FORMAT_MESSAGE_MAX_WIDTH_MASK,  // dwFlags
        nullptr,                        // lpSource
        error,                          // dwMessageId,
        0,                              // dwLanguageId
        reinterpret_cast<LPWSTR>(&wmsg),// lpBuffer
        0,                              // nSize
        nullptr ));                     // Arguments

    if( n != 0 )
    {
        n = WideCharToMultiByte(
            CP_UTF8,
            0,
            wmsg,
            -1,
            msg,
            sizeof(msg),
            nullptr,
            nullptr
            );
        LocalFree( wmsg );
    }
    if( n == 0 )
    {
        msg[0] = '\0';
    }

    return msg;
}


//
// Summary:
//  Traces the MPI Error string and error class for the specified
//  mpi error code.
//
ULONG MpiTraceError(
    REGHANDLE RegHandle,
    PCEVENT_DESCRIPTOR Descriptor,
    int ErrorCode
    )
{
    const ULONG EventDataCount = 2;
    EVENT_DATA_DESCRIPTOR EventData[ EventDataCount ];
    char message[ MPI_MAX_ERROR_STRING ];
    int error_class;

    //make sure the string starts null terminated.
    message[0] = 0;

    //
    // Get the error class
    //
    error_class = ERROR_GET_CLASS(ErrorCode);

    MPIR_Err_get_string( ErrorCode, message, MPI_MAX_ERROR_STRING);

    EventDataDescCreate(&EventData[0], &error_class, sizeof(error_class)  );
    EventDataDescCreate(&EventData[1], message, (ULONG)strlen(message) + sizeof('\0') );

    return EventWrite(RegHandle, Descriptor, EventDataCount, EventData);
}
