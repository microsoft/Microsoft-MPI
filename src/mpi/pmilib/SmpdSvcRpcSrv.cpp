// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "smpd.h"
#include "rpcutil.h"
#include "SmpdRpc.h"

_Use_decl_annotations_
void __RPC_USER
SmpdSvcHandle_rundown( SmpdSvcHandle hContext )
{
    smpd_context_t* pSmpdContext = static_cast<smpd_context_t*>( hContext );

    if (smpd_process.svcIfLaunch.CleanupLaunchCtx != nullptr)
    {
        HRESULT hr = smpd_process.svcIfLaunch.CleanupLaunchCtx(pSmpdContext);
        if( FAILED(hr) )
        {
            smpd_err_printf(L"MSPMS CleanupLaunchCtx failed with error 0x%08x\n", hr);
        }
    }

    if (pSmpdContext != nullptr)
    {
        smpd_free_context( pSmpdContext );
    }

    smpd_err_printf(L" Connection to client died \n");
}


_Use_decl_annotations_
int __stdcall
RpcSrvCreateSmpdSvcContext(
    handle_t       hBinding,
    PCSTR          jobContextStr,
    UINT           clientPMPVersion,
    PCSTR          jobObjName,
    PCSTR          pwd,
    BOOL           saveCreds,
    SmpdSvcHandle* phContext
    )
{
    *phContext = nullptr;

    if( clientPMPVersion != SMPD_PMP_VERSION )
    {
        //
        // TODO: The launch svc should log an event log entry
        //
        smpd_err_printf(
            L"Process Management Protocol version mismatch in connection\n"
            L"request: received version %u, expected %u.\n",
            clientPMPVersion,
            SMPD_PMP_VERSION
            );
        return ERROR_INVALID_FUNCTION;
    }

    smpd_dbg_printf(
        L"version check complete, using PMP version %u.\n",
        SMPD_PMP_VERSION
        );

    //
    // Due to backcompat, jobObjName and pwd are coming in as ASCII, we
    // convert them to wchar for the rest of the code
    //
    wchar_t* jobObjNameW;
    DWORD err = MPIU_MultiByteToWideChar( jobObjName, &jobObjNameW );
    if( err != NOERROR )
    {
        smpd_err_printf(L"Failed to convert job object name %S to unicode error %u\n",
                        jobObjName, err );
        return err;
    }

    wchar_t* pwdW;
    err = MPIU_MultiByteToWideChar( pwd, &pwdW );
    if( err != NOERROR )
    {
        delete[] jobObjNameW;
        smpd_err_printf(L"Failed to process user credential error %u\n", err);
        return err;
    }

    smpd_context_t* pSmpdContext = smpd_create_context(
        SMPD_CONTEXT_ROOT,
        smpd_process.set,
        jobObjNameW,
        pwdW,
        saveCreds);

    if( pSmpdContext == nullptr )
    {
        delete [] pwdW;
        delete [] jobObjNameW;
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    delete [] pwdW;
    delete [] jobObjNameW;

    size_t length = MPIU_Strlen(jobContextStr) + 1;
    pSmpdContext->job_context = new char[length];
    if( pSmpdContext->job_context == nullptr )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    MPIU_Strcpy( pSmpdContext->job_context, length, jobContextStr );

    pSmpdContext->hClientBinding = hBinding;
    *phContext = reinterpret_cast<SmpdSvcHandle*>( pSmpdContext );

    if (smpd_process.svcIfLaunch.CreateLaunchCtx != nullptr)
    {
        RPC_STATUS rpcResult;
        OACR_REVIEWED_CALL(
            mpicr,
            rpcResult = RpcImpersonateClient(pSmpdContext->hClientBinding) );
        if (rpcResult != RPC_S_OK)
        {
            smpd_err_printf(
                L"Failed to impersonate RPC Client. Error 0x%x.\n",
                rpcResult
                );
            return rpcResult;
        }

        HANDLE clientToken;
        if (!OpenThreadToken(GetCurrentThread(), TOKEN_ALL_ACCESS, TRUE, &clientToken))
        {
            DWORD gle = GetLastError();

            smpd_err_printf(
                L"Failed to open thread token for impersonated RPC client. Error 0x%x.\n",
                gle
                );
            return HRESULT_FROM_WIN32(gle);
        }

        rpcResult = RpcRevertToSelfEx(pSmpdContext->hClientBinding);
        if (rpcResult != RPC_S_OK)
        {
            smpd_err_printf(
                L"Failed to revert from RPC Client. Error 0x%x.\n",
                rpcResult
                );
            return rpcResult;
        }

        err = smpd_process.svcIfLaunch.CreateLaunchCtx(
            clientToken,
            pSmpdContext,
            pSmpdContext->job_context);
    }

    return err;
}


_Use_decl_annotations_
HRESULT __stdcall
RpcSrvStartMgr(
    SmpdSvcHandle hContext,
    UINT16*       reconPort
    )
{
    *reconPort = 0;

    smpd_context_t* pSmpdContext = static_cast<smpd_context_t*>( hContext );
    if( pSmpdContext == nullptr )
    {
        //
        // Need a better return error code here
        //
        return HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
    }

    EnterCriticalSection( &smpd_process.svcCriticalSection );
    HRESULT hr = smpd_start_win_mgr(pSmpdContext, smpd_process.dbg_state);
    LeaveCriticalSection( &smpd_process.svcCriticalSection );

    if( FAILED(hr) )
    {
        return hr;
    }

    *reconPort = static_cast<UINT16>(_wtoi(pSmpdContext->port_str));
    return S_OK;
}


_Use_decl_annotations_
int __stdcall
RpcSrvDeleteSmpdSvcContext(
    SmpdSvcHandle *ppContext
    )
{
    smpd_context_t* pSmpdContext = reinterpret_cast<smpd_context_t*>( *ppContext );
    if( pSmpdContext == nullptr )
    {
        return NOERROR;
    }

    smpd_free_context( pSmpdContext );
    *ppContext = nullptr;

    return NOERROR;
}
