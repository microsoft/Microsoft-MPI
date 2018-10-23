// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "rpc.h"

#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif

#include <security.h>

#define DISABLE_KERB_STR L"!Kerberos"
__declspec(selectany) extern wchar_t disableKerbStr[] = DISABLE_KERB_STR;

_Success_(return == NOERROR)
RPC_STATUS
StartRpcServer(
    _In_z_     PCWSTR              pProtSeq,
    _In_opt_z_ PCWSTR              pEndpoint,
    _In_       RPC_IF_HANDLE       rpcInterface,
    _In_opt_   RPC_IF_CALLBACK_FN* pSecurityCallbackFn,
    _Out_opt_  UINT16*             pPort,
    _Out_opt_  GUID*               pLrpcEndpoint,
    _In_       UINT                maxConcurrentCalls = RPC_C_LISTEN_MAX_CALLS_DEFAULT,
    _In_       bool                localOnly = false
    );


RPC_STATUS
StopRpcServer(
    _In_ RPC_IF_HANDLE    rpcInterface
    );


_Success_(return == NOERROR)
RPC_STATUS
CreateRpcBinding(
    _In_z_   PCWSTR                   pProtSeq,
    _In_opt_z_ PCWSTR                 pHostName,
    _In_z_   PCWSTR                   pEndpoint,
    _In_     UINT                     AuthnLevel,
    _In_     UINT                     AuthnSvc,
    _In_opt_ RPC_AUTH_IDENTITY_HANDLE pAuthIdentity,
    _Out_    handle_t*                phBinding
    );


void
InitializeAuthIdentity(
    _In_ PWSTR packageStr,
    _In_ DWORD packageLen,
    _Out_ SEC_WINNT_AUTH_IDENTITY_EXW* pSecAuth
    );
