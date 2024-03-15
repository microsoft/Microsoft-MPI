// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"
#include "rpcutil.h"


static void
root_client_abort_connect(
    _In_ PCWSTR host_name,
    _In_ int error
    )
{
    smpd_post_abort_command(
        L"%S on %s is unable to connect to the smpd service on %s:%d\n%S\n",
        smpd_process.appname,
        smpd_process.host,
        host_name,
        smpd_process.rootServerPort,
        get_sock_error_string(
            MPIU_ERR_CREATE(MPI_ERR_OTHER, "**sock_connect %s %d", get_error_string(error), error ) )
        );
}


//
// TODO: Move the following into a common place
//
#define MPIU_E_FAIL_GLE2(gle_) \
    MPIU_ERR_CREATE(MPI_ERR_OTHER, "**fail %s %d", get_error_string(gle_), gle_)

void
SmpdHandleCmdError(
    _In_ smpd_overlapped_t* pov,
    _In_ int error
    )
{
    UNREFERENCED_PARAMETER( pov );
    smpd_dbg_printf(
        L"error %d detected during command processing. Killing processes and exiting.\n",
        error );
    smpd_kill_all_processes();
    smpd_signal_exit_progress( error );
    return;
}


//
// smpd client connect completion callback. the smpd client framework calls this
// function after the connection to the smpd manager has been established
//
static void
smpd_connected_child_mgr(
    _In_ smpd_context_t* context
    )
{
    //
    // Set the left/right child context
    //
    ASSERT( context->type == SMPD_CONTEXT_LEFT_CHILD ||
            context->type == SMPD_CONTEXT_RIGHT_CHILD );

    context->on_cmd_error = SmpdHandleCmdError;
    if(context->type == SMPD_CONTEXT_LEFT_CHILD)
    {
        ASSERT(smpd_process.left_context == nullptr);
        smpd_process.left_context = context;
    }
    else if(context->type == SMPD_CONTEXT_RIGHT_CHILD)
    {
        ASSERT(smpd_process.right_context == nullptr);
        smpd_process.right_context = context;
    }
}


DWORD
smpd_connect_root_server(
    _In_ smpd_context_type_t context_type,
    _In_ int                 HostId,
    _In_ PCWSTR              name,
    _In_ bool                isChildSmpd,
    _In_ UINT32              retryCount,
    _In_ UINT32              retryInterval
    )
{
    ASSERT(HostId > smpd_process.tree_id);
    ASSERT(context_type == SMPD_CONTEXT_LEFT_CHILD
           || context_type == SMPD_CONTEXT_RIGHT_CHILD);
    ASSERT(context_type != SMPD_CONTEXT_LEFT_CHILD
           || smpd_process.left_context == nullptr);
    ASSERT(context_type != SMPD_CONTEXT_RIGHT_CHILD
           || smpd_process.right_context == nullptr);

    if(context_type == SMPD_CONTEXT_LEFT_CHILD)
    {
        smpd_process.left_child_id = (INT16)HostId;
    }
    else if(context_type == SMPD_CONTEXT_RIGHT_CHILD)
    {
        smpd_process.right_child_id = (INT16)HostId;
    }

    wchar_t portStrW[6];
    MPIU_Snprintf( portStrW, _countof(portStrW), L"%hu", smpd_process.rootServerPort );

    char* jobObjNameA = nullptr;
    char* pwdA = nullptr;
    char* nameA = nullptr;

    int mpi_errno = MPIU_WideCharToMultiByte( name, &nameA );
    if( mpi_errno != MPI_SUCCESS )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    RPC_AUTH_IDENTITY_HANDLE pAuthIdentity = nullptr;
    SEC_WINNT_AUTH_IDENTITY_EXW secAuth;
    if( smpd_process.authOptions == SMPD_AUTH_DISABLE_KERB )
    {
        InitializeAuthIdentity(
            disableKerbStr,
            _countof(DISABLE_KERB_STR) - 1,
            &secAuth );
        pAuthIdentity = &secAuth;
    }

    //
    // Create the RPC binding to connect to the SMPD Service
    //
    handle_t hBinding;
    RPC_STATUS status = CreateRpcBinding(
        L"ncacn_ip_tcp",
        name,
        portStrW,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        smpd_process.authOptions == SMPD_AUTH_NTLM ? RPC_C_AUTHN_WINNT : RPC_C_AUTHN_GSS_NEGOTIATE,
        pAuthIdentity,
        &hBinding );
    if( status != RPC_S_OK )
    {
        smpd_err_printf(L"Failed CreateRpcBinding error %ld\n", status );
        goto CleanUp;
    }

    wchar_t* pSpn = nullptr;
    status = RpcBindingInqAuthInfoW(
        hBinding,
        &pSpn,
        nullptr,
        nullptr,
        nullptr,
        nullptr );
    if( status == RPC_S_OK )
    {
        smpd_dbg_printf(L"using spn %s to contact server\n", pSpn);
        RpcStringFreeW( &pSpn );
    }

    status = MPIU_WideCharToMultiByte( smpd_process.jobObjName, &jobObjNameA );
    if( status != NOERROR )
    {
        smpd_err_printf(L"Failed to convert job object name to unicode error %ld\n",
                        status );
        goto CleanUp;
    }

    status = MPIU_WideCharToMultiByte( smpd_process.pwd, &pwdA );
    if( status != NOERROR )
    {
        smpd_err_printf(L"Failed to process user credential error %ld\n",
                        status );
        goto CleanUp;
    }

    PVOID hServerContext = nullptr;
    ULONG retry = 0;
    do
    {
        RpcTryExcept
        {
            status = RpcCliCreateSmpdSvcContext(
                hBinding,
                smpd_process.job_context,
                SMPD_PMP_VERSION,
                jobObjNameA,
                pwdA,
                smpd_process.saveCreds,
                &hServerContext );
        }
        RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
        {
            status = RpcExceptionCode();
        }
        RpcEndExcept;
        if( status == RPC_S_SERVER_UNAVAILABLE ||
            status == RPC_S_SERVER_TOO_BUSY )
        {
            if( ++retry >= retryCount )
            {
                break;
            }
            smpd_dbg_printf(
                L"Failed to connect with error %ld, retrying...\n", status);
            Sleep( retryInterval * 1000 );
            continue;
        }
        else
        {
            if( (status == ERROR_ACCESS_DENIED ||
                 status == RPC_S_SEC_PKG_ERROR) &&
                pAuthIdentity == nullptr &&
                !env_is_on(L"MPIEXEC_DISABLE_NTLM_FALLBACK",0) )
            {
                //
                // We will need a new binding to try again without Kerberos.
                //
                RpcBindingFree( &hBinding );

                smpd_dbg_printf(L"Previous attempt failed with error %ld, "
                                L"trying to authenticate without Kerberos\n", status);

                InitializeAuthIdentity(
                    disableKerbStr,
                    _countof(DISABLE_KERB_STR) - 1,
                    &secAuth );
                pAuthIdentity = &secAuth;

                status = CreateRpcBinding(
                    L"ncacn_ip_tcp",
                    name,
                    portStrW,
                    RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                    RPC_C_AUTHN_GSS_NEGOTIATE,
                    pAuthIdentity,
                    &hBinding );
                if( status != RPC_S_OK )
                {
                    smpd_err_printf(L"Failed CreateRpcBinding error %ld\n", status );
                    hBinding = nullptr;
                    break;
                }

                //
                // Reset the retrying counter to give the new binding a fair chance
                //
                retry = 0;
                continue;
            }
            else
            {
                //
                // Either we got some other non-recoverable errors or
                // we have already disabled Kerberos and still could
                // not connect.
                //
                break;
            }
        }
    } while(true);

    if( status != RPC_S_OK )
    {
        if( hBinding != nullptr )
        {
            RpcBindingFree( &hBinding );
        }

        smpd_err_printf(L"Failed RpcCliCreateContext error %ld\n", status );

        if( status == RPC_S_UNKNOWN_IF )
        {
            smpd_err_printf(L"Unknown or mismatched interface."
                            L" Ensure that the versions of mpiexec and smpd service/daemon are identical\n");
        }
        root_client_abort_connect(name, status);
        goto CleanUp;
    }

    //
    // Now that we have the context handle, the binding is no longer needed.
    //
    RpcBindingFree( &hBinding );
    ASSERT( hServerContext != nullptr );

    //
    // Request that the SMPD Service launch the SMPD Manager instance
    // and provide us with a port number so that we can connect
    // to the freshly launched SMPD Manager instance
    //
    UINT16 reconnectPort = 0;
    retry = 0;

    do
    {
        RpcTryExcept
        {
            status = RpcCliStartMgr( hServerContext, &reconnectPort );
        }
        RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
        {
            status = RpcExceptionCode();
        }
        RpcEndExcept;

        if (status == NTE_UI_REQUIRED)
        {
            //
            // Password is either not provided or changed or reset.
            //
            smpd_dbg_printf(L"Needs user password to start SMPD manager.\n");
            goto CleanUp;
        }

        //
        // We do not retry on RPC_S_SERVER_UNAVAILABLE here because
        // we just got the context from the server.
        //
        if( status == RPC_S_SERVER_TOO_BUSY &&
            ++retry < retryCount )
        {
            smpd_dbg_printf(
                L"Failed to start manager because the server is busy, retrying...\n");
            Sleep( retryInterval * 1000 );
            continue;
        }
        break;
    } while (true);

    //
    // Save the error status of the StartMgr call
    //
    // In the case that we failed the StartMgr call, delete the
    // context that we previously obtained. This will also give the
    // server side the chance for it to clean up the context.  We
    // intentionally ignore the status of the delete context call
    // because we will be aborting in just a second due to the
    // previous error anyway.
    //
    RPC_STATUS startMgrStatus = status;

    retry = 0;
    do
    {
        RpcTryExcept
        {
            status = RpcCliDeleteSmpdSvcContext( &hServerContext );
        }
        RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
        {
            status = RpcExceptionCode();
        }
        RpcEndExcept;

        if( status == RPC_S_SERVER_TOO_BUSY &&
            ++retry < retryCount )
        {
            smpd_dbg_printf(
                L"Failed to delete the context because the server is busy, retrying...\n");
            Sleep( retryInterval * 1000 );
            continue;
        }
        break;
    } while (true);

    if( startMgrStatus != RPC_S_OK )
    {
        smpd_err_printf(L"Failed RpcCliStartMgr error %ld\n", startMgrStatus );
        root_client_abort_connect(name, startMgrStatus);
        status = startMgrStatus;
        goto CleanUp;
    }

    if( status != RPC_S_OK )
    {
        smpd_err_printf(L"Failed to disconnect from SMPD Service error %ld\n", status );
        root_client_abort_connect(name, status);
        goto CleanUp;
    }

    smpd_context_t* pContext;
    MPIU_Snprintf( portStrW, _countof(portStrW), L"%hu", reconnectPort );

    //
    // Connect to the SMPD Manager Instance
    //
    status = smpd_connect_mgr_smpd( context_type,
                                    name,
                                    static_cast<INT16>(HostId),
                                    portStrW,
                                    &pContext );
    if( status != NOERROR )
    {
        goto CleanUp;
    }

    if( isChildSmpd )
    {
        smpd_connected_child_mgr( pContext );
    }
    else
    {
        //
        // This is the root (mpiexec), so assign the context to the
        // left child
        //
        ASSERT( pContext->type == SMPD_CONTEXT_LEFT_CHILD);
        ASSERT( smpd_process.tree_id == 0);

        ASSERT( smpd_process.left_context == nullptr );

        smpd_process.left_context = pContext;
    }

    return NOERROR;

CleanUp:
    delete [] nameA;
    delete [] jobObjNameA;
    delete [] pwdA;
    return status;
}
