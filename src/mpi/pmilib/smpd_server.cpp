// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "smpd.h"
#include "rpcutil.h"


DWORD
smpd_create_mgr_server(
    _Inout_opt_ UINT16* pPort,
    _Out_opt_   GUID*   pLrpcPort
    )
{
    const wchar_t* pPortStr;
    wchar_t portStr[6];
    if( pPort == nullptr || *pPort == 0 )
    {
        pPortStr = nullptr;
    }
    else
    {
        MPIU_Snprintf( portStr, _countof(portStr), L"%u", *pPort );
        pPortStr = portStr;
    }

    RPC_STATUS status = StartRpcServer( L"ncacn_ip_tcp",
                                        pPortStr,
                                        RpcSrvSmpdMgrRpc_v1_0_s_ifspec,
                                        SmpdSecurityCallbackFn,
                                        pPort,
                                        pLrpcPort,
                                        RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                                        smpd_process.local_root );
    if( status != RPC_S_OK )
    {
        return status;
    }

    return NOERROR;
}


void
smpd_stop_mgr_server()
{
    RPC_STATUS status = StopRpcServer( RpcSrvSmpdMgrRpc_v1_0_s_ifspec );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf(L"smpd manager failed to stop rpc server, error: %ld\n", status );
    }
    else
    {
        smpd_dbg_printf(L"smpd manager successfully stopped listening.\n");
    }
}
