// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "adio.h"


/* This is the utility file for info that contains the basic info items
   and storage management */
#ifndef MPID_INFO_PREALLOC
#define MPID_INFO_PREALLOC 8
#endif

C_ASSERT( HANDLE_GET_TYPE(MPI_INFO_NULL) == HANDLE_TYPE_INVALID );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_INFO_NULL) == MPID_INFO );

/* Preallocated info objects */
MPID_Info MPID_Info_direct[MPID_INFO_PREALLOC] = { { 0 } };
MPIU_Object_alloc_t MPID_Info_mem = { 0, 0, 0, 0, MPID_INFO,
                                      sizeof(MPID_Info), MPID_Info_direct,
                                      MPID_INFO_PREALLOC, };

MPID_Info* MPIU_Info_alloc_node(const char* key, const char* value)
{
    MPID_Info* p = (MPID_Info*)MPIU_Handle_obj_alloc(&MPID_Info_mem);
    if(p == NULL)
        goto fn_fail1;

    p->next = NULL;
    p->key = NULL;
    p->value = NULL;

    if(key != NULL)
    {
        p->key = MPIU_Strdup(key);
        if((p->key == NULL))
            goto fn_fail2;
    }

    if(value != NULL)
    {
        p->value = MPIU_Strdup(value);
        if((p->value == NULL))
            goto fn_fail3;
    }

    return p;

fn_fail3:
    MPIU_Free(p->key);
fn_fail2:
    MPIU_Handle_obj_free(&MPID_Info_mem, p);
fn_fail1:
    return NULL;
}


void MPIU_Info_free_node(MPID_Info* p)
{
    MPIU_Free(p->value);
    MPIU_Free(p->key);
    MPIU_Handle_obj_free(&MPID_Info_mem, p);
}


void MPIU_Info_free_list(MPID_Info* p)
{
    while (p != NULL)
    {
        MPID_Info* q = p->next;
        MPIU_Info_free_node(p);
        p = q;
    }
}
