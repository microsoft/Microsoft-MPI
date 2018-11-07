// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

struct MPIR_Comm_list
{
    int        sequence_number;   /* Used to detect changes in the list */
    MPID_Comm* head;              /* Head of the list */
};


//
// We save the tag/rank/context_id so that the debugger
// can simplify the data fetching.
//
//
struct MPIR_Sendq
{
    LIST_ENTRY         ListEntry;
    MPID_Request*      sreq;
    int                tag;
    int                rank;
    MPI_CONTEXT_ID     context_id;
};

extern MPIR_Sendq* MPIR_Sendq_head;
extern MPIR_Sendq* MPIR_Sendq_pool;
