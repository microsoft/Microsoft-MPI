/*
* Copyright (c) Microsoft Corporation. All rights reserved.
* Licensed under the MIT License.
*
*  This file includes definitions of utility functions:
*       Windows Event Logger Utilities
*       Registry Utilities
*       Security Utilities
*/

#include "precomp.h"
#include "SvcUtils.h"

/*---------------------------------------------------------------------------*/
/* Windows Event Logger Utility Class                                        */
/*---------------------------------------------------------------------------*/

EventLogger::EventLogger()
    : m_eventLog(nullptr)
{
}


EventLogger::~EventLogger()
{
    DeregisterEventSource(m_eventLog);
}


BOOL EventLogger::Open(_In_z_ PCWSTR pEventSource)
{
    m_eventLog = RegisterEventSourceW(nullptr, pEventSource);
    if (m_eventLog == nullptr)
    {
        wprintf(
            L"Cannot register event source %s with error 0x%x\n",
            pEventSource,
            GetLastError()
            );

        return FALSE;
    }

    return TRUE;
}


//
// Writes formatted sting to event log.
//
void
EventLogger::WriteEvent(
    _In_    WORD    type,
    _In_    WORD    category,
    _In_    DWORD   eventId,
    _Printf_format_string_ LPCWSTR pFormatStr,
    ...
    )
{
    WCHAR    buffer[MAX_LOG_TEXT];
    va_list args;

    va_start(args, pFormatStr);

    HRESULT hr = StringCchVPrintfW(buffer, _countof(buffer), pFormatStr, args);

    va_end(args);

    LPCWSTR eventStr = buffer;

    if(hr != S_OK && hr != STRSAFE_E_INSUFFICIENT_BUFFER)
    {
        //
        // STRSAFE_E_INSUFFICIENT_BUFFER is acceptable since truncated messages
        // are good enough.
        //
        eventStr = L"Failed to form event message.";
    }

    ReportEventW(
        m_eventLog,
        type,
        category,
        eventId,
        nullptr,
        1,
        0,
        &eventStr,
        nullptr
        );
}


/*---------------------------------------------------------------------------*/
/* Registry Access Utility Class                                             */
/*---------------------------------------------------------------------------*/

HRESULT
RegistryUtils::ReadKey(
    _In_     HKEY    key,
    _In_opt_z_ LPCWSTR pSubKey,
    _In_z_   LPCWSTR pValueName,
    _Inout_  LPDWORD pcbValue,
    _Out_    LPWSTR  pValue
    )
{
    HKEY    readKey;

    LONG result = RegOpenKeyExW(key, pSubKey, 0, KEY_READ, &readKey);

    if( result != ERROR_SUCCESS )
    {
        return HRESULT_FROM_WIN32( result );
    }

    result = RegGetValueW(
        readKey,
        nullptr,
        pValueName,
        RRF_RT_REG_SZ,
        nullptr,
        pValue,
        pcbValue
        );

    RegCloseKey(readKey);

    return HRESULT_FROM_WIN32( result );
}


//
// Reads a key in HKEY_CURRENT_USER for the user the current thread is impersonating.
//
HRESULT
RegistryUtils::ReadCurrentUserKey(
    _In_opt_z_                  LPCWSTR pSubKey,
    _In_z_                      LPCWSTR pValueName,
    _Inout_                     LPDWORD pCapValue,
    _Out_writes_z_(*pCapValue)  LPWSTR  pValue
)
{
    HKEY currentUser = nullptr;
    LONG result = RegOpenCurrentUser(KEY_READ, &currentUser);
    if (result != ERROR_SUCCESS)
    {
        return HRESULT_FROM_WIN32(result);
    }

    result = RegGetValueW(
        currentUser,
        pSubKey,
        pValueName,
        RRF_RT_REG_SZ,
        nullptr,
        pValue,
        pCapValue
        );

    RegCloseKey(currentUser);
    return HRESULT_FROM_WIN32(result);
}


//
// Writes to a key in HKEY_CURRENT_USER for the user the current thread is impersonating.
// Creates the key if it does not exist.
//
HRESULT
RegistryUtils::WriteCurrentUserKey(
    _In_opt_z_ LPCWSTR pSubKey,
    _In_     LPCWSTR pValueName,
    _In_     DWORD   capValue,
    _In_     LPCWSTR pValue
)
{
    HKEY    currentUser = nullptr;
    HKEY    hWrite      = nullptr;
    LONG    result      = ERROR_SUCCESS;
    DWORD   disposition;

    result = RegOpenCurrentUser(KEY_WRITE, &currentUser);
    if (result != ERROR_SUCCESS)
    {
        goto exit_fn;
    }

    //
    // Opens or creates registry key.
    //
    result = RegCreateKeyExW(
        currentUser,
        pSubKey,
        0,
        nullptr,
        0,
        KEY_WRITE,
        nullptr,
        &hWrite,
        &disposition
        );

    if (result != ERROR_SUCCESS)
    {
        goto exit_fn;
    }

    result = RegSetValueExW(
        hWrite,
        pValueName,
        0,
        RRF_RT_REG_SZ,
        reinterpret_cast<const BYTE*>(pValue),
        capValue
        );

exit_fn:
    RegCloseKey(currentUser);
    RegCloseKey(hWrite);
    return HRESULT_FROM_WIN32(result);
}


/*---------------------------------------------------------------------------*/
/* Security Access Utility Class                                             */
/*---------------------------------------------------------------------------*/

//
// Creates a primary token that duplicates the access token of the calling thread.
// Caller must CloseHandle when it is no longer needed.
//
HRESULT SecurityUtils::GetCurrentThreadPrimaryToken(_Out_ PHANDLE pPrimaryToken)
{
    HANDLE  token;
    HRESULT result = NO_ERROR;

    if (!OpenThreadToken(
        GetCurrentThread(),
        TOKEN_ALL_ACCESS,
        TRUE,
        &token)
        )
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (!DuplicateTokenEx(
        token,
        TOKEN_ALL_ACCESS,
        nullptr,
        SecurityImpersonation,
        TokenPrimary,
        pPrimaryToken)
        )
    {
        result = HRESULT_FROM_WIN32(GetLastError());
    }

    CloseHandle(token);
    return result;
}


//
// Checks if given token is a member of the well known group.
//
HRESULT
SecurityUtils::IsGroupMember(
    _In_  WELL_KNOWN_SID_TYPE wellKnownSidType,
    _In_  HANDLE              token,
    _Out_ PBOOL               pIsMember
    )
{
    SID*    pGroupSid   = nullptr;
    DWORD   cbSid       = 0;
    HRESULT result      = NO_ERROR;

    *pIsMember          = FALSE;

    //
    // Get the size of group sid
    //
    if (!CreateWellKnownSid(wellKnownSidType, nullptr, pGroupSid, &cbSid))
    {
        DWORD gle = GetLastError();
        if (gle != ERROR_INSUFFICIENT_BUFFER)
        {
            result = HRESULT_FROM_WIN32(gle);
            goto exit_fn;
        }
    }

    //
    // Allocate necessary memory for group sid and create it
    //
    pGroupSid = static_cast<SID*>(malloc(cbSid));
    if (pGroupSid == nullptr)
    {
        result = ERROR_OUTOFMEMORY;
        goto exit_fn;
    }

    if (!CreateWellKnownSid(wellKnownSidType, nullptr, pGroupSid, &cbSid))
    {
        result = HRESULT_FROM_WIN32(GetLastError());
        goto exit_fn;
    }

    //
    // Check token membership
    //
    if (!CheckTokenMembership(token, pGroupSid, pIsMember))
    {
        result = HRESULT_FROM_WIN32(GetLastError());
        goto exit_fn;
    }

exit_fn:
    free(pGroupSid);
    return result;
}


HRESULT
SecurityUtils::IsCurrentProcessInteractive(
    _Out_ PBOOL pIsInteractive
)
{
    if (pIsInteractive == nullptr)
    {
        return E_INVALIDARG;
    }

    //
    // nullptr token makes API use an impersonation token of the calling thread
    //
    return IsGroupMember(WinInteractiveSid, nullptr, pIsInteractive);
}


//
// Gets the user name and domain for given token.
//
HRESULT
SecurityUtils::GetTokenUser(
    _In_      HANDLE        token,
    _Out_opt_ LPWSTR        pUser,
    _Inout_   LPDWORD       pcchUser,
    _Out_opt_ LPWSTR        pDomain,
    _Inout_   LPDWORD       pcchDomain
    )
{
    PTOKEN_USER     pTokenUser  = nullptr;
    DWORD           cbTokenUser = 0;
    HRESULT         result      = S_OK;
    SID_NAME_USE    snu;

    while (!GetTokenInformation(
        token,
        TokenUser,
        pTokenUser,
        cbTokenUser,
        &cbTokenUser)
        )
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            result = HRESULT_FROM_WIN32(GetLastError());
            goto exit_fn;
        }

        free(pTokenUser);
        pTokenUser = static_cast<PTOKEN_USER>(malloc(cbTokenUser));
        if (pTokenUser == nullptr)
        {
            result = E_OUTOFMEMORY;
            goto exit_fn;
        }
    }

    Assert(pTokenUser != nullptr);

    if (!LookupAccountSidW(
        nullptr,
        pTokenUser->User.Sid,
        pUser,
        pcchUser,
        pDomain,
        pcchDomain,
        &snu))
    {
        result = HRESULT_FROM_WIN32(GetLastError());
    }

exit_fn:
    free(pTokenUser);
    return result;
}


//
// Gets the user name and domain of the current user.
//
HRESULT
SecurityUtils::GetCurrentUser(
    _Out_opt_ LPWSTR        pUser,
    _Inout_   LPDWORD       pcchUser,
    _Out_opt_ LPWSTR        pDomain,
    _Inout_   LPDWORD       pcchDomain
    )
{
    HANDLE  currentToken = nullptr;
    HRESULT result;

    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &currentToken))
    {
        DWORD gle = GetLastError();
        if (gle != ERROR_NO_TOKEN)
        {
            result = HRESULT_FROM_WIN32(gle);
            goto exit_fn;
        }

        //
        // If current thread does not have a token, use process token
        //
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &currentToken))
        {
            result = HRESULT_FROM_WIN32(GetLastError());
            goto exit_fn;
        }
    }

    result = GetTokenUser(currentToken, pUser, pcchUser, pDomain, pcchDomain);

exit_fn:
    CloseHandle(currentToken);
    return result;
}


HRESULT
SecurityUtils::GrantPrivilege(
    _In_ HANDLE     hToken,
    _In_ LPCTSTR    privilege,
    _In_ BOOL       enable
)
{
    TOKEN_PRIVILEGES    adjustedPrivs;
    LUID                luid;

    if (!LookupPrivilegeValue(
        nullptr,
        privilege,
        &luid))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    adjustedPrivs.PrivilegeCount = 1;
    adjustedPrivs.Privileges[0].Luid = luid;
    adjustedPrivs.Privileges[0].Attributes = (enable ? SE_PRIVILEGE_ENABLED : 0);

    AdjustTokenPrivileges(
            hToken,
            FALSE,
            &adjustedPrivs,
            0,
            nullptr,
            nullptr);

    return HRESULT_FROM_WIN32(GetLastError());
}


//
// Get the SID for a given account/group name.
// Caller is responsible for invoking free(*ppSid).
//
HRESULT
SecurityUtils::GetSidForAccount(
    _In_opt_z_ LPCWSTR pSystemName,
    _In_     LPCWSTR pAccountName,
    _Outptr_ PSID*   ppSid
)
{
    PSID         pSid        = nullptr;
    LPWSTR       pRefDomain  = nullptr;
    DWORD        cbRefDomain = 0;
    DWORD        cbSid       = 0;
    SID_NAME_USE eUse;

    if (LookupAccountNameW(
            pSystemName,
            pAccountName,
            0,
            &cbSid,
            0,
            &cbRefDomain,
            &eUse))
    {
        return HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
    }

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (cbSid != 0)
    {
        pSid = static_cast<PSID>(malloc(cbSid));
        if (pSid == nullptr)
        {
            return E_OUTOFMEMORY;
        }
    }

    if (cbRefDomain != 0)
    {
        pRefDomain = static_cast<LPWSTR>(malloc(cbRefDomain * sizeof(wchar_t)));
        if (pRefDomain == nullptr)
        {
            free(pSid);
            return E_OUTOFMEMORY;
        }
    }

    if (!LookupAccountNameW(
            pSystemName,
            pAccountName,
            pSid,
            &cbSid,
            pRefDomain,
            &cbRefDomain,
            &eUse))
    {
        free(pRefDomain);
        free(pSid);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    free(pRefDomain);
    *ppSid = pSid;
    return S_OK;
}
