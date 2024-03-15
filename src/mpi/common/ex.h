// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef EX_H
#define EX_H


//----------------------------------------------------------------------------
//
// The Executive, a generic progress engine
//
// Overview:
// The generic progress engine, the Executive, implements a simple asynchronous
// processing model. In this model records indicating events get queued up for
// processing. The Executive invokes the appropriate registered handler for the
// event to take further action if required. The executive exits when a sequence
// of events, representing a complete logical function is complete.
//
// This model is a producers-consumers model with a single events queue. The
// events are being processed in the order they have been queued.
// *** temporary extended to support multiple queues until the clients can ***
// *** use a single event queue. (e.g., PMI client implementation)         ***
//
// The function ExProcessCompletions is the heart of the consumers model. It
// dequeues the events records and invokes the appropriate completion processor.
// Multiple threads can call this function for processing. The Executive will
// run as many concurrent threads as there are processors in the system. When
// concurrent threads are running, the completion processors should be able to
// handle the concurrency and out-of-order execution.
//
// The Executive pre-registers a completion processor for Key-Zero with a simple
// handler signature. see description below.
//
//----------------------------------------------------------------------------


//
// ExSetHandle_t  *** temp extenssion ***
//
// Represents the set object
//
typedef HANDLE ExSetHandle_t;
#define EX_INVALID_SET nullptr

//to get NTSTATUS
#include <winternl.h>


//
// ExCreateSet  *** temp extenssion ***
//
// Create the set object.
// An EX_INVALID_SET value is returned for an out of memory condition.
//
_Success_(return != nullptr)
_Ret_valid_
ExSetHandle_t
ExCreateSet(
    void
    );

//
// ExCloseSet *** temp extenssion ***
//
// Close the set object.
//
void
ExCloseSet(
    _In_ _Post_invalid_ ExSetHandle_t Set
    );

//
// ExCompletionProcessor, function prototype
//
// The completion processor function is called by ExProcessCompletions function
// to process a completion event. The completion processor indicates whether the
// sequence of asynchronous events is complete or whether further processing is
// required.
//
// Parameters:
//   BytesTransferred
//      A DWORD value posted to the completion port. Usually the number of bytes
//      transferred in this operation
//
//   pOverlapped
//      A pointer value posted to the completion port. Usually a pointer to the
//      OVERLAPPED structure
//
// Return Value:
//      MPI error value indicating the result of the "logical" async function.
//      This value is meaningful only when the completion processor returns TRUE.
//
typedef
_Success_(return==MPI_SUCCESS)
int
(WINAPI * ExCompletionProcessor)(
    DWORD BytesTransferred,
    PVOID pOverlapped
    );


//
// Key values used when calling ExRegisterCompletionProcessor.
//
enum EX_PROCESSOR_KEYS
{
    EX_KEY_WAIT_INTERRUPT = -1,
    EX_KEY_RESERVED = 0,
    EX_KEY_SHM_NOTIFY_CONNECTION,
    EX_KEY_SHM_NOTIFY_MESSAGE,
    EX_KEY_PROGRESS_WAKEUP,
    EX_KEY_ND,
    EX_KEY_CONNECT_REQUEST,
    EX_KEY_CONNECT_COMPLETE,
    EX_KEY_DEFER_CONNECT,
    EX_KEY_DEFER_WRITE,
    EX_KEY_MAX
};


//
// ExRegisterCompletionProcessor
//
// Resister a completion processor for a specific Key.
// N.B. Current implementation supports keys 0 - 3, where key 0 is reserved.
//
void
ExRegisterCompletionProcessor(
    _In_ EX_PROCESSOR_KEYS Key,
    _In_ ExCompletionProcessor pfnCompletionProcessor
    );


//
// ExUnregisterCompletionProcessor
//
// Remove a registered completion processor
//
void
ExUnregisterCompletionProcessor(
    _In_ EX_PROCESSOR_KEYS Key
    );


//
// ExPostCompletion
//
// Post an event completion to the completion queue. The appropriate
// completion processor will be invoked by ExProcessCompletions thread
// with the passed in parameters.
//
void
ExPostCompletion(
    _In_ ExSetHandle_t Set,
    _In_ EX_PROCESSOR_KEYS Key,
    _Inout_opt_ PVOID pOverlapped,
    _In_ DWORD BytesTransferred
    );


//
// ExGetPortValue
//
// Returns the value of the completion queue handle
//
ULONG
ExGetPortValue(
    _In_ ExSetHandle_t Set
    );


//
// ExProcessCompletions
//
// Process all completion event types by invoking the appropriate completion
// processor function. This routine continues to process all events until an
// asynchronous sequence is complete (function with several async stages).
// If the caller indicated "no blocking" this routine will exit when no more
// events to process are available, regardles of the completion processor
// indication to continue.
//
// Parameters:
//      timeout - Milliseconds to wait for an event sequence to complete
//      interruptible - flag indicating whether the wait can be interrupted
//
// Return Value:
//      The result of the asynchronous function last to complete.
//      Returns MPI_ERR_PENDING if the wait was interrupted.
//      Returns MPI_SUCCESS if the wait timed out.
//
_Success_(return==MPI_SUCCESS || return==MPI_ERR_PENDING)
int
ExProcessCompletions(
    _In_ ExSetHandle_t set,
    _In_ DWORD timeout,
    _In_opt_ bool interruptible = false
    );


//
// ExInitialize
//
// Initialize the completion queue. This function can only be called once before
// before Finialize.
//
_Success_(return==MPI_SUCCESS)
int
ExInitialize(
    void
    );


//
// ExFinitialize
//
// Close the completion queue progress engine. This function can only be called
// once.
//
void
ExFinalize(
    void
    );


//----------------------------------------------------------------------------
//
// The Executive Key-Zero completion processor
//
// Overview:
// The Key-Zero completion processor enables the users of the Executive to
// associate a different completion routine with each operation rather than
// use a single completion processor per handle. The Key-Zero processor works
// with user supplied OVERLAPPED structure. It cannot work with system generated
// completion events (e.g., Job Object enets), or other external events.
//
// The Key-Zero processor uses the EXOVERLAPPED data structure which embeds the
// user success and failure completion routines. When the Key-Zero completion
// processor is invoked it calls the user success or failure routine base on
// the result of the async operation.
//
//----------------------------------------------------------------------------

//
// ExCompletionRoutine function prototype
//
// The ExCompletionRoutine callback routine is invoked by the built-in Key-Zero
// completion processor. The information required for processing the event is
// packed with the EXOVERLAPED structure and can be accessed with the Ex utility
// functions. The callback routine returns the logical error value.
//
// Parameters:
//   pOverlapped
//      A pointer to an EXOVERLAPPED structure associated with the completed
//      operation. This pointer is used to extract the caller context with the
//      CONTAINING_RECORD macro.
//
// Return Value:
//      MPI error value indicating the result of the "logical" async function.
//
struct EXOVERLAPPED;

typedef
_Success_(return==MPI_SUCCESS)
int
(WINAPI * ExCompletionRoutine)(
    _Inout_ struct EXOVERLAPPED* pOverlapped
    );


//
// struct EXOVERLAPPED
//
// The data structure used for Key-Zero completions processing. The pfnSuccess
// and pfnFailure are set by the caller before calling an async operation.
// The pfnSuccess is called if the async operation was successful.
// The pfnFailure is called if the async operation was unsuccessful.
//
struct EXOVERLAPPED
{

    OVERLAPPED ov;
    ExCompletionRoutine pfnSuccess;
    ExCompletionRoutine pfnFailure;
};


//
// ExInitOverlapped
//
// Initialize the success & failure callback function fields
// Rest the hEvent field of the OVERLAPPED, make it ready for use with the OS
// overlapped API's.
//
static
inline
void
ExInitOverlapped(
    _Inout_ EXOVERLAPPED* pOverlapped,
    _In_ ExCompletionRoutine pfnSuccess,
    _In_ ExCompletionRoutine pfnFailure
    )
{
    pOverlapped->ov.hEvent = nullptr;
    pOverlapped->pfnSuccess = pfnSuccess;
    pOverlapped->pfnFailure = pfnFailure;
}


//
// ExPostOverlapped
//
// Post an EXOVERLAPPED completion to the completion queue to be invoked by
// ExProcessCompletions.
//
void
ExPostOverlapped(
    _In_ ExSetHandle_t Set,
    _Inout_ EXOVERLAPPED* pOverlapped
    );


//
// ExPostOverlappedResult
//
// Post an EXOVERLAPPED completion to the completion queue to be invoked by
// ExProcessCompletions. Set the status and bytes transferred count.
//
static
inline
void
ExPostOverlappedResult(
    _In_ ExSetHandle_t Set,
    _Inout_ EXOVERLAPPED* pOverlapped,
    _In_ HRESULT Status,
    _In_ DWORD BytesTransferred
    )
{
    pOverlapped->ov.Internal = Status;
    pOverlapped->ov.InternalHigh = BytesTransferred;
    ExPostOverlapped(Set, pOverlapped);
}


//
// ExAttachHandle
//
// Associate an OS handle with the Executive completion queue. All asynchronous
// operations using the attached handle are processed with the completion
// processor associated with the provided Key.  If no key value is specified,
// the Key-Zero completion processor is used.  Key-Zero completion processor
// requires the use of EXOVERLAPPED data structure when calling an asynchronous
// operation with that Handle.
//
void
ExAttachHandle(
    _In_ ExSetHandle_t Set,
    _In_ HANDLE Handle,
    _In_opt_ EX_PROCESSOR_KEYS Key = EX_KEY_RESERVED
    );


//
// ExGetBytesTransferred
//
// Get the number of bytes transferred from the overlapped structure
//
static
inline
DWORD
ExGetBytesTransferred(
    _In_ const EXOVERLAPPED* pOverlapped
    )
{
    return (DWORD)pOverlapped->ov.InternalHigh;
}


//
// ExGetStatus
//
// Get the status return value from the overlapped structure
//
static
inline
NTSTATUS
ExGetStatus(
    _In_ const EXOVERLAPPED* pOverlapped
    )
{
    return (NTSTATUS)pOverlapped->ov.Internal;
}


//
// ExCallSuccess
//
// Set the completion status and bytes transferred and execute the EXOVERLAPPED
// Success completion routine
//
static
inline
int
ExCallSuccess(
    _Inout_ EXOVERLAPPED* pOverlapped,
    _In_ HRESULT Status,
    _In_ DWORD BytesTransferred
    )
{
    pOverlapped->ov.Internal = Status;
    pOverlapped->ov.InternalHigh = BytesTransferred;
    return pOverlapped->pfnSuccess(pOverlapped);
}


//
// ExCallFailure
//
// Set the completion status and bytes transferred and execute the EXOVERLAPPED
// Failure completion routine
//
static
inline
int
ExCallFailure(
    _Inout_ EXOVERLAPPED* pOverlapped,
    _In_ HRESULT Status,
    _In_ DWORD BytesTransferred
    )
{
    pOverlapped->ov.Internal = Status;
    pOverlapped->ov.InternalHigh = BytesTransferred;
    return pOverlapped->pfnFailure(pOverlapped);
}


//
// ExCompleteOverlapped
//
// Execute the EXOVERLAPPED success or failure completion routine based
// on the overlapped status value.
//
static
inline
int
ExCompleteOverlapped(
    _Inout_ EXOVERLAPPED* pOverlapped
    )
{
    if(SUCCEEDED(ExGetStatus(pOverlapped)))
        return pOverlapped->pfnSuccess(pOverlapped);

    return pOverlapped->pfnFailure(pOverlapped);
}

#endif /* EX_H */
