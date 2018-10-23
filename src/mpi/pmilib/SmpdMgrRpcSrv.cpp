// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "smpd.h"
#include "rpcutil.h"
#include "SmpdRpc.h"


static DWORD
InitChildSmpd(
    _In_ const SmpdMgrData* pMgrData
    )
{
    if( pMgrData->id < 0 )
    {
        smpd_err_printf(L"invalid id passed in session header: %hd\n", pMgrData->id);
        return ERROR_INVALID_DATA;
    }
    smpd_process.tree_id = pMgrData->id;

    if( pMgrData->parent < 0 )
    {
        smpd_err_printf(L"invalid parent id passed in session header: %hd\n", pMgrData->parent);
        return ERROR_INVALID_DATA;
    }
    smpd_process.parent_id = pMgrData->parent;

    if( pMgrData->level < 0 )
    {
        smpd_err_printf(L"invalid session level passed in session header: %hd\n", pMgrData->level);
        return ERROR_INVALID_DATA;
    }
    smpd_process.tree_level = pMgrData->level;

    if( pMgrData->worldSize == 0 || pMgrData->worldSize > MSMPI_MAX_RANKS )
    {
        smpd_err_printf(L"number of processes must be between 0 and %u\n",
                        static_cast<unsigned int>(MSMPI_MAX_RANKS));
        return ERROR_INVALID_DATA;
    }

    smpd_process.nproc = pMgrData->worldSize;

    if( pMgrData->authOptions < SMPD_AUTH_DEFAULT ||
        pMgrData->authOptions >= SMPD_AUTH_INVALID )
    {
        smpd_err_printf(L"authentication option must be between %d and %d\n", SMPD_AUTH_DEFAULT, SMPD_AUTH_INVALID );
        return ERROR_INVALID_DATA;
    }

    smpd_process.authOptions = pMgrData->authOptions;
    return NOERROR;
}


static
RPC_STATUS
ResolveParentAddress(
    _In_                   handle_t hBinding,
    _Out_writes_z_(length) PWSTR    parentHost,
    _In_                   size_t   length
    )
{

    RPC_BINDING_HANDLE parentBinding;

    //
    // Figure out the address of the parent by first obtaining
    // the partially bounded binding and then parse the network
    // address from the binding
    //
    RPC_STATUS status = RpcBindingServerFromClient(
        hBinding,
        &parentBinding );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf( L"failed to get parent binding error %ld.\n", status );
        return status;
    }

    wchar_t* bindingStr;
    status = RpcBindingToStringBindingW(
        parentBinding,
        &bindingStr );

    RpcBindingFree( &parentBinding );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf(L"failed to convert parent binding to string error %ld.\n",
                        status);
        return status;
    }

    wchar_t* tmpParentHost;
    status = RpcStringBindingParseW(
        bindingStr,
        nullptr,
        nullptr,
        &tmpParentHost,
        nullptr,
        nullptr
        );
    RpcStringFreeW( &bindingStr );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf(L"failed to retrieve parent network address error %ld.\n",
                        status);
        return status;
    }

    MPIU_Strcpy( parentHost, length, tmpParentHost );
    RpcStringFreeW( &tmpParentHost );
    return status;
}


_Success_(return == RPC_S_OK)
static RPC_STATUS
ConnectToParent(
    _In_opt_   handle_t         hBinding,
    _In_       PCWSTR           protSeq,
    _In_opt_z_ PCWSTR         parentHost,
    _In_       PCWSTR           portStr,
    _In_       UINT             authnSvc,
    _Outptr_   smpd_context_t** ppParentContext
    )
{
    //
    // OACR fails to reason the ppParentContext is only set when return status is RPC_S_OK
    //
    OACR_USE_PTR( ppParentContext );

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

    //
    // Use the provided parent address and the provided port to
    // construct a binding
    //
    RPC_BINDING_HANDLE parentBinding;
    RPC_STATUS status = CreateRpcBinding(
        protSeq,
        parentHost,
        portStr,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        authnSvc,
        pAuthIdentity,
        &parentBinding );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf(L"failed to create binding to callback to parent %ld.\n",
                        status );
        return status;
    }

    //
    // Allocate a context to use to call back to the parent
    //
    smpd_context_t* pParentContext = smpd_create_context(
        SMPD_CONTEXT_MGR_PARENT,
        smpd_process.set );
    if( pParentContext == nullptr )
    {
        RpcBindingFree( &parentBinding );
        return RPC_S_OUT_OF_MEMORY;
    }

    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD len = _countof(computerName);

    if( GetComputerNameW( computerName, &len ) == 0 )
    {
        DWORD gle = GetLastError();
        RpcBindingFree( &parentBinding );
        return gle;
    }

    SmpdMgrData mgrData = { FALSE,
                            smpd_process.authOptions,
                            smpd_process.tree_id,
                            0,
                            0,
                            0,
                            GUID_NULL,
                            0 };
    RpcTryExcept
    {
        status = RpcCliCreateSmpdMgrContext(
            parentBinding,
            SMPD_PMP_VERSION,
            computerName,
            reinterpret_cast<BYTE*>(&mgrData),
            sizeof(mgrData),
            &pParentContext->hServerContext );
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
        RpcBindingFree( &parentBinding );
        InitializeAuthIdentity(
            disableKerbStr,
            _countof(DISABLE_KERB_STR) - 1,
            &secAuth );
        pAuthIdentity = &secAuth;

        status = CreateRpcBinding(
            protSeq,
            parentHost,
            portStr,
            RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
            authnSvc,
            pAuthIdentity,
            &parentBinding );
        if( status != RPC_S_OK )
        {
            smpd_dbg_printf(L"failed to create binding to callback to parent %ld.\n",
                            status );
            return status;
        }

        RpcTryExcept
        {
            status = RpcCliCreateSmpdMgrContext(
                parentBinding,
                SMPD_PMP_VERSION,
                computerName,
                reinterpret_cast<BYTE*>(&mgrData),
                sizeof(mgrData),
                &pParentContext->hServerContext );
        }
        RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
        {
            status = RpcExceptionCode();
        }
        RpcEndExcept;

    }
    else if( status == RPC_S_SERVER_UNAVAILABLE && hBinding != nullptr )
    {
        //
        // We failed to connect back to the parent. Either the parent
        // machine is actively blocking the connection due to firewall
        // or the name of the parent cannot be resolved with DNS. We
        // retry by resolving the parent's address.
        //
        // We prevent doing this more than once by only passing in the parent
        // true binding the first time (hence the check for hBinding != nullptr)
        //
        wchar_t resolvedParentHost[SMPD_MAX_HOST_LENGTH];
        status = ResolveParentAddress( hBinding, resolvedParentHost, _countof(resolvedParentHost) );
        if( status == RPC_S_OK )
        {
            smpd_dbg_printf(L"Previous attempt failed, trying again with a resolved parent host %s:%s\n",
                            resolvedParentHost, portStr);
            status = ConnectToParent(
                nullptr,
                protSeq,
                resolvedParentHost,
                portStr,
                authnSvc,
                &pParentContext );
            if( status != RPC_S_OK )
            {
                smpd_free_context( pParentContext );
                RpcBindingFree( &parentBinding );
                return status;
            }
        }
    }

    if( status != RPC_S_OK )
    {
        smpd_dbg_printf(L"Failed to connect back to parent error %ld.\n", status );
        smpd_free_context( pParentContext );
    }
    else
    {
        pParentContext->on_cmd_error = SmpdHandleCmdError;
        *ppParentContext = pParentContext;
    }

    RpcBindingFree( &parentBinding );
    return status;
}


_Use_decl_annotations_
DWORD __stdcall
RpcSrvCreateSmpdMgrContext(
    handle_t       hBinding,
    UINT           clientPMPVersion,
    const wchar_t* parentHost,
    const BYTE*    pData,
    DWORD          dataLen,
    SmpdMgrHandle* phContext
    )
{
    *phContext = nullptr;

    if( clientPMPVersion != SMPD_PMP_VERSION )
    {
        smpd_err_printf(
            L"Process Management Protocol version mismatch in connection\n"
            L"request: received version %u, expected %u.\n",
            clientPMPVersion,
            SMPD_PMP_VERSION
            );
        smpd_process.abortExFunc();
        return ERROR_INVALID_FUNCTION;
    }

    smpd_dbg_printf(
        L"version check complete, using PMP version %u.\n",
        SMPD_PMP_VERSION
        );

    smpd_context_type_t context_type = SMPD_CONTEXT_MGR_PARENT;
    smpd_process_t* pMpiProc = nullptr;

    const SmpdMgrData* pMgrData = reinterpret_cast<const SmpdMgrData*>( pData );
    if( sizeof(*pMgrData) != dataLen )
    {
        smpd_err_printf(L"Received corrupted metadata from parent - aborting\n");
        smpd_process.abortExFunc();
        return ERROR_INVALID_DATA;
    }

    if( pMgrData->parentTcpPort != 0 || !IsEqualGUID( pMgrData->parentLrpcPort, GUID_NULL) )
    {
        ASSERT( pMgrData->isMpiProcess == FALSE );

        //
        // The case of the parent connecting to the child
        // id will be the id of the child (set by the parent)
        //
        smpd_dbg_printf(L"Received session header from parent id=%hd, parent=%hd, level=%hd\n",
                        pMgrData->id,
                        pMgrData->parent,
                        pMgrData->level );
        if( InitChildSmpd( pMgrData ) != NOERROR )
        {
            smpd_err_printf(L"Received bad data from parent - aborting \n");
            smpd_process.abortExFunc();
            return ERROR_INVALID_DATA;
        }

        const wchar_t* protSeq;
        wchar_t  portStr[GUID_STRING_LENGTH + 1];
        ULONG    authnSvc;

        if( smpd_process.local_root )
        {
            protSeq = L"ncalrpc";
            GuidToStr( pMgrData->parentLrpcPort, portStr, _countof(portStr) );
            authnSvc = RPC_C_AUTHN_WINNT;
        }
        else
        {
            if( env_is_on(L"MPIEXEC_USE_NP", FALSE ) )
            {
                protSeq = L"ncacn_np";
                MPIU_Snprintf( portStr, _countof(portStr), L"\\pipe\\msmpi\\smpd\\%u", pMgrData->parentTcpPort );
            }
            else
            {
                protSeq  = L"ncacn_ip_tcp";
                MPIU_Snprintf( portStr, _countof(portStr), L"%u", pMgrData->parentTcpPort );
            }
            if( smpd_process.authOptions == SMPD_AUTH_NTLM )
            {
                authnSvc = RPC_C_AUTHN_WINNT;
            }
            else
            {
                authnSvc = RPC_C_AUTHN_GSS_NEGOTIATE;
            }
        }

        smpd_dbg_printf( L"Connecting back to parent using host %s and endpoint %s\n",
                         parentHost,
                         portStr );

        RPC_STATUS status = ConnectToParent(
            hBinding,
            protSeq,
            smpd_process.local_root ? nullptr : parentHost ,
            portStr,
            authnSvc,
            &smpd_process.parent_context );
        if( status != RPC_S_OK )
        {
            smpd_err_printf(L"Failed to connect back to parent '%s:%s:%s' error %ld\n",
                            protSeq, parentHost, portStr, status);
            smpd_process.abortExFunc();
            return status;
        }
    }
    else
    {
        if( !pMgrData->isMpiProcess )
        {
            //
            // The case of the child connecting back to the parent
            // In this case the "id" will be the id of the child. The
            // child sends this id so the parent knows which side of
            // tree this child is from.
            //
            context_type = smpd_destination_context_type( pMgrData->id );
            ASSERT( context_type == SMPD_CONTEXT_LEFT_CHILD ||
                    context_type == SMPD_CONTEXT_RIGHT_CHILD );
        }
        else
        {
            context_type = SMPD_CONTEXT_PMI_CLIENT;
            pMpiProc = smpd_find_process_by_id( pMgrData->id );
            if( pMpiProc == nullptr )
            {
                smpd_err_printf(L"rejecting unknown process id %hd trying to connect.\n",
                                pMgrData->id);
                smpd_process.abortExFunc();
                return ERROR_INVALID_PARAMETER;
            }
        }
    }

    //
    // Allocate a context to keep track of client connection.
    //
    smpd_context_t* pContext = smpd_create_context(
        context_type,
        smpd_process.set );
    if( pContext == nullptr )
    {
        if( context_type == SMPD_CONTEXT_MGR_PARENT )
        {
            ASSERT( smpd_process.parent_context != nullptr );
            ASSERT( smpd_process.parent_context->hServerContext != nullptr );
            RPC_STATUS status = RPC_S_OK;
            RpcTryExcept
            {
                RpcCliDeleteSmpdMgrContext(
                    &(smpd_process.parent_context->hServerContext)
                    );
            }
            RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
            {
                status = RpcExceptionCode();
            }
            RpcEndExcept;

            if( status != RPC_S_OK )
            {
                smpd_dbg_printf(L"Failed to delete context during error clean up.\n");
            }
            smpd_free_context( smpd_process.parent_context );
        }

        if( !pMgrData->isMpiProcess )
        {
            smpd_process.abortExFunc();
        }
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    if( pMgrData->isMpiProcess )
    {
        ASSERT( pMpiProc != nullptr );
        pContext->process_id = pMgrData->id;

        //
        // Don't need to interlock this operation because at this time
        // the stdout and stderr have been setup already and thus the
        // context_refcount have been incremented.
        //
        pMpiProc->context_refcount++;
    }

    pContext->hClientBinding = hBinding;
    *phContext = reinterpret_cast<SmpdMgrHandle*>( pContext );

    return NOERROR;
}


_Use_decl_annotations_
void __stdcall
RpcSrvSmpdMgrCommandAsync(
    PRPC_ASYNC_STATE pAsync,
    SmpdMgrHandle    hContext,
    SMPD_CMD_TYPE    cmdType,
    SmpdCmd*         pCmd,
    SmpdRes*         pRes
    )
{
    smpd_context_t* pContext = static_cast<smpd_context_t*>( hContext );
    SmpdResHdr* pResHdr = reinterpret_cast<SmpdResHdr*>(pRes);
    if( pContext == nullptr )
    {
        goto fn_fail;
    }

    SmpdCmdHdr* pHeader = reinterpret_cast<SmpdCmdHdr*>( pCmd );
    if( pHeader->dest == smpd_process.tree_id )
    {
        //
        // This command is intended for me
        //
        smpd_overlapped_t* pov = smpd_create_overlapped();
        if( pov == nullptr )
        {
            goto fn_fail;
        }
        pov->pAsync = pAsync;
        pov->pCmd = pCmd;
        pov->pRes = pRes;
        smpd_init_overlapped( pov, smpd_handle_command, pContext );

        //
        // Post a completion to the EX engine so the command gets handled
        // when its turn comes
        //
        pov->uov.ov.Internal = NOERROR;
        ExPostOverlapped( smpd_process.set, &pov->uov );
        return;
    }

    HRESULT hr = S_OK;
    if( cmdType == SMPD_BCPUT || cmdType == SMPD_BCGET )
    {
        hr = SmpdProcessBizCard( false, pCmd, pRes );
    }

    //
    // Errors from SmpdProcessBizCard are not fatal
    //
    MPIU_Assert( SUCCEEDED(hr) );
    if( cmdType == SMPD_BCGET && hr == S_OK )
    {
        pResHdr->err = NOERROR;

        OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
            "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
        RpcAsyncCompleteCall( pAsync, nullptr );
        OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
            "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");

        return;
    }

    smpd_overlapped_t* pov = smpd_create_overlapped();
    if( pov == nullptr )
    {
        goto fn_fail;
    }
    pov->pAsync = pAsync;
    pov->pCmd = pCmd;
    pov->pRes = pRes;
    smpd_init_overlapped( pov, smpd_forward_command, pContext );

    //
    // Post a completion to the EX engine so the command gets handled
    // when its turn comes
    //
    pov->uov.ov.Internal = NOERROR;
    ExPostOverlapped( smpd_process.set, &pov->uov );
    return;

fn_fail:
    pResHdr->err = ERROR_NOT_ENOUGH_MEMORY;
    OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
    RpcAsyncCompleteCall(pAsync, nullptr);
    OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
    return;
}


_Use_decl_annotations_
void __stdcall
RpcSrvSmpdMgrCommandSync(
    SmpdMgrHandle    hContext,
    SMPD_CMD_TYPE    cmdType,
    SmpdCmd*         pCmd,
    SmpdRes*         pRes
    )
{
    //
    // Suppressing build warning/error for retail build
    //
    UNREFERENCED_PARAMETER( pCmd );

    smpd_context_t* pContext = static_cast<smpd_context_t*>( hContext );
    SmpdResHdr* pResHdr = reinterpret_cast<SmpdResHdr*>(pRes);
    if( cmdType != SMPD_CLOSE )
    {
        //
        // Right now the only command that should use sync. mode is
        // SMPD_CLOSE
        //
        smpd_err_printf(L"Unexpected command %s received by smpd.\n", CmdTypeToString(cmdType));
        pResHdr->err = ERROR_INVALID_PARAMETER;
        return;
    }

    ASSERT( pContext != nullptr );

    //
    // The only command supported by sync rpc mode is SMPD_CLOSE
    // and SMPD_CLOSE should always be from parent directly to child.
    // Thus we assert that this command is indeed intended for this
    // smpd, and no forwarding.
    //
    ASSERT( reinterpret_cast<SmpdCmdHdr*>(pCmd)->dest == smpd_process.tree_id );

    smpd_overlapped_t* pov = smpd_create_overlapped();
    if( pov == nullptr )
    {
        pResHdr->err = ERROR_NOT_ENOUGH_MEMORY;
        return;
    }
    pov->pAsync = nullptr;
    pov->pCmd = nullptr;
    pov->pRes = nullptr;

    //
    // TODO: We should refactor smpd_handle_command into
    // SmpdHandleAsyncCommand and introduce SmpdHandleSyncCommand
    // and call smpd_handle_close_command from within
    // SmpdHandleSyncCommand This should be ok-ish for now because
    // smpd_close is the only sync command (and hopefully remains
    // that way)
    //
    smpd_init_overlapped( pov, smpd_handle_close_command, pContext );

    //
    // Post a completion to the EX engine so the command gets handled
    // when its turn comes
    //
    pov->uov.ov.Internal = NOERROR;
    ExPostOverlapped( smpd_process.set, &pov->uov );
    pResHdr->err = NOERROR;
}


static int SmpdChildMgrExit( EXOVERLAPPED* )
{
    return MPI_SUCCESS;
}


static int SmpdProcessRundown(EXOVERLAPPED* pov)
{
    smpd_process_t* proc = CONTAINING_RECORD(pov, smpd_process_t, rundown_overlapped);
    return smpd_rundown_process(proc);
}


static
DWORD __stdcall
DeleteSmpdMgrContext(
    _Inout_ SmpdMgrHandle *ppContext,
    _In_    bool isRundown
    )
{
    smpd_context_t* pContext = reinterpret_cast<smpd_context_t*>( *ppContext );
    if( pContext == nullptr )
    {
        return NOERROR;
    }

    if( pContext->type == SMPD_CONTEXT_PMI_CLIENT )
    {
        smpd_process_t* pMpiProc = smpd_find_process_by_id( pContext->process_id );
        if( pMpiProc != nullptr )
        {
            ULONG refCnt = InterlockedDecrement( &pMpiProc->context_refcount);
            smpd_dbg_printf(L"process_id=%hu process refcount == %u, %s closed.\n",
                            pContext->process_id,
                            refCnt,
                            smpd_get_context_str(pContext));

            //
            // There are still two more context instances for the
            // stdout and stderr of this process. Those will get tear
            // down automatically when the process exits.
            //
            // However there are cases when the process is forcefully
            // killed by the debugger or when the process close its
            // stdout/stderr handle manually. In this case post an
            // overlapped call to main thread to rundown a process
            //
            if( refCnt == 0 )
            {
                ExInitOverlapped(
                    &(pMpiProc->rundown_overlapped),
                    SmpdProcessRundown,
                    SmpdProcessRundown
                    );
                ExPostOverlappedResult(
                    smpd_process.set, 
                    &(pMpiProc->rundown_overlapped),
                    0,
                    0
                    );
            }
        }
    }
    else if( !isRundown && pContext->type == SMPD_CONTEXT_MGR_PARENT )
    {
        //
        // The parent is closing our context in response to the
        // SMPD_CLOSED command. We will go ahead and clear the parent
        // context as well
        //
        RPC_STATUS status = RpcBindingSetOption(
            smpd_process.parent_context->hServerContext,
            RPC_C_OPT_CALL_TIMEOUT,
            5000 );
        if( status != RPC_S_OK )
        {
            smpd_dbg_printf( L"failed to set timeout for deleting parent context error %ld\n",
                             status );
        }

        RpcTryExcept
        {
            RpcCliDeleteSmpdMgrContext(
                &(smpd_process.parent_context->hServerContext)
                );
        }
        RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
        {
            status = RpcExceptionCode();
        }
        RpcEndExcept;

        if( status != RPC_S_OK )
        {
            //
            // The parent most likely already exited.
            //
            smpd_dbg_printf( L"error %ld while trying to delete parent context, "
                             L"parent likely has already exited.\n",
                             status );
        }

        smpd_free_context( smpd_process.parent_context );
        smpd_process.parent_context = nullptr;
        smpd_process.parent_closed = true;
        smpd_signal_exit_progress( NOERROR );

        //
        // Posting a dummy overlapped to the main thread's progress
        // engine so that it will exit cleanly.
        //
        static EXOVERLAPPED ov;
        ExInitOverlapped( &ov, SmpdChildMgrExit, SmpdChildMgrExit );
        ExPostOverlappedResult( smpd_process.set, &ov, 0, 0 );
    }

    smpd_free_context( pContext );
    *ppContext = nullptr;

    return NOERROR;
}


_Use_decl_annotations_
DWORD __stdcall
RpcSrvDeleteSmpdMgrContext(
    SmpdMgrHandle *ppContext
    )
{
    return DeleteSmpdMgrContext( ppContext, false );
}


_Use_decl_annotations_
void __RPC_USER
SmpdMgrHandle_rundown( SmpdMgrHandle hContext )
{
    smpd_context_t* pContext = static_cast<smpd_context_t*>( hContext );

    ASSERT( pContext != nullptr );

    if( pContext->type == SMPD_CONTEXT_MGR_PARENT )
    {
        DeleteSmpdMgrContext( &hContext, true );
        smpd_dbg_printf(L"parent terminated unexpectedly - initiating cleaning up.\n");

        //
        // The parent exited unexpectedly. We will kill the processes
        // and then terminate. When this process terminates it will
        // also trigger the chain reaction for the children.
        //
        smpd_process.parent_closed = true;
        if( smpd_get_first_process() == nullptr )
        {
            smpd_dbg_printf(L"no child processes to kill - exiting with error code -1\n");
            fflush( nullptr );
            exit( -1 );
        }
        else
        {
            smpd_dbg_printf(L"kill all child processes and exiting with error code -1\n");
            smpd_kill_all_processes();
            smpd_signal_exit_progress( -1 );

            //
            // Give the engine time to clean up and then terminate
            //
            Sleep( 10000 );
            smpd_dbg_printf(L"Waited more than 10 seconds for child processes to exit - exiting with error code -1\n");
            exit( -1 );
        }
    }

    else if( pContext->type == SMPD_CONTEXT_LEFT_CHILD ||
             pContext->type == SMPD_CONTEXT_RIGHT_CHILD )
    {
        DeleteSmpdMgrContext( &hContext, true );
        smpd_dbg_printf(L"child smpd terminated unexpectedly.\n");

        if( smpd_process.closing )
        {
            smpd_signal_exit_progress(0);
            smpd_process.abortExFunc();
            return;
        }

        if( smpd_process.tree_id == 0 )
        {
            smpd_post_abort_command(
                L"%S on %s failed to communicate with smpd manager on %s\n",
                smpd_process.appname,
                smpd_process.host,
                smpd_process.host_list->name );

            smpd_signal_exit_progress(-1);
            smpd_process.mpiexec_exit_code = -1;
            smpd_process.abortExFunc();
        }
        else
        {
            smpd_post_abort_command(
                L"%S on %s failed to communicate with child smpd manager\n",
                smpd_process.appname,
                smpd_process.host );
        }
        return;
    }

    RpcSrvDeleteSmpdMgrContext( &hContext );
}
