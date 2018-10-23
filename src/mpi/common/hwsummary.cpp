// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "hwtree.h"
#include "util.h"
#include "kernel32util.h"

HRESULT
HwInfoInitializeWin6(
    _Out_ HWINFO*           pInfo
    );


HRESULT
HwInfoInitializeWin7(
    _Inout_ UINT32*                               pnInfos,
    _Out_writes_to_opt_(*pnInfos,*pnInfos) HWINFO pInfos[]
    );


//
// Summary:
//  Initialize HWINFO array from the local logical processor information.
//
// Parameters:
//  pnInfos             - On input, max size of pInfos
//                        on output, size used.
//                          if return code is HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
//                          the required count will be written here.
//  pInfos              - pointer to buffer to fill with values
//  pFilters            - (optional) pointer to array of affinity filters
//                          must be same length as pInfos when specified.
//
HRESULT
HwInfoInitialize(
    _Inout_ UINT32*                               pnInfos,
    _Out_writes_to_opt_(*pnInfos,*pnInfos) HWINFO pInfos[]
    )
{
    if( FALSE != g_IsWin7OrGreater )
    {
        return HwInfoInitializeWin7(pnInfos,pInfos);
    }
    else
    {
        if( *pnInfos < 1 )
        {
            *pnInfos = 1;
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }
        Assert(pInfos != nullptr);
        HRESULT hr = HwInfoInitializeWin6(pInfos);
        if( SUCCEEDED( hr ) )
        {
            *pnInfos = 1;
        }
        return hr;
    }
}



//
// Summary:
//  Initialize the local HWSUMMARY information
//
// Parameters:
//  pcbSummary          - On input, size of pSummary buffer
//                        on output, size used.
//                          if return code is HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
//                          the required count will be written here.
//  pSummary            - The summary information to initialize.
//
HRESULT
HwSummaryInitialize(
    _Inout_ UINT32*                                               pcbSummary,
    _Inout_updates_bytes_to_(*pcbSummary, *pcbSummary) HWSUMMARY* pSummary
    )
{
    HRESULT hr;
    UINT32 nInfos = 0;
    UINT32 cb;

    hr = HwInfoInitialize( &nInfos, nullptr );
    if( FAILED( hr ) )
    {
        if( HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) != hr )
        {
            return hr;
        }
    }

    cb = sizeof(*pSummary) - sizeof(pSummary->Infos) +
                    (sizeof(pSummary->Infos[0]) * nInfos);

    if( *pcbSummary < cb )
    {
        *pcbSummary = cb;
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    Assert(pSummary != nullptr);

    pSummary->Size  = cb;
    pSummary->Count = nInfos;

    hr = HwInfoInitialize( &nInfos, pSummary->Infos );
    if( FAILED( hr ) )
    {
        return hr;
    }

    if (!(g_IsWin7OrGreater && nInfos > 1))
    {
        KAFFINITY sysMask;
        union
        {
            KAFFINITY procMask;
            UINT64    procMask64;
        };
        procMask64 = 0;
        if( FALSE == ::GetProcessAffinityMask( ::GetCurrentProcess(), &procMask, &sysMask ) )
        {
            return HRESULT_FROM_WIN32( ::GetLastError() );
        }
        HwSummaryFilter( pSummary, &procMask64 );
    }

    *pcbSummary = cb;
    return S_OK;
}



