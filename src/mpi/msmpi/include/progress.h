// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef PROGRESS_H
#define PROGRESS_H

int MPIDI_CH3I_Progress_init(void);
int MPIDI_CH3I_Progress_finalize(void);


/*@
  MPID_Progress_wait - Wait for some communication

    Return value:
    An mpi error code.

    Notes:
    This instructs the progress engine to wait until some communication event
    happens since 'MPID_Progress_start' was called.  This call blocks the
    calling thread (only, not the process).

 @*/
int MPID_Progress_wait(void);

/*@
  MPID_Progress_pump - Allow a progress engine to check for pending communication

  Return value:
  An mpi error code.

  Notes:
  This routine provides a way to invoke the progress engine in a polling
  implementation of the ADI.  This routine must be nonblocking.

  @*/
int MPID_Progress_pump(void);


/*
 * CH3 Progress routines (implemented as macros for performanace)
 */
int MPIDI_CH3I_Progress(DWORD timeout, bool interruptible);
#define MPID_Progress_wait() MPIDI_CH3I_Progress(INFINITE, false)
#define MPID_Progress_pump() MPIDI_CH3I_Progress(0, false)
#define MPID_Progress_wait_interruptible() MPIDI_CH3I_Progress(INFINITE, true)

MPI_RESULT MPIDI_CH3I_DeferConnect(_In_ struct MPIDI_VC_t* vc, _In_ struct MPID_Request* sreq);
MPI_RESULT MPIDI_CH3I_DeferWrite(_In_ MPIDI_VC_t* vc, _In_ MPID_Request* sreq);

//
// Stalls the caller for the specified timeout (in milliseconds), but makes progress
// on outstanding MPI operations.
//
// NOTE: Because this function calls into the progress engine, it should never be used
// from paths that could be called by the progress engine, to prevent recursion.
//
int MPIU_Sleep(DWORD timeout);


//
// CH3 completion set.
// Completions posted to this 'set' will be executed through the progress engine
// and will wake it up from a wait state.
//
extern ExSetHandle_t MPIDI_CH3I_set;


#endif // PROGRESS_H
