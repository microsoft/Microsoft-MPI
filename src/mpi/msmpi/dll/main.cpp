// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

main.cpp - DllMain implementation for MSMPI

--*/

#include "mpiimpl.h"


/* Provide a prototype for the mpirinitf function */
EXTERN_C void __cdecl mpirinitf( void );

extern HMODULE g_hModule;

EXTERN_C
BOOL
WINAPI
DllMain(
    __in HINSTANCE hInst,
    __in DWORD reason,
    __in VOID* /*reserved*/
    )
{
    switch( reason )
    {
    case DLL_PROCESS_DETACH:
#ifdef HAVE_DEBUGGER_SUPPORT
        //
        // Clean up all the allocated MPIR_Sendq
        //
        while( MPIR_Sendq_head != nullptr )
        {
            MPIR_Sendq* p = MPIR_Sendq_head;
            MPIR_Sendq_head =
                reinterpret_cast<MPIR_Sendq*>(MPIR_Sendq_head->ListEntry.Flink);
            MPIU_Free( p );
        }

        while( MPIR_Sendq_pool != nullptr )
        {
            MPIR_Sendq* p = MPIR_Sendq_pool;
            MPIR_Sendq_pool =
                reinterpret_cast<MPIR_Sendq*>(MPIR_Sendq_pool->ListEntry.Flink);
            MPIU_Free( p );
        }
#endif
        Mpi.PostFinalize();
        break;

    case DLL_THREAD_DETACH:
        Mpi.CallState.DetachCurrentThread();
        break;

    case DLL_PROCESS_ATTACH:
        {
            g_hModule = hInst;
            mpirinitf();
        }
        break;

    case DLL_THREAD_ATTACH:
        break;

    default:
        __assume( 0 );
    }
    return TRUE;
}
