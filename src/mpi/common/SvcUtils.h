/*
* Copyright (c) Microsoft Corporation. All rights reserved.
* Licensed under the MIT License.
*
*  This file includes declarations of utility classes:
*       Windows Event Logger Utilities
*       Registry Utilities
*       Security Utilities
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <strsafe.h>
#include <sddl.h>

/*---------------------------------------------------------------------------*/
/* Windows Event Logger Utility Class                                        */
/*---------------------------------------------------------------------------*/

#define MAX_LOG_TEXT    1024

class EventLogger
{
private:
    HANDLE      m_eventLog;

public:
    EventLogger();

    BOOL Open(_In_z_ PCWSTR pEventSource);

    void
    WriteEvent(
        _In_    WORD    type,
        _In_    WORD    category,
        _In_    DWORD   eventId,
        _Printf_format_string_ PCWSTR  pFormatStr,
        ...
        );

    ~EventLogger();
};


/*---------------------------------------------------------------------------*/
/* Registry Access Utility Class                                             */
/*---------------------------------------------------------------------------*/

class RegistryUtils
{
public:
    static HRESULT
    ReadKey(
        _In_     HKEY    key,
        _In_opt_z_ LPCWSTR pSubKey,
        _In_z_   LPCWSTR pValueName,
        _Inout_  LPDWORD pcbValue,
        _Out_    LPWSTR  pValue
        );

    static HRESULT
    ReadCurrentUserKey(
        _In_opt_z_                  LPCWSTR pSubKey,
        _In_z_                      LPCWSTR pValueName,
        _Inout_                     LPDWORD pcbValue,
        _Out_writes_z_(*pcbValue)   LPWSTR  pValue
        );

    static HRESULT
    WriteCurrentUserKey(
        _In_opt_z_ LPCWSTR pSubKey,
        _In_z_   LPCWSTR pValueName,
        _In_     DWORD   cbValue,
        _In_z_   LPCWSTR pValue
        );
};


/*---------------------------------------------------------------------------*/
/* Security Access Utility Class                                             */
/*---------------------------------------------------------------------------*/

class SecurityUtils
{
public:
    static HRESULT GetCurrentThreadPrimaryToken(_Out_ PHANDLE pPrimaryToken);

    static HRESULT
    IsGroupMember(
        _In_  WELL_KNOWN_SID_TYPE wellKnownSidType,
        _In_  HANDLE              token,
        _Out_ PBOOL               pIsMember
        );

    static HRESULT
    GetTokenUser(
        _In_      HANDLE        token,
        _Out_opt_ LPWSTR        pUser,
        _Inout_   LPDWORD       pcchUser,
        _Out_opt_ LPWSTR        pDomain,
        _Inout_   LPDWORD       pcchDomain
        );

    static HRESULT
    GetCurrentUser(
        _Out_opt_ LPWSTR        pUser,
        _Inout_   LPDWORD       pcchUser,
        _Out_opt_ LPWSTR        pDomain,
        _Inout_   LPDWORD       pcchDomain
        );

    static HRESULT
    GrantPrivilege(
        _In_ HANDLE     token,
        _In_ LPCTSTR    privilege,
        _In_ BOOL       enable
        );

    static HRESULT
    GetSidForAccount(
        _In_opt_z_ LPCWSTR pSystemName,
        _In_z_   LPCWSTR pAccountName,
        _Outptr_ PSID*   ppSid
        );

    static HRESULT
    IsCurrentProcessInteractive(
        _Out_ PBOOL pIsInteractive
        );
};
