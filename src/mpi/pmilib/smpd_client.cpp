// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"
#include "Rpcutil.h"

_Success_(return == RPC_S_OK)
DWORD
smpd_connect_mgr_smpd(
    _In_     smpd_context_type_t context_type,
    _In_opt_z_ PCWSTR hostName,
    _In_     INT16 destId,
    _In_     PCWSTR portStr,
    _Outptr_ smpd_context_t** ppContext
    )
{
    //
    // OACR  complains that in fn_fail we don't set the ppContext,
    // The code only jumps to fn_fail in the event of status not RPC_S_OK,
    // which should satisfy the _Success_ contract above
    //
    OACR_USE_PTR( ppContext );
    smpd_context_t* pContext;

    pContext = smpd_create_context(context_type, smpd_process.set);
    if(pContext == nullptr)
    {
        smpd_err_printf(L"unable to create a new context for the reconnection.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    smpd_dbg_printf(
        L"%s posting a re-connect to %s:%s in %s context.\n",
        smpd_process.host,
        hostName == nullptr ? L"localhost" : hostName,
        portStr,
        smpd_get_context_str(pContext)
        );

    const wchar_t* pHost = nullptr;

    BOOL isMpiProcess = ( context_type == SMPD_CONTEXT_PMI_CLIENT );
    const wchar_t* protSeq;

    wchar_t port[64];
    ULONG authnSvc;
    if( isMpiProcess || smpd_process.local_root )
    {
        //
        // LRPC supports only NTLM.
        //
        protSeq = L"ncalrpc";
        authnSvc = RPC_C_AUTHN_WINNT;
        MPIU_Snprintf( port, _countof(port), L"%s", portStr );
    }
    else
    {
        if( env_is_on(L"MPIEXEC_USE_NP", FALSE) )
        {
            protSeq = L"ncacn_np";
            MPIU_Snprintf( port, _countof(port), L"\\pipe\\msmpi\\smpd\\%s", portStr );
        }
        else
        {
            protSeq = L"ncacn_ip_tcp";
            MPIU_Snprintf( port, _countof(port), L"%s", portStr );
        }
        pHost = hostName;

        if( smpd_process.authOptions == SMPD_AUTH_NTLM )
        {
            authnSvc = RPC_C_AUTHN_WINNT;
        }
        else
        {
            authnSvc = RPC_C_AUTHN_GSS_NEGOTIATE;
        }
    }

    RPC_AUTH_IDENTITY_HANDLE pAuthIdentity = nullptr;
    SEC_WINNT_AUTH_IDENTITY_EXW secAuth;
    if( smpd_process.authOptions == SMPD_AUTH_DISABLE_KERB &&
        authnSvc == RPC_C_AUTHN_GSS_NEGOTIATE )
    {
        InitializeAuthIdentity(
            disableKerbStr,
            _countof(DISABLE_KERB_STR) - 1,
            &secAuth );
        pAuthIdentity = &secAuth;
    }

    handle_t hBinding = nullptr;
    RPC_STATUS status = CreateRpcBinding( protSeq,
                                          pHost,
                                          port,
                                          RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                                          authnSvc,
                                          pAuthIdentity,
                                          &hBinding );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf( L"Failed to create binding to SMPD Manager Instance error %ld\n", status );
        goto fn_fail;
    }

    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD   len = _countof(computerName);
    if( GetComputerNameW( computerName, &len ) == 0 )
    {
        status = GetLastError();
        goto fn_fail;
    }

    SmpdMgrData mgrData;
    mgrData = { isMpiProcess,
                smpd_process.authOptions,
                destId,
                smpd_process.tree_id,
                smpd_process.tree_level + 1,
                smpd_process.mgrServerPort,
                smpd_process.localServerPort,
                smpd_process.nproc };

    //
    // Connect to the SMPD Manager Instance and identifying
    // that I am the parent. The SMPD instance will connect
    // back as a child.
    //
    RpcTryExcept
    {
        status = RpcCliCreateSmpdMgrContext(
            hBinding,
            SMPD_PMP_VERSION,
            computerName,
            reinterpret_cast<BYTE*>(&mgrData),
            sizeof(mgrData),
            &pContext->hServerContext );
    }
    RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
    {
        status = RpcExceptionCode();
    }
    RpcEndExcept;

    if( (status == ERROR_ACCESS_DENIED || status == RPC_S_SEC_PKG_ERROR) &&
        authnSvc == RPC_C_AUTHN_GSS_NEGOTIATE &&
        pAuthIdentity == nullptr &&
        !env_is_on(L"MPIEXEC_DISABLE_NTLM_FALLBACK",0) )
    {
        smpd_dbg_printf(L"Previous attempt failed with error %ld, "
                        L"trying to authenticate without Kerberos\n", status);

        //
        // We need a new binding to try again without Kerberos
        //
        RpcBindingFree( &hBinding );
        InitializeAuthIdentity(
            disableKerbStr,
            _countof(DISABLE_KERB_STR) - 1,
            &secAuth );
        pAuthIdentity = &secAuth;

        status = CreateRpcBinding(
            protSeq,
            pHost,
            port,
            RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
            authnSvc,
            pAuthIdentity,
            &hBinding );
        if( status != RPC_S_OK )
        {
            smpd_dbg_printf( L"Failed to create binding to SMPD Manager Instance error %ld\n", status );
            hBinding = nullptr;
            goto fn_fail;
        }

        RpcTryExcept
        {
            status = RpcCliCreateSmpdMgrContext(
                hBinding,
                SMPD_PMP_VERSION,
                computerName,
                reinterpret_cast<BYTE*>(&mgrData),
                sizeof(mgrData),
                &pContext->hServerContext );
        }
        RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
        {
            status = RpcExceptionCode();
        }
        RpcEndExcept;

    }
    if( status != RPC_S_OK )
    {
        goto fn_fail;
    }

    //
    // We no longer need the binding after obtaining the context
    //
    RpcBindingFree( &hBinding );

    MPIU_Strcpy(
        pContext->name, _countof(pContext->name),
        hostName == nullptr ? L"localhost" : hostName );
    *ppContext = pContext;

    return NOERROR;

fn_fail:
    if( hBinding != nullptr )
    {
        RpcBindingFree( &hBinding );
    }

    smpd_err_printf( L"Failed to connect to SMPD Manager Instance error %ld\n", status );
    smpd_post_abort_command(
        L"%S on %s is unable to connect to the smpd manager on %s:%s error %u\n",
        smpd_process.appname,
        smpd_process.host,
        smpd_process.local_root != FALSE? L"localhost" : hostName,
        portStr,
        status
        );

    smpd_free_context( pContext );
    return status;
}
