// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


MPI_RESULT
MPID_Probe(
    _In_ int source,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Out_ MPI_Status* status
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    if (source == MPI_PROC_NULL)
    {
        MPIR_Status_set_procnull(status);
        return MPI_SUCCESS;
    }

    do
    {
        if (MPID_Recvq_probe_msg( source, tag, comm->recvcontext_id, status ))
            break;

        mpi_errno = MPID_Progress_wait();
    }
    while(mpi_errno == MPI_SUCCESS);

    return mpi_errno;
}


MPI_RESULT
MPID_Iprobe(
    _In_ int source,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Out_ int* flag,
    _Out_ MPI_Status* status
    )
{
    int found = 0;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    if (source == MPI_PROC_NULL)
    {
        MPIR_Status_set_procnull(status);
        /* We set the flag to true because an MPI_Recv with this rank will
           return immediately */
        *flag = TRUE;
        return MPI_SUCCESS;
    }

    /* FIXME: The routine MPID_Recvq_probe_msg is used only by the probe functions;
       it should atomically return the flag and status rather than create
       a request.  Note that in some cases it will be possible to
       atomically query the unexpected receive list (which is what the
       probe routines are for). */
    found = MPID_Recvq_probe_msg( source, tag, comm->recvcontext_id, status );
    if (!found)
    {
        /* Always try to advance progress before returning failure
           from the iprobe test. */
        /* FIXME: It would be helpful to know if the Progress_poke
         operation causes any change in state; we could then avoid
         a second test of the receive queue if we knew that nothing
         had changed */

        mpi_errno = MPID_Progress_pump();

        found = MPID_Recvq_probe_msg( source, tag, comm->recvcontext_id, status );
    }

    *flag = found;

    return mpi_errno;
}


MPI_RESULT
MPID_Mprobe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** message
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    *message = nullptr;

    do
    {
        if( MPID_Recvq_probe_dq_msg( source, tag, comm->recvcontext_id, message ) )
        {
            //
            // Store the reference to the comm here as it is considered
            // implicit in the MPID_Imrecv method
            //
            (*message)->comm = comm;
            comm->AddRef();
            break;
        }

        mpi_errno = MPID_Progress_wait();

    } while( mpi_errno == MPI_SUCCESS );

    return mpi_errno;
}


MPI_RESULT
MPID_Improbe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPID_Comm* comm,
    _Out_ _Deref_out_range_(0, 1) int* flag,
    _Outptr_result_maybenull_ MPID_Request** message
    )
{
    BOOL found = FALSE;
    *message = nullptr;

    found = MPID_Recvq_probe_dq_msg( source, tag, comm->recvcontext_id, message );
    if( found == FALSE )
    {
        //
        // Always try to advance progress before returning failure
        // from the improbe method.
        // FIXME: It would be helpful to know if the Progress_poke
        // operation causes any change in state; we could then avoid
        // a second test of the receive queue if we knew that nothing
        // had changed.
        //
        MPI_RESULT mpi_errno = MPID_Progress_pump();
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        found = MPID_Recvq_probe_dq_msg( source, tag, comm->recvcontext_id, message );
    }

    if( found == TRUE )
    {
        //
        // Store the reference to the comm here as it is considered
        // implicit in the MPID_Imrecv method
        //
        (*message)->comm = comm;
        comm->AddRef();
    }

    *flag = found;

    return MPI_SUCCESS;
}
