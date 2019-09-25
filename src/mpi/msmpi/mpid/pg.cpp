// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "pmi.h"

/* FIXME: These routines need a description.  What is their purpose?  Who
   calls them and why?  What does each one do?
*/
static MPIDI_PG_t * MPIDI_PG_list = NULL;
static MPIDI_PG_t * MPIDI_PG_iterator_next = NULL;

#define MPIDI_MAX_KVS_KEY_LEN      256


//
// Local macro used to release references to a PG without destroying
// that PG if it is no longer in use
//
#define MPIDI_PG_release_ref_preserve(pg_)      \
{                                               \
    int inuse_;                                 \
    MPIU_Object_release_ref(pg_, &inuse_);      \
}


/*@
   MPIDI_PG_Finalize - Finalize the process groups, including freeing all
   process group structures
  @*/
void MPIDI_PG_Finalize(void)
{
    MPIDI_PG_t *pg, *pgNext;

    /* Free the storage associated with the process groups */
    pg = MPIDI_PG_list;
    while (pg)
    {
        pgNext = pg->next;

        /* In finalize, we free all process group information, even if
           the ref count is not zero.  This can happen if the user
           fails to use MPI_Comm_disconnect on communicators that
           were created with the dynamic process routines.*/
        if (pg == MPIDI_Process.my_pg)
        {
            MPIDI_Process.my_pg = NULL;
        }

        MPIDI_PG_Destroy(pg);

        pg = pgNext;
    }

    /* If COMM_WORLD is still around (it normally should be),
       try to free it here.  The reason that we need to free it at this
       point is that comm_world (and comm_self) still exist, and
       hence the usual process to free the related VC structures will
       not be invoked. */
    if (MPIDI_Process.my_pg)
    {
        MPIDI_PG_Destroy(MPIDI_Process.my_pg);
    }
    MPIDI_Process.my_pg = NULL;
}

/* This routine creates a new process group description and appends it to
   the list of the known process groups.
   The new process group is returned in pg_ptr
*/
MPI_RESULT
MPIDI_PG_Create(
    _In_  int          vct_sz,
    _In_  const GUID&  pg_id,
    _Outptr_ MPIDI_PG_t** pg_ptr
    )
{
    int i;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPIDI_PG_t** pglist_p;

    MPIDI_PG_t* pg = static_cast<MPIDI_PG_t*>(
        MPIU_Malloc( sizeof(MPIDI_PG_t) )
        );
    if( pg == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    pg->vct = static_cast<MPIDI_VC_t*>(
        MPIU_Malloc( sizeof(MPIDI_VC_t) * vct_sz )
        );
    if( pg->vct == nullptr )
    {
        MPIU_Free( pg );
        return MPIU_ERR_NOMEM();
    }

    pg->handle = 0;
    /* The reference count indicates the number of vc's that are or
       have been in use and not disconnected. It starts at zero,
       except for MPI_COMM_WORLD. */
    MPIU_Object_set_ref(pg, 0);
    pg->size = vct_sz;
    pg->id   = pg_id;

    for (i = 0; i < vct_sz; i++)
    {
        /* Initialize device fields in the VC object */
        MPIDI_VC_Init(&pg->vct[i], pg, i);
    }

    /* We may first need to initialize the channel before calling the channel
       VC init functions.  This routine may be a no-op; look in the
       ch3_init.c file in each channel */
    MPIDI_CH3_PG_Init( pg );

    /* Add pg's at the tail so that comm world is always the first pg */
    pglist_p = &MPIDI_PG_list;
    while(*pglist_p != NULL)
    {
        pglist_p = &(*pglist_p)->next;
    }
    *pglist_p = pg;
    pg->next = NULL;

    *pg_ptr = pg;

    return mpi_errno;
}

void MPIDI_PG_Destroy(
    _Post_ptr_invalid_ MPIDI_PG_t* pg
    )
{
    MPIDI_PG_t** pg_p;

    pg_p = &MPIDI_PG_list;

    int i;
    int pg_size;
    while(*pg_p != NULL)
    {
        if (*pg_p == pg)
        {
            if (MPIDI_PG_iterator_next == pg)
            {
                MPIDI_PG_iterator_next = MPIDI_PG_iterator_next->next;
            }

            *pg_p = pg->next;

            pg_size = pg->size;
            for (i = 0; i < pg_size; i++)
            {
                MpiLockDelete(&pg->vct[i].ch.sendq.lock);
            }

            MPIU_Free(pg->vct);
            MPIDI_CH3_PG_Destroy(pg);
            MPIU_Free(pg);

            goto fn_exit;
        }

        pg_p = &(*pg_p)->next;
    }

    /* PG not found if we got here */
    MPIU_Assert(*pg_p != NULL);

fn_exit:
    ;
}


_Ret_maybenull_ MPIDI_PG_t*
MPIDI_PG_Find(
    _In_ const GUID& id
    )
{
    MPIDI_PG_t* pg;
    pg = MPIDI_PG_list;
    while (pg != NULL)
    {
        if( InlineIsEqualGUID( pg->id, id ) )
        {
            return pg;
        }
        pg = pg->next;
    }

    return NULL;
}


void
MPIDI_PG_Get_next(
    _Outptr_result_maybenull_ MPIDI_PG_t** pg_ptr
    )
{
    MPIDI_PG_t* pg = MPIDI_PG_iterator_next;
    if (MPIDI_PG_iterator_next != NULL)
    {
        MPIDI_PG_iterator_next = MPIDI_PG_iterator_next->next;
    }
    *pg_ptr = pg;
}

void MPIDI_PG_Iterate_reset()
{
    MPIDI_PG_iterator_next = MPIDI_PG_list;
}


/*
 * Managing connection information for process groups
 *
 *
 */

/* Setting a process's connection information

   This is a collective call (for scalability) over all of the processes in
   the same MPI_COMM_WORLD.
*/
MPI_RESULT MPIDI_PG_SetConnInfo( const MPIDI_PG_t* pg, int rank, const char *connString )
{
    MPI_RESULT mpi_errno = PMI_KVS_PublishBC(pg->id, static_cast<UINT16>(rank), static_cast<UINT16>(pg->size), connString);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    mpi_errno = PMI_KVS_Commit(pg->id);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    return PMI_Barrier();
}



/* For all of these routines, the format of the process group description
   that is created and used by the connTo/FromString routines is this:
   (All items are strings, terminated by null)

   process group id string
   sizeof process group (as string)
   conninfo for rank 0
   conninfo for rank 1
   ...

   The "conninfo for rank 0" etc. for the original (MPI_COMM_WORLD)
   process group are stored in the PMI_KVS space with the keys
   p<rank>-businesscard .

   Fixme: Add a routine to publish the connection info to this file so that
   the key for the businesscard is defined in just this one file.
*/


/* The "KVS" versions are for the process group to which the calling
   process belongs.  These use the PMI_KVS routines to access the
   process information */
MPI_RESULT
MPIDI_PG_GetConnString(
    _In_                    const MPIDI_PG_t* pg,
    _In_range_(>=, 0)       int               rank,
    _Out_writes_z_(bufsize) char*             buf,
    _In_                    size_t            bufsize
    )
{
    if( pg->pBusinessCards != NULL )
    {
        MPIU_Assert( rank < pg->size );
        HRESULT hr = StringCchCopyA( buf, bufsize, pg->pBusinessCards[rank]);
        if( FAILED( hr ) )
        {
            return MPIU_ERR_NOMEM();
        }

        return MPI_SUCCESS;
    }

    return PMI_KVS_RetrieveBC( pg->id, static_cast<UINT16>(rank), buf, bufsize );
}

/* FIXME: This routine should invoke a close method on the connection,
   rather than have all of the code here */
/*@
  MPIDI_PG_Close_VCs - Close all virtual connections on all process groups.

  Note:
  This routine is used in MPID_Finalize.  It is here to
  @*/
MPI_RESULT MPIDI_PG_Close_VCs( void )
{
    MPIDI_PG_t * pg = MPIDI_PG_list;
    MPI_RESULT mpi_errno;
    MPI_RESULT last_errno = MPI_SUCCESS;

    while (pg)
    {
        int i;

        for (i = 0; i < pg->size; i++)
        {
            MPIDI_VC_t * vc = &pg->vct[i];
            /* If the VC is myself then skip the close message */
            if (pg == MPIDI_Process.my_pg && i == MPIDI_Process.my_pg_rank)
            {
                if (vc->ref_count != 0)
                {
                    MPIDI_PG_release_ref_preserve(pg);
                }
                continue;
            }

            if (vc->state == MPIDI_VC_STATE_ACTIVE ||
                vc->state == MPIDI_VC_STATE_REMOTE_CLOSE
#ifdef MPIDI_CH3_USES_SSHM
                /* FIXME: Remove this IFDEF */
                /* sshm queues are uni-directional.  A VC that is connected
                 * in the read direction is marked MPIDI_VC_STATE_INACTIVE
                 * so that a connection will be formed on the first write.
                 * Since the other side is marked MPIDI_VC_STATE_ACTIVE for
                 * writing
                 * we need to initiate the close protocol on the read side
                 * even if the write state is MPIDI_VC_STATE_INACTIVE. */
                || ((vc->state == MPIDI_VC_STATE_INACTIVE) && vc->ch.shm.recv.connected)
#endif
                )
            {
                mpi_errno = MPIDI_CH3U_VC_SendClose( vc, i );
                if( mpi_errno != MPI_SUCCESS )
                {
                    //
                    // In the event of errors, we still need to
                    // continue to close all the other VC's. But
                    // we will save the last error to report it
                    // back.
                    //
                    last_errno = mpi_errno;
                }
            }
            else
            {
                if (vc->state == MPIDI_VC_STATE_INACTIVE && vc->ref_count != 0)
                {
                    /* FIXME: If the reference count for the vc is not 0, something is wrong */
                    MPIDI_PG_release_ref_preserve(pg);
                }
            }
        }
        pg = pg->next;
    }
    /* Note that we do not free the process groups within this routine, even
       if the reference counts have gone to zero. That is done once the
       connections are in fact closed (by the final progress loop that
       handles any close requests that this code generates) */

    return last_errno;
}


/* Providing a comm argument permits optimization, but this function is always
   allowed to return the max for the universe. */
int MPID_Get_max_node_id()
{
    /* easiest way to implement this is to track it at PG create/destroy time */
    int max_id_p = PMI_Get_size();
    MPIU_Assert( max_id_p >= 0 );
    return max_id_p;
}


//
// Fills in the node_id info from pmi info provided through SMPD.
//
MPI_RESULT MPIDI_Populate_vc_node_ids(MPIDI_PG_t *pg)
{
    //
    //would be nice to assert here that the pg represented is comm world
    //
    const UINT16* node_ids = PMI_Get_node_ids();
    if(node_ids != NULL)
    {
        for(int i = 0; i < pg->size; ++i)
        {
            pg->vct[i].node_id = node_ids[i];
        }
        return MPI_SUCCESS;
    }
    else
    {
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**nodeids");
    }
}
