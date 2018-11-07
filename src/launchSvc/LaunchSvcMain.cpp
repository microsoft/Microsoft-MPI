// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "LaunchSvc.h"
#include <oacr.h>

EventLogger gEventLogger;
WindowsSvc  WindowsSvc::ms_windowsSvc;

MsmpiLaunchService& Launcher()
{
    return WindowsSvc::ms_windowsSvc.m_launcher;
}

//
// Entry point
//
int __cdecl main(int /*argc*/, const char* /*argv[]*/)
{
    if (!gEventLogger.Open(SERVICE_NAME))
    {
        return GetLastError();
    }

    gEventLogger.WriteEvent(EVENTLOG_INFORMATION_TYPE, SVC_CATEGORY, SERVICE_EVENT, L"Starting Launch Service");

    //
    // Start MsMpi Launch Service
    //
    HRESULT result = WindowsSvc::ms_windowsSvc.Start();

    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to start launch service. Error=0x%x\n",
            result);
        return result;
    }

    gEventLogger.WriteEvent(
        EVENTLOG_INFORMATION_TYPE,
        SVC_CATEGORY,
        SERVICE_EVENT,
        L"Ended Launch Service. Result=0x%x\n",
        result);

    return result;
}


/*---------------------------------------------------------------------------*/
/* WindowsSvc Class Member Functions                                         */
/*---------------------------------------------------------------------------*/

WindowsSvc::WindowsSvc()
{
    m_serviceStatus.dwCheckPoint                = 0;
    m_serviceStatus.dwControlsAccepted          = 0;
    m_serviceStatus.dwCurrentState              = SERVICE_START_PENDING;
    m_serviceStatus.dwServiceSpecificExitCode   = 0;
    m_serviceStatus.dwServiceType               = SERVICE_WIN32_OWN_PROCESS;
    m_serviceStatus.dwWaitHint                  = 0;
    m_serviceStatus.dwWin32ExitCode             = 0;

    m_serviceStatusHandle                       = nullptr;

    m_ctrlDispatchTable[0]                      = { SERVICE_NAME, ServiceMain };
    m_ctrlDispatchTable[1]                      = { nullptr, nullptr };
}


HRESULT WindowsSvc::ChangeState(_In_ DWORD newState)
{
    switch (newState)
    {
    case SERVICE_RUNNING:
        m_serviceStatus.dwCurrentState = SERVICE_RUNNING;
        m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
        break;
    default:
        m_serviceStatus.dwCurrentState = newState;
        break;
    }

    if (!SetServiceStatus(m_serviceStatusHandle, &m_serviceStatus))
    {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to change service state to %d. Error=0x%x\n",
            newState,
            result);
        return result;
    }

    return S_OK;
}


//
// Does service specific initializations and registers service loop and control handlers
//
HRESULT WindowsSvc::Start()
{
    HANDLE  processToken;
    HRESULT result;

    //
    // Adjust process priviliges so it is able to load user profiles
    //
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &processToken))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    result = SecurityUtils::GrantPrivilege(processToken, SE_BACKUP_NAME, TRUE);
    if (FAILED(result))
    {
        CloseHandle(processToken);
        return result;
    }

    result = SecurityUtils::GrantPrivilege(processToken, SE_RESTORE_NAME, TRUE);

    CloseHandle(processToken);

    if (FAILED(result))
    {
        return result;
    }

    //
    // Load msmpi service launcher
    //
    result = m_launcher.Load();
    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"msmpi.dll did not load properly. Error=0x%x\n",
            result);

        return result;
    }

    //
    // Register to SCM
    //
    if (!StartServiceCtrlDispatcherW(m_ctrlDispatchTable))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to start launch service. Error=0x%x\n",
            result);

        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}


//
// Handles service control requests
//
VOID WINAPI WindowsSvc::ServiceCtrlHandler(_In_ DWORD ctrl)
{
    switch (ctrl)
    {
        //case SERVICE_CONTROL_SHUTDOWN ?
    case SERVICE_CONTROL_STOP:
        ms_windowsSvc.m_launcher.Stop();
        break;
    default:
        break;
    }
}


//
// Main service loop
//
VOID WINAPI WindowsSvc::ServiceMain(_In_ DWORD  argc, _In_ LPWSTR *argv)
{
    ms_windowsSvc.m_serviceStatusHandle =
        RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);

    if (ms_windowsSvc.m_serviceStatusHandle == nullptr)
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to register service control handler. Error=0x%x\n",
            HRESULT_FROM_WIN32(GetLastError()));

        return;
    }

    if (!ms_windowsSvc.m_launcher.ParseOptions(argc, argv))
    {
        ms_windowsSvc.m_serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        ms_windowsSvc.m_serviceStatus.dwServiceSpecificExitCode = ERROR_INVALID_PARAMETER;
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about the status of state change - no possible recovery.");
        ms_windowsSvc.ChangeState(SERVICE_STOPPED);
        return;
    }

    //
    // Start running launch service
    //

    HRESULT result = ms_windowsSvc.ChangeState(SERVICE_RUNNING);
    if (FAILED(result))
    {
        return;
    }

    result = ms_windowsSvc.m_launcher.Run();
    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to start listening to PMI clients. Error=0x%x\n",
            result);

        ms_windowsSvc.m_serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        ms_windowsSvc.m_serviceStatus.dwServiceSpecificExitCode = result;
    }

    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about the status of state change - no possible recovery.");
    ms_windowsSvc.ChangeState(SERVICE_STOPPED);
}
