// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef PT2PT_H
#define PT2PT_H


/*@
  MPID_Send - MPID entry point for MPI_Send

  Notes:
  The only difference between this and 'MPI_Send' is that the basic
  error checks (e.g., valid communicator, datatype, dest, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects, and a request may be returned.
  A request is returned only if the ADI implementation was unable to
  complete the send of the message.  In that case, the usual 'MPI_Wait'
  logic should be used to complete the request.  This approach is used to
  allow a simple implementation of the ADI.  The ADI is free to always
  complete the message and never return a request.

  Module:
  Communication

  @*/
MPI_RESULT
MPID_Send(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Rsend - MPID entry point for MPI_Rsend

  Notes:
  The only difference between this and 'MPI_Rsend' is that the basic
  error checks (e.g., valid communicator, datatype, dest, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects, and a request may be returned.
  A request is returned only if the ADI implementation was unable to
  complete the send of the message.  In that case, the usual 'MPI_Wait'
  logic should be used to complete the request.  This approach is used to
  allow a simple implementation of the ADI.  The ADI is free to always
  complete the message and never return a request.

  Module:
  Communication

  @*/
MPI_RESULT
MPID_Rsend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Ssend - MPID entry point for MPI_Ssend

  Notes:
  The only difference between this and 'MPI_Ssend' is that the basic
  error checks (e.g., valid communicator, datatype, dest, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects, and a request may be returned.
  A request is returned only if the ADI implementation was unable to
  complete the send of the message.  In that case, the usual 'MPI_Wait'
  logic should be used to complete the request.  This approach is used to
  allow a simple implementation of the ADI.  The ADI is free to always
  complete the message and never return a request.

  Module:
  Communication

  @*/
MPI_RESULT
MPID_Ssend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Recv

  Notes:
  The difference between this and 'MPI_Recv/MPI_Irecv' is that the basic
  error checks (e.g., valid communicator, datatype, source, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects, and a request is returned.

  @*/
MPI_RESULT
MPID_Recv(
    _Inout_opt_ void* buf,
    _In_range_(>=, 0)  int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Send_init - MPID entry point for MPI_Send_init

  Notes:
  The only difference between this and 'MPI_Send_init' is that the basic
  error checks (e.g., valid communicator, datatype, dest, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects.

  Module:
  Communication

  @*/
MPI_RESULT
MPID_Send_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

MPI_RESULT
MPID_Bsend_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Rsend_init - MPID entry point for MPI_Rsend_init

  Notes:
  The only difference between this and 'MPI_Rsend_init' is that the basic
  error checks (e.g., valid communicator, datatype, dest, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects, and a context id offset is provided in addition to the
  communicator.  This offset is added to the context of the communicator
  to get the context it used by the message.

  Module:
  Communication

  @*/
MPI_RESULT
MPID_Rsend_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );
/*@
  MPID_Ssend_init - MPID entry point for MPI_Ssend_init

  Notes:
  The only difference between this and 'MPI_Ssend_init' is that the basic
  error checks (e.g., valid communicator, datatype, dest, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects, and a context id offset is provided in addition to the
  communicator.  This offset is added to the context of the communicator
  to get the context it used by the message.

  Module:
  Communication

  @*/
MPI_RESULT
MPID_Ssend_init(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Recv_init - MPID entry point for MPI_Recv_init

  Notes:
  The only difference between this and 'MPI_Recv_init' is that the basic
  error checks (e.g., valid communicator, datatype, source, and tag)
  have been made, the MPI opaque objects have been replaced by
  MPID objects, and a context id offset is provided in addition to the
  communicator.  This offset is added to the context of the communicator
  to get the context it used by the message.

  Module:
  Communication

  @*/
MPI_RESULT
MPID_Recv_init(
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _In_ int rank,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Startall - MPID entry point for MPI_Startall

  Notes:
  The only difference between this and 'MPI_Startall' is that the basic
  error checks (e.g., count) have been made, and the MPI opaque objects
  have been replaced by pointers to MPID objects.

  Rationale:
  This allows the device to schedule communication involving multiple requests,
  whereas an implementation built on just 'MPID_Start' would force the
  ADI to initiate the communication in the order encountered.
  @*/
MPI_RESULT
MPID_Startall(
    _In_ int count,
    _Inout_updates_(count) MPID_Request* requests[]
    );

/*@
   MPID_Probe - Block until a matching request is found and return information
   about it

  Input Parameters:
+ source - rank to match (or 'MPI_ANY_SOURCE')
. tag - Tag to match (or 'MPI_ANY_TAG')
. comm - communicator to match.

  Output Parameter:
. status - 'MPI_Status' set as defined by 'MPI_Probe'


  Return Value:
  Error code.

  Notes:
  Note that the values returned in 'status' will be valid for a subsequent
  MPI receive operation only if no other thread attempts to receive the same
  message.
  (See the
  discussion of probe in Section 8.7.2 Clarifications of the MPI-2 standard.)

  The communicator is present to allow the device
  to use message-queues attached to particular communicators or connections
  between processes.

  Module:
  Request

  @*/
MPI_RESULT
MPID_Probe(
    _In_ int source,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Out_ MPI_Status* status
    );

/*@
   MPID_Iprobe - Look for a matching request in the receive queue
   but do not remove or return it

  Input Parameters:
+ source - rank to match (or 'MPI_ANY_SOURCE')
. tag - Tag to match (or 'MPI_ANY_TAG')
. comm - communicator to match.

  Output Parameter:
+ flag - true if a matching request was found, false otherwise.
- status - 'MPI_Status' set as defined by 'MPI_Iprobe' (only valid when return
  flag is true).

  Return Value:
  Error Code.

  Notes:
  Note that the values returned in 'status' will be valid for a subsequent
  MPI receive operation only if no other thread attempts to receive the same
  message.
  (See the
  discussion of probe in Section 8.7.2 (Clarifications) of the MPI-2 standard.)

  The communicator is present to allow the device
  to use message-queues attached to particular communicators or connections
  between processes.

  Devices that rely solely on polling to make progress should call
  MPID_Progress_pump() (or some equivalent function) if a matching request
  could not be found.  This insures that progress continues to be made even if
  the application is calling MPI_Iprobe() from within a loop not containing
  calls to any other MPI functions.

  Module:
  Request

  @*/
MPI_RESULT
MPID_Iprobe(
    _In_ int source,
    _In_ int tag,
    _In_ MPID_Comm* comm,
    _Out_ int* flag,
    _Out_ MPI_Status* status
    );

/*@
   MPID_Mprobe - Block until the matching request is found in the receive queue, 
   remove and return it when found.

   Input Parameters:
+  source - rank to match (or 'MPI_ANY_SOURCE')
.  tag - Tag to match (or 'MPI_ANY_TAG')
.  comm - communicator to match.

   Output Parameter:
+  message - The matched request.

  Return Value:
  Error Code.

  Module:
  Request

  @*/
MPI_RESULT
MPID_Mprobe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPID_Comm* comm,
    _Outptr_ MPID_Request** message
    );

/*@
   MPID_Improbe - Look for a matching request in the receive queue, 
   remove and return it when found.

   Input Parameters:
+  source - rank to match (or 'MPI_ANY_SOURCE')
.  tag - Tag to match (or 'MPI_ANY_TAG')
.  comm - communicator to match.

   Output Parameter:
+  message - The matched request.

  Return Value:
  Error Code.

  Module:
  Request

  @*/
MPI_RESULT
MPID_Improbe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ MPID_Comm* comm,
    _Out_ _Deref_out_range_(0, 1) int* flag,
    _Outptr_result_maybenull_ MPID_Request** message
    );

/*@
   MPID_Imrecv - Receive a message that is matched using MPI_Mprobe/MPI_Improbe.

  Return Value:
  Error Code.

  Module:
  Request

  @*/
MPI_RESULT
MPID_Imrecv(
    _Inout_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle datatype,
    _Inout_opt_ MPID_Request* message,
    _Outptr_ MPID_Request** request
    );

/*@
  MPID_Cancel_send - Cancel the indicated send request

  Input Parameter:
. request - Send request to cancel

  Return Value:
  MPI error code.

  Notes:
  Cancel is a tricky operation, particularly for sends.  Read the
  discussion in the MPI-1 and MPI-2 documents carefully.  This call
  only requests that the request be cancelled; a subsequent wait
  or test must first succeed (i.e., the request completion counter must be
  zeroed).

  Module:
  Request

  @*/
MPI_RESULT
MPID_Cancel_send(
    _In_ MPID_Request* sreq
    );

/*@
  MPID_Cancel_recv - Cancel the indicated recv request

  Input Parameter:
. request - Receive request to cancel

  Return Value:
  MPI error code.

  Notes:
  This cancels a pending receive request.  In many cases, this is implemented
  by simply removing the request from a pending receive request queue.
  However, some ADI implementations may maintain these queues in special
  places, such as within a NIC (Network Interface Card).
  This call only requests that the request be cancelled; a subsequent wait
  or test must first succeed (i.e., the request completion counter must be
  zeroed).

  Module:
  Request

  @*/
void
MPID_Cancel_recv(
    _In_ MPID_Request* rreq
    );


/* The following routines perform the callouts to the user routines registered
   as part of a generalized request.  They handle any language binding issues
   that are necessary. They are used when completing, freeing, cancelling or
   extracting the status from a generalized request. */
MPI_RESULT
MPIR_Grequest_cancel(
    _In_ MPID_Request* request
    );

MPI_RESULT
MPIR_Grequest_query(
    _In_ MPID_Request* request
    );

MPI_RESULT
MPIR_Grequest_free(
    _In_ MPID_Request* request
    );

MPI_RESULT
MPIR_Bsend_attach(
    _In_reads_bytes_(buffer_size) void *buffer,
    _In_ int buffer_size
    );

MPI_RESULT
MPIR_Bsend_detach(
    _Outptr_result_bytebuffer_(*size) void** bufferp,
    _Out_ int *size
    );

MPI_RESULT
MPIR_Bsend_isend(
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ TypeHandle dtype,
    _In_ int dest,
    _In_ int tag,
    _In_ MPID_Comm* comm_ptr,
    _Outptr_ MPID_Request** request
    );

int MPIR_Get_bsend_overhead();

#endif // PT2PT_H
