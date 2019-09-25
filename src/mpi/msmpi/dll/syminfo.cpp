// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "mpidimpl.h"
#include "mpidrma.h"
#include "ch3_compression.h"
#include "mpidi_ch3_impl.h"
#include "dbgtypes.h"
#include "mpitrace.h"

#define DECLARE_TYPE(Name) Name _DECL_##Name

DECLARE_TYPE(MPID_Request);
DECLARE_TYPE(MPID_Comm);
DECLARE_TYPE(MPID_Datatype);
DECLARE_TYPE(MPIDI_Request_t);
DECLARE_TYPE(MPIDI_Message_match);
DECLARE_TYPE(MPIR_Comm_list);
DECLARE_TYPE(MPIR_Sendq);
