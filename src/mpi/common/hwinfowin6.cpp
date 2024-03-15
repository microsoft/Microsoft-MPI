// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "hwtree.h"
#include "util.h"

typedef SYSTEM_LOGICAL_PROCESSOR_INFORMATION    SLPI;


inline const SLPI* HwInfoGetEnd( const SLPI* pInfo, DWORD cb )
{
    return (const SLPI*)((ULONG_PTR)pInfo + (ULONG_PTR)cb);
}


//
// Summary:
//  Get the full mask of all cores
//
UINT64
HwInfoGetGroupMask(
    _In_ const SLPI* pSlpi,
    _In_ DWORD       cb
    )
{
    UINT64 mask = 0;
    for( const SLPI* pCurrent = pSlpi;
         pCurrent < HwInfoGetEnd(pSlpi, cb);
         pCurrent++
         )
    {
        if( pCurrent->Relationship == RelationProcessorCore )
        {
            mask |= static_cast<UINT64>(pCurrent->ProcessorMask);
        }
    }
    Assert(0 != mask);
    return mask;
}


//
// Summary:
//  Get the count of logical cores per physical core
//
UINT8
HwInfoGetPcoreWidth(
    _In_ const SLPI* pSlpi,
    _In_ DWORD       cb
    )
{
    UINT8 c = 1;
    for( const SLPI* pCurrent = pSlpi;
         pCurrent < HwInfoGetEnd(pSlpi, cb);
         pCurrent++
         )
    {
        if( pCurrent->Relationship == RelationProcessorCore )
        {
            //
            // MSDN documents that if Flags == 1, HT is enabled.
            //  so we need to count the bits used.
            //
            if( 1 == pCurrent->ProcessorCore.Flags )
            {
                c = CountBits( pCurrent->ProcessorMask );
            }
            break;
        }
    }
    return c;
}


//
// Summary:
//  Get the count of logical cores per numa node
//
UINT8
HwInfoGetNumaWidth(
    _In_ const SLPI* pSlpi,
    _In_ DWORD       cb
    )
{
    UINT8 c = 0;
    for( const SLPI* pCurrent = pSlpi;
         pCurrent < HwInfoGetEnd(pSlpi, cb);
         pCurrent++
         )
    {
        if( pCurrent->Relationship == RelationNumaNode )
        {
            c = CountBits( pCurrent->ProcessorMask );
            break;
        }
    }
    Assert(0 != c);
    return c;
}


//
// Summary:
//  Allocate the SLPI information for this machine.
//
// Parameters:
//  ppSlpi              - pointer to recieve the allocate SLPI array
//  pcbSlpi             - pointer to recieve the total buffer size.
//
HRESULT
HwInfoGetSlpi(
    _Outptr_result_buffer_(*pcbSlpi) SLPI**  ppSlpi,
    _Out_ UINT32*       pcbSlpi
    )
{
    SLPI* pSlpi;
    DWORD ntError;
    DWORD cb = 0;

    BOOL bResult = ::GetLogicalProcessorInformation( nullptr, &cb );
    if( FALSE == bResult )
    {
        ntError = GetLastError();
        if( ntError != ERROR_INSUFFICIENT_BUFFER )
        {
            return HRESULT_FROM_WIN32(ntError);
        }
    }

    pSlpi = static_cast<SLPI*>(malloc(cb));
    if( nullptr == pSlpi )
    {
        return E_OUTOFMEMORY;
    }

    bResult = ::GetLogicalProcessorInformation( pSlpi, &cb );
    if( FALSE == bResult )
    {
        free(pSlpi);
        ntError = GetLastError();
        return HRESULT_FROM_WIN32(ntError);
    }

    *ppSlpi = pSlpi;
    *pcbSlpi = cb;

    return S_OK;
}


//
// Summary:
//  Initialize the array of pInfos from the current machine using Win7 apis
//
// Parameters:
//  pInfo               - HWINFO to initialize
//  pFilter             - Option bit mask filter
//
HRESULT
HwInfoInitializeWin6(
    _Out_ HWINFO*               pInfo
    )
{
    SLPI*   pSlpi;
    UINT32  cbSlpi;
    HRESULT hr;

    hr = HwInfoGetSlpi(&pSlpi, &cbSlpi );
    if( FAILED( hr ) )
    {
        return hr;
    }

    pInfo->Mask           = HwInfoGetGroupMask(pSlpi,cbSlpi);
    pInfo->ActiveMask     = pInfo->Mask;

    pInfo->Group          = 0;
    pInfo->GroupWidth     = CountBits(pInfo->Mask);
    pInfo->NumaWidth      = HwInfoGetNumaWidth(pSlpi,cbSlpi);
    pInfo->PcoreWidth     = HwInfoGetPcoreWidth(pSlpi,cbSlpi);

    Assert(pInfo->GroupWidth >= pInfo->NumaWidth);
    Assert(pInfo->NumaWidth >= pInfo->PcoreWidth);
    Assert(pInfo->PcoreWidth > 0 );

    free(pSlpi);
    return S_OK;
}
