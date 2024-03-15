// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"

#define CHAR_NULL   L'\0'
#define CHAR_SPACE  L' '
#define CHAR_TAB    L'\t'
#define CHAR_DQUOTE L'\"'
#define CHAR_SLASH  L'\\'

//
// These set of routines purpose is to "revert" the C runtime command line parsing
// and restore the quotes and backslashes to the command line arguments
//

#define PUSH_CHAR(str, end, a_) \
{ \
    ASSERT(str < end); \
    *str++ = a_; \
    if(str == end) \
    { \
        *(--str) = L'\0'; \
        return str; \
    } \
}


//
// smpd_quoted_copy
//
// A "quoted" strncpy function. The string is put in double quotes and any
// included quote is eascaped with a backslash.
//
// Return value: a pointer to the null terminating char.
//
static inline wchar_t* smpd_quoted_copy(_Out_cap_(maxlen) wchar_t* str, int maxlen, const wchar_t* arg)
{
    const wchar_t* end = str + maxlen;

    if(maxlen < 1)
        return str;

    PUSH_CHAR(str, end, CHAR_DQUOTE);

    while(*arg != L'\0')
    {
        if (*arg == CHAR_DQUOTE)
        {
            PUSH_CHAR(str, end, CHAR_SLASH);
        }
        PUSH_CHAR(str, end, *arg);
        arg++;
    }

    PUSH_CHAR(str, end, CHAR_DQUOTE);
    *str = L'\0';

    return str;
}


static inline BOOL need_quoted_copy(const wchar_t* arg)
{
    //
    // "arg" needs quoted print if it includes any of the following
    // characters" <space>, <tab>, <double-quotes>
    //
    return (wcspbrk(arg, L" \t\"%") != NULL);
}


//
// smpd_pack_cmdline
//
// Pack the argv set into a command line. Restore the quotes and backslashes to
// the command line, "reverting" the C runtime argv parsing.
// args are speperated by space.
//
// Return value: a pointer to the null terminating char.
//
_Ret_notnull_
wchar_t*
smpd_pack_cmdline(
    _In_ PCWSTR const argv[],
    _Out_writes_(size) wchar_t* buff,
    _In_ int size
    )
{
    const wchar_t* end = buff + size;

    //
    // With input size > 0, size never goes to zero as both copy functions below
    // return a pointer to the null char; thus the PUSH_CHAR(' ') below is safe.
    //
    ASSERT(size > 0);

    if(*argv == NULL)
    {
        buff[0] = L'\0';
        return buff;
    }

    for(;;)
    {
        wchar_t* p;
        if(need_quoted_copy(*argv))
        {
            p = smpd_quoted_copy(buff, size, *argv);
        }
        else
        {
            p = MPIU_Strncpy(buff, *argv, size);
            MPIU_Assert( p != nullptr );
        }

        ++argv;
        if(*argv == NULL)
            return p;

        PUSH_CHAR(p, end, L' ');

        size -= (int)(p - buff);
        buff = p;
    }
}


//
// smpd_unpack_cmdline
//
// Unpack a command line into argv set. The caller should allocate memory for the argv array
// and the copy of the command line. Calling smpd_unpack_cmdline with NULL for argv and args
// returns the size required for argv and args parameters.
// This function write the terminating null character for each arg and ends argv with a NULL
// pointer.
//
void
smpd_unpack_cmdline(
    _In_ PCWSTR cmdline,
    _Inout_ wchar_t** argv,
    _Out_writes_opt_(*nchars) wchar_t* args,
    _Out_ int* nargs,
    _Out_ int* nchars
    )
{
    *nargs = 0;
    *nchars = 0;
    if( args )
    {
        *args = CHAR_NULL;
    }

    const wchar_t* p = cmdline;
    bool inquote = false;

    for(;;)
    {
        //
        // Whitespace is not part of an argument. Skip it!
        //
        p = skip_ws(p);
        if(*p == CHAR_NULL)
            break;

        ++*nargs;
        if(argv)
        {
            *argv++ = args;
        }

        //
        // Scan a single argument
        //
        for( ;*p != CHAR_NULL; ++p )
        {
            if(*p == CHAR_DQUOTE)
            {
                //
                // check if the double quote is escaped
                //
                if (inquote && *(p - 1) == CHAR_SLASH)
                {
                    //
                    // undo escaping double quote
                    //
                    if (args)
                    {
                        args--;
                    }
                }
                else
                {
                    inquote = !inquote;
                    continue;
                }
            }

            /* if at end of arg, break loop */
            if( !inquote && iswspace(*p) )
            {
                break;
            }

            //
            // Copy the character into argument string
            //
            ++*nchars;
            if (args)
            {
                *args++ = *p;
            }
        }

        //
        // null-terminate the argument
        //
        ++*nchars;
        if(args)
        {
            *args++ = CHAR_NULL;          /* terminate string */
        }
    }

    //
    // Terminate the last argv with a NULL pointer
    //
    ++*nargs;
    if(argv)
    {
        *argv++ = NULL;
    }
}
