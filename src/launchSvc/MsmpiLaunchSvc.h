// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <Windows.h>
#include <initguid.h>
#include "SvcUtils.h"
#include "mspms.h"
#include "launchSvcMsg.h"


//
// MSMPI PMI settings
//
#define MSPMI_MODULE_PATH_KEY       L"Software\\Microsoft\\MPI"
#define MSPMI_PROVIDER_VALUE        L"MSPMSProvider"
#define MSPMI_PROC_GET_PMI          "MSMPI_Get_pm_interface"
#define MSPMI_PROC_PMI_QUERY_IF     "MSMPI_pm_query_interface"
#define MSPMI_CREDENTIAL_VALUE      L"MSMPI_Credentials"
#define DEFAULT_SERVICE_PORT        8677
#define MAXIMUM_CONTEXTS            (MAXIMUM_WAIT_OBJECTS - 1)
#define SHUTDOWN_TIMEOUT            60000
#define NEW_PROCESS_EVENT_IDX       0


/*--------------------------------------------------------------------------*/
/* LaunchContext                                                            */
/*--------------------------------------------------------------------------*/

//
// Holds information that is related to a launch context, between start and
// end of launch context calls from PMI
//
class LaunchContext
{
public:
    const void*     m_pmiHandle;
    HANDLE          m_parentThread;
    HANDLE          m_primaryToken;

    HANDLE          m_mgrProcess;
    HANDLE          m_userToken;
    HANDLE          m_userProfile;

    LaunchContext();
    ~LaunchContext();

    void Dispose();
};


/*--------------------------------------------------------------------------*/
/* ContextPool                                                              */
/*--------------------------------------------------------------------------*/

//
// Thread-safe collection of active launch contexts
//
class ContextPool
{
private:
    LaunchContext   m_contexts[MAXIMUM_CONTEXTS];
    DWORD           m_activeIndices[MAXIMUM_CONTEXTS];
    UINT            m_activeContextCount;
    SRWLOCK         m_lock;

public:
    template<typename T>
    using IsMatch = BOOL(*)(_In_ const LaunchContext* pCtx, _In_ const T* pData);

    ContextPool();

    ~ContextPool();

    LaunchContext* CreateNewContext();

    BOOL DestroyContext(_In_ LaunchContext* pDeleteContext);

    template<typename T>
    LaunchContext*
    FindContext(
        _In_ IsMatch<T>   compareFunct,
        _In_ const T*     pData
        );
};


/*--------------------------------------------------------------------------*/
/* ProcessQueue                                                             */
/*--------------------------------------------------------------------------*/
class ProcessQueue
{
private:
    HANDLE           m_runningProcesses[MAXIMUM_WAIT_OBJECTS];
    HANDLE           m_newProcessEvent;
    volatile DWORD   m_count;
    HANDLE           m_thread;
    volatile BOOL    m_run;
    CRITICAL_SECTION m_lock;

    static DWORD WINAPI WaitProcesses(_In_ LPVOID pData);

    DWORD GetCountSafe();

    HANDLE DeleteProcess(_In_ DWORD idx);

public:
    ProcessQueue();

    ~ProcessQueue();

    HRESULT Initialize();

    HRESULT AddProcess(_In_ HANDLE newProcess);

    HRESULT Start();

    HRESULT Stop();
};


/*--------------------------------------------------------------------------*/
/* MsmpiLaunchService                                                       */
/*--------------------------------------------------------------------------*/

class MsmpiLaunchService
{
    typedef
        HRESULT
        (WINAPI *PFN_MSMPI_GET_PM_INTERFACE)(
        _In_    REFGUID                 RequestedVersion,
        _Inout_ PmiServiceInterface*    Interface
        );

    typedef
        HRESULT
        (WINAPI *PFN_MSMPI_PM_QUERY_INTERFACE)(
        _In_    REFGUID              RequestedVersion,
        _Inout_ void**               Interface
        );

private:
    HMODULE                     m_pmiModule;
    PmiServiceInterface         m_pmiService;
    PmiServiceLaunchInterface*  m_pMspmiServiceLaunch;
    USHORT                      m_servicePort;
    PSID                        m_pMemberGroupSid;
    ContextPool                 m_contextPool;
    ProcessQueue                m_mgrQueue;

public:
    MsmpiLaunchService();
    ~MsmpiLaunchService();
    HRESULT Load();
    BOOL ParseOptions(_In_ DWORD  argc, _In_ LPWSTR *argv);
    HRESULT Run();
    VOID Stop();
    VOID ManagerProcessTerminated(_In_ HANDLE mgrProcess);

    static HRESULT
    WINAPI ServiceCreateManagerProcess(
        _In_z_  PCSTR   app,
        _In_z_  PCSTR   args,
        _In_z_  PCSTR   context
        );

    static HRESULT
    WINAPI ServiceCreateLaunchCtx(
        _In_    HANDLE      clientToken,
        _In_    const void* launchCtx,
        _In_z_  const char* jobCtx
        );

    static HRESULT WINAPI ServiceStartLaunchCtx(_In_ const void* launchCtx);
    static HRESULT WINAPI ServiceCleanupLaunchCtx(_In_ const void* launchCtx);

private:

    HRESULT
    DoLogonUser(
        _In_     LPCWSTR     pPwd,
        _In_     BOOL        saveCreds,
        _Outptr_ PHANDLE     pLogonHandle
        );
};
