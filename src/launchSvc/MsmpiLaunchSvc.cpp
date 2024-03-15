// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdlib.h>
#include <windows.h>
#include <Wincrypt.h>
#include <UserEnv.h>
#include "assertutil.h"
#include "MsmpiLaunchSvc.h"
#include "kernel32util.h"
#include "mpiutil.h"
#include "util.h"
#include <oacr.h>

extern EventLogger gEventLogger;
extern MsmpiLaunchService& Launcher();

/*--------------------------------------------------------------------------*/
/* LaunchContext                                                            */
/*--------------------------------------------------------------------------*/

LaunchContext::LaunchContext()
    : m_pmiHandle(nullptr)
    , m_parentThread(nullptr)
    , m_mgrProcess(nullptr)
    , m_primaryToken(nullptr)
    , m_userToken(nullptr)
    , m_userProfile(nullptr)
{
}


LaunchContext::~LaunchContext()
{
    Dispose();
}


void LaunchContext::Dispose()
{
    CloseHandle(m_parentThread);
    CloseHandle(m_mgrProcess);
    CloseHandle(m_primaryToken);
    UnloadUserProfile(m_userToken, m_userProfile);
    CloseHandle(m_userToken);

    ZeroMemory(this, sizeof(*this));
}


/*--------------------------------------------------------------------------*/
/* ContextPool                                                              */
/*--------------------------------------------------------------------------*/

ContextPool::ContextPool()
    : m_activeContextCount(0)
{
    InitializeSRWLock(&m_lock);

    for (DWORD i = 0; i < _countof(m_activeIndices); ++i)
    {
        m_activeIndices[i] = i;
    }
}


ContextPool::~ContextPool()
{
    AcquireSRWLockExclusive(&m_lock);

    WORD eType = m_activeContextCount == 0 ? EVENTLOG_INFORMATION_TYPE : EVENTLOG_WARNING_TYPE;
    gEventLogger.WriteEvent(
        eType,
        SVC_CATEGORY,
        SERVICE_EVENT,
        L"Service has %d active launch contexts",
        m_activeContextCount);

    for (DWORD i = 0; i < m_activeContextCount; ++i)
    {
        m_contexts[m_activeIndices[i]].Dispose();
    }

    ReleaseSRWLockExclusive(&m_lock);
}


LaunchContext* ContextPool::CreateNewContext()
{
    Assert(m_activeContextCount <= _countof(m_contexts));

    LaunchContext* pNewContext = nullptr;

    AcquireSRWLockExclusive(&m_lock);

    if (m_activeContextCount == _countof(m_contexts))
    {
        goto exit_fn;
    }

    pNewContext = m_contexts + m_activeIndices[m_activeContextCount];
    ++m_activeContextCount;

exit_fn:
    ReleaseSRWLockExclusive(&m_lock);
    return pNewContext;
}


BOOL ContextPool::DestroyContext(_In_opt_ LaunchContext* pDeleteContext)
{
    BOOL result = TRUE;
    if (pDeleteContext == nullptr)
    {
        return result;
    }

    AcquireSRWLockExclusive(&m_lock);

    ptrdiff_t idx = pDeleteContext - m_contexts;

    if (m_activeContextCount == 0
        || idx < 0 || static_cast<UINT>(idx) >= _countof(m_contexts))
    {
        Assert(false);
        result = FALSE;
        goto exit_fn;
    }

    m_contexts[idx].Dispose();

    DWORD idxActive = 0;
    for (; idxActive < m_activeContextCount; ++idxActive)
    {
        if (m_activeIndices[idxActive] == static_cast<UINT>(idx))
        {
            break;
        }
    }
    Assert(idxActive < m_activeContextCount);

    --m_activeContextCount;
    DWORD destroyedIdx = m_activeIndices[idxActive];
    m_activeIndices[idxActive] = m_activeIndices[m_activeContextCount];
    m_activeIndices[m_activeContextCount] = destroyedIdx;

exit_fn:
    ReleaseSRWLockExclusive(&m_lock);
    return result;
}


//
// Returns the first context that satisfies the given match function
//
template<typename T>
LaunchContext*
ContextPool::FindContext(
    _In_ IsMatch<T>   compareFunct,
    _In_ const T*     pData
    )
{
    AcquireSRWLockShared(&m_lock);

    LaunchContext* pCursor = nullptr;

    for (DWORD i = 0; i < m_activeContextCount; ++i)
    {
        if (compareFunct(m_contexts + m_activeIndices[i], pData))
        {
            pCursor = m_contexts + m_activeIndices[i];
            break;
        }
    }

    ReleaseSRWLockShared(&m_lock);
    return pCursor;
}


/*--------------------------------------------------------------------------*/
/* LaunchContext match functions                                            */
/*--------------------------------------------------------------------------*/

//
// Returns true if the launch ctx's thread matches given thread handle
//
BOOL IsMatchCtxThread(_In_ const LaunchContext* pCtx, _In_ const HANDLE* pThread)
{
    if (pCtx == nullptr || pThread == nullptr)
    {
        return FALSE;
    }

    return pCtx->m_parentThread == *pThread;
}


//
// Returns true if the launch ctx matches given data
//
BOOL IsMatchCtxHandle(_In_ const LaunchContext* pCtx, _In_ const void* pData)
{
    return (pCtx != nullptr) && (pCtx->m_pmiHandle == pData);
}


//
// Returns true if the launch context has a valid but empty job object
//
BOOL IsMatchMgrProcess(_In_ const LaunchContext* pCtx, _In_ const HANDLE* pProcHandle)
{
    return (pCtx != nullptr) && (pCtx->m_mgrProcess == *pProcHandle);
}


/*--------------------------------------------------------------------------*/
/* ProcessQueue                                                             */
/*--------------------------------------------------------------------------*/

ProcessQueue::ProcessQueue()
    : m_newProcessEvent(nullptr)
    , m_count(0)
    , m_thread(nullptr)
    , m_run(FALSE)
{
    InitializeCriticalSection(&m_lock);
}


ProcessQueue::~ProcessQueue()
{
    Stop();
    DeleteCriticalSection(&m_lock);
    CloseHandle(m_newProcessEvent);
}


HRESULT ProcessQueue::Initialize()
{
    if (m_newProcessEvent != nullptr)
    {
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    }

    m_newProcessEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_newProcessEvent == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    m_runningProcesses[NEW_PROCESS_EVENT_IDX] = m_newProcessEvent;
    m_count = 1;

    return S_OK;
}


//
// Safely adds a new process to running processes list and signals new process event
//
HRESULT ProcessQueue::AddProcess(_In_ HANDLE newProcess)
{
    EnterCriticalSection(&m_lock);

    Assert(m_count < _countof(m_runningProcesses));

    m_runningProcesses[m_count] = newProcess;
    ++m_count;

    LeaveCriticalSection(&m_lock);

    SetEvent(m_newProcessEvent);

    return S_OK;
}


DWORD ProcessQueue::GetCountSafe()
{
    EnterCriticalSection(&m_lock);

    DWORD countSafe = m_count;

    LeaveCriticalSection(&m_lock);

    return countSafe;
}


//
// Safely updates the running processes list and returns the handle of the deleted process
//
HANDLE ProcessQueue::DeleteProcess(_In_ DWORD idx)
{
    EnterCriticalSection(&m_lock);

    Assert(idx < m_count);

    HANDLE deletedProcess = m_runningProcesses[idx];

    --m_count;
    m_runningProcesses[idx] = m_runningProcesses[m_count];

    LeaveCriticalSection(&m_lock);

    return deletedProcess;
}


HRESULT ProcessQueue::Start()
{
    m_run = TRUE;

    m_thread = CreateThread(
        nullptr,
        0,
        WaitProcesses,
        this,
        0,
        nullptr);

    if (m_thread == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}


HRESULT ProcessQueue::Stop()
{
    m_run = FALSE;

    SetEvent(m_newProcessEvent);

    DWORD result = WaitForSingleObject(m_thread, SHUTDOWN_TIMEOUT);

    switch (result)
    {
    case WAIT_OBJECT_0:
        result = NO_ERROR;
        break;
    case WAIT_TIMEOUT:
        result = ERROR_APP_HANG;
        break;
    case WAIT_FAILED:
        result = GetLastError();
        break;
    case WAIT_ABANDONED:
    default:
        result = ERROR_INVALID_STATE;
        break;
    }

    CloseHandle(m_thread);

    return HRESULT_FROM_WIN32(result);
}


DWORD ProcessQueue::WaitProcesses(_In_ LPVOID pData)
{
    if (pData == nullptr)
    {
        return ERROR_BAD_ARGUMENTS;
    }

    ProcessQueue* pProcessQueue = static_cast<ProcessQueue*>(pData);

    while (pProcessQueue->m_run)
    {
        DWORD count = pProcessQueue->GetCountSafe();

        DWORD signalIdx = WaitForMultipleObjects(
            count,
            pProcessQueue->m_runningProcesses,
            FALSE,
            INFINITE);

        if (signalIdx == WAIT_FAILED)
        {
            return GetLastError();
        }

        signalIdx -= WAIT_OBJECT_0;
        Assert(signalIdx < count);

        if (signalIdx == NEW_PROCESS_EVENT_IDX)
        {
            //
            // A new process was added. Update the count, and continue waiting
            //
            continue;
        }

        //
        // A process terminated, delete it from the wait list and trigger it's context cleanup.
        //
        HANDLE terminatedProcess = pProcessQueue->DeleteProcess(signalIdx);
        Launcher().ManagerProcessTerminated(terminatedProcess);
    }

    return S_OK;
}


/*--------------------------------------------------------------------------*/
/* MsmpiLaunchService                                                       */
/*--------------------------------------------------------------------------*/

MsmpiLaunchService::MsmpiLaunchService()
    : m_pmiModule(nullptr)
    , m_servicePort(DEFAULT_SERVICE_PORT)
    , m_pMemberGroupSid(nullptr)
{
    m_pmiService.Size = sizeof(PmiServiceInterface);
}


MsmpiLaunchService::~MsmpiLaunchService()
{
    free(m_pMemberGroupSid);
    FreeLibrary(m_pmiModule);
    m_pmiModule = nullptr;
}


//
// Loads the MSPMS provider and initializes PMI interfaces
//
HRESULT MsmpiLaunchService::Load()
{
    wchar_t msmpiModulePath[MAX_PATH];
    DWORD   cbPath = sizeof(msmpiModulePath);
    HRESULT result;

    //
    // Initialize the queue that checks smpd manager process lifetimes
    //
    result = m_mgrQueue.Initialize();

    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to initialize smpd manager queue. Error=0x%x\n",
            result);

        return result;
    }

    //
    // MSPMS provider that is installed on the system is written in registry.
    // Read module path.
    //
    result = RegistryUtils::ReadKey(
        HKEY_LOCAL_MACHINE,
        MSPMI_MODULE_PATH_KEY,
        MSPMI_PROVIDER_VALUE,
        &cbPath,
        msmpiModulePath
        );

    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to read registry %s %s Error=0x%x\n",
            MSPMI_MODULE_PATH_KEY,
            MSPMI_PROVIDER_VALUE,
            result);

        return result;
    }

    //
    // Load the registered MSPMS module.
    //
    OACR_REVIEWED_CALL(
        mpicr,
        m_pmiModule = LoadLibraryExW(msmpiModulePath, nullptr, 0));

    if (m_pmiModule == nullptr)
    {
        DWORD error = GetLastError();

        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to load library. Error=0x%x\n",
            error);

        return HRESULT_FROM_WIN32(error);
    }

    //
    // Get interface function addresses and get interface structures
    //
    PFN_MSMPI_PM_QUERY_INTERFACE fnPmiQueryIf = (PFN_MSMPI_PM_QUERY_INTERFACE)
        GetProcAddress(m_pmiModule, MSPMI_PROC_PMI_QUERY_IF);
    if (fnPmiQueryIf == nullptr)
    {
        DWORD error = GetLastError();

        // %S in wide format string means parameter is 8b char
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to get process address %S. Error=0x%x\n",
            MSPMI_PROC_PMI_QUERY_IF,
            error);

        return HRESULT_FROM_WIN32(error);
    }

    result = fnPmiQueryIf(
        PM_SERVICE_INTERFACE_LAUNCH,
        reinterpret_cast<void**>(&m_pMspmiServiceLaunch));
    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to get PmiServiceLaunchInterface. Error=0x%x\n",
            result);

        return result;
    }

    PFN_MSMPI_GET_PM_INTERFACE fnGetPmi = (PFN_MSMPI_GET_PM_INTERFACE)
        GetProcAddress(m_pmiModule, MSPMI_PROC_GET_PMI);
    if (fnGetPmi == nullptr)
    {
        DWORD error = GetLastError();

        // %S in wide format string means parameter is 8b char
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to get process address %S. Error=0x%x\n",
            MSPMI_PROC_GET_PMI,
            error);

        return HRESULT_FROM_WIN32(error);
    }

    result = fnGetPmi(PM_SERVICE_INTERFACE_V1, &m_pmiService);
    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to get PmiServiceInterface. Error=0x%x\n",
            result);

        return result;
    }

    //
    // Initialize interface structures
    //
    PmiServiceInitData  pmiSvcInitData;

    pmiSvcInitData.Size = sizeof(pmiSvcInitData);
    pmiSvcInitData.Name = "MSMPI Launch Service";

    result = m_pmiService.Initialize(&pmiSvcInitData);
    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to initialize PMI service. Error=0x%x\n",
            result);

        return result;
    }

    m_pMspmiServiceLaunch->CreateLaunchCtx  = ServiceCreateLaunchCtx;
    m_pMspmiServiceLaunch->StartLaunchCtx   = ServiceStartLaunchCtx;
    m_pMspmiServiceLaunch->CleanupLaunchCtx = ServiceCleanupLaunchCtx;

    return result;
}


BOOL MsmpiLaunchService::ParseOptions(_In_ DWORD argc, _In_ LPWSTR *argv)
{
    WCHAR    logBuffer[MAX_LOG_TEXT] = { 0 };
    HRESULT hr = S_OK;

    for (DWORD i = 0; i < argc && SUCCEEDED(hr); i += 1)
    {
        hr = StringCchCatW(logBuffer, _countof(logBuffer), argv[i]);
        hr = StringCchCatW(logBuffer, _countof(logBuffer), L" ");
    }

    gEventLogger.WriteEvent(
        EVENTLOG_INFORMATION_TYPE,
        SVC_CATEGORY,
        SERVICE_EVENT,
        L"Service parameters are :\n %s",
        logBuffer
        );

    for (DWORD i = 1; i < argc; i += 2)
    {
        if (i + 1 == argc)
        {
            gEventLogger.WriteEvent(
                EVENTLOG_ERROR_TYPE,
                SVC_CATEGORY,
                SERVICE_EVENT,
                L"Missing arguments in options.\n"
                );
            return FALSE;
        }

        if ((_wcsicmp(argv[i], L"-p") == 0)
            || (_wcsicmp(argv[i], L"-port") == 0))
        {
            //
            // Server port: Service listens to clients on specified port
            //
            int port = _wtoi(argv[i + 1]);
            if (port <= 0 || port > USHRT_MAX)
            {
                gEventLogger.WriteEvent(
                    EVENTLOG_ERROR_TYPE,
                    SVC_CATEGORY,
                    SERVICE_EVENT,
                    L"Invalid value of server port is set for service : %s\n",
                    argv[i+1]
                    );
                return FALSE;
            }
            m_servicePort = static_cast<USHORT>(port);
        }
        else if ((_wcsicmp(argv[i], L"-g") == 0)
            || (_wcsicmp(argv[i], L"-group") == 0))
        {
            //
            // Client group membership: Clients must be member of this group to be
            // able to connect
            //
            HRESULT result = SecurityUtils::GetSidForAccount(
                nullptr,
                argv[i + 1],
                &m_pMemberGroupSid);
            if (FAILED(result))
            {
                gEventLogger.WriteEvent(
                    EVENTLOG_ERROR_TYPE,
                    SVC_CATEGORY,
                    SERVICE_EVENT,
                    L"Invalid group for client membership : %s\nError=0x%x",
                    argv[i + 1],
                    result
                    );
                return FALSE;
            }
        }
    }

    return TRUE;
}


HRESULT MsmpiLaunchService::Run()
{
    HRESULT             result;
    SOCKADDR_INET       svcAddr;
    PmiManagerInterface pmiManager;

    //
    // Start manager process lifetime management
    //
    result = m_mgrQueue.Start();

    if (FAILED(result))
    {
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            SVC_CATEGORY,
            SERVICE_EVENT,
            L"Failed to start smpd manager queue. Error=0x%x\n",
            result);

        return result;
    }

    //
    // Start PMI service
    //
    pmiManager.Size               = sizeof(PmiManagerInterface);
    pmiManager.LaunchType         = PmiLaunchTypeImpersonate;
    pmiManager.Launch.Impersonate = ServiceCreateManagerProcess;

    svcAddr.si_family                 = AF_INET;
    svcAddr.Ipv4.sin_port             = _byteswap_ushort(m_servicePort);
    svcAddr.Ipv4.sin_addr.S_un.S_addr = INADDR_ANY;

    result = m_pmiService.Listen(&svcAddr, &pmiManager, PM_MANAGER_INTERFACE_V1);

    m_pmiService.Finalize();

    return result;
}


OACR_WARNING_DISABLE(HRESULT_NOT_CHECKED, "Don't care about the result of stop commands - no possible recovery.");
VOID MsmpiLaunchService::Stop()
{
    m_pmiService.PostStop();
    m_mgrQueue.Stop();
}
OACR_WARNING_ENABLE(HRESULT_NOT_CHECKED, "Don't care about the result of stop commands - no possible recovery.");


VOID MsmpiLaunchService::ManagerProcessTerminated(_In_ HANDLE mgrProcess)
{
    LaunchContext* pCtx = m_contextPool.FindContext(IsMatchMgrProcess, &mgrProcess);

    Assert(pCtx != nullptr);

    Launcher().m_contextPool.DestroyContext(pCtx);

    gEventLogger.WriteEvent(
        EVENTLOG_INFORMATION_TYPE,
        1,
        100,
        L"SmpdMgr terminated for launch context 0x%p",
        pCtx);
}


//
// MSPMS passes us multibyte arguments for backcompat purposes.  The
// function should convert multibyte args into wchar_t because they
// might contain true unicode.
//
HRESULT WINAPI
MsmpiLaunchService::ServiceCreateManagerProcess(
    _In_z_      PCSTR   app,
    _In_z_      PCSTR   args,
    _In_z_      PCSTR   /*context*/
    )
{
    STARTUPINFOW        si;
    PROCESS_INFORMATION pi;
    HRESULT             result;
    HANDLE              currentThread   = GetCurrentThread();
    LPCSTR              pJobObjName     = nullptr;
    HANDLE              jobObj          = nullptr;
    PVOID               pEnvBlock       = nullptr;
    LaunchContext*      pCtx            =
        Launcher().m_contextPool.FindContext(IsMatchCtxThread, &currentThread);
    wchar_t             currentDirectory[MAX_PATH]  = L"";
    const wchar_t*      pCurrentDirectoryPointer;
    wchar_t*            appW = nullptr;
    wchar_t*            argsW = nullptr;
    wchar_t*            pJobObjNameW = nullptr;

    if (pCtx == nullptr)
    {
        return E_HANDLE;
    }

    Launcher().m_pMspmiServiceLaunch->GetLaunchInfo(pCtx->m_pmiHandle, &pJobObjName, nullptr, nullptr);
    if (pJobObjName != nullptr && pJobObjName[0] != 0)
    {
        DWORD len = static_cast<DWORD>(strlen( pJobObjName ) + 1);
        pJobObjNameW = new wchar_t[len];
        if( pJobObjNameW == nullptr )
        {
            gEventLogger.WriteEvent(
                EVENTLOG_ERROR_TYPE,
                1,
                100,
                L"Cannot convert job object name to unicode .\nError=0x%x\nJobName %S",
                E_OUTOFMEMORY,
                pJobObjName);
            result = E_OUTOFMEMORY;
            goto exit_fn;
        }

        if( MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                pJobObjName,
                -1,
                pJobObjNameW,
                len ) == 0 )
        {
            DWORD gle = GetLastError();
            gEventLogger.WriteEvent(
                EVENTLOG_ERROR_TYPE,
                1,
                100,
                L"Cannot convert job object name to unicode .\nError=0x%x\nJobName %S",
                gle,
                pJobObjName);
            result = HRESULT_FROM_WIN32(gle);
            goto exit_fn;

        }

        jobObj = OpenJobObjectW(JOB_OBJECT_ASSIGN_PROCESS, TRUE, pJobObjNameW);

        if (jobObj == nullptr)
        {
            DWORD gle = GetLastError();
            gEventLogger.WriteEvent(
                EVENTLOG_ERROR_TYPE,
                1,
                100,
                L"Cannot open job object.\nError=0x%x\nJobName %s",
                gle,
                pJobObjNameW);
            result = HRESULT_FROM_WIN32(gle);
            goto exit_fn;
        }
    }

    BOOL success = CreateEnvironmentBlock(&pEnvBlock, pCtx->m_primaryToken, TRUE);
    if (!success)
    {
        DWORD gle = GetLastError();
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            1,
            100,
            L"CreateEnvironmentBlock Error=0x%x\nCtx 0x%p - Token 0x%p",
            gle,
            pCtx,
            pCtx->m_primaryToken);
        result = HRESULT_FROM_WIN32(gle);
        goto exit_fn;
    }

    success = ExpandEnvironmentStringsForUserW(
        pCtx->m_primaryToken,
        L"%USERPROFILE%",
        currentDirectory,
        _countof(currentDirectory)
        );
    if (!success)
    {
        DWORD gle = GetLastError();
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            1,
            100,
            L"ExpandEnvironmentStringsForUser Error=0x%x\nCtx 0x%p - Token 0x%p",
            gle,
            pCtx,
            pCtx->m_primaryToken);
        result = HRESULT_FROM_WIN32(gle);
        goto exit_fn;
    }

    if (currentDirectory[0] != L'\0')
    {
        pCurrentDirectoryPointer = currentDirectory;
    }
    else
    {
        pCurrentDirectoryPointer = nullptr;
    }

    //
    // MSPMS passes us multibyte arguments for backcompat purposes.
    // We need to convert the multibyte into wchar_t because they might
    // contain true unicode
    //
    DWORD len = static_cast<DWORD>( strlen( app ) + 1 );

    //
    // At most n UTF-16 characters are required per n UTF-8 characters
    //
    appW = new wchar_t[len];
    if( appW == nullptr )
    {
        result = E_OUTOFMEMORY;
        goto exit_fn;
    }

    if( MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            app,
            -1,
            appW,
            len ) == 0 )
    {
        DWORD gle = GetLastError();
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            1,
            100,
            L"MultiByteToWideChar Error=0x%x\nCtx 0x%p - String %S",
            gle,
            pCtx,
            app);
        result = HRESULT_FROM_WIN32( gle );
        goto exit_fn;
    }

    len = static_cast<DWORD>( strlen( args ) + 1 );
    argsW = new wchar_t[len];
    if( argsW == nullptr )
    {
        result = E_OUTOFMEMORY;
        goto exit_fn;
    }

    if( MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            args,
            -1,
            argsW,
            len ) == 0 )
    {
        DWORD gle = GetLastError();
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            1,
            100,
            L"MultiByteToWideChar Error=0x%x\nCtx 0x%p - String %S",
            gle,
            pCtx,
            args);
        result = HRESULT_FROM_WIN32( gle );
        goto exit_fn;
    }

    GetStartupInfoW(&si);
    si.lpDesktop = L"";

    BOOL mgrCreated = OACR_REVIEWED_CALL(
        mpicr,
        CreateProcessAsUserW(
            pCtx->m_primaryToken,
            appW,                    // Application Name
            argsW,                   // Command Line
            nullptr,                 // Process Security Attributes,
            nullptr,                 // Thread Security Attributes,
            TRUE,                    // Inherit Parent Handles,
            CREATE_NO_WINDOW |        // Process CreationFlags,
            CREATE_UNICODE_ENVIRONMENT |
            CREATE_SUSPENDED,
            pEnvBlock,               // lpEnvironment,
            pCurrentDirectoryPointer,// lpCurrentDirectory,
            &si,                     // lpStartupInfo,
            &pi                      // lpProcessInformation
            ));

    if (!mgrCreated)
    {
        DWORD gle = GetLastError();
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            1,
            100,
            L"CreateProcessAsUser Error=0x%x\nCtx 0x%p - Token 0x%p",
            gle,
            pCtx,
            pCtx->m_primaryToken);
        result = HRESULT_FROM_WIN32(gle);
        goto exit_fn;
    }

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    pCtx->m_mgrProcess = pi.hProcess;
    pCtx->m_parentThread = nullptr;
    pCtx->m_pmiHandle = nullptr;

    if (jobObj != nullptr)
    {
        if (!AssignProcessToJobObject(jobObj, pi.hProcess))
        {
            DWORD gle = GetLastError();
            gEventLogger.WriteEvent(
                EVENTLOG_ERROR_TYPE,
                1,
                100,
                L"Failed to assign process to job object.\nError=0x%x JobObj=%s\n",
                gle,
                pJobObjNameW);

            TerminateProcess(pi.hProcess, gle);
            result = HRESULT_FROM_WIN32(gle);
            goto exit_fn;
        }
    }

    result = Launcher().m_mgrQueue.AddProcess(pi.hProcess);

exit_fn:
    if (pEnvBlock != nullptr)
    {
        DestroyEnvironmentBlock(pEnvBlock);
    }
    if (jobObj != nullptr)
    {
        CloseHandle(jobObj);
    }
    delete[] appW;
    delete[] argsW;
    delete[] pJobObjNameW;
    if (FAILED(result))
    {
        Launcher().m_contextPool.DestroyContext(pCtx);
    }
    return result;
}


HRESULT
MsmpiLaunchService::ServiceCreateLaunchCtx(
    _In_    HANDLE      clientToken,
    _In_    const void* launchCtx,
    _In_z_  const char* /*jobCtx*/
    )
{
    //
    // This function is called in process's security context; i.e. under system account
    //

    PROFILEINFOW    userProfile;
    WCHAR           user[MAX_PATH]      = { 0 };
    WCHAR           domain[MAX_PATH]    = { 0 };
    DWORD           cchUser             = _countof(user);
    DWORD           cchDomain           = _countof(domain);
    LaunchContext*  pNewContext         = nullptr;

    HRESULT result = SecurityUtils::GetTokenUser(clientToken, user, &cchUser, domain, &cchDomain);
    if (FAILED(result))
    {
        return result;
    }

    if (Launcher().m_pMemberGroupSid != nullptr)
    {
        //
        // Check if client is member of required group
        //
        BOOL isMember;
        if (!CheckTokenMembership(clientToken, Launcher().m_pMemberGroupSid, &isMember))
        {
            result = HRESULT_FROM_WIN32(GetLastError());

            gEventLogger.WriteEvent(
                EVENTLOG_ERROR_TYPE,
                1,
                100,
                L"Cannot verify the membership of %s\\%s.\nError=0x%x",
                domain,
                user,
                result);

            return result;
        }

        if (!isMember)
        {
            gEventLogger.WriteEvent(
                EVENTLOG_ERROR_TYPE,
                1,
                100,
                L"Client %s\\%s is not a member of required group.",
                domain,
                user);

            return HRESULT_FROM_WIN32(ERROR_MEMBER_NOT_IN_GROUP);
        }
    }

    pNewContext = Launcher().m_contextPool.CreateNewContext();
    if (pNewContext == nullptr)
    {
        gEventLogger.WriteEvent(
            EVENTLOG_WARNING_TYPE,
            1,
            100,
            L"Server is busy. Rejecting new clients"
            );

        return HRESULT_FROM_WIN32(RPC_S_SERVER_TOO_BUSY);
    }

    Assert(pNewContext->m_mgrProcess == nullptr && pNewContext->m_pmiHandle == nullptr &&
        pNewContext->m_parentThread == nullptr && pNewContext->m_primaryToken == nullptr &&
        pNewContext->m_userProfile == nullptr && pNewContext->m_userToken == nullptr);

    pNewContext->m_pmiHandle = launchCtx;
    pNewContext->m_userToken = clientToken;

    ZeroMemory(&userProfile, sizeof(PROFILEINFOW));
    userProfile.dwSize = sizeof(PROFILEINFOW);
    userProfile.lpUserName = user;

    if (!LoadUserProfileW(clientToken, &userProfile))
    {
        result = HRESULT_FROM_WIN32(GetLastError());
        gEventLogger.WriteEvent(
            EVENTLOG_ERROR_TYPE,
            1,
            100,
            L"Failed to load user profile for %s\\%s. Error: 0x%x",
            domain,
            user,
            result);

        goto exit_fn;
    }

    pNewContext->m_userProfile = userProfile.hProfile;

    gEventLogger.WriteEvent(
        EVENTLOG_INFORMATION_TYPE,
        1,
        100,
        L"Created new launch context 0x%p",
        pNewContext);

exit_fn:
    if (FAILED(result))
    {
        Launcher().m_contextPool.DestroyContext(pNewContext);
    }
    return result;
}


HRESULT WINAPI MsmpiLaunchService::ServiceStartLaunchCtx(_In_ const void* launchCtx)
{
    //
    // This function is called in client's security context;
    // i.e. uses client's impersonation token
    //

    HANDLE          hToken          = nullptr;
    HANDLE          hPrimaryToken   = nullptr;
    HANDLE          hCurrentThread  = GetCurrentThread();
    HRESULT         result;
    BOOL            tokenIsInteractive;
    WORD            logType         = EVENTLOG_INFORMATION_TYPE;
    LPCSTR          pPwd            = nullptr;
    BOOL            saveCreds       = FALSE;
    LaunchContext*  pCtx            =
        Launcher().m_contextPool.FindContext(IsMatchCtxHandle, launchCtx);

    if (pCtx == nullptr)
    {
        result = E_HANDLE;
        goto fail_fn;
    }

    pCtx->m_parentThread = hCurrentThread;

    if (!OpenThreadToken(
        hCurrentThread,
        TOKEN_ALL_ACCESS,
        TRUE,
        &hToken))
    {
        result = HRESULT_FROM_WIN32(GetLastError());
        goto fail_fn;
    }

    result = SecurityUtils::IsGroupMember(WinInteractiveSid, hToken, &tokenIsInteractive);
    if (FAILED(result))
    {
        goto fail_fn;
    }

    gEventLogger.WriteEvent(
        EVENTLOG_INFORMATION_TYPE,
        1,
        100,
        L"CreateManagerProcess request user interactive: %d.\n",
        tokenIsInteractive
        );

    //
    // MSPMS passes us multibyte arguments for backcompat purposes.
    // pPwd will need to be converted to wchar_t before usage
    //
    Launcher().m_pMspmiServiceLaunch->GetLaunchInfo(launchCtx, nullptr, &pPwd, &saveCreds);

    if (tokenIsInteractive && (pPwd == nullptr || pPwd[0] == '\0'))
    {
        //
        // User did not provide a pwd but the token is interactive and we can use it
        //
        if (!DuplicateTokenEx(
            hToken,
            TOKEN_ALL_ACCESS,
            nullptr,
            SecurityImpersonation,
            TokenPrimary,
            &hPrimaryToken)
            )
        {
            result = HRESULT_FROM_WIN32(GetLastError());
            goto fail_fn;
        }
    }
    else
    {
        wchar_t* pPwdW = nullptr;
        if( pPwd != nullptr )
        {
            DWORD len = static_cast<DWORD>( strlen( pPwd ) + 1 );
            pPwdW = new wchar_t[len * 2];
            if( pPwdW == nullptr )
            {
                result = E_OUTOFMEMORY;
                goto fail_fn;
            }

            if( MultiByteToWideChar(
                    CP_UTF8,
                    MB_ERR_INVALID_CHARS,
                    pPwd,
                    -1,
                    pPwdW,
                    len * 2) == 0 )
            {
                delete[] pPwdW;
                result = HRESULT_FROM_WIN32( GetLastError() );
                goto fail_fn;
            }
        }

        //
        // Token does not have the sufficient rights. Need to do LogonUser.
        //
        result = Launcher().DoLogonUser(pPwdW, saveCreds, &hPrimaryToken);
        delete[] pPwdW;
        if (FAILED(result))
        {
            goto fail_fn;
        }
    }

    pCtx->m_primaryToken = hPrimaryToken;
    result = S_OK;

    goto exit_fn;

fail_fn:
    logType = EVENTLOG_INFORMATION_TYPE;
    if (hPrimaryToken != nullptr)
    {
        CloseHandle(hPrimaryToken);
    }
    if (pCtx != nullptr)
    {
        Launcher().m_contextPool.DestroyContext(pCtx);
    }

exit_fn:

    gEventLogger.WriteEvent(logType, 1, 100, L"StartLaunchCtx 0x%x", result);

    if (hToken != nullptr)
    {
        CloseHandle(hToken);
    }
    return result;
}


HRESULT WINAPI MsmpiLaunchService::ServiceCleanupLaunchCtx(_In_ const void* launchCtx)
{
    LaunchContext*  pCtx =
        Launcher().m_contextPool.FindContext(IsMatchCtxHandle, launchCtx);

    if (pCtx == nullptr)
    {
        return E_HANDLE;
    }

    if (pCtx->m_mgrProcess != 0)
    {
        //
        // There is a process started with it, so leave cleanup to process termination.
        //
        return S_OK;
    }

    Launcher().m_contextPool.DestroyContext(pCtx);

    gEventLogger.WriteEvent(
        EVENTLOG_INFORMATION_TYPE,
        1,
        100,
        L"Cleaned up launch context 0x%p",
        pCtx);

    return S_OK;
}


HRESULT MsmpiLaunchService::DoLogonUser(
    _In_     PCWSTR      pPwd,
    _In_     BOOL        saveCreds,
    _Outptr_ PHANDLE     pLogonHandle
    )
{
    DWORD     cbValue          = MAX_PATH;
    LPWSTR    pValue           = nullptr;
    DATA_BLOB credsEncrypted   = { 0, nullptr };
    DATA_BLOB credsDecrypted   = { 0, nullptr };
    WCHAR     user[MAX_PATH]   = { 0 };
    WCHAR     domain[MAX_PATH] = { 0 };
    DWORD     cchUser          = _countof(user);
    DWORD     cchDomain        = _countof(domain);
    DWORD     cbPwd            = 0;
    BYTE*     pFree            = nullptr;
    BOOL      usingCachedPwd   = FALSE;
    BOOL      pwdRequested     = FALSE;

    HRESULT result = SecurityUtils::GetCurrentUser(user, &cchUser, domain, &cchDomain);
    if (FAILED(result))
    {
        goto exit_fn;
    }

    if (pPwd == nullptr || pPwd[0] == L'\0')
    {
        //
        // User did not provide a pwd, check if it is cached in the system
        //
        usingCachedPwd = TRUE;

        result = HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        while (result == HRESULT_FROM_WIN32(ERROR_MORE_DATA))
        {
            free(pValue);
            pValue = static_cast<LPWSTR>(malloc(cbValue * sizeof (wchar_t)));
            if (pValue == nullptr)
            {
                result = E_OUTOFMEMORY;
                goto exit_fn;
            }

            result = RegistryUtils::ReadCurrentUserKey(
                MSPMI_MODULE_PATH_KEY,
                MSPMI_CREDENTIAL_VALUE,
                &cbValue,
                pValue
                );
        }

        if (FAILED(result))
        {
            //
            // If pwd is not cached before, result is ERROR_FILE_NOT_FOUND
            //
            pwdRequested = (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
            goto exit_fn;
        }

        credsEncrypted.cbData = cbValue;
        credsEncrypted.pbData = reinterpret_cast<BYTE*>(pValue);

        if (!CryptUnprotectData(&credsEncrypted, nullptr, nullptr, nullptr, nullptr, 0, &credsDecrypted))
        {
            //
            // If pwd was reset after previous encryption, result is ERROR_INVALID_PASSWORD
            //
            result = HRESULT_FROM_WIN32(GetLastError());
            pwdRequested = (GetLastError() == ERROR_INVALID_PASSWORD);
            goto exit_fn;
        }

        pPwd = reinterpret_cast<const wchar_t*>(credsDecrypted.pbData);
        cbPwd = credsDecrypted.cbData;
        pFree = credsDecrypted.pbData;
    }

    if (!LogonUserW(
        user,
        domain,
        pPwd,
        LOGON32_LOGON_INTERACTIVE,
        LOGON32_PROVIDER_DEFAULT,
        pLogonHandle
        ))
    {
        //
        // If pwd is not correct, result is ERROR_LOGON_FAILURE.
        // If cached pwd is used, this means user changed the pwd after it was cached,
        // we need to request the new pwd.
        // Otherwise, it means user input a wrong pwd.
        //
        result = HRESULT_FROM_WIN32(GetLastError());
        pwdRequested = usingCachedPwd && (GetLastError() == ERROR_LOGON_FAILURE);
        goto exit_fn;
    }

    if (saveCreds && !usingCachedPwd)
    {
        cbPwd = static_cast<DWORD>((wcslen(pPwd) + 1) * sizeof(wchar_t));

        credsDecrypted.pbData = const_cast<BYTE*>(
            reinterpret_cast<const BYTE*>(pPwd));
        credsDecrypted.cbData = cbPwd;

        if (!CryptProtectData(
                &credsDecrypted,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                CRYPTPROTECT_LOCAL_MACHINE,
                &credsEncrypted))
        {
            result = HRESULT_FROM_WIN32(GetLastError());
            goto exit_fn;
        }
        pFree = credsEncrypted.pbData;

        result = RegistryUtils::WriteCurrentUserKey(
            MSPMI_MODULE_PATH_KEY,
            MSPMI_CREDENTIAL_VALUE,
            credsEncrypted.cbData,
            reinterpret_cast<wchar_t*>(credsEncrypted.pbData)
            );

        if (FAILED(result))
        {
            goto exit_fn;
        }
    }

exit_fn:
    if (usingCachedPwd)
    {
        SecureZeroMemory(pFree, cbPwd);
    }
    LocalFree(pFree);
    free(pValue);
    if (pwdRequested)
    {
        result = NTE_UI_REQUIRED;
    }
    return result;
}
