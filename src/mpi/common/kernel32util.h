// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <oacr.h>
#include <assert.h>
//
// Make some shorter names.
//
typedef SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX SLPIEX;


typedef  _Success_(return==TRUE) BOOL ( WINAPI FN_GetLogicalProcessorInformationEx )(
    _In_       LOGICAL_PROCESSOR_RELATIONSHIP           relationshipType,
    _Out_writes_to_opt_(*pReturnedLength,*pReturnedLength) PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pBuffer,
    _Inout_ PDWORD                                   pReturnedLength
    );


typedef  _Success_(return==TRUE) BOOL ( WINAPI FN_GetLogicalProcessorInformationEx )(
    _In_       LOGICAL_PROCESSOR_RELATIONSHIP           relationshipType,
    _Out_writes_to_opt_(*pReturnedLength,*pReturnedLength) PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pBuffer,
    _Inout_    PDWORD                                   pReturnedLength
    );

typedef  _Success_(return==TRUE) BOOL ( WINAPI FN_SetThreadGroupAffinity)(
    _In_ HANDLE hThread,
    _In_       const GROUP_AFFINITY *GroupAffinity,
    _Out_opt_  PGROUP_AFFINITY PreviousGroupAffinity
    );

typedef  _Success_(return==TRUE) BOOL (WINAPI FN_GetNumaNodeProcessorMaskEx) (
    _In_      USHORT Node,
    _Out_     PGROUP_AFFINITY ProcessorMask
    );

typedef  _Success_(return==TRUE) BOOL (WINAPI FN_GetProcessGroupAffinity) (
    _In_ HANDLE hProcess,
    _Inout_ PUSHORT GroupCount,
    _Out_writes_to_(*GroupCount,*GroupCount) PUSHORT GroupArray
    );


typedef  _Success_(return == TRUE) BOOL (WINAPI FN_InitializeProcThreadAttributeList) (
    _In_opt_ LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
    _In_ DWORD dwAttributeCount,
    _In_ DWORD dwFlags,
    _Inout_ PSIZE_T lpSize
    );


typedef  _Success_(return == TRUE) BOOL(WINAPI FN_UpdateProcThreadAttribute) (
    _In_ LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
    _In_ DWORD dwFlags,
    _In_ DWORD_PTR Attribute,
    _In_ PVOID lpValue,
    _In_ SIZE_T cbSize,
    _In_opt_ PVOID lpPreviousValue,
    _In_opt_ PSIZE_T lpReturnSize
    );


typedef  _Success_(return==TRUE) BOOL  (WINAPI FN_WaitOnAddress)(
    _In_      VOID volatile *Address,
    _In_      PVOID CompareAddress,
    _In_      SIZE_T AddressSize,
    _In_opt_  DWORD dwMilliseconds
    );


typedef VOID  (WINAPI FN_WakeByAddressAll)(
    _In_  PVOID Address
    );


extern BOOL     g_IsWin7OrGreater;
extern BOOL     g_IsWin8OrGreater;


//
// Summary:
//  Singleton class to load and GetProcAddress
//
struct Kernel32
{
    static Kernel32 Methods;

    FN_GetLogicalProcessorInformationEx*    GetLogicalProcessorInformationEx;
    FN_SetThreadGroupAffinity*              SetThreadGroupAffinity;
    FN_GetNumaNodeProcessorMaskEx*          GetNumaNodeProcessorMaskEx;
    FN_GetProcessGroupAffinity*             GetProcessGroupAffinity;
    FN_InitializeProcThreadAttributeList*   InitializeProcThreadAttributeList;
    FN_UpdateProcThreadAttribute*           UpdateProcThreadAttribute;
    FN_WaitOnAddress*                       WaitOnAddress;
    FN_WakeByAddressAll*                    WakeByAddressAll;


private:
    Kernel32()
    {
        HMODULE kernel32 = ::GetModuleHandleW(L"kernel32");
        if(kernel32 != nullptr)
        {
            GetLogicalProcessorInformationEx = reinterpret_cast<FN_GetLogicalProcessorInformationEx*>(
                                                        ::GetProcAddress(
                                                                kernel32,
                                                                "GetLogicalProcessorInformationEx"
                                                                ) );
            SetThreadGroupAffinity = reinterpret_cast<FN_SetThreadGroupAffinity*>(
                                                        ::GetProcAddress(
                                                                kernel32,
                                                                "SetThreadGroupAffinity"
                                                                ) );
            GetNumaNodeProcessorMaskEx = reinterpret_cast<FN_GetNumaNodeProcessorMaskEx*>(
                                                        ::GetProcAddress(
                                                                kernel32,
                                                                "GetNumaNodeProcessorMaskEx"
                                                                ) );
            GetProcessGroupAffinity = reinterpret_cast<FN_GetProcessGroupAffinity*>(
                                                        ::GetProcAddress(
                                                                kernel32,
                                                                "GetProcessGroupAffinity"
                                                                ) );
            InitializeProcThreadAttributeList = reinterpret_cast<FN_InitializeProcThreadAttributeList*>(
                                                        ::GetProcAddress(
                                                                kernel32,
                                                                "InitializeProcThreadAttributeList"
                                                                ) );
            UpdateProcThreadAttribute = reinterpret_cast<FN_UpdateProcThreadAttribute*>(
                                                        ::GetProcAddress(
                                                                kernel32,
                                                                "UpdateProcThreadAttribute"
                                                                ) );
        }

        assert(!g_IsWin7OrGreater || GetLogicalProcessorInformationEx != nullptr);
        assert(!g_IsWin7OrGreater || SetThreadGroupAffinity != nullptr);
        assert(!g_IsWin7OrGreater || GetNumaNodeProcessorMaskEx != nullptr);
        assert(!g_IsWin7OrGreater || GetProcessGroupAffinity != nullptr);
        assert(!g_IsWin7OrGreater || InitializeProcThreadAttributeList != nullptr);
        assert(!g_IsWin7OrGreater || UpdateProcThreadAttribute != nullptr);

        if (g_IsWin8OrGreater)
        {
            HMODULE kernelBase = ::GetModuleHandleW(L"kernelbase");
            if(kernelBase != nullptr)
            {
                WakeByAddressAll = reinterpret_cast<FN_WakeByAddressAll*>(
                                                            ::GetProcAddress(
                                                                    kernelBase,
                                                                    "WakeByAddressAll"
                                                                    ));

                WaitOnAddress = reinterpret_cast<FN_WaitOnAddress*>(
                                                            ::GetProcAddress(
                                                                    kernelBase,
                                                                    "WaitOnAddress"
                                                                    ));
            }

            assert(WakeByAddressAll != nullptr);
            assert(WaitOnAddress != nullptr);
        }
        else
        {
            WakeByAddressAll = nullptr;
            WaitOnAddress = nullptr;
        }
    }
};
