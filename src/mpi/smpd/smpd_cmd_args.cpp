// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"

void smpd_print_options(void)
{
    wprintf(L"smpd options:\n");
    wprintf(L" -help\n");
    wprintf(L" -port <port> or -p <port>\n");
    wprintf(L" -unicode\n");
    wprintf(L" -debug [flags] or -d [flags] (flags are optional)\n");
    wprintf(L"        flags are the sum of: 1 <error>, 2 <debug>\n");
    wprintf(L" -logfile <logFileName>\n");
    wprintf(L"\n");
    wprintf(L"\"smpd -d\" will start the smpd in debug mode.\n");
    fflush(stdout);
}

struct smpd_options_t
{
    HANDLE hWrite;

    smpd_options_t() : hWrite(NULL) {}
};

typedef bool (*pfn_on_option_t)(wchar_t* *argvp[], smpd_options_t* o);
struct smpd_option_handler_t
{
    const wchar_t* option;
    pfn_on_option_t on_option;

};


//----------------------------------------------------------------------------
//
// Option parsing functions (begin)
//
//----------------------------------------------------------------------------

//
// Suppressing OACR 28718 Unannotated buffer
//
#ifdef _PREFAST_
#pragma warning(push)
#pragma warning(disable:28718)
#endif

static bool
parse_help(
    wchar_t** /*argvp*/[],
    smpd_options_t*
    )
{
    smpd_print_options();
    exit(0);
}

#ifdef _PREFAST_
#pragma warning(pop)
#endif


_Success_( return == true )
static bool
parse_debug(
    _Inout_ wchar_t* *argvp[],
    smpd_options_t*
    )
{
    const wchar_t* s = (*argvp)[1];
    if(s != NULL && isdigits(s))
    {
        smpd_process.dbg_state = _wtoi(s) & SMPD_DBG_STATE_ALL;
        (*argvp) += 1;
    }

    smpd_process.dbg_state |= SMPD_DBG_STATE_PREPEND_RANK;

    (*argvp) += 1;
    return true;
}


_Success_( return == true )
static bool
parse_localonly(
    _Inout_ wchar_t* *argvp[],
    smpd_options_t*
    )
{
    smpd_process.local_root = true;
    (*argvp) += 1;
    return true;
}


_Success_( return == true )
static bool
parse_unicode(
    _Inout_ wchar_t* *argvp[],
    smpd_options_t*
    )
{
    if (!EnableUnicodeOutput())
    {
        return false;
    }
    (*argvp) += 1;
    return true;
}


_Success_(return == true)
static bool
parse_logFile(
    _Inout_ wchar_t* *argvp[],
    smpd_options_t*
    )
{
    const wchar_t* option = (*argvp)[0];
    const wchar_t* logFile = (*argvp)[1];
    if (logFile == NULL)
    {
        wprintf(L"Error: expecting a fileName following the %s option.\n", option);
        return false;
    }

    //
    // Make sure logFile folder does exist
    //
    wchar_t logFileFolder[MAX_PATH] = { 0 };
    wcscpy_s(logFileFolder, MAX_PATH, logFile);
    if (RemoveFileSpec(logFileFolder))
    {
        if (!DirectoryExists(logFileFolder))
        {
            wprintf(L"Error: folder %s does not exist.\n", logFileFolder);
            return false;
        }
    }

    if (_wfreopen(logFile, L"w", stdout) == NULL ||
        _wfreopen(logFile, L"a+", stderr) == NULL)
    {
        wprintf(L"Error: Failed to redirect logs to %s.\n", logFile);
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_port(
    _Inout_ wchar_t* *argvp[],
    smpd_options_t*
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL || !isposnumber(s))
    {
        wprintf(L"Error: expecting a positive port number following the %s option.\n", opt);
        return false;
    }

    smpd_process.rootServerPort = static_cast<UINT16>(_wtoi(s));
    if( smpd_process.rootServerPort == 0 )
    {
        wprintf(L"Error: expecting a valid TCP port number following the %s option.\n", opt);
        return false;
    }

    (*argvp) += 2;
    return true;
}

_Success_( return == true )
static bool
parse_mgr(
    _Inout_ wchar_t* *argvp[],
    _Out_ smpd_options_t* o
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL || !isposnumber(s))
    {
        wprintf(L"Error: expecting a positive handle number following the %s option.\n", opt);
        return false;
    }

    const wchar_t* job = (*argvp)[2];
    if(job == NULL)
    {
        wprintf(L"Error: expecting job string following the %s option.\n", opt);
        return false;
    }

    o->hWrite = (HANDLE)(INT_PTR)_wtoi(s);

    DWORD err = MPIU_WideCharToMultiByte( job, &smpd_process.job_context );
    if( err != NOERROR )
    {
        wprintf(L"Error: failed to convert job context %s to multibyte, error %u\n", job, err);
        return false;
    }

    (*argvp) += 3;
    return true;
}

//----------------------------------------------------------------------------
//
// Options parser
//
//----------------------------------------------------------------------------

#if DBG

static void Assert_Options_table_sorted(const smpd_option_handler_t* p, size_t n)
{
    if(n <= 1)
        return;

    for(size_t i = 1; i < n; i++)
    {
        if( CompareStringW( LOCALE_INVARIANT,
                            0,
                            p[i].option,
                            -1,
                            p[i-1].option,
                            -1 ) != CSTR_GREATER_THAN )
        {
            ASSERT(("command handlers table not sorted", 0));
        }
    }
}

#else

#define Assert_Options_table_sorted(p, n) ((void)0)


#endif


static int __cdecl mp_compare_option(const void* pv1, const void* pv2)
{
    const wchar_t* option = static_cast<const wchar_t*>(pv1);
    const smpd_option_handler_t* entry = static_cast<const smpd_option_handler_t*>(pv2);

    //
    // Subtracting 2 from the return value to maintain backcompat to
    // CRT comparison behavior.
    //
    return CompareStringW( LOCALE_INVARIANT,
                           NORM_IGNORECASE,
                           option,
                           -1,
                           entry->option,
                           -1 ) - 2;
}


_Success_( return == true )
static bool
smpd_parse_options(
    _Inout_ wchar_t* *argvp[],
    _In_ const smpd_option_handler_t* oh,
    _In_ size_t ohsize,
    _Pre_valid_ _Out_ smpd_options_t* o
    )
{
    Assert_Options_table_sorted(oh, ohsize);

    while(is_flag_char(**argvp))
    {
        const void* p;
        p = bsearch(
                &(**argvp)[1],
                oh,
                ohsize,
                sizeof(oh[0]),
                mp_compare_option
                );

        //
        // Option flag was not found
        //
        if(p == NULL)
            return true;

        //
        // Call the option handler
        //
        bool fSucc = static_cast<const smpd_option_handler_t*>(p)->on_option(argvp, o);
        if(!fSucc)
            return false;
    }

    return true;
}


//----------------------------------------------------------------------------
//
// Option handlers table (must be sorted)
//
//----------------------------------------------------------------------------

static const smpd_option_handler_t g_smpd_option_handlers[] =
{
    { L"?",      parse_help },
    { L"d",      parse_debug },
    { L"debug",  parse_debug },
    { L"help",   parse_help },
    { L"localonly", parse_localonly }, // Used by mpiexec to flag single-machine mode.
    { L"logfile",parse_logFile },
    { L"mgr",    parse_mgr },
    { L"p",      parse_port },
    { L"port",   parse_port },
    { L"unicode",parse_unicode },
};


_Success_( return == true )
static bool
parse_smpd_cmdline(
    _In_  wchar_t* argv[],
    _Pre_valid_ _Out_ smpd_options_t* o
    )
{
    //
    // Parse all known options
    //
    bool fSucc = smpd_parse_options(
                        &argv,
                        g_smpd_option_handlers,
                        _countof(g_smpd_option_handlers),
                        o
                        );

    if(!fSucc)
        return false;

    if(is_flag_char(argv[0]))
    {
        wprintf(L"Unknown option: %s\n", argv[0]);
        return false;
    }

    if(argv[0] != NULL)
    {
        wprintf(L"Unexpected parameters: %s\n", argv[0]);
        return false;
    }

    return true;
}


_Success_( return == true )
bool
smpd_parse_command_args(
    _In_     wchar_t* argv[],
    _Outptr_ HANDLE* phManagerWritePipe
    )
{
    //
    // Skip argv[0], the executable name.
    //
    argv++;

    if(*argv == NULL)
    {
        smpd_print_options();
        exit(0);
    }

    smpd_options_t options;
    if(!parse_smpd_cmdline(argv, &options))
    {
        return false;
    }

    //
    // If this is an smpd manager instance, options.hWrite is set to
    // a non-null value from the parse_smpd_cmdline invocation above
    //
    *phManagerWritePipe = options.hWrite;

    return true;
}
