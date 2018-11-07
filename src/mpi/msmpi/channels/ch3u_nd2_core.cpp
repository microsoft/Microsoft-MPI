// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_core.cpp - Network Direct MPI CH3 Channel core functionality

--*/

#include "precomp.h"
#include "ch3u_nd2.h"


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


HMODULE g_hKernel32 = GetModuleHandle( TEXT("kernel32.dll") );;
PREMOVE_CACHE_CALLBACK g_pfnRemoveCacheCallback = NULL;


}   // namespace CH3_ND


using namespace CH3_ND;


void
MPIDI_VC_Init_nd(
    __inout MPIDI_VC_t* vc
    )
{
    //
    // NDv1 CEndpoint pointer overlaps this one, so doesn't need special init.
    //
    vc->ch.nd.pEp = NULL;
}


static
SIZE_T
CalculateCorrectRange(
    __in const VOID* pBuf,
    __in SIZE_T cbBuf
    )
{
    //
    // Work around win7 bug 650328, where it provides too big of an input range.
    //
    // Call VirtualQuery repeatedly to find the extent of a single region that
    // contains pBuf.  Note that the input range could be smaller than the region
    // for the case where pages are being decommited, and in this case we don't
    // change the range.
    //
    const BYTE* p = static_cast<const BYTE*>(pBuf);
    const VOID* allocationBase = pBuf;

    for(;;)
    {
        MEMORY_BASIC_INFORMATION info;
        SIZE_T ret = VirtualQuery( p, &info, sizeof(info) );
        if( ret != sizeof(info) )
        {
            break;
        }

        if( info.State == MEM_FREE )
        {
            break;
        }

        if( info.AllocationBase < allocationBase )
        {
            allocationBase = info.AllocationBase;
        }
        else if( info.AllocationBase != allocationBase )
        {
            break;
        }
        p = (BYTE*)info.BaseAddress + info.RegionSize;
    }

    SIZE_T cbVad = reinterpret_cast<ULONG_PTR>(p) - reinterpret_cast<ULONG_PTR>(pBuf);

    if( cbVad < cbBuf )
    {
        return cbVad;
    }

    return cbBuf;
}


static
BOOLEAN
CALLBACK
RegistrationCacheCallback(
    __in VOID* pBuf,
    __in SIZE_T cbBuf
    )
{
    OACR_USE_PTR( pBuf );
    cbBuf = CalculateCorrectRange( pBuf, cbBuf );

    g_NdEnv.FlushMrCache( static_cast<const char*>(pBuf), cbBuf );
    return TRUE;
}


void
MPIDI_CH3I_Nd_finalize(
    void
    )
{
    v1::Shutdown();

    g_NdEnv.Shutdown();

    if( g_pfnRemoveCacheCallback == NULL )
    {
        return;
    }

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

    PADD_CACHE_CALLBACK pfnAddCacheCallback = reinterpret_cast<PADD_CACHE_CALLBACK>(
        GetProcAddress(
            g_hKernel32,
            "AddSecureMemoryCacheCallback"
            )
        );
    if( pfnAddCacheCallback == NULL )
    {
        return;
    }

    PREMOVE_CACHE_CALLBACK pfnRemoveCacheCallback = reinterpret_cast<PREMOVE_CACHE_CALLBACK>(
        GetProcAddress(
            g_hKernel32,
            "RemoveSecureMemoryCacheCallback"
            )
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


int
MPIDI_CH3I_Nd_init(
    __in ExSetHandle_t hExSet
    )
{
    if( env_is_on_ex(L"MSMPI_DISABLE_ND", L"MPICH_DISABLE_ND", FALSE) == TRUE )
    {
        return MPI_SUCCESS;
    }

    DWORD cbZCopyThreshold = static_cast<DWORD>(
        env_to_int_ex(
            L"MSMPI_ND_ZCOPY_THRESHOLD",
            L"MPICH_ND_ZCOPY_THRESHOLD",
            ULONG_MAX,
            ULONG_MAX )
        );

    RegisterSecureMemoryCacheCallback();
    if( g_pfnRemoveCacheCallback == NULL )
    {
        cbZCopyThreshold = ULONG_MAX;
    }

    int cbEagerLimit = env_to_int_ex(
        L"MSMPI_ND_EAGER_LIMIT",
        L"MPICH_ND_EAGER_LIMIT",
        MPIDI_CH3_EAGER_LIMIT_DEFAULT,
        MPIDI_CH3_EAGER_LIMIT_MIN
        );

    SYSTEM_INFO sysInfo;
    GetSystemInfo( &sysInfo );

    //
    // By default the MR cache limit is half of physical memory divided by the number
    // of cores.  If the GlobalMemoryStatusEx call fails we use 4MB as the cache limit.
    //
    const SIZE_T x_Megabyte = 1024 * 1024;
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    unsigned int mrCacheLimitMb = 4;
    if( GlobalMemoryStatusEx( &memInfo ) == TRUE )
    {
        mrCacheLimitMb = static_cast<unsigned int>(
            memInfo.ullTotalPhys / 2 / sysInfo.dwNumberOfProcessors / x_Megabyte);
    }

    mrCacheLimitMb = static_cast<unsigned int>(
        env_to_int_ex(
            L"MSMPI_ND_MR_CACHE_SIZE",
            L"MPICH_ND_MR_CACHE_SIZE",
            mrCacheLimitMb,
            0
            )
        );

    UINT64 cbMrCacheLimit = static_cast<UINT64>(mrCacheLimitMb) * x_Megabyte;

    /* get the connect max retry count */
    int nConnectRetries = env_to_int_ex(
        L"MSMPI_CONNECT_RETRIES",
        L"MPICH_CONNECT_RETRIES",
        MSMPI_DEFAULT_CONNECT_RETRIES,
        0
        );

    int mpi_errno = g_NdEnv.Init(
        hExSet,
        cbZCopyThreshold,
        cbEagerLimit,
        cbMrCacheLimit,
        nConnectRetries,
        env_is_on_ex( L"MSMPI_ND_ENABLE_FALLBACK", L"MPICH_ND_ENABLE_FALLBACK", FALSE ),
        env_to_int( L"MSMPI_ND_SENDQ_DEPTH", ND_SENDQ_DEPTH, ND_SENDQ_MINDEPTH ),
        env_to_int( L"MSMPI_ND_RECVQ_DEPTH", ND_RECVQ_DEPTH, ND_RECVQ_MINDEPTH )
        );

    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = g_NdEnv.Listen();
    }

    return mpi_errno;
}

