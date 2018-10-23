// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"
#include "pmi.h"
#include "mpierror.h"
#include "mpierrs.h"

#define PMIU_E_INTERN1(arg1_)\
    MPIU_ERR_CREATE(MPI_ERR_INTERN, "**intern %s", arg1_)

#define PMIU_E_FAIL(gle_) \
    MPIU_ERR_CREATE(MPI_ERR_OTHER, "**fail %s %d", get_error_string(gle_), gle_)

typedef struct pmi_process_t
{
    GUID            kvs;
    ExSetHandle_t   set;
    UINT16          iproc;
    UINT16          nproc;
    INT16           smpd_id;
    INT16           smpd_key;
    int             appnum;
    smpd_context_t* context;
    HANDLE          node_id_handle;
    UINT16*         node_ids;
    HANDLE          rank_affinity_handle;
    HWAFFINITY*     rank_affinities;
    bool            local_kvs;
} pmi_process_t;


/* global variables */
static pmi_process_t pmi_process =
{
    GUID_NULL,           /* kvs                   */
    EX_INVALID_SET,      /* set                   */
    USHRT_MAX,           /* iproc                 */
    USHRT_MAX,           /* nproc                 */
    -1,                  /* smpd_id               */
    0,                   /* smpd_key              */
    0,                   /* appnum                */
    NULL,                /* context               */
    NULL,                /* node id handle        */
    NULL,                /* node ids              */
    NULL,                /* rank affinity handle  */
    NULL,                /* rank affinities       */
    FALSE,               /* local_kvs             */
};


#if DBG

static BOOL g_pmi_initialized = FALSE;

void PmiSetInitialized(void)
{
    ASSERT(!g_pmi_initialized);
    g_pmi_initialized = TRUE;
}

void PmiSetFinialized(void)
{
    ASSERT(g_pmi_initialized);
    g_pmi_initialized = FALSE;
}

#define PmiAssertValid() ASSERT(g_pmi_initialized)

#else

#define PmiSetInitialized() ((void)0)
#define PmiSetFinialized() ((void)0)
#define PmiAssertValid() ((void)0)

#endif // _DBG


static void
pmi_handle_command_result(
    smpd_overlapped_t* pov
    )
{
    SmpdCmdHdr* pCmdHdr = reinterpret_cast<SmpdCmdHdr*>( pov->pCmd );
    SmpdResHdr* pResHdr = reinterpret_cast<SmpdResHdr*>( pov->pRes );
    DWORD       err     = pResHdr->err;

    smpd_dbg_printf( L"command %s result = %u\n",
                     CmdTypeToString(pCmdHdr->cmdType), err );

    *pov->pPmiTmpErr = err;
    if( err == NOERROR )
    {
        if( pCmdHdr->cmdType == SMPD_DBGET )
        {
            *pov->pPmiTmpErr = MPIU_Strcpy(
                pov->pPmiTmpBuffer,
                SMPD_MAX_DBS_VALUE_LEN,
                reinterpret_cast<char*>(pov->pRes->DbgetRes.value) );
        }
        else if( pCmdHdr->cmdType == SMPD_BCGET )
        {
            *pov->pPmiTmpErr = MPIU_Strcpy(
                pov->pPmiTmpBuffer,
                SMPD_MAX_DBS_VALUE_LEN,
                reinterpret_cast<char*>(pov->pRes->BcgetRes.value) );
        }
    }

    smpd_signal_exit_progress( MPI_SUCCESS );
}


static DWORD
pmi_create_post_command(
    _In_     SMPD_CMD_TYPE cmdType,
    _In_     const GUID&   kvs,
    _When_((cmdType == SMPD_INIT || cmdType == SMPD_FINALIZE || cmdType == SMPD_BARRIER || cmdType == SMPD_BCGET || cmdType == SMPD_BCPUT), _In_opt_z_)
    _When_((cmdType == SMPD_DBPUT || cmdType == SMPD_DBGET), _In_z_)
    PCSTR       key,
    _When_((cmdType == SMPD_INIT || cmdType == SMPD_FINALIZE || cmdType == SMPD_BARRIER || cmdType == SMPD_BCGET || cmdType == SMPD_DBGET), _In_opt_z_)
    _When_((cmdType == SMPD_DBPUT || cmdType == SMPD_BCPUT), _In_z_)
    PCSTR       value,
    _In_opt_ char*         pPmiTmpBuffer,
    _In_     DWORD*        pPmiTmpErr,
    _In_     UINT16        rank = 0,
    _In_     UINT16        nprocs = 0
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        cmdType,
        pmi_process.smpd_id,
        SMPD_IS_TOP );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create a '%s' command.\n", CmdTypeToString(cmdType) );
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        pmi_process.smpd_id,
        pmi_handle_command_result );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for '%s' command.\n",
                        CmdTypeToString(cmdType) );
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    reinterpret_cast<SmpdCmdHdr*>(pCmd)->ctx_key = pmi_process.smpd_key;
    switch( cmdType )
    {
    case SMPD_INIT:
    {
        SmpdInitCmd* pInitCmd = &pCmd->InitCmd;
        pInitCmd->header.dest = SMPD_IS_ROOT;
        pInitCmd->kvs = kvs;
        pInitCmd->node_id = pmi_process.smpd_id;
        pInitCmd->rank = pmi_process.iproc;
        pInitCmd->size = pmi_process.nproc;
        break;
    }
    case SMPD_FINALIZE:
    {
        SmpdFinalizeCmd* pFinalizeCmd = &pCmd->FinalizeCmd;
        pFinalizeCmd->header.dest = SMPD_IS_ROOT;
        pFinalizeCmd->kvs = kvs;
        pFinalizeCmd->node_id = pmi_process.smpd_id;
        pFinalizeCmd->rank =  pmi_process.iproc;
        break;
    }
    case SMPD_BARRIER:
    {
        SmpdBarrierCmd* pBarrierCmd = &pCmd->BarrierCmd;
        pBarrierCmd->header.dest = pmi_process.smpd_id;
        pBarrierCmd->kvs = kvs;
        break;
    }
    case SMPD_DBPUT:
    {
        SmpdDbputCmd* pDbputCmd = &pCmd->DbputCmd;
        pDbputCmd->kvs = kvs;

        DWORD err = MPIU_Strcpy( reinterpret_cast<char*>(pDbputCmd->key), _countof(pDbputCmd->key), key );
        if( err != NOERROR )
        {
            delete pRes;
            delete pCmd;
            return err;
        }

        err = MPIU_Strcpy( reinterpret_cast<char*>(pDbputCmd->value), _countof(pDbputCmd->value), value );
        if( err != NOERROR )
        {
            delete pRes;
            delete pCmd;
        }
        break;
    }
    case SMPD_DBGET:
    {
        SmpdDbgetCmd* pDbgetCmd = &pCmd->DbgetCmd;
        pDbgetCmd->kvs = kvs;

        DWORD err = MPIU_Strcpy( reinterpret_cast<char*>(pDbgetCmd->key), _countof(pDbgetCmd->key), key );
        if( err != NOERROR )
        {
            delete pRes;
            delete pCmd;
            return err;
        }
        break;
    }
    case SMPD_BCPUT:
    {
        SmpdBcputCmd* pBcputCmd = &pCmd->BcputCmd;
        pBcputCmd->kvs = kvs;
        pBcputCmd->rank = rank;
        pBcputCmd->nprocs = nprocs;

        DWORD err = MPIU_Strcpy( reinterpret_cast<char*>(pBcputCmd->value), _countof(pBcputCmd->value), value );
        if( err != NOERROR )
        {
            delete pRes;
            delete pCmd;
        }
        break;
    }
    case SMPD_BCGET:
    {
        SmpdBcgetCmd* pBcgetCmd = &pCmd->BcgetCmd;
        pBcgetCmd->kvs = kvs;
        pBcgetCmd->rank = rank;
        pBcgetCmd->nprocs = nprocs;

        break;
    }

    default:
    {
        smpd_err_printf(L"unable to recognize command type %u\n", static_cast<unsigned int>(cmdType));
        return ERROR_INVALID_DATA;
    }
    }

    DWORD rc = smpd_post_command( pmi_process.context,
                                  pCmd,
                                  &pRes->Res,
                                  nullptr,
                                  pPmiTmpBuffer,
                                  pPmiTmpErr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"unable to post %s command error %u\n",
                        CmdTypeToString(cmdType),
                        rc);
        return rc;
    }

    //
    // Go to the PMI progress engine and wait for the result to come
    // back.
    //
    rc = smpd_progress(pmi_process.set);
    if( rc != MPI_SUCCESS )
    {
        return rc;
    }

    return NOERROR;
}


static int
pmi_client_error(
    EXOVERLAPPED* /*pexov*/
    )
{
    //
    // The real error was set earlier so here we just return -1
    //
    return -1;
}


static void
PmiHandleCmdError(
    smpd_overlapped_t* /*pov*/,
    int error
    )
{
    static EXOVERLAPPED ov;

    ExInitOverlapped(&ov, pmi_client_error, pmi_client_error);
    ExPostOverlappedResult(pmi_process.set, &ov, 0, 0);

    smpd_signal_exit_progress( error );
}


static DWORD
pmi_client_connect_to_server(
    _In_ PCWSTR portStr
    )
{
    DWORD rc = smpd_connect_mgr_smpd(
        SMPD_CONTEXT_PMI_CLIENT,
        nullptr,
        pmi_process.smpd_key,
        portStr,
        &pmi_process.context );
    if( rc == NOERROR )
    {
        pmi_process.context->on_cmd_error = PmiHandleCmdError;
    }
    return rc;
}


int
PMI_Init(
    _Out_                     int*   spawned,
    _Outptr_result_maybenull_ char** parentPortName
    )
{
    //
    // suppress OACR errors about not setting the _Out_ parameter
    // parentPortName is set only if the process was spawned with MPI_Comm_spawn
    //
    OACR_USE_PTR(parentPortName);

    PmiSetInitialized();
    ASSERT(spawned != NULL);

    smpd_init_process("mpi appplication");

    pmi_process.appnum = env_to_int(L"PMI_APPNUM", 0, 0);

    *spawned = env_to_int( L"PMI_SPAWN", 0, 0 );
    if (*spawned == 1)
    {
        wchar_t tmpPortNameStr[SMPD_MAX_ENV_LENGTH];
        DWORD err = MPIU_Getenv(L"PMI_PARENT_PORT_NAME",
            tmpPortNameStr,
            _countof(tmpPortNameStr));
        if (err != NOERROR)
        {
            return PMIU_E_FAIL(err);
        }
        err = MPIU_WideCharToMultiByte(tmpPortNameStr, parentPortName);
        if (err != NOERROR)
        {
            return PMIU_E_FAIL(err);
        }
    }

    //
    // Max of GUID plus null terminating character is 37
    //
    wchar_t tmpGuidStr[GUID_STRING_LENGTH + 1];
    DWORD err = MPIU_Getenv( L"PMI_KVS",
                             tmpGuidStr,
                             _countof(tmpGuidStr) );
    if( err == NOERROR )
    {
        err = UuidFromStringW(
            reinterpret_cast<RPC_WSTR>(tmpGuidStr), &pmi_process.kvs );
        if( err != RPC_S_OK )
        {
            return PMIU_E_FAIL( err );
        }
    }
    else
    {
        pmi_process.iproc = 0;
        pmi_process.nproc = 1;
        pmi_process.local_kvs = true;
        if(!smpd_dbs_init())
        {
            return MPIU_ERR_NOMEM();
        }

        if( smpd_dbs_create( pmi_process.nproc, &pmi_process.kvs ) != true )
        {
            return MPIU_ERR_NOMEM();
        }

        pmi_process.node_ids = static_cast<UINT16*>( MPIU_Malloc(sizeof(UINT16)) );
        if( pmi_process.node_ids == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
        pmi_process.node_ids[0] = 0;

        return MPI_SUCCESS;
    }

    int val = env_to_int( L"PMI_RANK", -1, INT_MIN );
    if( val < 0 || val > USHRT_MAX )
    {
        return PMIU_E_INTERN1("invalid negative PMI_RANK");
    }
    pmi_process.iproc = static_cast<UINT16>(val);


    val = env_to_int( L"PMI_SIZE", -1, INT_MIN ) ;
    if( val < 1 || val > USHRT_MAX )
    {
        return PMIU_E_INTERN1("invalid value for PMI_SIZE");
    }
    pmi_process.nproc = static_cast<UINT16>(val);

    //
    // This code is dependent on the pmi_process.nproc values being correctly
    // set. Do not move above this point in the function.
    //
    wchar_t pNodeIdName[SMPD_MAX_ENV_LENGTH];

    err = MPIU_Getenv( L"PMI_NODE_IDS",
                       pNodeIdName,
                       _countof(pNodeIdName) );
    if( err == NOERROR )
    {
        pmi_process.node_id_handle = OpenFileMappingW(
            FILE_MAP_READ,
            FALSE,
            pNodeIdName);

        if(pmi_process.node_id_handle != NULL)
        {
            pmi_process.node_ids = static_cast<UINT16*>( MapViewOfFile(
                pmi_process.node_id_handle,
                FILE_MAP_READ,
                0,
                0,
                pmi_process.nproc * sizeof(UINT16)) );

            if(pmi_process.node_ids == nullptr)
            {
                return PMIU_E_INTERN1("could not map view of node id region.\n");
            }
        }
        else
        {
            char msg[1024];
            MPIU_Snprintf(msg,
                          _countof(msg),
                          "Could not open file mapping for %d (%d).\n",
                          pmi_process.iproc,
                          GetLastError());
            return PMIU_E_INTERN1(msg);
        }
    }
    else
    {
        pmi_process.node_ids = static_cast<UINT16*>( MPIU_Malloc(sizeof(UINT16)) );
        if(pmi_process.node_ids == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        //
        // All ranks are on the same node.
        //
        pmi_process.node_ids[0] = 0;
    }

    wchar_t pRankAffinitiesSectionName[SMPD_MAX_ENV_LENGTH];

    err = MPIU_Getenv( L"PMI_RANK_AFFINITIES",
                       pRankAffinitiesSectionName,
                       _countof(pRankAffinitiesSectionName) );
    if( err == NOERROR )
    {
        pmi_process.rank_affinity_handle = OpenFileMappingW(
            FILE_MAP_READ,
            FALSE,
            pRankAffinitiesSectionName);

        if(pmi_process.rank_affinity_handle != nullptr)
        {
            pmi_process.rank_affinities= static_cast<HWAFFINITY*>( MapViewOfFile(
                pmi_process.rank_affinity_handle,
                FILE_MAP_READ,
                0,
                0,
                pmi_process.nproc * sizeof(HWAFFINITY)) );

            if(pmi_process.rank_affinities == nullptr)
            {
                return PMIU_E_INTERN1("could not map view of rank affinity region.\n");
            }
        }
        else
        {
            char msg[1024];
            MPIU_Snprintf(msg,
                          _countof(msg),
                          "Could not open file mapping for %d (%d).\n",
                          pmi_process.iproc,
                          GetLastError());
            return PMIU_E_INTERN1(msg);
        }
    }
    else
    {
        //
        // It is OK if we don't find this env var. NUMA socket logic
        // will simply assume ranks aren't affinitized
        //
    }

    pmi_process.smpd_id = static_cast<INT16>(
        env_to_int( L"PMI_SMPD_ID", pmi_process.smpd_id, 0 ) );

    smpd_process.tree_id = static_cast<INT16>(
        pmi_process.smpd_id );

    pmi_process.smpd_key = static_cast<INT16>(
        env_to_int( L"PMI_SMPD_KEY", pmi_process.smpd_key, 0 ) );

    //
    // We no longer read PMI_HOST because the PMI client and SMPD
    // instance connections are assumed to be LRPC now, which only
    // works for localhost.  Having SMPD instance and PMI clients on
    // different hosts will also prevent the node id and affinity
    // reading from working (since they both rely on filemap).
    //
    wchar_t portStr[SMPD_MAX_ENV_LENGTH];
    err = MPIU_Getenv( L"PMI_PORT",
                       portStr,
                       _countof(portStr) );
    if( err == NOERROR )
    {
        pmi_process.set = ExCreateSet();
        if(pmi_process.set == EX_INVALID_SET)
        {
            return MPIU_ERR_NOMEM();
        }
        smpd_process.set = pmi_process.set;

        err = pmi_client_connect_to_server(portStr);
        if( err != NOERROR )
        {
            return PMIU_E_FAIL( err );
        }
    }
    else
    {
        return PMIU_E_INTERN1("invalid or missing PMI_PORT information for connecting the process manager");
    }

    DWORD pmiErr = NOERROR;
    err = pmi_create_post_command(
        SMPD_INIT,
        pmi_process.kvs,
        nullptr,
        nullptr,
        nullptr,
        &pmiErr );
    if( err != NOERROR )
    {
        return PMIU_E_FAIL( err );
    }

    if( pmiErr == ERROR_INVALID_DATA )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - init called when another process has exited without calling init");
    }
    else if( pmiErr != NOERROR )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error");
    }

    return MPI_SUCCESS;
}


int PMI_Finalize()
{
    if(pmi_process.local_kvs)
    {
        smpd_dbs_finalize();
        PmiSetFinialized();
        return MPI_SUCCESS;
    }

    DWORD pmiErr = NOERROR;
    DWORD rc = pmi_create_post_command(
        SMPD_FINALIZE,
        pmi_process.kvs,
        nullptr,
        nullptr,
        nullptr,
        &pmiErr );
    if( rc != NOERROR )
    {
        return PMIU_E_FAIL(rc);
    }

    if( pmiErr != NOERROR )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error: "
                               "failed to post finalize command");
    }

    //
    // Ignore error from PMI_Barrier because we should proceed
    // to do other clean up locally anyway.
    //
    PMI_Barrier();


    //
    // This is the equivalent of the old PMI's done command.
    // We are letting SMPD know that there will be no further
    // command.
    //
    RPC_STATUS status = RPC_S_OK;
    RpcTryExcept
    {
        RpcCliDeleteSmpdMgrContext(
            &(pmi_process.context->hServerContext)
            );
    }
    RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
    {
        status = RpcExceptionCode();
    }
    RpcEndExcept;

    smpd_free_context( pmi_process.context );
    if( status != RPC_S_OK )
    {
        smpd_err_printf(L"failed to delete parent context error %ld.\n", status );
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error: "
                               "failed to delete parent context");
    }

    CloseHandle(pmi_process.node_id_handle);

    if(pmi_process.rank_affinity_handle)
    {
        CloseHandle(pmi_process.rank_affinity_handle);
    }

    PmiSetFinialized();

    return MPI_SUCCESS;
}


static DWORD
pmi_send_abort_job_command(
    _In_ BOOL intern,
    _In_ int exit_code,
    _In_ const wchar_t error_msg[]
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_ABORT_JOB,
        pmi_process.smpd_id,
        SMPD_IS_ROOT );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create an abort job command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        pmi_process.smpd_id,
        nullptr );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for abort job command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdAbortJobCmd* pAbortJobCmd = reinterpret_cast<SmpdAbortJobCmd*>( pCmd );
    pAbortJobCmd->kvs = pmi_process.kvs;
    pAbortJobCmd->rank = pmi_process.iproc;
    pAbortJobCmd->exit_code = exit_code;
    pAbortJobCmd->intern = intern;
    pAbortJobCmd->error_msg = const_cast<wchar_t*>(error_msg);

    DWORD rc = smpd_post_command(
        pmi_process.context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf( L"unable to post abort job command error %u\n", rc );
    }

    /* let the state machine send the command and receive the result */
    rc = smpd_progress(pmi_process.set);
    if(rc != MPI_SUCCESS)
    {
        return rc;
    }

    return NOERROR;
}


DECLSPEC_NORETURN void PMI_Abort(BOOL intern, int exit_code, const char error_msg[])
{
    PmiAssertValid();

    /* flush any output before aborting */
    fflush(stdout);
    fflush(stderr);

    if(pmi_process.local_kvs)
    {
        printf("\njob aborted:\n");
        printf("[ranks] message\n");
        if(intern)
        {
            printf("\n[0] fatal error\n");
        }
        else
        {
            printf("\n[0] application aborted\n");
        }

        if(error_msg != NULL)
        {
            printf("%s", error_msg);
        }
        printf("\n");
        fflush(stdout);

        smpd_dbs_finalize();

        ExitProcess(exit_code);
    }

    if(pmi_process.context != NULL)
    {
        //
        // PMI_Abort might be called even if PMI_Init failed; check
        // that the context exists.
        //
        wchar_t* errMsgW;
        if( MPIU_MultiByteToWideChar( error_msg, &errMsgW ) == NOERROR )
        {
            pmi_send_abort_job_command(intern, exit_code, errMsgW);
            delete[] errMsgW;
        }
        else
        {
            pmi_send_abort_job_command(intern, exit_code, L"job failed - error message not available");
        }
    }
    else
    {
        //
        // This error happens when we have no connection to the SMPD manager
        // (most likely the error happens during connection establishment to SMPD)
        // We should output the message and abort
        //
        printf("\njob aborted:\n");
        printf("[ranks] message\n");
        if(intern)
        {
            printf("\n[%hu] fatal error\n", pmi_process.iproc);
        }
        else
        {
            printf("\n[%hu] application aborted\n", pmi_process.iproc);
        }

        if(error_msg != NULL)
        {
            printf("%s", error_msg);
        }
        printf("\n");
        fflush(stdout);

        if( pmi_process.rank_affinity_handle != nullptr )
        {
            CloseHandle( pmi_process.rank_affinity_handle );
        }

        if( pmi_process.node_id_handle != nullptr )
        {
            CloseHandle( pmi_process.node_id_handle );
        }
        else if( pmi_process.node_ids != nullptr )
        {
            MPIU_Free( pmi_process.node_ids );
        }
    }

    ExitProcess(exit_code);
}


int PMI_Get_size(void)
{
    PmiAssertValid();
    return pmi_process.nproc;
}


int PMI_Get_rank(void)
{
    PmiAssertValid();
    return pmi_process.iproc;
}


int PMI_Get_universe_size(void)
{
    PmiAssertValid();
    return -1;
}


int PMI_Get_appnum(void)
{
    PmiAssertValid();
    return pmi_process.appnum;
}


UINT16* PMI_Get_node_ids()
{
    PmiAssertValid();
    return pmi_process.node_ids;
}


void* PMI_Get_rank_affinities()
{
    return (void*) pmi_process.rank_affinities;
}


const GUID&
PMI_KVS_Get_id()
{
    return pmi_process.kvs;
}


int
PMI_Barrier()
{
    PmiAssertValid();

    if(pmi_process.nproc == 1)
    {
        return MPI_SUCCESS;
    }


    DWORD pmiErr = NOERROR;
    DWORD rc = pmi_create_post_command(
        SMPD_BARRIER,
        pmi_process.kvs,
        nullptr,
        nullptr,
        nullptr,
        &pmiErr );

    if( rc != NOERROR )
    {
        return PMIU_E_FAIL(rc);
    }

    if( pmiErr != NOERROR )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error");
    }

    return MPI_SUCCESS;
}


_Success_(return == MPI_SUCCESS)
int
PMI_KVS_Put(
    _In_   const GUID& kvs,
    _In_z_ const char key[],
    _In_z_ const char value[]
    )
{
    PmiAssertValid();

    if(pmi_process.local_kvs)
    {
        if(smpd_dbs_put(kvs, key, value))
        {
            return MPI_SUCCESS;
        }

        return MPIU_ERR_NOMEM();
    }

    DWORD pmiErr = NOERROR;
    DWORD rc = pmi_create_post_command(
        SMPD_DBPUT,
        kvs,
        key,
        value,
        nullptr,
        &pmiErr );
    if( rc != NOERROR )
    {
        return PMIU_E_FAIL(rc);
    }

    if( pmiErr != NOERROR )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error");
    }

    return MPI_SUCCESS;
}


_Success_(return == MPI_SUCCESS)
int
PMI_KVS_PublishBC(
    _In_   const GUID& kvs,
    _In_   UINT16 rank,
    _In_   UINT16 nprocs,
    _In_z_ const char value[]
    )
{
    PmiAssertValid();

    if(pmi_process.local_kvs)
    {
        if(smpd_dbs_bcput(kvs, rank, value))
        {
            return MPI_SUCCESS;
        }

        return MPIU_ERR_NOMEM();
    }

    DWORD pmiErr = NOERROR;
    DWORD rc = pmi_create_post_command(
        SMPD_BCPUT,
        kvs,
        nullptr,
        value,
        nullptr,
        &pmiErr,
        rank,
        nprocs);
    if( rc != NOERROR )
    {
        return PMIU_E_FAIL(rc);
    }

    if( pmiErr != NOERROR )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error");
    }

    return MPI_SUCCESS;
}


_Success_(return == MPI_SUCCESS)
int
PMI_KVS_Commit(const GUID& /*kvs*/)
{
    PmiAssertValid();

    if(pmi_process.local_kvs)
        return MPI_SUCCESS;

    /* Make the puts return when the commands are written but not acknowledged.
       Then have this function wait until all outstanding puts are acknowledged.
       */

    return MPI_SUCCESS;
}


_Success_(return == MPI_SUCCESS)
int
PMI_KVS_Get(
    _In_   const GUID& kvs,
    _In_z_ const char key[],
    _Out_writes_(length) char value[],
    _In_ size_t length
    )
{
    PmiAssertValid();

    if(pmi_process.local_kvs)
    {
        if(smpd_dbs_get(kvs, key, value, length))
        {
            return MPI_SUCCESS;
        }
        return MPIU_ERR_NOMEM();
    }

    DWORD pmiErr = NOERROR;
    char tmpBuffer[SMPD_MAX_DBS_VALUE_LEN];

    DWORD rc = pmi_create_post_command(
        SMPD_DBGET,
        kvs,
        key,
        nullptr,
        tmpBuffer,
        &pmiErr );
    if( rc != NOERROR )
    {
        return PMIU_E_FAIL(rc);
    }

    if( pmiErr != NOERROR )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error");
    }

    if( MPIU_Strcpy( value, length, tmpBuffer ) != 0 )
    {
        return MPIU_ERR_NOMEM();
    }

    return MPI_SUCCESS;
}


_Success_(return == MPI_SUCCESS)
PMI_KVS_RetrieveBC(
    _In_   const GUID& kvs,
    _In_   UINT16 rank,
    _Out_writes_(length) char value[],
    _In_ size_t length
    )
{
    PmiAssertValid();

    if(pmi_process.local_kvs)
    {
        if(smpd_dbs_bcget(kvs, rank, value, length))
        {
            return MPI_SUCCESS;
        }
        return MPIU_ERR_NOMEM();
    }

    DWORD pmiErr = NOERROR;
    char tmpBuffer[SMPD_MAX_DBS_VALUE_LEN];

    DWORD rc = pmi_create_post_command(
        SMPD_BCGET,
        kvs,
        nullptr,
        nullptr,
        tmpBuffer,
        &pmiErr,
        rank );
    if( rc != NOERROR )
    {
        return PMIU_E_FAIL(rc);
    }

    if( pmiErr != NOERROR )
    {
        return PMIU_E_INTERN1( SMPD_FAIL_STR" - Unexpected Internal Error");
    }

    if( MPIU_Strcpy( value, length, tmpBuffer ) != 0 )
    {
        return MPIU_ERR_NOMEM();
    }

    return MPI_SUCCESS;
}


//----------------------------------------------------------------------------
//
// Functionality to parse INFO argument of MPI_Spawn
//
//----------------------------------------------------------------------------
static MPI_RESULT
ValueToMultibyte(
    _In_z_            const char* value,
    _Outptr_result_z_ wchar_t** wideValue
    )
{
    wchar_t* tempEnv;
    DWORD len = static_cast<DWORD>(strlen(value) + 1);

    tempEnv = new wchar_t[len];
    if (tempEnv == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    if (MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value,
        -1,
        tempEnv,
        len) == 0)
    {
        delete[] tempEnv;
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", value);
    }
    (*wideValue) = tempEnv;
    return MPI_SUCCESS;
}


_Success_(return == MPI_SUCCESS)
static MPI_RESULT
parse_info_env(
    _In_z_ const char* value,
    _Inout_ SmpdSpawnCmd* pSpawnCmd
    )
{
    return ValueToMultibyte(value, &pSpawnCmd->envList);
}


_Success_(return == MPI_SUCCESS)
static MPI_RESULT
parse_info_path(
    _In_z_ const char* value,
    _Inout_ SmpdSpawnCmd* pSpawnCmd
    )
{
    return ValueToMultibyte(value, &pSpawnCmd->path);
}


_Success_(return == MPI_SUCCESS)
static MPI_RESULT
parse_info_hosts(
    _In_z_ const char* value,
    _Inout_ SmpdSpawnCmd* pSpawnCmd
    )
{
    return ValueToMultibyte(value, &pSpawnCmd->hosts);
}


_Success_(return == MPI_SUCCESS)
static MPI_RESULT
parse_info_wdir(
    _In_z_ const char* value,
    _Inout_ SmpdSpawnCmd* pSpawnCmd
    )
{
    return ValueToMultibyte(value, &pSpawnCmd->wDir);
}


typedef _Success_(return == MPI_SUCCESS) MPI_RESULT(*pfn_on_info_t)(
    const char* value,
    SmpdSpawnCmd* pSpawnCmd
    );


typedef struct mp_info_handler_t
{
    const char* option;
    pfn_on_info_t on_option;

} mp_info_handler_t;



static const mp_info_handler_t g_smpd_info_handlers[] =
{
    { "env", parse_info_env },
    { "hosts", parse_info_hosts },
    { "path", parse_info_path },
    { "wdir", parse_info_wdir },
};


static int __cdecl pmi_compare_info_key(
    const void* pv1,
    const void* pv2
    )
{
    const char* info_key = static_cast<const char*>(pv1);
    const mp_info_handler_t* entry = static_cast<const mp_info_handler_t*>(pv2);
    return _stricmp(info_key, entry->option);
}


static DWORD
TrySetDefaultValue(
    _Inout_ wchar_t** pStr
    )
{
    if ((*pStr) == nullptr)
    {
        wchar_t* str = new wchar_t[1];
        if (str == nullptr)
        {
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        str[0] = L'\0';
        (*pStr) = str;
    }
    return NOERROR;
}


static DWORD
pmi_create_post_spawn_command(
    _In_     GUID          kvs,
    _In_z_   wchar_t*      cmd,
    _In_z_   wchar_t*      argv,
    _In_     int           info_keyval_size,
    _In_     PMI_keyval_t* info_keyval_vector,
    _In_z_   wchar_t*      wdir,
    _In_     UINT16        maxprocs,
    _In_     UINT16        totalRankCount,
    _In_     UINT16        startRank,
    _In_     UINT8         appNum,
    _In_     bool          last,
    _In_opt_ char*         pPmiTmpBuffer,
    _In_z_   char*         parentPortName,
    _In_     DWORD*        pPmiTmpErr
    )
{
    DWORD mpi_errno;
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_SPAWN,
        pmi_process.smpd_id,
        SMPD_IS_ROOT);
    if (pCmd == nullptr)
    {
        smpd_err_printf(L"unable to create a '%s' command.\n", CmdTypeToString(SMPD_SPAWN));
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        pmi_process.smpd_id,
        pmi_handle_command_result);
    if (pRes == nullptr)
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for '%s' command.\n",
            CmdTypeToString(SMPD_SPAWN));
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    reinterpret_cast<SmpdCmdHdr*>(pCmd)->ctx_key = pmi_process.smpd_key;

    SmpdSpawnCmd* pSpawnCmd = &pCmd->SpawnCmd;

    pSpawnCmd->kvs = kvs;
    pSpawnCmd->appExe = cmd;
    pSpawnCmd->appArgv = argv;
    pSpawnCmd->rankCount = maxprocs;
    pSpawnCmd->totalRankCount = totalRankCount;
    pSpawnCmd->parentPortName = parentPortName;

    pSpawnCmd->startRank = startRank;
    pSpawnCmd->appNum = appNum;
    pSpawnCmd->done = last;

    pSpawnCmd->envList = nullptr;
    pSpawnCmd->wDir = nullptr;
    pSpawnCmd->path = nullptr;
    pSpawnCmd->hosts = nullptr;

    bool wdirCleanup = true;

    for (int i = 0; i < info_keyval_size; i++)
    {
        const void* p;
        p = bsearch(
            info_keyval_vector[i].key,
            g_smpd_info_handlers,
            _countof(g_smpd_info_handlers),
            sizeof(g_smpd_info_handlers[0]),
            pmi_compare_info_key
            );

        //
        // Info flag was not found
        // Return true to enable further parsing.
        //
        if (p == nullptr)
        {
            continue;
        }

        //
        // Call the option handler
        //
        MPI_RESULT fSucc = static_cast<const mp_info_handler_t*>(p)->on_option(info_keyval_vector[i].val, pSpawnCmd);
        if (fSucc != MPI_SUCCESS)
        {
            mpi_errno = ERROR_NOT_ENOUGH_MEMORY;
            goto CleanUp;
        }
    }

    //
    // if INFO variables are not passed, set the arguments to default values
    //
    mpi_errno = TrySetDefaultValue(&pSpawnCmd->envList);
    if (mpi_errno != NOERROR)
    {
        goto CleanUp;
    }

    mpi_errno = TrySetDefaultValue(&pSpawnCmd->path);
    if (mpi_errno != NOERROR)
    {
        goto CleanUp;
    }
    mpi_errno = TrySetDefaultValue(&pSpawnCmd->hosts);
    if (mpi_errno != NOERROR)
    {
        goto CleanUp;
    }

    //
    // if working directory was not passed as part of INFO argument, set it to the wdir of
    // root spawning process
    //
    if (pSpawnCmd->wDir == nullptr)
    {
        pSpawnCmd->wDir = wdir;
        wdirCleanup = false;
    }

    DWORD rc = smpd_post_command(pmi_process.context,
        pCmd,
        &pRes->Res,
        nullptr,
        pPmiTmpBuffer,
        pPmiTmpErr);
    if (rc != RPC_S_OK)
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"unable to post %s command error %u\n",
            CmdTypeToString(SMPD_SPAWN),
            rc);
        return rc;
    }
    //
    // Go to the PMI progress engine and wait for the result to come
    // back.
    //
    rc = smpd_progress(pmi_process.set);
    if (rc != MPI_SUCCESS)
    {
        return rc;
    }

    mpi_errno = NOERROR;

CleanUp:
    delete[] pSpawnCmd->envList;
    delete[] pSpawnCmd->path;
    delete[] pSpawnCmd->hosts;

    if (wdirCleanup)
    {
        delete[] pSpawnCmd->wDir;
    }

    return mpi_errno;
}


_Success_(return == MPI_SUCCESS)
int
PMI_Spawn_multiple(
    _In_range_(>= , 0)   int       count,
    _In_reads_(count)    wchar_t** cmds,
    _In_reads_(count)    wchar_t** argvs,
    _In_count_(count)    const int* maxprocs,
    _In_count_(count)    int* cinfo_keyval_sizes,
    _In_count_(count)    PMI_keyval_t** info_keyval_vectors,
    _In_z_               char*     parentPortName,
    _Out_                int*      /*errors*/
    )
{
    if (pmi_process.local_kvs)
    {
        return PMIU_E_INTERN1(SMPD_FAIL_STR"spawn not supported without process manager");
    }

    DWORD pmiErr = NOERROR;

    //
    // count the number of processes to spawn
    //
    UINT16 nprocs = 0;
    for (int i = 0; i < static_cast<UINT8>(count); i++)
    {
        if (maxprocs[i] + nprocs > MSMPI_MAX_RANKS)
        {
            return PMIU_E_INTERN1(SMPD_FAIL_STR"too many processes to start");
        }
        nprocs += static_cast<UINT16>(maxprocs[i]);
    }

    wchar_t tmpdir[MAX_PATH];
    DWORD dirLen = GetCurrentDirectoryW(_countof(tmpdir), tmpdir);
    BOOL fSucc = (dirLen > 0 && dirLen <= _countof(tmpdir));
    if (!fSucc)
    {
        return PMIU_E_INTERN1(SMPD_FAIL_STR"unable to set working directory for the spawning processes");
    }
    GUID kvs;
    DWORD rc = UuidCreate(&kvs);
    if (rc != RPC_S_OK && rc != RPC_S_UUID_LOCAL_ONLY)
    {
        return PMIU_E_INTERN1(SMPD_FAIL_STR"unable to create GUID for the new process group");
    }

    UINT16 startRank = 0;
    for (UINT8 i = 0; i < count; i++)
    {
        if (maxprocs[i] > MSMPI_MAX_RANKS)
        {
            return PMIU_E_INTERN1(SMPD_FAIL_STR"too many processes to start");
        }
        else if (maxprocs[i] == 0)
        {
            //
            // no processes to start on this iteration
            //
            continue;
        }
        UINT16 procRanks = static_cast<UINT16>(maxprocs[i]);
        bool last = (i == count - 1);
        DWORD rc = pmi_create_post_spawn_command(
            kvs,
            cmds[i],
            argvs[i],
            cinfo_keyval_sizes[i],
            info_keyval_vectors[i],
            tmpdir,
            procRanks,
            nprocs,
            startRank,
            i,
            last,
            nullptr,
            parentPortName,
            &pmiErr);
        if (rc != NOERROR)
        {
            return PMIU_E_FAIL(rc);
        }

        if (pmiErr != NOERROR)
        {
            return PMIU_E_INTERN1(SMPD_FAIL_STR" - Unexpected Internal Error");
        }
        startRank += procRanks;
    }

    return MPI_SUCCESS;
}


static GUID namepub_kvs = GUID_NULL;
static int setup_name_service()
{
    wchar_t tmpGuidStr[GUID_STRING_LENGTH + 1];
    DWORD err = MPIU_Getenv( L"PMI_NAMEPUB_KVS",
                             tmpGuidStr,
                             _countof(tmpGuidStr) );
    if( err == NOERROR )
    {
        err = UuidFromStringW(
            reinterpret_cast<RPC_WSTR>(tmpGuidStr), &namepub_kvs );
        if( err != RPC_S_OK )
        {
            return PMIU_E_FAIL( err );
        }
    }
    else
    {
        err = MPIU_Getenv( L"PMI_DOMAIN",
                           tmpGuidStr,
                           _countof(tmpGuidStr) );
        if( err == NOERROR )
        {
            err = UuidFromStringW(
                reinterpret_cast<RPC_WSTR>(tmpGuidStr), &namepub_kvs );
            if( err != RPC_S_OK )
            {
                return PMIU_E_FAIL( err );
            }
        }
        else
        {
            return PMIU_E_FAIL( err );
        }
    }

    return MPI_SUCCESS;
}


int PMI_Publish_name( const char service_name[], const char port[] )
{
    int rc;

    ASSERT(port != NULL);
    ASSERT(service_name != NULL);

    if( IsEqualGUID(namepub_kvs, GUID_NULL) )
    {
        rc = setup_name_service();
        if(rc != MPI_SUCCESS)
            return rc;
    }
    /*printf("publish kvs: <%s>\n", namepub_kvs);fflush(stdout);*/
    rc = PMI_KVS_Put(namepub_kvs, service_name, port);
    if(rc != MPI_SUCCESS)
        return MPIU_ERR_FAIL(rc);

    rc = PMI_KVS_Commit(namepub_kvs);
    if(rc != MPI_SUCCESS)
        return MPIU_ERR_FAIL(rc);

    return MPI_SUCCESS;
}


int PMI_Unpublish_name( const char service_name[] )
{
    int rc;

    ASSERT(service_name != NULL);

    if( IsEqualGUID(namepub_kvs, GUID_NULL) )
    {
        rc = setup_name_service();
        if(rc != MPI_SUCCESS)
            return rc;
    }
    /*printf("unpublish kvs: <%s>\n", namepub_kvs);fflush(stdout);*/
    /* This assumes you can put the same key more than once which breaks the PMI specification */
    rc = PMI_KVS_Put(namepub_kvs, service_name, "");
    if(rc != MPI_SUCCESS)
        return MPIU_ERR_FAIL(rc);

    rc = PMI_KVS_Commit(namepub_kvs);
    if(rc != MPI_SUCCESS)
        return MPIU_ERR_FAIL(rc);

    return MPI_SUCCESS;
}


int PMI_Lookup_name( _In_z_ const char service_name[], _Out_writes_(MPI_MAX_PORT_NAME) char port[] )
{
    int rc;

    ASSERT(port != NULL);
    ASSERT(service_name != NULL);

    port[0] = '\0';
    if( IsEqualGUID(namepub_kvs, GUID_NULL) )
    {
        rc = setup_name_service();
        if(rc != MPI_SUCCESS)
            return rc;
    }
    /*printf("lookup kvs: <%s>\n", namepub_kvs);fflush(stdout);*/
    rc = PMI_KVS_Get(namepub_kvs, service_name, port, MPI_MAX_PORT_NAME);
    if(rc != MPI_SUCCESS)
        return MPIU_ERR_FAIL(rc);

    if(port[0] == '\0')
        return MPIU_ERR_CREATE(MPI_ERR_NAME, "**namepubnotfound %s", service_name);

    return MPI_SUCCESS;
}
