// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiexec.h"
#include "PmiDbgImpl.h"
#include "mpitrace.h"
#include "SvcUtils.h"
#include "cs.h"
#include <fcntl.h>
#include <io.h>

#define MPIEXEC_CONNECT_RETRIES_DEFAULT 12
#define MPIEXEC_CONNECT_RETRY_INTERVAL_DEFAULT 5

#define MPIEXEC_EXIT_TIMEOUT 30000

static int
mpiexec_exit_from_rpc(
    EXOVERLAPPED* /*pexov*/
    )
{
    if(smpd_process.mpiexec_exit_code == 0)
    {
        smpd_process.mpiexec_exit_code = SMPD_EXIT_FROM_RPC;
    }

    smpd_signal_exit_progress(MPI_SUCCESS);
    return MPI_SUCCESS;
}


static void mpiexec_abort_from_rpc_thread()
{
    //
    // This abort is triggered during the reconnect from the smpd top
    // of tree manager to mpiexec. At this point no other smpd manager
    // nor processes have been launched yet. It's safe (or rather,
    // graceful enough) to just exit mpiexec
    //

    EXOVERLAPPED ov;
    ExInitOverlapped(&ov, mpiexec_exit_from_rpc, mpiexec_exit_from_rpc);
    ExPostOverlappedResult(smpd_process.set, &ov, 0, 0);

    //
    // Give the engine some time to shutdown and then force an exit
    //
    Sleep( MPIEXEC_EXIT_TIMEOUT );
    smpd_err_printf(
        L"\nFailed to terminate the job after 30 seconds. Forcing mpiexec exit.\n");
    exit( SMPD_EXIT_FROM_RPC );
}


static void
mpiexec_init(
    _In_ bool localonly
    )
{
    smpd_init_process("mpiexec", localonly);

    smpd_process.abortExFunc = mpiexec_abort_from_rpc_thread;

    for( unsigned int i = 0; i < SMPD_CMD_MAX; ++i )
    {
        smpd_process.cmdHandler[i] = mpiexec_handle_invalid_command;
    }

    //
    // Setup handler array to handle commands from SMPD.  Mpiexec
    // should only receives these commands from SMPD. To prevent bogus
    // commands due to bugs/corruption everything else is trapped in
    // mpiexec_handle_invalid_command which will prompty abort the
    // process.
    //
    smpd_process.cmdHandler[SMPD_ABORT] = mpiexec_handle_abort_command;
    smpd_process.cmdHandler[SMPD_ABORT_JOB] = mpiexec_handle_abort_job_command;
    smpd_process.cmdHandler[SMPD_CLOSED] = smpd_handle_closed_command;
    smpd_process.cmdHandler[SMPD_EXIT] = mpiexec_handle_exit_command;
    smpd_process.cmdHandler[SMPD_FINALIZE] = mpiexec_handle_finalize_command;
    smpd_process.cmdHandler[SMPD_INIT] = mpiexec_handle_init_command;
    smpd_process.cmdHandler[SMPD_STDERR] = mpiexec_handle_stderr_command;
    smpd_process.cmdHandler[SMPD_STDOUT] = mpiexec_handle_stdout_command;
    smpd_process.cmdHandler[SMPD_PING] = smpd_handle_ping_command;
    smpd_process.cmdHandler[SMPD_SPAWN] = mpiexec_handle_spawn_command;

    //
    // 0 : default behavior (no exclusion)
    // 1 : disable Kerberos as a package (!Kerberos) - but still use SpNego
    // 2 : force the use of NTLM
    //
    int val = env_to_int( L"MPIEXEC_DISABLE_KERB", 0, 0 );
    if( val == 1 )
    {
        smpd_process.authOptions = SMPD_AUTH_DISABLE_KERB;
    }
    else if( val == 2 )
    {
        smpd_process.authOptions = SMPD_AUTH_NTLM;
    }
}


static int mpiexec_exit_on_timeout(EXOVERLAPPED* /*pexov*/)
{
    if(smpd_process.mpiexec_exit_code == 0)
    {
        smpd_process.mpiexec_exit_code = SMPD_EXIT_FROM_TIMEOUT;
    }

    smpd_signal_exit_progress(MPI_SUCCESS);
    return MPI_SUCCESS;
}


static void CALLBACK TimeoutTimerCallback(void* /*p*/, BOOLEAN /*TimerFired*/)
{
    //
    // *** This function is called on the timer thread. ***
    //

    //
    // Queue the timer expiration callback back to the Executive thread.
    //
    EXOVERLAPPED ov;
    ExInitOverlapped(&ov, mpiexec_exit_on_timeout, mpiexec_exit_on_timeout);

    smpd_err_printf(L"\nTimeout after %d seconds. Terminating job...\n",
                    smpd_process.timeout);
    ExPostOverlappedResult(smpd_process.set, &ov, 0, 0);

    //
    // Give the engine some time to shutdown and then force an exit
    //
    Sleep( MPIEXEC_EXIT_TIMEOUT );
    smpd_err_printf(
        L"\nFailed to terminate the job after 30 seconds. Forcing mpiexec exit.\n");
    exit( SMPD_EXIT_FROM_TIMEOUT );
}


static DWORD
gle_start_timeout_timer(
    DWORD dueTime
    )
{
    HANDLE hTimer;
    if( CreateTimerQueueTimer(
                &hTimer,
                NULL,
                TimeoutTimerCallback,
                0,
                dueTime,
                0,
                WT_EXECUTEDEFAULT ) == FALSE )
    {
        return GetLastError();
    }

    return NO_ERROR;
}


static void CALLBACK PeriodicSmpdPing(void* /*p*/, BOOLEAN /*TimerFired*/)
{
    //
    // *** This function is called on the Smpd Ping Timer thread. ***
    //
    //
    // Ping the left child
    //
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_PING,
        SMPD_IS_ROOT,
        SMPD_IS_TOP );
    if( pCmd == nullptr )
    {
        smpd_dbg_printf(L"failed to create ping command");
        return;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        nullptr );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_dbg_printf(L"failed to create ping result command");
        return;
    }

    DWORD rc = smpd_post_command(
        smpd_process.left_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_dbg_printf(L"failed to post ping command error %lu\n", rc);
    }

    return;
}


static DWORD
StartSmpdPingTimer()
{
    //
    // User-specified heartbeat in number of seconds. By default there's no
    // heartbeat.
    //
    int period = env_to_int(L"MPIEXEC_SMPD_PING_INTERVAL", 0, 0) * 1000;
    if( period <= 0 )
    {
        return NOERROR;
    }

    if( CreateTimerQueueTimer(
                &smpd_process.hHeartbeatTimer,
                NULL,
                PeriodicSmpdPing,
                0,
                period,
                period,
                WT_EXECUTEDEFAULT ) == FALSE )
    {
        return GetLastError();
    }

    return NO_ERROR;
}


static int mpiexec_ctrl_abort(EXOVERLAPPED* /*pexov*/)
{
    if( smpd_process.pg_list == nullptr )
    {
        /* no processes have been started yet, so exit here */
        exit(-1);
    }

    //
    // mpiexec_abort_job will do the following
    // 1) Suspend all the processes
    // 2) Kill all the processes
    // 3) Tear down the job tree
    //
    DWORD rc = mpiexec_abort_job( smpd_process.pg_list, 0, arCtrlC, 0, nullptr );
    if(rc != NOERROR)
    {
        //
        // Could not abort; exit the progress engine
        //
        smpd_signal_exit_progress( rc );
        return static_cast<int>( rc ) ;
    }

    return NOERROR;
}


static BOOL WINAPI
mpiexec_ctrl_handler(
    DWORD dwCtrlType
    )
{
    static bool ctrlHit = false;

    static EXOVERLAPPED exov;

    /* This handler could be modified to send the event to the remote
     * processes instead of killing the job. */
    /* Doing so would require new command types for smpd */

    switch (dwCtrlType)
    {
    case CTRL_LOGOFF_EVENT:
        /* The logoff event could be a problem if two users are logged
         * on to the same machine remotely.  If user A runs mpiexec
         * and user B logs out of his session, this event will cause
         * user A's mpiexec command to kill the job and make user A
         * mad.  But if we ignore the logoff event then when user A
         * logs out mpiexec will hang?
         */
        break;
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if( ctrlHit )
        {
            /* The second break signal unconditionally exits mpiexec */
            exit(-1);
        }

        /* The first break signal tries to abort the job with an abort message */
        ctrlHit = true;
        fwprintf(stderr, L"mpiexec aborting job...\n");
        fflush(stderr);

        ExInitOverlapped(&exov, mpiexec_ctrl_abort, mpiexec_ctrl_abort);
        ExPostOverlappedResult(smpd_process.set, &exov, 0, 0);
        return TRUE;
    }

    return FALSE;
}


//
// smpd client connect completion callback. the smpd client framework calls this
// function after the connection to the smpd manager has been established
///
static DWORD
mpiexec_connected_child_mgr(
    _In_ smpd_context_t* context,
    _In_ smpd_host_t*    host
    )
{
    //
    // Trigger the connection of the rest of the tree
    //
    return mpiexec_handle_node_connection(context, host);
}


static void
print_node_tree()
{
    smpd_dbg_printf(L"host tree:\n");
    for( smpd_host_t* host = smpd_process.host_list;
         host != nullptr;
         host = static_cast<smpd_host_t*>( host->Next )
        )
    {
        smpd_dbg_printf(L" host: %s, parent: %d, id: %d\n",
                        host->name,
                        host->parent,
                        host->HostId);
    }
}


static void mpiexec_clear_global()
{
    delete smpd_process.host_list;
    smpd_process.host_list = nullptr;

    smpd_node_id_node_t* pNodeIdNode = smpd_process.node_id_list;
    while (pNodeIdNode != nullptr)
    {
        smpd_node_id_node_t* pDelete = pNodeIdNode;
        pNodeIdNode = pNodeIdNode->next;
        delete pDelete;
    }

    smpd_launch_node_t* pNode = smpd_process.launch_list;
    while(pNode != nullptr)
    {
        smpd_launch_node_t* pDelete = pNode;
        pNode = pNode->next;
        delete pDelete;
    }

    if (smpd_process.pwd != nullptr && smpd_process.pwd[0] != '\0')
    {
        delete[] smpd_process.pwd;
    }

    if( smpd_process.hJobObj != nullptr )
    {
        CloseHandle( smpd_process.hJobObj );
    }

    delete[] smpd_process.job_context;
    delete reinterpret_cast<mp_global_options_t*>(smpd_process.pGlobalBlockOpt);

    if (smpd_process.pNumaNodeInfo != nullptr)
    {
        MPIU_Free(smpd_process.pNumaNodeInfo);
    }
}

//
// For now all the errors detected at mpiexec will lead to the
// aborting of the entire chain. In the future we might have different
// kind of errors that don't always abort.
//
static void
MpiexecHandleCmdError(
    smpd_overlapped_t* pov,
    _In_ int error
    )
{
    SmpdCmd* pCmd = pov->pCmd;
    SmpdCmdHdr* pHeader = reinterpret_cast<SmpdCmdHdr*>(pCmd);
    SmpdResWrapper* pRes =
        CONTAINING_RECORD(pov->pRes, SmpdResWrapper, Res);

    smpd_dbg_printf( L"error %d detected during previous command, initiating abort.\n",
                     error );

    if( pHeader->cmdType == SMPD_LAUNCH )
    {
        if( pRes->Res.LaunchRes.count != 0 )
        {
            smpd_err_printf( L"Error reported: %s\n",
                             pRes->Res.LaunchRes.error_msg );
            midl_user_free( pRes->Res.LaunchRes.error_msg );
        }

        smpd_process_group_t* pg = find_pg( pCmd->LaunchCmd.kvs );
        for( unsigned i = 0; i < pCmd->LaunchCmd.rankCount; ++i )
        {
            UINT16 rank = pCmd->LaunchCmd.rankArray[i];
            ASSERT( pg != nullptr );
            if( pg->processes[rank].suspend_pending == TRUE )
            {
                pg->processes[rank].suspend_pending = FALSE;
                pg->num_pending_suspends--;

                ASSERT(pg->num_pending_suspends >= 0);
            }
            pg->processes[rank].failed_launch = TRUE;
        }

        if( pg->aborted && pg->num_pending_suspends == 0 )
        {
            mpiexec_tear_down_tree();
            return;
        }
    }

    //
    // Walk the list of pg's and abort them all
    //
    for( smpd_process_group_t* pg = smpd_process.pg_list;
         pg != nullptr;
         pg = pg->next )
    {
        DWORD rc = mpiexec_abort_job(
            pg,
            0,
            arFatalError,
            -1,
            nullptr );
        if( rc != NOERROR )
        {
            smpd_post_abort_command( L"failed to abort the job error %u.\n", rc );
            return;
        }
    }
}


extern FN_PmiLaunch MpiexecCreateManagerProcess;


int __cdecl wmain(
    _In_ int argc,
    _In_reads_(argc+1) wchar_t* argv[]
    )
{
    if( env_is_on( L"MPIEXEC_UNICODE_OUTPUT", FALSE ) )
    {
        if (!EnableUnicodeOutput())
        {
            goto CleanUp1;
        }
    }

    int rc = 0;
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    /* Make mpiexec an event provider. */
    EventRegisterMicrosoft_MPI_Channel_Provider();

    /* set default debug options */
    smpd_process.dbg_state = SMPD_DBG_STATE_ERROUT;

    /* catch an empty command line */
    if(argc < 2)
    {
        mp_print_options();
        goto CleanUp1;
    }

    mpiexec_init(true);
    smpd_process.tree_id = 0;

    if(!mp_parse_command_args(argv))
    {
        rc = -1;
        goto CleanUp1;
    }

    if(smpd_process.dbg_state & SMPD_DBG_STATE_STDOUT)
    {
        print_node_tree();
    }

    LoadPmiDbgExtensions(PMIDBG_HOST_CONTROLLER);
    NotifyPmiDbgExtensions(MpiexecNotifyInitialize);

    ExSetHandle_t set = ExCreateSet();
    if(set == EX_INVALID_SET)
    {
        smpd_err_printf(L"Error: failed to create mpiexec completion port; out of memory\n");
        rc = -1;
        goto CleanUp1;
    }
    smpd_process.set = set;

    if( !smpd_process.local_root )
    {
        UINT16 port = static_cast<UINT16>( env_to_int(L"SMPD_MANAGER_PORT",0,0) );
        rc = smpd_create_mgr_server( &port, nullptr );
        if( rc != NOERROR )
        {
            smpd_err_printf(L"failed to create the manager server\n");
            rc = -1;
            goto CleanUp2;
        }

        smpd_process.mgrServerPort = port;
        smpd_dbg_printf(L"mpiexec started smpd manager listening on port %u\n", port);
    }
    else
    {
        GUID lrpcPort;
        rc = smpd_create_mgr_server( nullptr, &lrpcPort );
        if( rc != NOERROR )
        {
            smpd_err_printf(L"failed to create local root server\n");
            rc = -1;
            goto CleanUp2;
        }

        smpd_process.localServerPort = lrpcPort;
        smpd_dbg_printf(
            L"mpiexec started smpd manager listening on port "
            L"%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
            lrpcPort.Data1, lrpcPort.Data2, lrpcPort.Data3,
            lrpcPort.Data4[0], lrpcPort.Data4[1], lrpcPort.Data4[2], lrpcPort.Data4[3],
            lrpcPort.Data4[4], lrpcPort.Data4[5], lrpcPort.Data4[6], lrpcPort.Data4[7] );
    }

    //
    // Indicate the function to use to provide authentication when
    // launching the local SMPD instance.
    //
    PmiManagerInterface manager;
    manager.Size = sizeof(manager);
    manager.Launch.AsSelf = MpiexecCreateManagerProcess;
    manager.LaunchType = PmiLaunchTypeSelf;

    smpd_process.manager_interface = &manager;

    /* Start the timeout mechanism if specified */
    if(smpd_process.timeout > 0)
    {
        DWORD gle = gle_start_timeout_timer(smpd_process.timeout * 1000);
        if(gle != NO_ERROR)
        {
            smpd_err_printf(L"Error: unable to create the timeout timer, gle %u.\n", gle);
            rc = -1;
            goto CleanUp2;
        }
    }

    /* Create a break handler to handle aborting the job when mpiexec
     * receives break signals */
    if(!SetConsoleCtrlHandler(mpiexec_ctrl_handler, TRUE))
    {
        /* Don't error out; allow the job to run without a ctrl handler? */
        rc = GetLastError();
        smpd_dbg_printf(L"unable to set a ctrl handler for mpiexec, error %d\n", rc);
    }

    if(smpd_process.local_root)
    {
        smpd_context_t *pContext;
        pContext = smpd_create_context(SMPD_CONTEXT_LEFT_CHILD, set);
        if( pContext == nullptr )
        {
            smpd_err_printf(
                L"Error: unable to create a context for the first host in the tree.\n");
            rc = -1;
            goto CleanUp3;
        }

        int dbg_state = smpd_process.verbose ?  smpd_process.dbg_state : 0;

        size_t len = MPIU_Strlen( smpd_process.job_context ) + 1;
        pContext->job_context = new char[len];
        if( pContext->job_context == nullptr )
        {
            smpd_err_printf(L"Error: not enough memory to create context for mpiexec.\n");
            rc = -1;
            goto CleanUp3;
        }
        MPIU_Strcpy(pContext->job_context,
                    len,
                    smpd_process.job_context );

        //
        // set MSMPI_LOCALONLY so that MPI can disable sockets automatically.
        //
        SetEnvironmentVariableW( L"MSMPI_LOCAL_ONLY", L"1" );

        //
        // Launch the manager process
        //
        HRESULT hr = smpd_start_win_mgr( pContext, dbg_state );
        if( FAILED(hr) )
        {
            smpd_free_context( pContext );
            smpd_err_printf(L"Error: unable to start the local smpd manager.\n");
            rc = -1;
            goto CleanUp3;
        }

        //
        // Establish connection to the Smpd Manager Instance that we just launched
        //
        rc = smpd_connect_mgr_smpd(
                    SMPD_CONTEXT_LEFT_CHILD,
                    smpd_process.host_list->name,
                    static_cast<INT16>(smpd_process.host_list->HostId),
                    pContext->port_str,
                    &smpd_process.left_context
                    );

        smpd_free_context( pContext );
        if(rc != MPI_SUCCESS)
        {
            rc = -1;
            goto CleanUp3;
        }
    }
    else
    {
        smpd_process.connectRetryCount = env_to_int( L"MPIEXEC_CONNECT_RETRIES",
                                                     MPIEXEC_CONNECT_RETRIES_DEFAULT,
                                                     0 );
        smpd_process.connectRetryInterval = env_to_int( L"MPIEXEC_CONNECT_RETRY_INTERVAL",
                                                        MPIEXEC_CONNECT_RETRY_INTERVAL_DEFAULT,
                                                        1 );
        //
        // Start connecting the tree by posting a connect to the first host
        //
        rc = smpd_connect_root_server(
                    SMPD_CONTEXT_LEFT_CHILD,
                    smpd_process.host_list->HostId,
                    smpd_process.host_list->name,
                    false,
                    smpd_process.connectRetryCount,
                    smpd_process.connectRetryInterval
                    );

        if(rc == NTE_UI_REQUIRED)
        {
            //
            // Prompt for password
            //
            if (!MpiexecGetUserInfoInteractive())
            {
                //
                // Session is not interactive or cannot perform this operation.
                // Simply fail.
                //
                smpd_err_printf(
                    L"Failed to get user credentials. Shutting down.\n"
                    L"Use -pwd <password> and -savecreds options to provide and save "
                    L"credentials.\n"
                    );
                goto CleanUp3;
            }

            //
            // Retry with input credentials
            //
            rc = smpd_connect_root_server(
                SMPD_CONTEXT_LEFT_CHILD,
                smpd_process.host_list->HostId,
                smpd_process.host_list->name,
                false,
                smpd_process.connectRetryCount,
                smpd_process.connectRetryInterval
                );
        }

        if(rc != MPI_SUCCESS)
        {
            rc = -1;
            goto CleanUp3;
        }
    }

    smpd_process.left_context->on_cmd_error = MpiexecHandleCmdError;
    rc = mpiexec_connected_child_mgr( smpd_process.left_context, smpd_process.host_list );
    if( rc != MPI_SUCCESS )
    {
        rc = -1;
        goto CleanUp3;
    }

    rc = StartSmpdPingTimer();
    if( rc != NOERROR )
    {
        rc = -1;
        goto CleanUp3;
    }


    rc = smpd_progress(set);

    if((rc != MPI_SUCCESS) && (smpd_process.mpiexec_exit_code == 0))
    {
        smpd_process.mpiexec_exit_code = -1;
    }

    rc = smpd_process.mpiexec_exit_code;


    NotifyPmiDbgExtensions(MpiexecNotifyFinalize);
    UnloadPmiDbgExtensions();

CleanUp3:
    if( smpd_process.hHeartbeatTimer != NULL )
    {
        BOOL timer_marked_for_deletion = DeleteTimerQueueTimer(nullptr, smpd_process.hHeartbeatTimer, nullptr);
        while (!timer_marked_for_deletion)
        {
            DWORD last_error = GetLastError();
            if (last_error == ERROR_IO_PENDING)
            {
                break;
            }
            else
            {
                timer_marked_for_deletion = DeleteTimerQueueTimer(nullptr, smpd_process.hHeartbeatTimer, nullptr);
            }
        }
    }
    smpd_stop_mgr_server();

CleanUp2:
    ExCloseSet( smpd_process.set );

CleanUp1:
    EventUnregisterMicrosoft_MPI_Channel_Provider();
    mpiexec_clear_global();
    return rc;
}


bool MpiexecGetUserInfoInteractive()
{
    DWORD   mode;
    DWORD   newMode;
    DWORD   numRead          = 0;
    HANDLE  stdIn            = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE  stdOut           = GetStdHandle(STD_OUTPUT_HANDLE);
    WCHAR   user[MAX_PATH]   = { 0 };
    WCHAR   domain[MAX_PATH] = { 0 };
    DWORD   cchUser          = _countof(user);
    DWORD   cchDomain        = _countof(domain);
    BOOL    isInteractive    = FALSE;
    HRESULT result;
    BOOL    success;

    result = SecurityUtils::IsCurrentProcessInteractive(&isInteractive);
    if (FAILED(result) || GetFileType(stdOut) != FILE_TYPE_CHAR)
    {
        return false;
    }

    static CriticalSection csUI;
    static BOOL inputRetrieved = FALSE;

    CS lockUI(csUI);

    if (inputRetrieved)
    {
        return true;
    }

    inputRetrieved = TRUE;

    result = SecurityUtils::GetCurrentUser(user, &cchUser, domain, &cchDomain);
    if (FAILED(result))
    {
        smpd_err_printf(L"Failed to retrieve user name. Error 0x%x\n", result);
        return false;
    }

    fflush(stdout);

    GetConsoleMode(stdIn, &mode);

    //
    // Hide typed in characters
    //
    newMode = (mode & ~ENABLE_ECHO_INPUT) | ENABLE_LINE_INPUT;
    SetConsoleMode(stdIn, newMode);

    //
    // Read in user's password
    //
    smpd_process.pwd = new wchar_t[MAX_PATH];
    if (smpd_process.pwd == nullptr)
    {
        smpd_err_printf(L"Failed to allocate memory for retrieving password.\n");
        SetConsoleMode(stdIn, mode);
        return false;
    }

    smpd_process.pwd[0] = L'\0';
    wprintf(L"\nEnter Password for %s\\%s: ", domain, user);

    success = ReadConsoleW(stdIn, smpd_process.pwd, MAX_PATH, &numRead, nullptr);
    if (!success || numRead == 0)
    {
        SetConsoleMode(stdIn, mode);
        return false;
    }

    //
    // ReadConsole adds CR+LF to buffer, so remove them
    //
    bool newLineRemoved = false;
    for (size_t i = numRead; i-- > 0;)
    {
        if (smpd_process.pwd[i] == L'\n' || smpd_process.pwd[i] == L'\r')
        {
            smpd_process.pwd[i] = L'\0';
            newLineRemoved = true;
        }
        else if (newLineRemoved)
        {
            break;
        }
    }

    //
    // Recover console mode
    //
    SetConsoleMode(stdIn, mode);

    //
    // Read in saveCreds switch
    //
    wprintf(L"\nSave Credentials[y|n]? ");
    wchar_t saveCreds = L'n';
    ReadConsoleW(stdIn, &saveCreds, 1, &numRead, nullptr);
    smpd_process.saveCreds = (saveCreds == L'y' || saveCreds == L'Y');

    return true;
}
