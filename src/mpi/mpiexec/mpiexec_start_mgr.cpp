// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "smpd.h"

HRESULT
WINAPI
MpiexecCreateManagerProcess(
    _In_z_ const char* app,
    _In_z_ const char* args,
    _In_z_ const char* /*context*/
    )
{
    //
    // MSPMS passes us multibyte arguments for backcompat purposes.
    // We need to convert the multibyte into wchar_t because they might
    // contain true unicode
    //
    smpd_dbg_printf(L"create manager process (using mpiexec credentials)\n");
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

    if( smpd_process.unicodeOutput )
    {
        SetEnvironmentVariableW(L"MPIEXEC_UNICODE_OUTPUT", L"1");
    }
    STARTUPINFOW si;
    GetStartupInfoW( &si );

    HRESULT hr = S_OK;
    PROCESS_INFORMATION pi;

    BOOL fSucc = OACR_REVIEWED_CALL(
        mpicr,
        CreateProcessW(
            appW,   // Application Name
            argsW,  // Command Line (must be read/write)
            NULL,   // Process Security Attributes,
            NULL,   // Thread Security Attributes,
            TRUE,   // Inherit Parent Handles,
            CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, // Process CreationFlags,
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
        //
        // Successfully launched the SMPD manager. Create a job object
        // and assign it so that if mpiexec is terminated unexpectedly,
        // the SMPD instance will be cleaned up.
        //
        if( g_IsWin8OrGreater && !env_is_on(L"SMPD_NO_JOBOBJ", FALSE) )
        {
            ASSERT( smpd_process.hJobObj == nullptr );
            smpd_process.hJobObj = CreateJobObjectW( nullptr, nullptr );
            if( smpd_process.hJobObj == nullptr )
            {
                smpd_dbg_printf(L"mpiexec failed to create job object to launch smpd, error %u",
                                GetLastError());
            }
            else
            {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobLimit = {0};
                jobLimit.BasicLimitInformation.LimitFlags =
                    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
                fSucc = SetInformationJobObject(
                    smpd_process.hJobObj,
                    JobObjectExtendedLimitInformation,
                    &jobLimit,
                    sizeof(jobLimit) );
                if( !fSucc )
                {
                    smpd_dbg_printf(L"mpiexec failed to set job object limit to launch smpd, error %u",
                                    GetLastError());
                }
                else
                {
                    fSucc = AssignProcessToJobObject(
                        smpd_process.hJobObj,
                        pi.hProcess);
                    if (!fSucc)
                    {
                        smpd_dbg_printf(L"mpiexec failed to assign smpd to job object, error %u",
                            GetLastError());
                    }
                }
            }
        }
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    if( smpd_process.unicodeOutput )
    {
        SetEnvironmentVariableW(L"MPIEXEC_UNICODE_OUTPUT", nullptr);
    }

    delete[] appW;
    delete[] argsW;
    return hr;
}
