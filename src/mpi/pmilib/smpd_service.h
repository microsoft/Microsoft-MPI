// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef SMPD_SERVICE_H
#define SMPD_SERVICE_H

//
// Service Control utility functions
//
DWORD SvcDispatch(const char* pServiceName);
DWORD SvcReportProgress(DWORD MilliSecondsToNextTick);
DWORD SvcQueryState(void);
DWORD SvcReportState(DWORD State);
DWORD SvcQueryControls(void);
DWORD SvcDisableControls(DWORD Controls);
DWORD SvcEnableControls(DWORD Controls);
void SvcSetWin32ExitCode(DWORD ExitCode);
void SvcSetSpecificExitCode(DWORD ExitCode);

//
// Service specific implementation override functions
//
void service_main(int argc, char* argv[]);
void service_stop(void);
void service_pause(void);
void service_continue(void);
void service_shutdown(void);

#define SERVICE_LISTENER_PORT 8677
#define SERVICE_NAME TEXT("msmpi")

#endif // SMPD_SERVICE_H
