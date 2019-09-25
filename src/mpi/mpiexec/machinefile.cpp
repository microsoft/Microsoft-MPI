// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "smpd.h"
#include <errno.h>
#include <wchar.h>

//
// machinefile_size
//
// Calculate the machinefile buffer size to allocate by adding 3 to the size of the file.
// Zero is returned in indicate an error and errno is set to the error value.
//
static size_t machinefile_size(FILE* fin)
{
    size_t size = 0;
    wchar_t buffer[128];


    while(fgetws(buffer, _countof(buffer), fin))
    {
        //
        // fgets skips over NULL characters: it is unsafe to use strlen to get the number
        // of bytes read, since the user may erroneously supply a binary file.
        //
        size += _countof(buffer);
    }

    if(ferror(fin))
        return 0;

    if(fseek(fin, 0, SEEK_SET) != 0)
        return 0;

    return size + 3;
}


//
// read_machinefile
//
// Read the complete machinefile into a buffer (space seperated), with the the number of
// machines (lines) prepended to the buffer.
// NULL is returned to indicate success; Error string is returned to indicate error.
//
_Success_( return == NULL )
_Ret_maybenull_
static PCWSTR
read_machinefile(
    _In_ FILE* fin,
    _Outptr_result_z_ wchar_t** phosts
    )
{
    const int x_space = 8;
    size_t size = machinefile_size(fin);
    if(size == 0)
        return NULL;

    if(size > INT_MAX)
        return _wcserror(E2BIG);

    size += x_space;

    wchar_t* hosts = static_cast<wchar_t*>( malloc( size * sizeof(wchar_t) ) );
    if(hosts == NULL)
    {
        return _wcserror(ENOMEM);
    }

    size_t hosts_len = size;
    wchar_t* line = hosts;

    //
    // Prepend spaces
    //
    wmemset(line, L' ', x_space);
    size -= x_space;
    line += x_space;

    int nhosts = 0;
    while(fgetws(line, (int)(size), fin))
    {
        ASSERT(size > 1);

        wchar_t* p = const_cast<wchar_t*>(skip_ws(line));

        //
        // On whitespace or comment, read the next line to the same buffer location.
        //
        if(*p == L'\0' || *p == L'#')
            continue;

        //
        // Skip the machine name (must exist) and the optional whitespace and processors count.
        //
        p = const_cast<wchar_t*>(skip_graph(p));
        p = const_cast<wchar_t*>(skip_ws(p));
        p = const_cast<wchar_t*>(skip_digits(p));

        //
        // If we found explicit affinity masks, skip them so they get added to the string
        //  Explicit masks have the following format:
        //    hostname nproc,mask0[:group],...,maskN[:group]
        //
        while( *p == L',' )
        {
            p++;
            if( *p != L'\0' )
            {
                p = const_cast<wchar_t*>(skip_hex(p));
                if( *p == L':' )
                {
                    p++;
                    if( *p != L'\0' )
                    {
                        p = const_cast<wchar_t*>(skip_digits(p));
                    }
                };
            }
        }

        p = const_cast<wchar_t*>(skip_ws(p));

        if(*p != L'\0' && *p != L'#')
        {
            free(hosts);
            return L"expecting a positive number of cores following the host name";
        }

        //
        // Trim whitespace at the the end of the line (remove CR or LF characters).
        //
        --p;
        while( iswspace(*p) )
        {
            p--;
        }

        *++p = L' ';
        ++p;
        size -= p - line;
        line = p;
        nhosts++;
    }

    if(ferror(fin))
    {
        free(hosts);
        const wchar_t* res = _wcserror(errno);
        _Analysis_assume_( res != nullptr );
        return res;
    }

    if(nhosts == 0)
    {
        free(hosts);
        return L"expecting host names in file";
    }

    *--line = L'\0';
    _itow_s(nhosts, hosts, hosts_len, 10);
    line = hosts + MPIU_Strlen( hosts, hosts_len );
    *line = L' ';

    *phosts = hosts;
    return NULL;
}


//
// smpd_get_hosts_from_file
//
// Read the entire machinefile into a string.
// NULL is returned to indicate success; Error string is returned to indicate error.
//
_Success_( return == NULL )
_Ret_maybenull_
PCWSTR
smpd_get_hosts_from_file(
    _In_ PCWSTR filename,
    _Outptr_result_z_ wchar_t** phosts
    )
{
    FILE* fin = _wfopen(filename, L"r");
    if(fin == NULL)
    {
        const wchar_t* res = _wcserror(errno);
        _Analysis_assume_( res != nullptr );
        return res;
    }

    const wchar_t* error = read_machinefile(fin, phosts);
    fclose(fin);
    return error;
}
