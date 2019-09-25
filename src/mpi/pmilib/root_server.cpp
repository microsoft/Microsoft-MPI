// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"
#include "rpcutil.h"
#include "sddl.h"

HRESULT
smpd_get_user_from_token(
    _In_ HANDLE hToken,
    _Outptr_ PTOKEN_USER* ppTokenUser
    )
{
    ASSERT( ppTokenUser != nullptr );

    BOOL fSucc;
    DWORD BufferSize;
    PTOKEN_USER pTokenUser;

    fSucc = GetTokenInformation(
                hToken,
                TokenUser, // Request for a TOKEN_USER structure.
                NULL,
                0,
                &BufferSize
                );

    ASSERT(fSucc == FALSE);

    pTokenUser = (PTOKEN_USER)malloc(BufferSize);
    if(pTokenUser == NULL)
    {
        smpd_err_printf(L"Failed to allocate %u bytes for user token\n", BufferSize);
        return E_OUTOFMEMORY;
    }

    fSucc = GetTokenInformation(
                hToken,
                TokenUser,
                pTokenUser,
                BufferSize,
                &BufferSize
                );

    if(!fSucc)
    {
        DWORD gle = GetLastError();
        smpd_err_printf(L"GetTokenInformation failed,\nsize %u, error: %u\n", BufferSize, gle);
        free(pTokenUser);
        return HRESULT_FROM_WIN32(gle);
    }

    *ppTokenUser = pTokenUser;
    return S_OK;
}


//
// Remarks: Upon Callers need to free the returned string using LocalFree
//
static inline LPWSTR
smpd_sid_to_string(
    _In_ PSID pSid
    )
{
    LPWSTR pString = NULL;
    ConvertSidToStringSidW(pSid, &pString);
    return pString;
}


HRESULT
smpd_equal_user_sid(
    _In_ PSID pUserSid1,
    _In_ PSID pUserSid2
    )
{
    BOOL fEqualUsers;

    fEqualUsers = EqualSid(pUserSid1, pUserSid2);
    if(!fEqualUsers)
    {
        /* FIXME: this function should not know which token is which */
        LPWSTR pString1 = smpd_sid_to_string(pUserSid1);
        LPWSTR pString2 = smpd_sid_to_string(pUserSid2);
        smpd_err_printf(L"connecting user id does not match the running process user id.\nproc(%s)\nuser(%s)\n", pString1, pString2);
        LocalFree(pString1);
        LocalFree(pString2);
    }

    return (fEqualUsers ? S_OK : S_FALSE);
}


RPC_STATUS CALLBACK
SmpdSecurityCallbackFn(
    _In_ RPC_IF_HANDLE,
    _In_ void* Context )
{
    AUTHZ_CLIENT_CONTEXT_HANDLE hAuthzClientContext;
    LUID luid= {0,0};
    RPC_STATUS status = RPC_S_OK;

    RPC_BINDING_HANDLE hClientBinding = reinterpret_cast<RPC_BINDING_HANDLE>(
        Context );
    status = RpcGetAuthorizationContextForClient(
        hClientBinding,
        FALSE,
        nullptr,
        nullptr,
        luid,
        0,
        nullptr,
        reinterpret_cast<PVOID*>(&hAuthzClientContext) );

    if( status != RPC_S_OK )
    {
        smpd_err_printf(L"Failed RpcGetAuthorizationContextForClient with status = %ld\n",
                        status );
        return status;
    }
    smpd_dbg_printf(L"Authentication completed. Successfully obtained Context for Client.\n");

    if( smpd_process.mgrServerPort == 0 ||
        env_is_on( L"MSMPI_DISABLE_AUTHZ", FALSE) )
    {
        //
        // Smpd Service only authenticates. The launched Smpd instance will
        // do authorization check.
        //
        RpcFreeAuthorizationContext( reinterpret_cast<PVOID*>(&hAuthzClientContext) );
        return RPC_S_OK;
    }

    BOOL fSucc;
    DWORD BufferSize;
    PTOKEN_USER pClientTokenUser;

    fSucc = AuthzGetInformationFromContext(
        hAuthzClientContext,
        AuthzContextInfoUserSid,
        0,
        &BufferSize,
        nullptr
        );
    assert( fSucc == false );

    pClientTokenUser = static_cast<PTOKEN_USER>( malloc(BufferSize) );
    if( pClientTokenUser == nullptr )
    {
        RpcFreeAuthorizationContext( reinterpret_cast<PVOID*>(&hAuthzClientContext) );
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    fSucc = AuthzGetInformationFromContext(
        hAuthzClientContext,
        AuthzContextInfoUserSid,
        BufferSize,
        &BufferSize,
        pClientTokenUser
        );
    if( !fSucc )
    {
        DWORD gle = GetLastError();
        free( pClientTokenUser );
        RpcFreeAuthorizationContext( reinterpret_cast<PVOID*>(&hAuthzClientContext) );
        return gle;
    }
    RpcFreeAuthorizationContext( reinterpret_cast<PVOID*>(&hAuthzClientContext) );

    HANDLE hProcessToken;
    fSucc = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hProcessToken);
    if(!fSucc)
    {
        DWORD gle = GetLastError();

        free(pClientTokenUser);
        RpcFreeAuthorizationContext( reinterpret_cast<PVOID*>(&hAuthzClientContext) );
        return gle;
    }

    PTOKEN_USER pOwnTokenUser;
    HRESULT hr = smpd_get_user_from_token( hProcessToken, &pOwnTokenUser );
    if( FAILED(hr) )
    {
        free(pClientTokenUser);
        CloseHandle(hProcessToken);
        return ERROR_ACCESS_DENIED;
    }
    CloseHandle( hProcessToken );

    hr = smpd_equal_user_sid( pClientTokenUser->User.Sid, pOwnTokenUser->User.Sid );
    free( pClientTokenUser );
    free( pOwnTokenUser );

    if( hr != S_OK )
    {
        return ERROR_ACCESS_DENIED;
    }
    smpd_dbg_printf(L"Authorization completed.\n");
    return RPC_S_OK;
}


DWORD
smpd_create_root_server(
    _In_ UINT16 port,
    _In_ UINT16 numThreads
    )
{
    //
    // This process is the root_smpd.  All sessions are child processes
    // of this process.
    //
    smpd_process.tree_id = -1;

    ASSERT(port != 0);

    wchar_t portStr[6];
    MPIU_Snprintf( portStr, _countof(portStr), L"%u", port );

    RPC_STATUS status = StartRpcServer( L"ncacn_ip_tcp",
                                        portStr,
                                        RpcSrvSmpdSvcRpc_v1_0_s_ifspec,
                                        SmpdSecurityCallbackFn,
                                        nullptr,
                                        nullptr,
                                        numThreads );
    if( status != RPC_S_OK )
    {
        smpd_err_printf(L"smpd service failed to start rpc server, error: %ld\n", status );
        return status;
    }


    smpd_dbg_printf(L"smpd listening on port %u\n", port);
    return NOERROR;
}


void
smpd_stop_root_server()
{
    RPC_STATUS status = StopRpcServer( RpcSrvSmpdSvcRpc_v1_0_s_ifspec );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf(L"smpd service failed to stop rpc server, error: %ld\n", status );
    }
    else
    {
        smpd_dbg_printf(L"smpd manager successfully stopped listening.\n");
    }
}
