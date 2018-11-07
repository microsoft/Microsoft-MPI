// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "kernel32util.h"
#include "util.h"

//
// Summary:
//  Global to indicate if we should use Win7+ features.
//
// NOTE:
//  This is not static so we can manipulate this bit from unit tests
//
BOOL     g_IsWin7OrGreater = CheckOSVersion(6,1);
BOOL     g_IsWin8OrGreater = CheckOSVersion(6,2);

Kernel32 Kernel32::Methods;