// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef MPIDUMP_H_INCLUDED
#define MPIDUMP_H_INCLUDED

#include <dbghelp.h>

//
// Constants used to control dump file generation.
//
enum MSMPI_DUMP_MODE
{
    MsmpiDumpNone = 0,
    MsmpiDumpMini,
    MsmpiDumpAllMini,
    MsmpiDumpFull,
    MsmpiDumpAllFull,
    MsmpiDumpMaximumValue
};


MSMPI_DUMP_MODE GetDumpMode();


void
CreateFinalDumpFile(
    _In_   HANDLE tempFileHandle,
    _In_   int rank,
    _In_z_ const wchar_t* dumpPath,
    _In_   int jobid,
    _In_   int taskid,
    _In_   int taskinstid
    );


HANDLE
CreateTempDumpFile(
    __in HANDLE hProcess,
    __in DWORD pid,
    __in MINIDUMP_TYPE dumpType,
    __in const wchar_t* dumpPath,
    __in_opt MINIDUMP_EXCEPTION_INFORMATION* pExrParam
    );

#endif // MPIDUMP_H_INCLUDED
