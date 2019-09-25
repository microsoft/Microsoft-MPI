/*++

 Copyright (c) Microsoft Corporation. All rights reserved.
 Licensed under the MIT License.

Module Name:

    service_lib.cpp

Abstract:

    Functions that implement the interface used to enable communication between mpiexec.exe and msmpisvc.exe.

--*/

#include "precomp.h"
#include <initguid.h>
#include "smpd.h"


//
// Summary:
//  Initialize the sevice.
//
// Parameters:
//  pInitData      - Init data
//
static
HRESULT
WINAPI
ServiceInitialize(
    _In_ const PmiServiceInitData* InitData         // Init data
)
{
    if(InitData == NULL)
    {
        return E_INVALIDARG;
    }

    if(InitData->Size != sizeof(PmiServiceInitData))
    {
        return E_NOINTERFACE;
    }

    if(InitData->Name == NULL)
    {
        return E_INVALIDARG;
    }

    smpd_init_process(InitData->Name);
    InitializeCriticalSection( &smpd_process.svcCriticalSection );

    return S_OK;
}

//
// Summary:
//  Listen for requests to launch the smpd manager.
//
// Parameters:
//  pAddress       - The IPv4 address on which to listen
//  pManager       - The interface to use to launch the smpd manager
//  Version        - The version GUID of the PmiManagerInterface
//
static
HRESULT
WINAPI
ServiceListen(
    _In_ const SOCKADDR_INET*       Address,        // INET address on which to listen
    _In_ const PmiManagerInterface* Manager,        // Interface to use to launch the smpd manager
    _In_ REFGUID                    Version         // Version GUID of the PmiManagerInterface
)
{
    if(Manager == NULL)
    {
        return E_INVALIDARG;
    }

    if(Manager->Size != sizeof(PmiManagerInterface) ||
       !IsEqualGUID(Version, PM_MANAGER_INTERFACE_V1))
    {
        return E_NOINTERFACE;
    }

    if(Manager->Launch.AsSelf == NULL ||
       Address == NULL ||
       Address->si_family != AF_INET)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    smpd_process.manager_interface = Manager;
    smpd_process.rootServerPort = static_cast<UINT16>(_byteswap_ushort(Address->Ipv4.sin_port));

    ExSetHandle_t set;
    set = ExCreateSet();
    if(set == EX_INVALID_SET)
    {
        return E_OUTOFMEMORY;
    }

    smpd_process.set = set;

    UINT16 numThreads = RPC_C_LISTEN_MAX_CALLS_DEFAULT;

    //
    // Detect whether this is the MSMPILaunchsvc. The MSMPILaunchSvc
    // requires no more than one active launch at a time
    //

    wchar_t fullModuleName[MAX_PATH+1];
    DWORD ret = GetModuleFileNameW(
        nullptr,
        fullModuleName,
        _countof(fullModuleName) );
    if( ret == 0 )
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto fn_fail;
    }

    //
    // We find the last occurence of '\' and use the rest
    // as the module name.
    //
    const wchar_t* pModuleName = wcsrchr( fullModuleName, L'\\');
    if( pModuleName != nullptr )
    {
        pModuleName++;
    }
    else
    {
        pModuleName = fullModuleName;
    }

    if( CompareStringW( LOCALE_INVARIANT,
                        NORM_IGNORECASE,
                        L"msmpilaunchsvc.exe",
                        -1,
                        pModuleName,
                        -1 ) == CSTR_EQUAL )
    {
        numThreads = 1;
    }


    DWORD rc = smpd_create_root_server(
        smpd_process.rootServerPort,
        numThreads
        );
    if( rc != NOERROR )
    {
        hr = HRESULT_FROM_WIN32(ERROR_INCORRECT_ADDRESS);
        goto fn_fail;
    }

    rc = smpd_progress(set);
    if(rc != MPI_SUCCESS)
    {
        hr = E_ABORT;
    }

    smpd_stop_root_server();

fn_fail:
    ExCloseSet(set);

    return hr;
}

//
// Summary:
//  Finish the transaction that signals to the listening thread
//  that it is time to stop.
//
// Parameters:
//  pOverlapped    - Not used
//
static
int
WINAPI
SmpdServiceExitMain(
    EXOVERLAPPED* /*pOverlapped*/
    )
{
    smpd_stop_root_server();
    smpd_signal_exit_progress( MPI_SUCCESS );
    return MPI_SUCCESS;
}

//
// Summary:
//  Initiate the transaction to signal to the listening thread
//  that it is time to stop and wait until the listening thread
//  is about to return.
//
static
HRESULT
WINAPI
ServiceStop()
{
    static EXOVERLAPPED exov;

    //
    // Stop the main thread
    //
    ExInitOverlapped(&exov, SmpdServiceExitMain, SmpdServiceExitMain);
    ExPostOverlappedResult(smpd_process.set, &exov, 0, 0);

    return S_OK;
}

//
// Summary:
//  Finalize the service
//
static
VOID
WINAPI
ServiceFinalize()
{
    DeleteCriticalSection( &smpd_process.svcCriticalSection );
}

//
// Summary:
//  Entry point to request the launch interface.
//
// Parameters:
//  pInterface     - On return this structure will contain the initialize, listen,
//                   stop and finalize function pointers.
//
HRESULT
WINAPI
MSMPI_Get_pm_interface(
    _In_ REFGUID                 RequestedVersion,
    _Inout_ PmiServiceInterface* Interface
)
{
    if(Interface == NULL)
    {
        return E_INVALIDARG;
    }

    if(Interface->Size == sizeof(PmiServiceInterface) &&
       IsEqualGUID(RequestedVersion, PM_SERVICE_INTERFACE_V1))
    {
        Interface->Initialize = ServiceInitialize;
        Interface->Listen     = ServiceListen;
        Interface->PostStop   = ServiceStop;
        Interface->Finalize   = ServiceFinalize;
        return S_OK;
    }

    return E_NOINTERFACE;
}


HRESULT
WINAPI
MSMPI_pm_query_interface(
    _In_ REFGUID                 RequestedVersion,
    _Inout_ void**               Interface
    )
{
    if (Interface == nullptr)
    {
        return E_INVALIDARG;
    }

    if (IsEqualGUID(RequestedVersion, PM_SERVICE_INTERFACE_LAUNCH))
    {
        *Interface = &(smpd_process.svcIfLaunch);
        return S_OK;
    }

    *Interface = nullptr;
    return E_NOINTERFACE;
}
