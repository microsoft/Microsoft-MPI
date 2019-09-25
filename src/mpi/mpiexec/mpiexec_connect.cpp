// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiexec.h"


static void
mpiexec_handle_connect_result(
    _In_ smpd_overlapped_t* pov
    )
{
    for( smpd_host_t* pHost = smpd_process.host_list;
         pHost != nullptr;
         pHost = static_cast<smpd_host_t*>( pHost->Next ) )
    {
        if( pHost->HostId == pov->pCmd->ConnectCmd.HostId )
        {
            SmpdConnectRes* pConnectRes = &pov->pRes->ConnectRes;

            if (pConnectRes->userInfoNeeded)
            {
                if (!MpiexecGetUserInfoInteractive())
                {
                    //
                    // Session is not interactive, cannot perform this operation. Simply fail.
                    //
                    smpd_post_abort_command(L"Invalid Credentials: Failed to connect the smpd tree.");
                    return;
                }

                //
                // Reconnect with the user provided information
                //
                mpiexec_send_connect_command(pov->pContext, pHost);

                return;
            }

            smpd_dbg_printf(L"successful connect to %s.\n", pHost->name);
            DWORD rc = mpiexec_handle_node_connection( pov->pContext, pHost );
            if( rc == NOERROR )
            {
                return;
            }

            smpd_post_abort_command(L"failed to connect the smpd tree. error %d\n", rc);
            return;
        }
    }
}


DWORD
mpiexec_send_connect_command(
    _In_ smpd_context_t*    pContext,
    _In_ const smpd_host_t* host
    )
{
    smpd_dbg_printf(L"creating connect command to '%s'\n", host->name);

    SmpdCmd* pCmd = smpd_create_command(
        SMPD_CONNECT,
        SMPD_IS_ROOT,
        static_cast<INT16>(host->parent) );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create a connect command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    int rc = MPIU_Strcpy( pCmd->ConnectCmd.hostName,
                          _countof(pCmd->ConnectCmd.hostName),
                          host->name );
    if( rc != MPI_SUCCESS )
    {
        smpd_err_printf( L"unable to add the host parameter to the connect command for host %s\n",
                         host->name );
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pCmd->ConnectCmd.HostId = static_cast<UINT16>(host->HostId);
    pCmd->ConnectCmd.retryCount = smpd_process.connectRetryCount;
    pCmd->ConnectCmd.retryInterval = smpd_process.connectRetryInterval;
    pCmd->ConnectCmd.jobObjName = smpd_process.jobObjName;
    pCmd->ConnectCmd.pwd = smpd_process.pwd;
    pCmd->ConnectCmd.saveCreds = smpd_process.saveCreds;

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        &mpiexec_handle_connect_result );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for connect command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD ret = smpd_post_command( pContext,
                                   pCmd,
                                   &pRes->Res,
                                   nullptr );
    if( ret != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"Unable to post connect command error %u.\n", ret);
    }

    return ret;
}


static DWORD
mpiexec_connect_node_children(
    _In_ smpd_context_t* context,
    _In_ smpd_host_t*    host
    )
{
    DWORD rc;
    smpd_host_t* left = host->left;
    smpd_host_t* right = host->right;

    if( left != nullptr )
    {
        smpd_dbg_printf(L"creating connect command for left node\n");

        rc = mpiexec_send_connect_command( context, left );
        if( rc != NOERROR )
        {
            return rc;
        }
    }

    if( right != nullptr )
    {
        smpd_dbg_printf(L"creating connect command for right node\n");

        rc = mpiexec_send_connect_command( context, right );
        if( rc != NOERROR )
        {
            return rc;
        }
    }

    return NOERROR;
}


static bool
mpiexec_tree_is_connected()
{
    const smpd_host_t* host;
    for( host = smpd_process.host_list;
         host != nullptr;
         host = static_cast<const smpd_host_t*>( host->Next ) )
    {
        if(!host->connected)
        {
            smpd_dbg_printf(L"host %s is not connected yet\n", host->name);
            return false;
        }
    }
    return true;
}


DWORD
mpiexec_handle_node_connection(
    _In_  smpd_context_t* context,
    _Out_ smpd_host_t*    host
    )
{
    host->connected = TRUE;

    DWORD rc = mpiexec_connect_node_children(context, host);
    if( rc != NOERROR )
    {
        return rc;
    }

    if( mpiexec_tree_is_connected() == false )
    {
        return NOERROR;
    }

    return mpiexec_send_collect_commands();
}
