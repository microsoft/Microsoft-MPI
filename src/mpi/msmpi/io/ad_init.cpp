// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"

ADIOI_Flatlist_node *ADIOI_Flatlist = NULL;

/* for f2c and c2f conversion */
MPI_File *ADIOI_Ftable = NULL;
int ADIOI_Ftable_ptr = 0, ADIOI_Ftable_max = 0;

MPI_Errhandler ADIOI_DFLT_ERR_HANDLER = MPI_ERRORS_RETURN;

int ADIO_Init()
{
    /* initialize the linked list containing flattened datatypes */
    ADIOI_Flatlist = (ADIOI_Flatlist_node *) ADIOI_Malloc(sizeof(ADIOI_Flatlist_node));
    if( ADIOI_Flatlist == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    ADIOI_Flatlist->type = MPI_DATATYPE_NULL;
    ADIOI_Flatlist->next = NULL;
    ADIOI_Flatlist->blocklens = NULL;
    ADIOI_Flatlist->indices = NULL;

    return MPI_SUCCESS;
}


void ADIO_Finalize()
{
    ADIOI_Flatlist_node *curr, *next;

    /* delete the flattened datatype list */
    curr = ADIOI_Flatlist;
    while (curr)
    {
        if (curr->blocklens) ADIOI_Free(curr->blocklens);
        if (curr->indices) ADIOI_Free(curr->indices);
        next = curr->next;
        ADIOI_Free(curr);
        curr = next;
    }
    ADIOI_Flatlist = NULL;

    /* free file and info tables used for Fortran interface */
    if (ADIOI_Ftable) ADIOI_Free(ADIOI_Ftable);
}