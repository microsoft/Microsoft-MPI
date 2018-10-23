/*++

 Copyright (c) Microsoft Corporation. All rights reserved.
 Licensed under the MIT License.

dump.cpp - MPI minidump functionality

--*/
#include "precomp.h"
#include "mpidump.h"
#include <dbghelp.h>
#include <excpt.h>


typedef BOOL
(WINAPI *PFN_MiniDumpWriteDump)(
    __in HANDLE hProcess,
    __in DWORD ProcessId,
    __in HANDLE hFile,
    __in MINIDUMP_TYPE DumpType,
    __in_opt PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    __in_opt PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    __in_opt PMINIDUMP_CALLBACK_INFORMATION CallbackParam
    );


MSMPI_DUMP_MODE GetDumpMode()
{
    wchar_t env[12];

    DWORD err = MPIU_Getenv( L"MSMPI_DUMP_MODE", env, _countof(env) );
    if( err != NOERROR )
    {
        return MsmpiDumpNone;
    }

    int val = _wtoi(env);
    if( val > MsmpiDumpNone && val < MsmpiDumpMaximumValue )
    {
        return static_cast<MSMPI_DUMP_MODE>(val);
    }

    return MsmpiDumpNone;
}


void
CreateFinalDumpFile(
    _In_   HANDLE tempFileHandle,
    _In_   int rank,
    _In_z_ const wchar_t* dumpPath,
    _In_   int jobid,
    _In_   int taskid,
    _In_   int taskinstid
    )
{
    MPIU_Assert( tempFileHandle != INVALID_HANDLE_VALUE );
    wchar_t tempFileName[MAX_PATH];
    DWORD err = GetFinalPathNameByHandleW(
        tempFileHandle,
        tempFileName,
        _countof( tempFileName ),
        0
        );

    if( err == 0 )
    {
        return;
    }

    wchar_t        dPath[MAX_PATH];
    HRESULT        hr;

    if( dumpPath != NULL && dumpPath[0] != L'\0' )
    {
        hr = StringCchCopyW( dPath, _countof( dPath ), dumpPath );
    }
    else
    {
        hr = StringCchCopyW( dPath, _countof( dPath ), L"%USERPROFILE%");
    }

    if( FAILED( hr ) )
    {
        return;
    }

    wchar_t        name[MAX_PATH];

    //
    // For CCP, task instance id starts at 0. Thus, we only verify
    // jobid and taskid
    //
    if( jobid == 0 || taskid == 0 )
    {
        //
        // In the SDK environment use the sdk default dumpfile
        //
        hr = StringCchPrintfW(
            name,
            _countof( name ),
            L"%s\\mpi_dump_%d.dmp",
            dPath,
            rank
            );
    }
    else
    {
        //
        // In the cluster environment use the cluster default dumpfile
        // (incl. jobid.taskid.taskinstid)
        //
        hr = StringCchPrintfW(
            name,
            _countof( name ),
            L"%s\\mpi_dump_%d.%d.%d.%d.dmp",
            dPath,
            jobid,
            taskid,
            taskinstid,
            rank
            );
    }

    if( FAILED( hr ) )
    {
        return;
    }

    DWORD ccPath = ExpandEnvironmentStringsW(
        name,
        dPath,
        _countof( dPath )
        );
    if( ccPath == 0 )
    {
        return;
    }

    //
    // For MPI Process, it is possible that it will be suspended
    // by SMPD during the CopyFile operation. However, if this happens,
    // this means this process is not the failing process (otherwise SPMD
    // would not have suspended it). In the case of all processes generating
    // dump files, SMPD will then write the dump for this process, which will
    // result in a good dupm file.
    //
    CopyFileW( tempFileName, dPath, FALSE );
}


HANDLE
CreateTempDumpFile(
    __in HANDLE hProcess,
    __in DWORD pid,
    __in MINIDUMP_TYPE dumpType,
    __in const wchar_t* dumpPath,
    __in_opt MINIDUMP_EXCEPTION_INFORMATION* pExrParam
    )
{
    HANDLE hFile = INVALID_HANDLE_VALUE;

    // Create target file
    wchar_t path[MAX_PATH];

    // Load dbghelp library.
    DWORD ccPath = GetSystemDirectoryW( path, _countof(path) );
    if( ccPath == 0 )
    {
        return INVALID_HANDLE_VALUE;
    }

    HRESULT hr = StringCchCopyW( &path[ccPath],
                                 _countof(path) - ccPath,
                                 L"\\dbghelp.dll" );
    if( FAILED( hr ) )
    {
        return INVALID_HANDLE_VALUE;
    }

    HMODULE hDbgHelp;
    OACR_REVIEWED_CALL(
        mpicr,
        hDbgHelp = LoadLibraryExW( path, nullptr, 0 ) );
    if( hDbgHelp == NULL )
    {
        return INVALID_HANDLE_VALUE;
    }

    PFN_MiniDumpWriteDump pfnWriteDump = (PFN_MiniDumpWriteDump)GetProcAddress(
        hDbgHelp,
        "MiniDumpWriteDump"
        );
    if( pfnWriteDump == NULL )
    {
        goto free_dbghelp_and_exit;
    }

    if( dumpPath != NULL && dumpPath[0] != L'\0' )
    {
        ccPath = ExpandEnvironmentStringsW(
            dumpPath,
            path,
            _countof(path)
            );
    }
    else
    {
        ccPath = ExpandEnvironmentStringsW(
            L"%USERPROFILE%",
            path,
            _countof(path)
        );
    }

    if( ccPath == 0 )
    {
        goto free_dbghelp_and_exit;
    }

    wchar_t name[MAX_PATH];
    int err = GetTempFileNameW(
        path,
        L"_mp",
        0,
        name
        );
    if( err == 0 )
    {
        goto free_dbghelp_and_exit;
    }

    hFile = CreateFileW(
        name,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        NULL
        );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        goto free_dbghelp_and_exit;
    }

    pfnWriteDump(
        hProcess,
        pid,
        hFile,
        dumpType,
        pExrParam,
        NULL,
        NULL
        );

free_dbghelp_and_exit:
    FreeLibrary( hDbgHelp );
    return hFile;
}
