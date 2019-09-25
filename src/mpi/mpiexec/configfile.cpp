// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "smpd.h"
#include <errno.h>


//
// configfile_size
//
// Calculate the configfile size to allocate by adding 3 chars for each line len for the block
// seperator " : ".
// Zero is returned in indicate an error and errno is set to the error value.
//
static size_t configfile_size(FILE* fin)
{
    size_t size = 0;
    wchar_t buffer[128];

    while(fgetws(buffer, _countof(buffer), fin))
    {
        //
        // fgetws skips over NULL characters: it is unsafe to use strlen to get the number
        // of bytes read, since the user may erroneously supply a binary file.
        //
        size += _countof(buffer) + 3;
    }

    if(ferror(fin))
        return 0;

    if(fseek(fin, 0, SEEK_SET) != 0)
        return 0;

    return size + 3;
}


//
// read_configfile
//
// Read the complete configfile into a buffer ( ':' seperated) and return the buffer.
// NULL is returned in indicate an error and errno is set to the error value.
//
static wchar_t* read_configfile(FILE* fin)
{
    size_t size = configfile_size(fin);
    if(size == 0)
        return NULL;

    if(size > INT_MAX)
    {
        _set_errno(E2BIG);
        return NULL;
    }

    wchar_t* cmdline = static_cast<wchar_t*>( malloc( size * sizeof(wchar_t) ) );
    if(cmdline == NULL)
    {
        _set_errno(ENOMEM);
        return NULL;
    }

    wchar_t* line = cmdline;
    int concat = 0;
    while(fgetws(line, (int)(size), fin))
    {
        ASSERT(size > 1);

        wchar_t* p = const_cast<wchar_t*>(skip_ws(line));

        //
        // On comment, read the next line to the same buffer location.
        // Comment lines do not terminate concatenation, allowing commenting out parts
        // of a long block
        //
        if(*p == L'#')
            continue;

        //
        // On whitespace lines, read the next line to the same buffer location.
        // Note that whitespace lines terminate line concatenation, and append the block
        // end sequence " : ".
        //
        if(*p == L'\0')
        {
            if(!concat)
                continue;

            p = line - 1;
        }
        else
        {

            //
            // Trim whitespace at the the end of the line (remove CR or LF characters).
            // N.B. The line contain at least one non whitespace character; thus this code
            //      will not underflow the line.
            //
            p += MPIU_Strlen( p ) - 1;
            while( iswspace(*p) )
            {
                p--;
            }

            size -= p - line;
            line = p;

            //
            // Line break marker; read the next line into the same location
            //
            if(*p == L'\\')
            {
                concat = 1;
                continue;
            }
        }

        concat = 0;
        *++p = L' ';
        *++p = L':';
        *++p = L' ';
        ++p;

        size -= p - line;
        line = p;
    }

    if(ferror(fin))
    {
        free(cmdline);
        return NULL;
    }

    if((line - cmdline > 3) && *(line - 2) == ':')
    {
        line -= 3;
    }

    *line = L'\0';

    return cmdline;
}


//
// smpd_get_argv_from_file
//
// Read the entire config file and set argv.
// NULL is returned to indicate success; Error string is returned to indicate error.
//
_Success_( return == NULL )
_Ret_maybenull_
PCWSTR
smpd_get_argv_from_file(
    _In_ PCWSTR filename,
    _Outptr_ wchar_t ***argvp
    )
{
    FILE* fin = _wfopen(filename, L"r");
    if(fin == NULL)
    {
        const wchar_t* res = _wcserror(errno);
        _Analysis_assume_( res != nullptr );
        return res;
    }

    wchar_t* cmdline = read_configfile(fin);

    fclose(fin);

    if (cmdline == nullptr)
    {
        const wchar_t* res = _wcserror(errno);
        _Analysis_assume_( res != nullptr );
        return res;
    }

    int numargs;
    int numchars;
    smpd_unpack_cmdline(cmdline, NULL, NULL, &numargs, &numchars);

    if(numargs <= 1)
    {
        free(cmdline);
        return L"no commands in file";
    }

    wchar_t** argv = (wchar_t**)malloc(numargs * sizeof(wchar_t*) +
                                    numchars * sizeof(wchar_t));
    if(argv == NULL)
    {
        free(cmdline);
        const wchar_t* res = _wcserror(ENOMEM);
        _Analysis_assume_( res != nullptr );
        return res;
    }

    smpd_unpack_cmdline(cmdline, argv, (wchar_t*)(argv + numargs), &numargs, &numchars);

    free(cmdline);
    *argvp = argv;
    return NULL;
}
