// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

//
// The interface used by smpd.exe running in debug mode (-d)
// to launch the smpd instance.
//
#include "precomp.h"
#include "smpd.h"
#include <oacr.h>


extern TOKEN_USER* g_pTokenUser;


HRESULT
WINAPI
SdkCreateManagerProcess(
    _In_   PSID sid,
    _In_z_ const char* app,
    _In_z_ const char* args,
    _In_z_ const char* /*context*/
    )
{
#if !defined(MSMPI_NO_SEC)
    if( !env_is_on( L"MSMPI_DISABLE_AUTHZ", FALSE ) )
    {
        if( FAILED(smpd_equal_user_sid( g_pTokenUser->User.Sid, sid )) )
        {
            return HRESULT_FROM_WIN32(ERROR_NO_SUCH_USER);
        }
    }
#else
    UNREFERENCED_PARAMETER(sid);
#endif

    if( smpd_process.unicodeOutput )
    {
        SetEnvironmentVariableW(L"MPIEXEC_UNICODE_OUTPUT", L"1");
    }

    //
    // MSPMS passes us multibyte arguments for backcompat purposes.
    // We need to convert the multibyte into wchar_t because they might
    // contain true unicode
    //
    smpd_dbg_printf(L"create manager process (using smpd daemon credentials)\n");
    wchar_t* appW;
    wchar_t* argsW;

    DWORD err = MPIU_MultiByteToWideChar( app, &appW );
    if( err != NOERROR )
    {
        smpd_err_printf(L"Failed to convert smpd name to unicode error %u\n", err);
        return HRESULT_FROM_WIN32( err );
    }

    err = MPIU_MultiByteToWideChar( args, &argsW );
    if( err != NOERROR )
    {
        delete[] appW;
        smpd_err_printf(L"Failed to convert smpd arguments to unicode error %u\n", err);
        return HRESULT_FROM_WIN32( err );
    }

    smpd_dbg_printf(L"Launching smpd as '%s %s'\n", appW, argsW);

    HRESULT hr = S_OK;
    STARTUPINFOW si;
    GetStartupInfoW(&si);

    PROCESS_INFORMATION pi;
    BOOL fSucc = OACR_REVIEWED_CALL(
        mpicr,
        CreateProcessW(
            appW,   // Application Name
            argsW,  // Command Line (must be read/write)
            NULL,   // Process Security Attributes,
            NULL,   // Thread Security Attributes,
            TRUE,   // Inherit Parent Handles,
            CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,// Process CreationFlags,
            NULL,   // lpEnvironment,
            NULL,   // lpCurrentDirectory,
            &si,    // lpStartupInfo,
            &pi     // lpProcessInformation (out)
            ));
    if(!fSucc)
    {
        err = GetLastError();
        smpd_err_printf(L"CreateProcess '%s %s' failed, error %u\n", appW, argsW, err);
        hr = HRESULT_FROM_WIN32( err );
    }
    else
    {
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    if( smpd_process.unicodeOutput )
    {
        SetEnvironmentVariableW(L"MPIEXEC_UNICODE_OUTPUT", nullptr );
    }

    delete[] appW;
    delete[] argsW;

    return S_OK;
}
