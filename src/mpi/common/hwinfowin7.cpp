// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "hwtree.h"
#include "util.h"
#include "kernel32util.h"
#include <oacr.h>
#include<string.h>
//
// Summary:
//  Utility function to get the specificied processor relations using the existing buffer if possible.
//
// Parameters:
//  relation        - the specific relation that we want.
//  ppSlpi          - pointer to an existing buffer on input (optional)
//                    pointer to new buffer if current buffer is null or too small
//  pcbSlpi         - on input, points to the current size of the buffer
//                    on output, contains the new alloc size or the size used if the
//                      existing buffer is large enough.
//
static
HRESULT
HwInfoGetSlpiEx(
    _In_ LOGICAL_PROCESSOR_RELATIONSHIP      relation,
    _Inout_ _Outptr_result_bytebuffer_to_(*pcbSlpi,*pcbSlpi) SLPIEX**   ppSlpi,
    _Inout_ UINT32*                          pcbSlpi
    )
{
    SLPIEX* pSlpi = *ppSlpi;
    DWORD cb = *pcbSlpi;
    DWORD ntError;
    BOOL bResult;
    for(;;)
    {
        Assert( nullptr != Kernel32::Methods.GetLogicalProcessorInformationEx );

        bResult = Kernel32::Methods.GetLogicalProcessorInformationEx( relation, pSlpi, &cb );
        if( FALSE != bResult )
        {
            break;
        }
        ntError = GetLastError();
        if( nullptr != pSlpi )
        {
            //
            // ensure that we null out the input buffer if
            // and free the input buffer on error.
            // this prevents the caller from having to
            // free it on failure.
            //
            *ppSlpi = nullptr;
            free(pSlpi);
        }
        if( ntError != ERROR_INSUFFICIENT_BUFFER )
        {
            return HRESULT_FROM_WIN32(ntError);
        }

        pSlpi = static_cast<SLPIEX*>( malloc( cb ) );
        if( nullptr == pSlpi )
        {
            return E_OUTOFMEMORY;
        }

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
//  pnInfos             - On input, the count of elements in pInfos.
//                        On output, the count of elements used in pInfos or size required if too small
//  pInfos              - Array of HWINFO to populate
//
HRESULT
HwInfoInitializeWin7(
    _Inout_ UINT32*                     pnInfos,
    _Out_writes_to_opt_(*pnInfos,*pnInfos) HWINFO pInfos[]
    )
{
    SLPIEX* pSlpiEx = nullptr;
    UINT32  cbSlpiEx = 0;
    UINT32  cbSlpiExAlloc = 0;
    UINT8   numaWidth;
    UINT8   pcoreWidth = 1;
    HRESULT hr;

    hr = HwInfoGetSlpiEx(RelationNumaNode,&pSlpiEx, &cbSlpiExAlloc );
    if( FAILED( hr ) )
    {
        return hr;
    }

    __analysis_assume(pSlpiEx != nullptr);

    numaWidth = CountBits( pSlpiEx->NumaNode.GroupMask.Mask );

    cbSlpiEx = cbSlpiExAlloc;
    hr = HwInfoGetSlpiEx( RelationProcessorCore, &pSlpiEx, &cbSlpiEx );
    if( FAILED( hr ) )
    {
        return hr;
    }

    //
    // If HT is enabled, then calculate the pcoreWidth to include the
    //  logical cores it contains.
    //
    __analysis_assume(pSlpiEx != nullptr);
    if( 0 != (pSlpiEx->Processor.Flags & LTP_PC_SMT) )
    {
        pcoreWidth = CountBits( pSlpiEx->Processor.GroupMask[0].Mask );
    }

    //
    // if the second call to HwInfoGetSlpiEx reallocated the buffer
    //  we calculate the new alloc max based on previous and current size
    //
    cbSlpiExAlloc   = max(cbSlpiExAlloc,cbSlpiEx);
    cbSlpiEx        = cbSlpiExAlloc;
    hr = HwInfoGetSlpiEx(RelationGroup,&pSlpiEx, &cbSlpiEx );
    if( FAILED( hr ) )
    {
        return hr;
    }

    //
    // To make the data structures pack nicely, we have limited ourselves to
    //  UINT8 worth of processor groups.  There is no real world case where
    //  the number of processor groups will exceed this limit.
    //
    __analysis_assume(pSlpiEx != nullptr);
    UINT8 nGroups = static_cast<UINT8>( pSlpiEx->Group.ActiveGroupCount );

    //
    // This should be a very rare case where group count > 1
    //  so I don't feel bad not pre-emptively checking the group count
    //  before calculating the PCores and Numa widths.
    //
    if( nGroups > *pnInfos )
    {
        *pnInfos = nGroups;
        free(pSlpiEx);
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }


    for( UINT8 i = 0; i < nGroups;  i++ )
    {
        pInfos[i].Mask          = pSlpiEx->Group.GroupInfo[i].ActiveProcessorMask;
        pInfos[i].ActiveMask    = pInfos[i].Mask;

        pInfos[i].Group          = i;
        pInfos[i].GroupWidth     = pSlpiEx->Group.GroupInfo[ i ].ActiveProcessorCount;
        pInfos[i].NumaWidth      = numaWidth;
        pInfos[i].PcoreWidth     = pcoreWidth;

        Assert(pInfos[i].GroupWidth >= pInfos[i].NumaWidth);
        Assert(pInfos[i].NumaWidth >= pInfos[i].PcoreWidth);
        Assert(pInfos[i].PcoreWidth > 0 );
    }
    free(pSlpiEx);
    *pnInfos = nGroups;
    return S_OK;
}

