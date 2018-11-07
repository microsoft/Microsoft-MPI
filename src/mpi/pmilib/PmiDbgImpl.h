// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "PmiDbg.h"


typedef struct _PMIDBG_NOTIFICATION
{
    PMIDBG_NOTIFY_TYPE      Type;
    FN_PmiDbgControl*       Control;

} PMIDBG_NOTIFICATION;

extern const PMIDBG_NOTIFICATION SmpdNotifyInitialize;
extern const PMIDBG_NOTIFICATION SmpdNotifyFinalize;
extern const PMIDBG_NOTIFICATION SmpdNotifyBeforeCreateProcess;
extern const PMIDBG_NOTIFICATION SmpdNotifyAfterCreateProcess;

extern const PMIDBG_NOTIFICATION MpiexecNotifyInitialize;
extern const PMIDBG_NOTIFICATION MpiexecNotifyFinalize;
extern const PMIDBG_NOTIFICATION MpiexecNotifyBeforeCreateProcesses;
extern const PMIDBG_NOTIFICATION MpiexecNotifyAfterCreateProcesses;

void LoadPmiDbgExtensions( PMIDBG_HOST_TYPE hostType );

void UnloadPmiDbgExtensions();

void NotifyPmiDbgExtensions( PMIDBG_NOTIFICATION notify, ... );
