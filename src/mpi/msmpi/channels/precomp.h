// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

precomp.h - Network Direct MPI CH3 Channel precompiled header

--*/

#ifndef _ND_PRECOMP_H_
#define _ND_PRECOMP_H_

#pragma once

#include <ntstatus.h>
#define WIN32_NO_STATUS

#include "mpidi_ch3_impl.h"
#include "mpitrace.h"

#include "ndsupport.h"
#include "autoptr.h"
#include "list.h"
#include "cs.h"
#include "util.h"

#include "comm.inl"
#include "mpimem.inl"
#include "request.inl"

#endif // _ND_PRECOMP_H_
