// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"
#include "affinity_calculation.h"

#define ERR_MSG_SIZE    512


DWORD
smpd_handle_invalid_command(
    _In_ smpd_overlapped_t* pov
    )
{
    UNREFERENCED_PARAMETER( pov );
    smpd_err_printf(L"Internal error: Invalid command received by SMPD.\n");
    smpd_post_abort_command(L"Internal error: Invalid command received by SMPD.\n");

    return ERROR_INVALID_DATA;
}


//
// Summary:
//  Send results for the collect message back to MPIEXEC
//
// Parameters:
//  pov         - pointer to current context
//  cbSummary   - size of summary
//  pSummary    - pointer to HWSUMMARY to send back to MPIEXEC
//  cbTree      - size of tree
//  pTree       - pointer to HWTREE to send back to MPIEXEC
//  hostId      - the hostid provided in the request.
//  sendAffinity- whether affinity is requested back or not
//  sendHwTree  - whether hwtree is requested back or not
//
static DWORD
smpd_send_collect_results(
    _In_     smpd_overlapped_t*     pov,
    _In_     int                    cbSummary,
    _In_opt_ HWSUMMARY*             pSummary,
    _In_     int                    cbTree,
    _In_opt_ HWTREE*                pTree,
    _In_     UINT16                 hostId,
    _In_     BOOL                   sendAffinity,
    _In_     BOOL                   sendHwTree
    )
{
    SmpdCollectRes* pRes = &pov->pRes->CollectRes;
    pRes->HostId = hostId;
    pRes->nodeProcCount = pov->pContext->processCount;

    AffinityOptions            affinityOptions = pov->pCmd->CollectCmd.affOptions;
    if( sendAffinity && affinityOptions.isSet )
    {
        pRes->cbAffList = pov->pContext->processCount * sizeof(HWAFFINITY);
        pRes->affList = static_cast<BYTE*>(midl_user_allocate( pRes->cbAffList ));
        if( pRes->affList == nullptr )
        {
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        CopyMemory( pRes->affList, pov->pContext->affinityList, pRes->cbAffList );
        pRes->cbHwSummary = cbSummary;
        pRes->hwsummary = reinterpret_cast<BYTE*>(pSummary);
    }
    else
    {
        //
        // Unnecessary to send affinity related info. Act as if they don't exist
        //
        pRes->cbAffList = 0;
        pRes->affList = nullptr;
        pRes->cbHwSummary = 0;
        pRes->hwsummary = nullptr;
    }

    if( sendHwTree && affinityOptions.isSet )
    {
        pRes->cbHwTree = cbTree;
        pRes->hwtree = reinterpret_cast<BYTE*>(pTree);
    }
    else
    {
        //
        // Unnecessary to send hwtree. Act as if it doesn't exist
        //
        pRes->cbHwTree = 0;
        pRes->hwtree = nullptr;
    }

    return NOERROR;
}


//
// Summary:
//  Handler for the collect command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_collect_command(
    _In_ smpd_overlapped_t* pov
    )
{
    HWVIEW*                    pView      = nullptr;
    HWTREE*                    pTree      = nullptr;
    HWSUMMARY*                 pSummary   = nullptr;
    HRESULT                    hr = S_OK;
    unsigned int               cbView       = 0;
    unsigned int               cbTree       = 0;
    unsigned int               cbSummary    = 0;
    HANDLE                     arLock = nullptr;
    SmpdCollectCmd* pCollectCmd = &pov->pCmd->CollectCmd;
    AffinityOptions            affinityOptions = pCollectCmd->affOptions;

    if( affinityOptions.isSet == 0 )
    {
        goto SendResponse;
    }


    BOOL    checkOccupied = FALSE;
    if(affinityOptions.isAuto)
    {
        arLock = LockAutoAffinity();
        checkOccupied = arLock != nullptr;
    }

    wchar_t errorMsg[ERR_MSG_SIZE];
    hr = ConstructHwInfo(
        checkOccupied,
        &cbView,
        &pView,
        &cbTree,
        &pTree,
        &cbSummary,
        &pSummary,
        errorMsg,
        ERR_MSG_SIZE);

    if(hr != S_OK)
    {
        goto fail_exit;
    }

    pov->pContext->processCount = pCollectCmd->nodeProcCount;

    size_t affinityLen = sizeof(HWAFFINITY) * pov->pContext->processCount;

    pov->pContext->affinityList = static_cast<HWAFFINITY*>(malloc(affinityLen));
    if(pov->pContext->affinityList == nullptr)
    {
        hr = E_OUTOFMEMORY;
        goto fail_exit;
    }

    if( affinityOptions.isExplicit )
    {
        CopyMemory( pov->pContext->affinityList,
                    pov->pCmd->CollectCmd.ExplicitAffinity,
                    pov->pCmd->CollectCmd.cbExplicitAffinity );
    }

    hr = SetAffinity(&affinityOptions, pSummary, pView, pov->pContext, errorMsg, ERR_MSG_SIZE);
    if(hr != S_OK)
    {
        goto fail_exit;
    }

    if(affinityOptions.isAuto && checkOccupied)
    {
        if( WriteAutoAffinity(pov->pContext, pSummary->Count) != S_OK )
        {
            goto fail_exit;
        }

        UnlockAutoAffinity(arLock);
    }

SendResponse:
    /* send the result command */
    DWORD rc = smpd_send_collect_results(
        pov,
        cbSummary,
        pSummary,
        cbTree,
        pTree,
        pCollectCmd->HostId,
        affinityOptions.affinityTableStyle > 0 || affinityOptions.hwTableStyle > 0,
        affinityOptions.hwTableStyle > 0
        );

    if( rc != NOERROR )
    {
        hr = HRESULT_FROM_WIN32( rc );
        goto fail_exit;
    }

    if( affinityOptions.isSet && !affinityOptions.affinityTableStyle )
    {
        midl_user_free( pSummary );
    }

    if( affinityOptions.isSet && !affinityOptions.hwTableStyle )
    {
        midl_user_free( pTree );
    }

    midl_user_free(pView);
    return NOERROR;

fail_exit:
    midl_user_free(pView);
    midl_user_free(pTree);
    midl_user_free(pSummary);
    if (arLock != nullptr)
    {
        UnlockAutoAffinity(arLock);
    }

    pov->pRes->CollectRes.cbAffList = 0;
    pov->pRes->CollectRes.affList = nullptr;
    pov->pRes->CollectRes.cbHwSummary = 0;
    pov->pRes->CollectRes.hwsummary = nullptr;
    pov->pRes->CollectRes.cbHwTree = 0;
    pov->pRes->CollectRes.hwtree = nullptr;

    // the two routines that set errorMsg do not return a printf format string; any
    // required formatting is done in the routines themselves.
    OACR_WARNING_SUPPRESS(PRINTF_FORMAT_STRING_PARAM_NEEDS_REVIEW, "error message is returned by routines called by this function");
    smpd_post_abort_command( errorMsg );

    return static_cast<DWORD>( hr );
}


//
// Summary:
//  Handler for the dbput command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_dbput_command(
    _In_ smpd_overlapped_t* pov
    )
{
    const SmpdDbputCmd* pDbputCmd = &pov->pCmd->DbputCmd;

    const char* key = pDbputCmd->key;
    const char* value = pDbputCmd->value;

    bool fSucc = smpd_dbs_put(pDbputCmd->kvs, key, value);
    smpd_dbg_printf( L"Handling SMPD_DBPUT command from smpd %hd%\n"
                     L"\tctx_key=%hu\n"
                     L"\tkey=%S\n"
                     L"\tvalue=%S\n"
                     L"\tresult=%s\n",
                     pDbputCmd->header.src,
                     pDbputCmd->header.ctx_key,
                     key,
                     value,
                     fSucc ? L"success":L"failed");

    if( !fSucc )
    {
        return ERROR_INVALID_DATA;
    }
    return NOERROR;
}


//
// Summary:
//  Handler for the bcput command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_bcput_command(
    _In_ smpd_overlapped_t* pov
    )
{
    const SmpdBcputCmd* pBcputCmd = &pov->pCmd->BcputCmd;

    UINT16 rank = pBcputCmd->rank;
    const char* value = pBcputCmd->value;

    bool fSucc = smpd_dbs_bcput(pBcputCmd->kvs, rank, value);
    smpd_dbg_printf( L"Handling SMPD_BCPUT command from smpd %hd%\n"
                     L"\tctx_key=%hu\n"
                     L"\trank=%hu\n"
                     L"\tvalue=%S\n"
                     L"\tresult=%s\n",
                     pBcputCmd->header.src,
                     pBcputCmd->header.ctx_key,
                     rank,
                     value,
                     fSucc ? L"success":L"failed");

    if( !fSucc )
    {
        return ERROR_INVALID_DATA;
    }
    return NOERROR;
}


//
// Summary:
//  Handler for the dbget command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_dbget_command(
    _In_ smpd_overlapped_t* pov
    )
{
    const SmpdDbgetCmd* pDbgetCmd = &pov->pCmd->DbgetCmd;
    const char* key = pDbgetCmd->key;

    BOOL fSucc = smpd_dbs_get(
        pDbgetCmd->kvs,
        key,
        pov->pRes->DbgetRes.value,
        SMPD_MAX_DBS_VALUE_LEN );
    smpd_dbg_printf( L"Handling SMPD_DBGET command from smpd %hd\n"
                     L"\tctx_key=%hu\n"
                     L"\tkey=%S\n"
                     L"\tvalue=%S\n"
                     L"\tresult=%s\n",
                     pDbgetCmd->header.src,
                     pDbgetCmd->header.ctx_key,
                     key,
                     fSucc ? pov->pRes->DbgetRes.value : "n/a",
                     fSucc ? L"success":L"failed");

    if( !fSucc )
    {
        return ERROR_INVALID_DATA;
    }
    return NOERROR;

}


//
// Summary:
//  Handler for the bczget command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_bcget_command(
    _In_ smpd_overlapped_t* pov
    )
{
    const SmpdBcgetCmd* pBcgetCmd = &pov->pCmd->BcgetCmd;
    UINT16 rank = pBcgetCmd->rank;

    BOOL fSucc = smpd_dbs_bcget(
        pBcgetCmd->kvs,
        rank,
        pov->pRes->BcgetRes.value,
        _countof( pov->pRes->BcgetRes.value ) );
    smpd_dbg_printf( L"Handling SMPD_BCGET command from smpd %hd\n"
                     L"\tctx_key=%hu\n"
                     L"\trank=%hu\n"
                     L"\tvalue=%S\n"
                     L"\tresult=%s\n",
                     pBcgetCmd->header.src,
                     pBcgetCmd->header.ctx_key,
                     rank,
                     fSucc ? pov->pRes->BcgetRes.value : "n/a",
                     fSucc ? L"success":L"failed");

    if( !fSucc )
    {
        return ERROR_INVALID_DATA;
    }
    return NOERROR;

}


static void
smpd_create_node_id_section(
    _In_ const SmpdCmd* pCmd
    )
{
    if( smpd_process.node_id_region != nullptr )
    {
        return;
    }

    //const SmpdBcastNodeIdsCmd* pNodeIdsCmd = &pCmd->BcastNodeIdsCmd;
    const SmpdLaunchCmd* pLaunchCmd = &pCmd->LaunchCmd;
    DWORD buf_size = smpd_process.nproc * sizeof(UINT16);

    //
    // Determine name for shm region, currently make unique using smpd pid
    //
    HRESULT hr = StringCchPrintfW(
        smpd_process.node_id_region_name,
        _countof(smpd_process.node_id_region_name),
        L"smp_region_%u",
        GetCurrentProcessId()
        );
    if( FAILED( hr ) )
    {
        smpd_err_printf(L"error getting name for shm region.\n");
        return;
    }

    //
    // Create shm region
    //
    smpd_process.node_id_region = CreateFileMappingW(
        INVALID_HANDLE_VALUE, //use the page file
        NULL,                 //default security
        PAGE_READWRITE,       //read/write access
        0,                    //object size, high-order dword
        buf_size,             //object size, low-order dword
        smpd_process.node_id_region_name); // region name

    if(smpd_process.node_id_region == NULL)
    {
        smpd_err_printf(
            L"could not create shared memory region for node id mapping (%u).",
            GetLastError());
        return;
    }

    //
    // Get a pointer to the region
    //
    PUCHAR pBuf = static_cast<PUCHAR>(
        MapViewOfFile(smpd_process.node_id_region, FILE_MAP_ALL_ACCESS, 0, 0, buf_size) );

    if( pBuf == nullptr )
    {
        smpd_err_printf(L"could not map view of shared memory region for node id mapping.");
        return;
    }

    CopyMemory( pBuf, pLaunchCmd->node_ids, buf_size );
    smpd_dbg_printf(L"Successfully handled bcast nodeids command.\n");
}


//
// Summary:
//  Handler for the launch command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_launch_command(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdLaunchRes* pLaunchRes = &pov->pRes->LaunchRes;

    //
    // Initialize the result error message so that we don't send back
    // garbage in the case of success
    //
    pLaunchRes->count = 0;
    pLaunchRes->error_msg = nullptr;

    smpd_create_node_id_section( pov->pCmd );

    DWORD rc = smpd_launch_processes(
        pov->pContext,
        &pov->pCmd->LaunchCmd,
        pLaunchRes,
        smpd_process.set );
    if( rc != NOERROR )
    {
        smpd_err_printf(L"launch_process failed %s.\n", pLaunchRes->error_msg);
        return rc;
    }

    return NOERROR;
}


//
// Summary:
//  Handler for the connect command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_connect_command(
    _In_ smpd_overlapped_t* pov
    )
{
    const SmpdConnectCmd* pConnectCmd = &pov->pCmd->ConnectCmd;
    SmpdConnectRes* pConnectRes = &pov->pRes->ConnectRes;

    pConnectRes->userInfoNeeded = FALSE;

    smpd_dbg_printf(L"now connecting to %s\n", pConnectCmd->hostName);

    smpd_context_type_t context_type;
    context_type = smpd_destination_context_type(pConnectCmd->HostId);
    ASSERT( context_type != 0 );

    MPIU_Strcpy(smpd_process.jobObjName, MAX_PATH, pConnectCmd->jobObjName);

    if( pConnectCmd->pwd != nullptr )
    {
        size_t length = MPIU_Strlen(pConnectCmd->pwd) + 1;
        smpd_process.pwd = new wchar_t[length];
        if( smpd_process.pwd == nullptr )
        {
            smpd_post_abort_command(L"insufficient memory to connect %s.\n",
                pConnectCmd->hostName);
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        MPIU_Strcpy( smpd_process.pwd, length, pConnectCmd->pwd );
        smpd_process.saveCreds = pConnectCmd->saveCreds;
    }

    DWORD rc = smpd_connect_root_server(
        context_type,
        pConnectCmd->HostId,
        pConnectCmd->hostName,
        true,
        pConnectCmd->retryCount,
        pConnectCmd->retryInterval );
    if( rc == NTE_UI_REQUIRED )
    {
        pConnectRes->userInfoNeeded = TRUE;
        return NOERROR;
    }
    else if( rc != NOERROR )
    {
        smpd_err_printf(
            L"smpd running on %s is unable to connect to smpd service on %s:%hu\n",
            smpd_process.host,
            pConnectCmd->hostName,
            smpd_process.rootServerPort );
    }

    return rc;
}


//
// Summary:
//  Handler for the start_dbs command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_start_dbs_command(
    _In_ smpd_overlapped_t* pov
    )
{
    if(!smpd_process.have_dbs)
    {
        smpd_dbs_init();
        smpd_process.have_dbs = TRUE;
    }

    SmpdStartDbsRes* pRes = &pov->pRes->StartDbsRes;

    pRes->domain = smpd_process.domain;

    ASSERT( smpd_process.nproc != 0 );
    if( smpd_dbs_create( smpd_process.nproc, &pRes->kvs ) != true )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    wchar_t kvs_name[GUID_STRING_LENGTH + 1];
    GuidToStr( pRes->kvs, kvs_name, _countof(kvs_name) );
    smpd_dbg_printf(L"sending start_dbs result command kvs = %s.\n", kvs_name);

    return NOERROR;
}


//
// Summary:
//  Handler for the add_dbs command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_add_dbs_command(
    _In_ smpd_overlapped_t* pov
    )
{
    ASSERT(smpd_process.have_dbs);
    ASSERT(smpd_process.nproc != 0);

    SmpdAddDbsCmd* pCmd = &pov->pCmd->AddDbsCmd;

    if (smpd_dbs_create(pCmd->nproc, &pCmd->kvs) != true)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    wchar_t kvs_name[GUID_STRING_LENGTH + 1];
    GuidToStr(pCmd->kvs, kvs_name, _countof(kvs_name));
    smpd_dbg_printf(L"sending add_dbs result command kvs = %s.\n", kvs_name);

    return NOERROR;
}


//
// Summary:
//  Handler for the suspend command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_suspend_command(
    _In_ smpd_overlapped_t* pov
    )
{
    UINT16 process_id = pov->pCmd->SuspendCmd.header.ctx_key;

    //
    // Find the process by its process id and suspend it.
    // Note that the process could have been exited already but the exit message
    // has not reached mpiexec yet.
    //
    smpd_process_t* process = smpd_find_process_by_id( process_id );
    if( process == nullptr )
    {
        //
        // The process has already exited. SMPD_EXIT signal
        // is probably on the way to mpiexec, but mpiexec
        // is asking us to suspend this process due to
        // some other process' exit.
        //
        return NOERROR;
    }

    DWORD err = NOERROR;
    if(SuspendThread(process->wait.hThread) == -1)
    {
        err = GetLastError();
        smpd_err_printf(L"SuspendThread failed with error %u for process rank=%hu, pid=%d\n",
                        err, process->rank, process->pid);
    }

    smpd_dbg_printf(L"suspending proc_id=%hu %s, sending result to %s context\n",
                    process_id, (err == NOERROR) ? L"succeeded":L"failed",
                    smpd_get_context_str( pov->pContext ) );

    //
    // Suspending a process might fail, but we ignore the failure and continue
    //
    return NOERROR;
}


//
// Summary:
//  Handler for the kill command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_kill_command(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdKillCmd* pKillCmd = &pov->pCmd->KillCmd;

    UINT16 process_id = pKillCmd->header.ctx_key;
    INT32 exit_code = pKillCmd->exit_code;

    smpd_process_t* proc;
    proc = smpd_find_process_by_id(process_id);

    //
    // It is possible the process is already terminated by the
    // debugger attached to it
    //
    if(proc != nullptr)
    {
        smpd_kill_process(proc, exit_code);
    }
    return NOERROR;
}
