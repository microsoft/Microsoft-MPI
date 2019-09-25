// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include <initguid.h>
#include "smpd.h"
#include "rpcutil.h"
#include "PmiDbgImpl.h"
#include "mpitrace.h"
#include <fcntl.h>
#include <io.h>


extern FN_PmiLaunchUserSid SdkCreateManagerProcess;
TOKEN_USER* g_pTokenUser;

static BOOL WINAPI
smpd_ctrl_handler(
    _In_ DWORD dwCtrlType
    )
{
    static BOOL fCtrlHit = FALSE;

    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if(fCtrlHit)
        {
            exit(-1);
        }
        break;
    case CTRL_LOGOFF_EVENT:
        return FALSE;
    }

    fCtrlHit = TRUE;

    if( smpd_process.mgrServerPort == 0 )
    {
        smpd_stop_root_server();
    }
    else
    {
        NotifyPmiDbgExtensions(SmpdNotifyFinalize);
        smpd_kill_all_processes();
    }

    return TRUE;
}


static int
smpd_exit_from_rpc(
    EXOVERLAPPED* /*pexov*/
    )
{
    smpd_signal_exit_progress( SMPD_EXIT_FROM_RPC );
    return MPI_SUCCESS;
}


static void SmpdAbortFromRPCThread()
{
    //
    // This abort is triggered by an error in one of these scenarios
    // 1) mpiexec/smpd connecting to smpd manager/instance
    // 2) smpd reconnect to mpiexec/another smpd
    //
    // Under our current implementation, in both cases the error is
    // happening at a leaf smpd node and no processes have been
    // launched. It is safe (or rather, graceful enough) to just
    // exit this process
    //
    EXOVERLAPPED ov;
    ExInitOverlapped(&ov, smpd_exit_from_rpc, smpd_exit_from_rpc);
    ExPostOverlappedResult(smpd_process.set, &ov, 0, 0);

    //
    // Give the engine 30 seconds to shutdown and then force an exit
    //
    Sleep(30000);
    smpd_err_printf(
        L"\nFailed to terminate smpd manager after 30 seconds. Forcing smpd exit.\n");
    exit( SMPD_EXIT_FROM_RPC );

}


static void
smpd_init_cmd_handlers()
{
    for( unsigned int i = 0; i < SMPD_CMD_MAX; ++i )
    {
        smpd_process.cmdHandler[i] = smpd_handle_invalid_command;
    }

    //
    // Setup handler array to handle commands.  Smpd should only
    // receives these commands. To prevent bogus commands due to
    // bugs/corruption everything else is trapped in
    // smpd_handle_invalid_command which will trigger the abort job
    // process
    //
    smpd_process.cmdHandler[SMPD_BARRIER] = smpd_handle_barrier_command;
    smpd_process.cmdHandler[SMPD_CLOSE] = nullptr;
    smpd_process.cmdHandler[SMPD_CLOSED] = smpd_handle_closed_command;
    smpd_process.cmdHandler[SMPD_COLLECT] = smpd_handle_collect_command;
    smpd_process.cmdHandler[SMPD_CONNECT] = smpd_handle_connect_command;
    smpd_process.cmdHandler[SMPD_DBGET] = smpd_handle_dbget_command;
    smpd_process.cmdHandler[SMPD_DBPUT] = smpd_handle_dbput_command;
    smpd_process.cmdHandler[SMPD_BCGET] = smpd_handle_bcget_command;
    smpd_process.cmdHandler[SMPD_BCPUT] = smpd_handle_bcput_command;
    smpd_process.cmdHandler[SMPD_KILL] = smpd_handle_kill_command;
    smpd_process.cmdHandler[SMPD_LAUNCH] = smpd_handle_launch_command;
    smpd_process.cmdHandler[SMPD_STARTDBS] = smpd_handle_start_dbs_command;
    smpd_process.cmdHandler[SMPD_ADDDBS] = smpd_handle_add_dbs_command;
    smpd_process.cmdHandler[SMPD_STDIN] = smpd_handle_stdin_command;
    smpd_process.cmdHandler[SMPD_STDIN_CLOSE] = smpd_handle_stdin_close_command;
    smpd_process.cmdHandler[SMPD_SUSPEND] = smpd_handle_suspend_command;
    smpd_process.cmdHandler[SMPD_PING] = smpd_handle_ping_command;

    smpd_process.abortExFunc = SmpdAbortFromRPCThread;
}


static void
smpd_clear_global()
{
    while (smpd_process.pBizCardsList != nullptr)
    {
        smpd_process_biz_cards_t* pDelete = smpd_process.pBizCardsList;
        smpd_process.pBizCardsList = smpd_process.pBizCardsList->pNext;
        for (unsigned i = 0; i < pDelete->nproc; ++i)
        {
            delete[] pDelete->ppBizCards[i];
        }
        delete[] pDelete->ppBizCards;
        delete[] pDelete;
    }

    smpd_numchildren_t* pNumChildrenNode = smpd_process.num_children_list;
    while (pNumChildrenNode != nullptr)
    {
        smpd_numchildren_t* pDelete = pNumChildrenNode;
        pNumChildrenNode = pNumChildrenNode->next;
        delete pDelete;
    }

    while (smpd_process.barrier_forward_list != nullptr)
    {
        smpd_barrier_forward_t* pDelete = smpd_process.barrier_forward_list;
        smpd_process.barrier_forward_list = smpd_process.barrier_forward_list->next;
        delete pDelete;
    }

    if( smpd_process.hJobObj != nullptr )
    {
        CloseHandle( smpd_process.hJobObj );
    }

    if( smpd_process.rank_affinities != nullptr )
    {
        UnmapViewOfFile( smpd_process.rank_affinities );
    }

    if( smpd_process.rank_affinity_region != nullptr )
    {
        CloseHandle( smpd_process.rank_affinity_region );
    }

    if( smpd_process.node_id_region != nullptr )
    {
        CloseHandle( smpd_process.node_id_region );
    }

    if (smpd_process.pwd != nullptr && smpd_process.pwd[0] != '\0')
    {
        delete[] smpd_process.pwd;
    }
    delete[] smpd_process.job_context;
}


static DWORD
smpd_manager(
    _In_ HANDLE hWrite
    )
{
    smpd_dbg_printf(L"Launching smpd manager instance.\n");

    /* Set a ctrl-handler to kill child processes if this smpd is killed */
    if(!SetConsoleCtrlHandler(smpd_ctrl_handler, TRUE))
    {
        DWORD gle = GetLastError();
        smpd_dbg_printf(
            L"unable to set the ctrl handler for the smpd manager, error %u.\n",
            gle);
    }

    ExSetHandle_t set;
    set = ExCreateSet();
    if(set == EX_INVALID_SET)
    {
        smpd_err_printf(L"ExCreateSet(listener) failed, no memory\n");
        return GetLastError();
    }

    smpd_process.set = set;
    smpd_dbg_printf(L"created set for manager listener %u\n", ExGetPortValue(set));

    DWORD rc = NOERROR;
    GUID lrpcPort;
    wchar_t str[SMPD_MAX_PORT_STR_LENGTH];
    if( !smpd_process.local_root )
    {
        UINT16 tcpPort = static_cast<UINT16>( env_to_int(L"SMPD_MANAGER_PORT",0,0) );
        DWORD rc = smpd_create_mgr_server( &tcpPort, &lrpcPort );
        if( rc != NOERROR )
        {
            smpd_err_printf(L"failed to create the local root server\n");
            return rc;
        }

        smpd_process.mgrServerPort = tcpPort;
        MPIU_Snprintf(str, _countof(str), L"%hu", tcpPort);
    }
    else
    {
        DWORD rc = smpd_create_mgr_server( nullptr, &lrpcPort );
        if( rc != NOERROR )
        {
            smpd_err_printf(L"failed to create the manager server\n");
            return rc;
        }
        GuidToStr( lrpcPort, str, _countof(str) );
    }

    smpd_process.localServerPort = lrpcPort;
    smpd_dbg_printf(L"smpd manager listening on port %s\n", str);

    DWORD num_written;
    if(!WriteFile(hWrite, str, sizeof(str), &num_written, NULL))
    {
        DWORD gle = GetLastError();
        smpd_err_printf(L"WriteFile failed, error %u\n", gle);
        return gle;
    }
    CloseHandle(hWrite);

    if(num_written != sizeof(str))
    {
        smpd_err_printf(L"wrote only %u bytes of %Iu\n", num_written, sizeof(str));
        return ERROR_INSUFFICIENT_BUFFER;
    }

    bool useJobObj = g_IsWin8OrGreater && !env_is_on( L"SMPD_NO_JOBOBJ", FALSE );
    if( useJobObj )
    {
        ASSERT( smpd_process.hJobObj == nullptr );
        smpd_process.hJobObj = CreateJobObjectW( nullptr, nullptr );
        if( smpd_process.hJobObj != nullptr )
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobLimit = {0};
            jobLimit.BasicLimitInformation.LimitFlags =
                JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;

            BOOL fSucc = SetInformationJobObject(
                smpd_process.hJobObj,
                JobObjectExtendedLimitInformation,
                &jobLimit,
                sizeof( jobLimit ) );
            if( !fSucc )
            {
                smpd_dbg_printf(L"failed to set job object information, error %u\n", GetLastError());
                CloseHandle( smpd_process.hJobObj );
                smpd_process.hJobObj = nullptr;
            }
        }
        else
        {
            smpd_dbg_printf(L"failed to create a job object, error %u\n", GetLastError());
        }
    }
    else
    {
        smpd_dbg_printf(L"smpd will not assign child processes to job object\n");
    }

    LoadPmiDbgExtensions(PMIDBG_HOST_MANAGER);
    NotifyPmiDbgExtensions(SmpdNotifyInitialize);

    rc = smpd_progress(set);

    NotifyPmiDbgExtensions(SmpdNotifyFinalize);
    UnloadPmiDbgExtensions();

    smpd_kill_all_processes();
    smpd_stop_mgr_server();

    smpd_dbg_printf( L"SMPD exiting with error code %u.\n", rc );
    return rc;
}


int __cdecl wmain(int /*argc*/, _In_ wchar_t* argv[])
{
    if( env_is_on( L"MPIEXEC_UNICODE_OUTPUT", FALSE ) )
    {
        if (!EnableUnicodeOutput())
        {
            smpd_err_printf(L"Cannot set unicode mode for std streams\n");
            goto fn_exit1;
        }
    }

    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    HRESULT hr;
    PmiServiceInterface pmiServiceInterface;

    /* Make SMPD a trace event provider. */
    EventRegisterMicrosoft_MPI_Channel_Provider();

    HANDLE hProcessToken;
    BOOL fSucc = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hProcessToken);
    if(!fSucc)
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        smpd_err_printf(L"OpenProcessToken failed, error 0x%08X\n", hr);
        goto fn_exit1;
    }

    hr = smpd_get_user_from_token( hProcessToken, &g_pTokenUser );
    CloseHandle( hProcessToken );
    if( FAILED(hr) )
    {
        goto fn_exit1;
    }

    pmiServiceInterface.Size = sizeof(PmiServiceInterface);

    hr = MSMPI_Get_pm_interface(
        PM_SERVICE_INTERFACE_V1,
        &pmiServiceInterface
        );

    if(FAILED(hr))
    {
        smpd_err_printf(L"Get_PMI_Service_Interface failed with error 0x%08X.\n", hr);
        goto fn_exit2;
    }

    PmiServiceInitData initData;
    initData.Size = sizeof(PmiServiceInitData);
    initData.Name = "smpd";

    hr = pmiServiceInterface.Initialize(&initData);

    if(FAILED(hr))
    {
        smpd_err_printf(L"PMI service interface initialize failed with error 0x%08X.\n", hr);
        goto fn_exit2;
    }

    smpd_init_cmd_handlers();

    /* set default debug options */
    smpd_process.dbg_state = SMPD_DBG_STATE_ERROUT | SMPD_DBG_STATE_PREPEND_RANK;

    //
    // Parse the command line and find out if this is a smpd service
    // or a smpd manager instance
    //
    HANDLE hManagerWritePipe;
    if(!smpd_parse_command_args(argv, &hManagerWritePipe))
    {
        hr = E_INVALIDARG;
        goto fn_exit3;
    }

    //
    // In the case of SMPD Manager Instance, execute the manager logic
    //
    if( hManagerWritePipe != nullptr )
    {
        DWORD rc = smpd_manager( hManagerWritePipe );
        hr = HRESULT_FROM_WIN32( rc );
        goto fn_exit3;
    }

    //
    // The rest of the code is only executed by SMPD Service.
    //
    smpd_dbg_printf(L"Launching SMPD service.\n");

    SOCKADDR_INET addr;
    addr.si_family = AF_INET;
    addr.Ipv4.sin_port = _byteswap_ushort(static_cast<USHORT>(smpd_process.rootServerPort));
    addr.Ipv4.sin_addr.S_un.S_addr = INADDR_ANY;

    PmiManagerInterface manager;
    manager.Size = sizeof(manager);
    manager.Launch.UserSid = SdkCreateManagerProcess;
    manager.LaunchType = PmiLaunchTypeUserSid;

    hr = pmiServiceInterface.Listen(
        &addr,
        &manager,
        PM_MANAGER_INTERFACE_V1
        );
    if(FAILED(hr))
    {
        smpd_err_printf(L"state machine failed with error 0x%08X.\n", hr);
        goto fn_exit3;
    }

    smpd_dbg_printf( L"SMPD exiting with error code 0x%08x.\n", hr );

fn_exit3:
    pmiServiceInterface.Finalize();

fn_exit2:
    free( g_pTokenUser );

fn_exit1:
    EventUnregisterMicrosoft_MPI_Channel_Provider();
    smpd_clear_global();
    return hr;
}
