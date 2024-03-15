// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "smpd.h"

#pragma once

#ifndef MSMPI_AA_VERSION
#define MSMPI_AA_VERSION 0x100
#endif


typedef struct _AutoAffinityInfo
{
    typedef UINT64 MaskType;

    UINT32      version;
    UINT32      groupCount;
    MaskType    occupiedCoresMask[ANYSIZE_ARRAY];
    BOOL        published;

    static UINT32 GetSize(UINT32 groupCount)
    {
        return sizeof(_AutoAffinityInfo) + sizeof(MaskType) * (groupCount - 1);
    }

    static _AutoAffinityInfo* Create(UINT32 groupCount)
    {
        _AutoAffinityInfo* pInfo = nullptr;

        UINT32 infoSize = _AutoAffinityInfo::GetSize(groupCount);
        pInfo = static_cast<_AutoAffinityInfo*>(malloc(infoSize));
        if(pInfo != nullptr)
        {
            ZeroMemory(pInfo, infoSize);
            pInfo->version = MSMPI_AA_VERSION;
            pInfo->groupCount = groupCount;
        }

        return pInfo;
    }
} AutoAffinityInfo;


HANDLE LockAutoAffinity();


HRESULT WriteAutoAffinity(_In_ smpd_context_t *context, _In_ UINT32 groupCount);


void UnlockAutoAffinity(_In_ HANDLE affinityLock);


HRESULT
ConstructHwInfo(
    _In_    BOOL         checkOccupied,
    _Inout_ UINT32*      pcbView,
    _Inout_ HWVIEW**     ppView,
    _Inout_ UINT32*      pcbTree,
    _Inout_ HWTREE**     ppTree,
    _Inout_ UINT32*      pcbSummary,
    _Inout_ HWSUMMARY**  ppSummary,
    _Inout_ wchar_t*     errorMsg,
    _In_    size_t       msgSize
    );


HRESULT
SetAffinity(
    _In_ const AffinityOptions* pOptions,
    _In_ const HWSUMMARY*       pSummary,
    _In_ const HWVIEW*          pView,
    _Inout_ smpd_context_t*     pContext,
    _Inout_ wchar_t*            errorMsg,
    _In_    size_t              msgSize
    );
