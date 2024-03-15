// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "dbg_printf.h"

//
// This constant is used only by the debug extension to query about the minimum
// version of the extension that this version of MSMPI can support.
// When the debug extension attaches to a running MPI process, it does not know
// whether the current MSMPI has the correct data structure layout to support
// this current version of the debugger extension (or that the user would need
// to upgrade to a newer version of the debug extension, as indicated by this
// constant).
//
static const unsigned int MSMPI_DBGEXT_VER = 200;


//
// enable breaking on initialization
//
// -env MSMPI_INIT_BREAK preinit
//      breaks into all ranks before initialization
//
// -env MSMPI_INIT_BREAK all
//      breaks into all ranks after initialization
//
// -env MSMPI_INIT_BREAK a,b,d-f,x-z
//      breaks into ranks "a b d e f x y z" after initialization.
//      ranks are decimal numbers; separator is any character except '-'
//

_Success_(return==true)
static inline
bool
get_break_env(
    _Out_writes_z_(cchEnv) char* env,
    _In_ DWORD cchEnv
    )
{
    DWORD err = MPIU_Getenv( "MSMPI_INIT_BREAK",
                             env,
                             cchEnv );
    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        err = MPIU_Getenv( "MPICH_INIT_BREAK",
                           env,
                           cchEnv );
    }

    return err == NOERROR;
}


void
MPIU_dbg_preinit()
{
    char env[_countof("preinit")];
    if( get_break_env( env, _countof(env) ) == false )
    {
        return;
    }

    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        env,
                        -1,
                        "preinit",
                        -1 ) != CSTR_EQUAL )
    {
        return;
    }

    MPIU_Debug_break();
}

_Success_(return==MPI_SUCCESS)
int
MPIU_dbg_init(
    _In_ unsigned int rank,
    _In_ unsigned int world_size
    )
{
    char env[32767];
    if( get_break_env(env, _countof(env) ) == false )
    {
        return MPI_SUCCESS;
    }

    //
    // This one is called after MPIU_dbg_preinit but it parses the same
    // environment variable so if we already parsed preinit earlier we
    // will not do it again here.
    //
    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        env,
                        -1,
                        "preinit",
                        -1 ) == CSTR_EQUAL )
    {
        return MPI_SUCCESS;
    }

    unsigned int unique_ranks;
    bool         isDebug;
    int          mpi_errno = MPI_SUCCESS;

    mpi_errno = MPIU_Parse_rank_range( rank, env, world_size, &isDebug, &unique_ranks);

    if ( mpi_errno == MPI_SUCCESS &&
        isDebug == true )
    {
        MPIU_Debug_break();
    }
    return mpi_errno;
}

void
MPIU_dbg_printf(
    _Printf_format_string_ const char * str,
    ...
    )
{
    va_list list;
    va_start(list, str);
    vfprintf(stderr, str, list); fflush(stderr);
    va_end(list);
}
