// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <Windows.h>
#include "MsmpiLaunchSvc.h"

//
// Service settings
//
#define SERVICE_NAME             L"MsmpiLaunchSvc"
#define SERVICE_START_TYPE       SERVICE_AUTO_START

//
// A singleton class that will run as a windows service application.
// It handles the interaction with SCM and manages the launch service.
//
class WindowsSvc
{
private:
    SERVICE_TABLE_ENTRYW        m_ctrlDispatchTable[2];
    SERVICE_STATUS_HANDLE       m_serviceStatusHandle;
    SERVICE_STATUS              m_serviceStatus;

private:
    WindowsSvc();
    HRESULT ChangeState(_In_ DWORD newState);

public:
    MsmpiLaunchService          m_launcher;

    HRESULT Start();

    static WindowsSvc ms_windowsSvc;

    static VOID WINAPI ServiceMain(_In_ DWORD argc, _In_ LPWSTR * argv);
    static VOID WINAPI ServiceCtrlHandler(_In_ DWORD ctrl);
};
