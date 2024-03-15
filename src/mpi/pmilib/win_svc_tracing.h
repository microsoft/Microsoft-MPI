// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

//
// Main entry point for enabling the MPI Diagnostics trace session
//
ULONG StartTraceSession(void);


//
// Main entry point for stopping the MPI Diagnostics trace session
//
ULONG StopTraceSession(void);


//
// Entry points for diagnostic tracing in the service.
//
int ServicePrintErrorEvent(const char* pStr, ...);
int ServicePrintInfoEvent(const char* pStr, ...);
