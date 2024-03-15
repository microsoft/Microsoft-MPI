// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "smpd.h"

//
// This file contains stubs for some MPI internal functions.
// The stubs allow binaries outside of msmpi to use the common static library.
//
// For example, to use MPIR_Err_get_string and to link with MPIR_Err_create_code
// (required by the sock module) without dragging the entire MPI code base.
//

//
// Implement MPIR vsprintf funciton without the mpi special format specifiers
//
_Success_(return==MPI_SUCCESS)
int MPIR_Err_vsnprintf_mpi(
    _Out_writes_z_(maxlen) char* str,
    _In_ size_t maxlen,
    _Printf_format_string_ const char* fmt,
    _In_ va_list list
    )
{
    return MPIU_Vsnprintf(str, maxlen, fmt, list);
}


//
// Implement MPID Abort function to support MPIU_Assert in debug builds
//
#pragma warning(push)
#pragma warning(disable: 4702) // unreachable code
#pragma warning(disable: 4646) // nonvoid noreturn call
_Analysis_noreturn_
DECLSPEC_NORETURN
int
MPID_Abort(
    _Inout_opt_ struct MPID_Comm* /*comm*/,
    _In_ BOOL /*intern*/,
    _In_ int exit_code,
    _In_z_ const char* /*error_msg*/
    )
{
    exit(exit_code);
}

#pragma warning(pop)


void* MPIU_Malloc( _In_ SIZE_T size )
{
    return ::HeapAlloc( ::GetProcessHeap(), 0, size );
}


void* MPIU_Calloc( _In_ SIZE_T elements, _In_ SIZE_T size )
{
    return ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, size * elements );
}


void MPIU_Free( _In_opt_ _Post_ptr_invalid_ void* pMem )
{
    if( pMem != nullptr )
    {
        ::HeapFree( ::GetProcessHeap(), 0, pMem );
    }
}


void* MPIU_Realloc( _In_ void* pMem, _In_ SIZE_T size )
{
    return ::HeapReAlloc( ::GetProcessHeap(), 0, pMem, size );
}
