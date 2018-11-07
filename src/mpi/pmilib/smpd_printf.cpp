// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"
#include <process.h>

/* For MPIR_Err_get_string */
#include "mpierror.h"


int smpd_getpid()
{
    return GetCurrentProcessId();
}


void
smpd_translate_win_error(
    _In_ int error,
    _Out_writes_z_(maxlen) wchar_t* msg,
    _In_ int maxlen,
    _Printf_format_string_ PCWSTR prepend,
    ...
    )
{
    int n;

    if(prepend != NULL)
    {
        int len;
        va_list list;
        va_start(list, prepend);
        len = MPIU_Vsnprintf(msg, maxlen, prepend, list);
        ASSERT(len > 0);
        va_end(list);
        msg += len;
        maxlen -= len;
    }


    va_list list;
    va_start(list, prepend);
    OACR_REVIEWED_CALL(mpicr,
        n = FormatMessageW(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, // dwFlags
                NULL,   // lpSource
                error,  // dwMessageId,
                0,      // dwLanguageId
                msg,    // lpBuffer
                maxlen, // nSize
                &list    // Arguments
                ));

    va_end(list);
    /*
     * Make sure that the string is null terminated. The returned value is the
     * number of chars stored in the buffer (excluding the null terminating char).
     * If FomrmatMessage failes it returns 0 which is just fine.
     */
    msg[n] = L'\0';
}


const char* get_sock_error_string(int error)
{
    static char str[1024];
    str[0] = '\0';
    MPIR_Err_get_string(error, str, _countof(str));
    return str;
}


const wchar_t* smpd_get_context_str(const smpd_context_t* context)
{
    if(context == NULL)
        return L"null";

    switch (context->type)
    {
    case SMPD_CONTEXT_APP_STDIN:
        return L"stdin";
    case SMPD_CONTEXT_APP_STDOUT:
        return L"stdout";
    case SMPD_CONTEXT_APP_STDERR:
        return L"stderr";
    case SMPD_CONTEXT_LEFT_CHILD:
        return L"left child";
    case SMPD_CONTEXT_RIGHT_CHILD:
        return L"right child";
    case SMPD_CONTEXT_ROOT:
        return L"root";
    case SMPD_CONTEXT_MGR_PARENT:
        return L"parent";
    case SMPD_CONTEXT_PMI_CLIENT:
        return L"pmi client";
    }

    ASSERT(0);
    return L"unknown";
}


void smpd_flush_printf()
{
    fflush(stdout);
    fflush(stderr);
}


//
// Slightly under 16K to avoid OACR 6262 warning about large stack buffer
//
#define SMPD_MAX_PRINTF_BUFFER 16000

static int
smpd_dbg_printf(
    _In_ bool isErr,
    _In_ _Printf_format_string_ PCWSTR str,
    _In_ va_list args
    )
{
    if( (isErr && !(smpd_process.dbg_state & SMPD_DBG_STATE_ERROUT)) ||
        (!isErr && !(smpd_process.dbg_state & SMPD_DBG_STATE_STDOUT)) )
    {
        return 0;
    }

    int num_bytes = 0;
    wchar_t tmpBuffer[SMPD_MAX_PRINTF_BUFFER] = {0};

    if(smpd_process.dbg_state & SMPD_DBG_STATE_PREPEND_RANK)
    {
        //
        // Prepend output with the process tree node id
        //
        num_bytes = MPIU_Snprintf(
            tmpBuffer,
            _countof(tmpBuffer),
            L"[%02d:%d] ",
            smpd_process.tree_id,
            smpd_getpid() );
    }

    if( isErr )
    {
        num_bytes += MPIU_Snprintf(
            tmpBuffer + num_bytes,
            _countof(tmpBuffer) - num_bytes,
            L"ERROR: " );
    }

    num_bytes += MPIU_Vsnprintf(
        tmpBuffer + num_bytes,
        _countof(tmpBuffer) - num_bytes,
        str,
        args );

    bool writeToConsole = true;
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hStdErr == INVALID_HANDLE_VALUE)
    {
        writeToConsole = false;
    }
    else
    {
        DWORD mode;
        BOOL fSucc = GetConsoleMode( hStdErr, &mode );
        if( !fSucc )
        {
            writeToConsole = false;
        }
    }

    if( writeToConsole )
    {
        DWORD bytesWritten = 0;
        WriteConsoleW(
            hStdErr,
            tmpBuffer,
            num_bytes,
            &bytesWritten,
            nullptr );
    }
    else
    {
        fwprintf(stderr, L"%s", tmpBuffer);
        fflush(stderr);
    }

    return num_bytes;
}


int
smpd_dbg_printf(
    _In_ _Printf_format_string_ PCWSTR str,
    ...
    )
{
    va_list list;
    va_start( list, str );
    int ret = smpd_dbg_printf( false, str, list );
    va_end( list );

    return ret;
}


int
smpd_err_printf(
    _In_ _Printf_format_string_ PCWSTR str,
    ...
    )
{
    va_list list;
    va_start( list,  str);

    int num_bytes = smpd_dbg_printf( true, str, list );

    va_end( list );
    return num_bytes;
}
