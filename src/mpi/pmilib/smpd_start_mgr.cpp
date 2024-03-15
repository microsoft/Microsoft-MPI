// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"
#include "rpc.h"
#include <winbase.h>
#include <winreg.h>

//
// Add support for reasonable machine names on azure by detecting and
// passing on the _CLUSTER_NETWORK_NAME and _CLUSTER_NETWORK_HOSTNAME
//
static const wchar_t * const AZURE_NET_NAME        = L"_CLUSTER_NETWORK_NAME_";
static const wchar_t * const AZURE_NET_HOSTNAME    = L"_CLUSTER_NETWORK_HOSTNAME_";

struct AzureNetworkNames
{
    wchar_t ClusterNetName[ SMPD_MAX_HOST_LENGTH ];
    wchar_t ClusterNetHostname[ SMPD_MAX_HOST_LENGTH ];
};

//
// Detects whether we are running on azure and if so adds
// the appropriate cluster variables to our envioronment block.
//
static bool FixupAzureEnvironment( AzureNetworkNames* pCurrent )
{
    wchar_t  buffer[SMPD_MAX_HOST_LENGTH + 1];
    DWORD size = _countof( buffer );

    if( get_azure_node_logical_name( buffer, size ) == false )
    {
        return false;
    }

    //
    // Yes we are running on azure, so preserve the old environment variables.
    //
    DWORD status = MPIU_Getenv( AZURE_NET_NAME,
                                pCurrent->ClusterNetName,
                                _countof( pCurrent->ClusterNetName ) );
    if( status == ERROR_INSUFFICIENT_BUFFER )
    {
        return false;
    }

    status = MPIU_Getenv( AZURE_NET_HOSTNAME,
                          pCurrent->ClusterNetHostname,
                          _countof(pCurrent->ClusterNetHostname) );
    if( status == ERROR_INSUFFICIENT_BUFFER )
    {
        return false;
    }

    //
    // Extract the shorter name.
    //
    wchar_t shortName[ MAX_COMPUTERNAME_LENGTH+1 ];
    size = _countof( shortName );

    if( DnsHostnameToComputerNameW( buffer, shortName, &size ) )
    {
        //
        // Update to the new environment, if this fails there isn't much we can do
        // so try and fail silently.
        //
        SetEnvironmentVariableW( AZURE_NET_HOSTNAME, buffer );
        SetEnvironmentVariableW( AZURE_NET_NAME, shortName );
        return true;
    }
    return false;
}


static inline void RestoreAzureEnvironment( const AzureNetworkNames& names )
{
    SetEnvironmentVariableW( AZURE_NET_NAME,     names.ClusterNetName );
    SetEnvironmentVariableW( AZURE_NET_HOSTNAME, names.ClusterNetHostname );
}


HRESULT smpd_start_win_mgr(smpd_context_t* context, int dbg_state)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE hRead;
    HANDLE hWriteRemote;
    DWORD num_read;
    DWORD gle;
    wchar_t cmd[1024];

    /* start the manager */
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    sa.nLength = sizeof(sa);

    /* create a pipe to send the listening port information through */
    if(!CreatePipe(&hRead, &hWriteRemote, &sa, 0))
    {
        gle = GetLastError();
        smpd_err_printf(L"CreatePipe failed, error %u\n", gle);
        return HRESULT_FROM_WIN32(gle);
    }

    /* prevent the local read end of the pipe from being inherited */
    if(!DuplicateHandle(GetCurrentProcess(), hRead, GetCurrentProcess(), &hRead, 0, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS))
    {
        gle = GetLastError();
        smpd_err_printf(L"Unable to duplicate the read end of the pipe, error %u\n", gle);
        CloseHandle(hRead);
        CloseHandle(hWriteRemote);
        return HRESULT_FROM_WIN32(gle);
    }

    wchar_t* jobCtxW;
    gle = MPIU_MultiByteToWideChar( context->job_context, &jobCtxW );
    if( gle != NOERROR )
    {
        smpd_err_printf(L"Failed to convert job context string to unicode error %u\n", gle);
        CloseHandle(hRead);
        CloseHandle(hWriteRemote);
        return HRESULT_FROM_WIN32( gle );
    }

    //
    // Encode the command line
    //
    HRESULT hr = StringCchPrintfW(
        cmd,
        _countof(cmd),
        L"\"%s\" -p %hu -d %d -mgr %Iu \"%s\"%s",
        smpd_process.szExe,
        smpd_process.rootServerPort,
        dbg_state,
        reinterpret_cast<SIZE_T>(hWriteRemote),
        jobCtxW,
        smpd_process.local_root == TRUE ? L" -localonly" : L""
        );
    delete[] jobCtxW;
    if( FAILED(hr) )
    {
        smpd_err_printf(L"Unable to construct the command to launch the manager, error 0x%08x\n", hr);
        CloseHandle(hRead);
        CloseHandle(hWriteRemote);
        return hr;
    }

    ASSERT((ULONG_PTR)hWriteRemote == (ULONG)(ULONG_PTR)hWriteRemote);

    //
    // For backward compat with the MSPMS interface, we need
    // to pass the smpd name and the command line as char*
    //
    char* szExeA;
    gle = MPIU_WideCharToMultiByte( smpd_process.szExe, &szExeA );
    if( gle != NOERROR )
    {
        CloseHandle(hRead);
        CloseHandle(hWriteRemote);
        smpd_err_printf(L"Failed to convert smpd name %s to multibyte, error %u\n", smpd_process.szExe, gle);
        return HRESULT_FROM_WIN32( gle );
    }

    char* cmdA;
    gle = MPIU_WideCharToMultiByte( cmd, &cmdA );
    if( gle != NOERROR )
    {
        CloseHandle(hRead);
        CloseHandle(hWriteRemote);
        delete[] szExeA;
        smpd_err_printf(L"Failed to convert cmd %s to ascii error %u\n", cmd, gle);
        return HRESULT_FROM_WIN32( gle );
    }

    //
    // Fixup the azure environment if necessary.
    //
    AzureNetworkNames azureNames;
    bool restoreState = FixupAzureEnvironment( &azureNames );

    TOKEN_USER* pTokenUser = nullptr;
    bool restoreIdentity = false;

    RPC_STATUS status;
    switch( smpd_process.manager_interface->LaunchType )
    {
    case PmiLaunchTypeImpersonate:
        //
        // Impersonate the client user since we are going to authenticate.
        //
        if( context->hClientBinding == nullptr )
        {
            smpd_err_printf(L"Launch type requires impersonation, but no security context provided.\n");
            hr = SEC_E_NO_IMPERSONATION;
            break;
        }

        OACR_REVIEWED_CALL(
            mpicr,
            status = RpcImpersonateClient( context->hClientBinding ) );
        if( status != RPC_S_OK )
        {
            smpd_err_printf(L"RpcImpersonateClient failed, error %ld\n", status );
            hr = HRESULT_FROM_WIN32(status);
            break;
        }
        restoreIdentity = true;

        if (smpd_process.svcIfLaunch.StartLaunchCtx != nullptr)
        {
            hr = smpd_process.svcIfLaunch.StartLaunchCtx(context);
            if (FAILED(hr))
            {
                break;
            }
        }

        hr = smpd_process.manager_interface->Launch.Impersonate(
            szExeA,
            cmdA,
            context->job_context
            );
        break;

    case PmiLaunchTypeUserSid:
    {
        //
        // Get the TOKEN_USER for the client user.
        //
        if( context->hClientBinding == nullptr )
        {
            smpd_err_printf(L"Launch type requires user token, but no security context provided.\n");
            hr = SEC_E_NO_IMPERSONATION;
            break;
        }

        PSID pSid = nullptr;
#if !defined(MSMPI_NO_SEC)        
        if( !smpd_process.local_root )
        {
            AUTHZ_CLIENT_CONTEXT_HANDLE hAuthzClientContext;
            LUID luid = {0,0};
            status = RpcGetAuthorizationContextForClient(
                context->hClientBinding,
                FALSE,
                nullptr,
                nullptr,
                luid,
                0,
                nullptr,
                reinterpret_cast<PVOID*>( &hAuthzClientContext ) );
            if( status != RPC_S_OK )
            {
                smpd_err_printf(L"RpcGetAuthorizationContextForClient failed, error %ld\n", status );
                hr =  HRESULT_FROM_WIN32(status);
                break;
            }

            BOOL fSucc;
            DWORD BufferSize;
            fSucc = AuthzGetInformationFromContext(
                hAuthzClientContext,
                AuthzContextInfoUserSid,
                0,
                &BufferSize,
                nullptr );
            MPIU_Assert( fSucc == FALSE );

            pTokenUser = static_cast<PTOKEN_USER>( malloc( BufferSize ) );
            if( pTokenUser == nullptr )
            {
                smpd_err_printf(L"Failed to create token user. Out of memory\n");
                RpcFreeAuthorizationContext( reinterpret_cast<PVOID*>(&hAuthzClientContext) );
                hr = E_OUTOFMEMORY;
                break;
            }

            fSucc = AuthzGetInformationFromContext(
                hAuthzClientContext,
                AuthzContextInfoUserSid,
                BufferSize,
                &BufferSize,
                pTokenUser );
            RpcFreeAuthorizationContext( reinterpret_cast<PVOID*>(&hAuthzClientContext) );
            if( fSucc == FALSE )
            {
                DWORD gle = GetLastError();
                smpd_err_printf(L"AuthzGetInformationFromContext failed, error %u\n", gle );
                hr = HRESULT_FROM_WIN32(gle);
                break;
            }
            pSid = pTokenUser->User.Sid;
        }
#endif
        hr = smpd_process.manager_interface->Launch.UserSid(
            pSid,
            szExeA,
            cmdA,
            context->job_context
            );
    }
        break;

    case PmiLaunchTypeSelf:
        __fallthrough;

    default:
        hr = smpd_process.manager_interface->Launch.AsSelf(
            szExeA,
            cmdA,
            context->job_context
            );
        break;
    }

    delete[] szExeA;
    delete[] cmdA;
    CloseHandle(hWriteRemote);

    switch( smpd_process.manager_interface->LaunchType )
    {
    case PmiLaunchTypeImpersonate:
        MPIU_Assert( context->hClientBinding != nullptr ||
                     hr == SEC_E_NO_IMPERSONATION );

        if (smpd_process.svcIfLaunch.EndLaunchCtx != nullptr)
        {
            HRESULT tmpHr = smpd_process.svcIfLaunch.EndLaunchCtx(context);
            if( FAILED(tmpHr) )
            {
                smpd_err_printf(L"MSPMS EndLaunchCtx failed with error 0x%08x\n", tmpHr );
            }
        }

        if( restoreIdentity == false )
        {
            break;
        }

        status = RpcRevertToSelf();
        if( status != RPC_S_OK )
        {
            smpd_err_printf(L"RpcRevertToSelf failed, error %ld\n", status );
        }

        break;

    case PmiLaunchTypeUserSid:
        free( pTokenUser );
        __fallthrough;

    case PmiLaunchTypeSelf:
        __fallthrough;

    default:
        break;
    }

    //
    // If we changed our environment block for azure, be a good citizen
    // and undo the change.
    //
    if( restoreState )
    {
        RestoreAzureEnvironment( azureNames );
    }

    if( FAILED(hr) )
    {
        CloseHandle(hRead);
        smpd_err_printf(L"SMPD manager launch request failed with error 0x%08X.\n", hr);

        if(smpd_process.svcIfLaunch.CleanupLaunchCtx != nullptr)
        {
            HRESULT tmpHr = smpd_process.svcIfLaunch.CleanupLaunchCtx(context);
            if( FAILED(tmpHr) )
            {
                smpd_err_printf(L"MSPMS CleanupLaunchCtx failed with error 0x%08x\n", tmpHr);
            }
        }
        return hr;
    }

    smpd_dbg_printf(L"smpd reading the port string from the manager\n");

    //
    // Read the listener port from the pipe to the manager
    //
    if(!ReadFile(hRead, context->port_str, sizeof(context->port_str), &num_read, NULL))
    {
        gle = GetLastError();
        smpd_err_printf(L"ReadFile() failed, error %u\n", gle);
        CloseHandle(hRead);
        return HRESULT_FROM_WIN32(gle);
    }

    CloseHandle(hRead);
    if( num_read != SMPD_MAX_PORT_STR_LENGTH * sizeof(wchar_t) )
    {
        smpd_err_printf(L"partial port string read, %u bytes of %Iu\n",
                        num_read,
                        SMPD_MAX_PORT_STR_LENGTH * sizeof(wchar_t));
        return HRESULT_FROM_WIN32(ERROR_INCORRECT_SIZE);
    }
    smpd_dbg_printf(L"closing the pipe to the manager\n");

    return S_OK;
}
