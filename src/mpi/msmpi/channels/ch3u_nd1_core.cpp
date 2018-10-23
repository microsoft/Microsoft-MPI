// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_core.cpp - Network Direct MPI CH3 Channel core functionality

--*/

#include "precomp.h"
#include "ch3u_nd1.h"

typedef
BOOLEAN
(CALLBACK *PSECURE_MEMORY_CACHE_CALLBACK)(
    _In_reads_bytes_(Range) VOID* Addr,
    _In_ SIZE_T Range
    );

typedef
BOOL
(WINAPI *PADD_CACHE_CALLBACK)(
    __in PSECURE_MEMORY_CACHE_CALLBACK pfnCallback
    );

typedef
BOOL
(WINAPI *PREMOVE_CACHE_CALLBACK)(
    __in PSECURE_MEMORY_CACHE_CALLBACK pfnCallback
    );


namespace CH3_ND
{
namespace v1
{


HMODULE g_hKernel32 = GetModuleHandle( TEXT("kernel32.dll") );;
PREMOVE_CACHE_CALLBACK g_pfnRemoveCacheCallback = NULL;


static
SIZE_T
CalculateCorrectRange(
    __in const VOID* Addr,
    __in SIZE_T Range
    )
{
    //
    // Work around win7 bug 650328, where it provides too big of an input range.
    //
    // Call VirtualQuery repeatedly to find the extent of a single region that
    // contains Addr.  Note that the input range could be smaller than the region
    // for the case where pages are being decommited, and in this case we don't change
    // the range.
    //
    const BYTE* p = static_cast<const BYTE*>(Addr);
    const VOID* AllocationBase = Addr;

    for(;;)
    {
        MEMORY_BASIC_INFORMATION Info;
        SIZE_T ret = VirtualQuery( p, &Info, sizeof(Info) );
        if( ret != sizeof(Info) )
            break;

        if( Info.State == MEM_FREE )
            break;

        if( Info.AllocationBase < AllocationBase )
        {
            AllocationBase = Info.AllocationBase;
        }
        else if( Info.AllocationBase != AllocationBase )
        {
            break;
        }
        p = (BYTE*)Info.BaseAddress + Info.RegionSize;
    }

    SIZE_T VadRange = (ULONG_PTR)p - (ULONG_PTR)Addr;

    if( VadRange < Range )
        return VadRange;

    return Range;
}


static
BOOLEAN
CALLBACK
RegistrationCacheCallback(
    __in VOID* Addr,
    __in SIZE_T Range
    )
{
    OACR_USE_PTR( Addr );
    Range = CalculateCorrectRange( Addr, Range );

    g_NdEnv.FlushMrCache( (const char*)Addr, Range );
    return TRUE;
}


void
Shutdown(
    void
    )
{
    g_NdEnv.Shutdown();

    if( g_pfnRemoveCacheCallback == NULL )
        return;

    g_pfnRemoveCacheCallback( RegistrationCacheCallback );
    g_pfnRemoveCacheCallback = NULL;
}


void
RegisterSecureMemoryCacheCallback()
{
    if( g_hKernel32 == NULL )
    {
        return;
    }

    PADD_CACHE_CALLBACK pfnAddCacheCallback = (PADD_CACHE_CALLBACK)GetProcAddress(
        g_hKernel32,
        "AddSecureMemoryCacheCallback"
        );
    if( pfnAddCacheCallback == NULL )
    {
        return;
    }

    PREMOVE_CACHE_CALLBACK pfnRemoveCacheCallback = (PREMOVE_CACHE_CALLBACK)GetProcAddress(
        g_hKernel32,
        "RemoveSecureMemoryCacheCallback"
        );
    if( pfnRemoveCacheCallback == NULL )
    {
        return;
    }

    BOOL fSuccess = pfnAddCacheCallback( RegistrationCacheCallback );
    if( fSuccess != TRUE )
    {
        return;
    }

    g_pfnRemoveCacheCallback = pfnRemoveCacheCallback;
}


int Init(
    __in ExSetHandle_t hExSet,
    __in SIZE_T cbZCopyThreshold,
    __in MPIDI_msg_sz_t cbEagerLimit,
    __in UINT64 cbMrCacheLimit,
    __in int fEnableFallback
    )
{
    RegisterSecureMemoryCacheCallback();
    if( g_pfnRemoveCacheCallback == NULL )
    {
        cbZCopyThreshold = SIZE_MAX;
    }

    return g_NdEnv.Init(
        hExSet,
        cbZCopyThreshold,
        cbEagerLimit,
        cbMrCacheLimit,
        fEnableFallback
        );
}


int Listen()
{
    return g_NdEnv.Listen();
}


int GetBusinessCard(
    _Deref_pre_cap_c_( *pcbBusinessCard ) _Deref_out_z_cap_c_( *pcbBusinessCard ) char** pszBusinessCard,
    _Inout_ int* pcbBusinessCard
    )
{
    return g_NdEnv.GetBusinessCard( pszBusinessCard, pcbBusinessCard );
}


int
Connect(
    _Inout_ MPIDI_VC_t* pVc,
    _In_z_ const char* szBusinessCard,
    _In_ int fForceUse,
    _Out_ int* pbHandled
    )
{
    return g_NdEnv.Connect( pVc, szBusinessCard, fForceUse, pbHandled );
}


int Arm()
{
    return g_NdEnv.Arm();
}


int Poll( _Out_ BOOL* pfProgress )
{
    return g_NdEnv.Poll( pfProgress );
}

}   // namespace CH3_ND::v1
}   // namespace CH3_ND
