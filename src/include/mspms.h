/*++

 Copyright (c) Microsoft Corporation. All rights reserved.
 Licensed under the MIT License.

Module:

    mspms.h

Abstract:

    MSMPI process management service interface definitions.

--*/

#ifdef DEFINE_GUID

DEFINE_GUID(
    PM_SERVICE_INTERFACE_V1,
    0x5aa2c905,
    0xd8af,
    0x4cf2,
    0x92, 0x56, 0x80, 0xf3, 0xbc, 0xa0, 0x0f, 0x94
    );

DEFINE_GUID(
    PM_MANAGER_INTERFACE_V1,
    0x4b1130e6,
    0x724d,
    0x43b8,
    0xb7, 0x63, 0x75, 0x72, 0xcb, 0xe6, 0x00, 0xfa
    );

DEFINE_GUID(
    PM_SERVICE_INTERFACE_LAUNCH,
    0x2a89cc11,
    0x21ff,
    0x47d5,
    0x94, 0xf4, 0x28, 0x52, 0x9f, 0x7f, 0x4f, 0x5e
    );

#endif

#ifndef _MSPMS_H_
#define _MSPMS_H_

#ifndef MSMPI_NO_SAL
#include <sal.h>
#endif

#include <WS2tcpip.h>
#include <windows.h>


/*---------------------------------------------------------------------------*/
/* SAL ANNOTATIONS                                                           */
/*---------------------------------------------------------------------------*/
/*
* Define SAL annotations if they aren't defined yet.
*/
#ifndef _In_
#define _In_
#endif
#ifndef _In_z_
#define _In_z_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Outptr_
#define _Outptr_
#endif


#define MSPMS_MAX_NAME_LENGTH 256

//
// The launch type allows launch managers to specify their security context
// requirements for their launch callback.
//  PmiLaunchTypeSelf:
//      The launch callback is invoked without any impersonation or client context.
//      This is used to launch processes under the same credentials as the launch manager.
//  PmiLaunchTypeImpersonate:
//      The client security context is impersonated for the launch callback.
//  PmiLaunchTypeUserSid:
//      The client's user identifier is passed to the launch callback.
//
enum PmiLaunchType
{
    PmiLaunchTypeSelf,
    PmiLaunchTypeImpersonate,
    PmiLaunchTypeUserSid
};

typedef HRESULT (WINAPI FN_PmiLaunch)(
    _In_z_ const char* App,              //smpd.exe
    _In_z_ const char* Args,             //args to smpd.exe
    _In_z_ const char* Context           //job context string
);

typedef HRESULT (WINAPI FN_PmiLaunchUserSid)(
    _In_opt_   PSID        Sid,
    _In_z_     const char* App,              //smpd.exe
    _In_z_     const char* Args,             //args to smpd.exe
    _In_z_     const char* Context           //job context string
);

typedef struct _PmiManagerInterface
{
    size_t               Size;
    union
    {
        FN_PmiLaunch*           AsSelf;
        FN_PmiLaunch*           Impersonate;
        FN_PmiLaunchUserSid*    UserSid;
    } Launch;
    enum PmiLaunchType  LaunchType;
} PmiManagerInterface;

typedef struct _PmiServiceInitData
{
    size_t      Size;
    const char* Name;
} PmiServiceInitData;

//
// Initialize the service
//
typedef HRESULT (WINAPI FN_PmiServiceInitialize)(
    _In_ const PmiServiceInitData* InitData         // Init data
);

//
// Cause the calling thread to listen on the supplied address for requests
// from Mpiexec.exe to launch the manager process.
//
typedef HRESULT (WINAPI FN_PmiServiceListen)(
    _In_ const SOCKADDR_INET*       Address,        // INET address on which to listen
    _In_ const PmiManagerInterface* Manager,        // Interface to use to launch the smpd manager
    _In_ REFGUID                    Version         // Version GUID of the PmiManagerInterface
);

//
// Signal to the thread that it is time to stop processing completions.
//
typedef HRESULT (WINAPI FN_PmiServicePostStop)();

//
// Finalize the service
//
typedef VOID (WINAPI FN_PmiServiceFinalize)();

typedef struct _PmiServiceInterface
{
    size_t                   Size;
    FN_PmiServiceInitialize* Initialize;
    FN_PmiServiceListen*     Listen;
    FN_PmiServicePostStop*   PostStop;
    FN_PmiServiceFinalize*   Finalize;
} PmiServiceInterface;


HRESULT
WINAPI
MSMPI_Get_pm_interface(
    _In_ REFGUID                 RequestedVersion,
    _Inout_ PmiServiceInterface* Interface
);


HRESULT
WINAPI
MSMPI_pm_query_interface(
    _In_ REFGUID                 RequestedVersion,
    _Inout_ void**               Interface
);


typedef HRESULT(WINAPI FN_PmiCreateLaunchCtx)(
    _In_    HANDLE      clientToken,
    _In_z_  const void* launchCtx,
    _In_z_  const char* jobCtx
    );

typedef HRESULT(WINAPI FN_PmiStartLaunchCtx)(_In_ const void* launchCtx);
typedef HRESULT(WINAPI FN_PmiEndLaunchCtx)(_In_ const void* launchCtx);
typedef HRESULT(WINAPI FN_PmiCleanupLaunchCtx)(_In_ const void* launchCtx);
typedef void (WINAPI FN_PmiGetLaunchInfo)(
    _In_         const void*  launchCtx,
    _Outptr_opt_ const char** ppJobObjName,
    _Outptr_opt_ const char** ppPwd,
    _Out_opt_    BOOL*        pSaveCreds
    );

typedef struct _PmiServiceLaunchInterface
{
    FN_PmiCreateLaunchCtx*  CreateLaunchCtx;
    FN_PmiStartLaunchCtx*   StartLaunchCtx;
    FN_PmiEndLaunchCtx*     EndLaunchCtx;
    FN_PmiCleanupLaunchCtx* CleanupLaunchCtx;
    FN_PmiGetLaunchInfo*    GetLaunchInfo;
}PmiServiceLaunchInterface;



#endif // _MSPMS_H_
