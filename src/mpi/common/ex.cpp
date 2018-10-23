// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */

#include "precomp.h"
#include "ex.h"
#include <oacr.h>

_Success_(return==NO_ERROR)
static
int
WINAPI
ExpKeyZeroCompletionProcessor(
    _In_opt_ DWORD BytesTransfered,
    _In_ PVOID pOverlapped
    );


#ifdef EXSINGLETONE
//
// The only completion port, supporting all events
//
static HANDLE s_port;
#endif

//
// The registered processors. This module supports up to 4 processors where
// processor zero is pre-registered for overlapped operations completion.
// the completion processor is registered by the completion key.
//
static ExCompletionProcessor s_processors[EX_KEY_MAX] = {

    ExpKeyZeroCompletionProcessor,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};


static inline BOOL
IsValidSet(
    _In_ ExSetHandle_t Set
    )
{
    return (Set != nullptr && Set != INVALID_HANDLE_VALUE);
}

_Success_(return != nullptr)
_Ret_valid_
ExSetHandle_t
ExCreateSet(
    void
    )
{
    ExSetHandle_t Set;
    Set = CreateIoCompletionPort(
            INVALID_HANDLE_VALUE,   // FileHandle
            nullptr,   // ExistingCompletionPort
            0,      // CompletionKey
            MAXDWORD       // NumberOfConcurrentThreads
            );

    return Set;
}


void
ExCloseSet(
    _In_ _Post_invalid_ ExSetHandle_t Set
    )
{
    MPIU_Assert(IsValidSet(Set));
    CloseHandle(Set);
}


void
ExRegisterCompletionProcessor(
    _In_ EX_PROCESSOR_KEYS Key,
    _In_ ExCompletionProcessor pfnCompletionProcessor
    )
{
    MPIU_Assert(Key > 0);
    MPIU_Assert(Key < RTL_NUMBER_OF(s_processors));
    MPIU_Assert(s_processors[Key] == nullptr);
    s_processors[Key] = pfnCompletionProcessor;
}


void
ExUnregisterCompletionProcessor(
    _In_ EX_PROCESSOR_KEYS Key
    )
{
    MPIU_Assert(Key > 0);
    MPIU_Assert(Key < RTL_NUMBER_OF(s_processors));
    MPIU_Assert(s_processors[Key] != nullptr);
    s_processors[Key] = nullptr;
}


void
ExPostCompletion(
    _In_ ExSetHandle_t Set,
    _In_ EX_PROCESSOR_KEYS Key,
    _Inout_opt_ PVOID pOverlapped,
    _In_ DWORD BytesTransfered
    )
{
    MPIU_Assert(IsValidSet(Set));

    for(;;)
    {
        if(PostQueuedCompletionStatus(Set, BytesTransfered, Key, (OVERLAPPED*)pOverlapped))
            return;

        MPIU_Assert(GetLastError() == ERROR_NO_SYSTEM_RESOURCES);
        Sleep(10);
    }
}


ULONG
ExGetPortValue(
    _In_ ExSetHandle_t Set
    )
{
    MPIU_Assert(IsValidSet(Set));
    return HandleToUlong(Set);
}

_Success_(return==MPI_SUCCESS)
int
ExInitialize(
    void
    )
{
#ifdef EXSINGLETONE
    MPIU_Assert(s_port == nullptr);
    s_port = CreateIoCompletionPort(
                INVALID_HANDLE_VALUE,   // FileHandle
                nullptr,   // ExistingCompletionPort
                0,      // CompletionKey
                0       // NumberOfConcurrentThreads
                );

    if(s_port != nullptr)
        return MPI_SUCCESS;

    return MPI_ERR_INTERN;
#endif

    return MPI_SUCCESS;
}


void
ExFinalize(
    void
    )
{
#ifdef EXSINGLETONE
    MPIU_Assert(s_port != nullptr);
    CloseHandle(s_port);
    s_port = nullptr;
#endif
}

_Success_(return==MPI_SUCCESS || return==MPI_ERR_PENDING)
int
ExProcessCompletions(
    _In_ ExSetHandle_t set,
    _In_ DWORD timeout,
    _In_opt_ bool interruptible
    )
{
    MPIU_Assert(IsValidSet(set));

    BOOL fSucc;
    DWORD bytesTransfered;
    ULONG_PTR key;
    OVERLAPPED* pOverlapped;
    for (;;)
    {
        fSucc = GetQueuedCompletionStatus(
                    set,
                    &bytesTransfered,
                    &key,
                    &pOverlapped,
                    timeout
                    );

        if(!fSucc && pOverlapped == nullptr)
        {
            //
            // Return success on timeout per caller request. The Executive progress
            // engine will not wait for the async processing to complete
            //
            DWORD gle = GetLastError();
            if (gle == WAIT_TIMEOUT)
            {
                if( timeout == 0 )
                {
                    return MPI_SUCCESS;
                }
                return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
            }

            //
            // Io Completion port internal error, try again
            //
            continue;
        }

        if( key == EX_KEY_WAIT_INTERRUPT )
        {
            if( interruptible == false )
            {
                continue;
            }
            return MPI_ERR_PENDING;
        }

        MPIU_Assert(key < RTL_NUMBER_OF(s_processors));
        MPIU_Assert(s_processors[key] != nullptr);

        //
        // Call the completion processor and return the result.
        //
        return s_processors[key](bytesTransfered, pOverlapped);
    }
}


//----------------------------------------------------------------------------
//
// Preregistered completion processor for Key-Zero
//

_Success_(return==NO_ERROR)
static
int
WINAPI
ExpKeyZeroCompletionProcessor(
    _In_opt_ DWORD /*BytesTransfered*/,
    _In_ PVOID pOverlapped
    )
{
    EXOVERLAPPED* pov = CONTAINING_RECORD(pOverlapped, EXOVERLAPPED, ov);
    return ExCompleteOverlapped(pov);
}


void
ExPostOverlapped(
    _In_ ExSetHandle_t Set,
    _Inout_ EXOVERLAPPED* pOverlapped
    )
{
    MPIU_Assert(IsValidSet(Set));

    ExPostCompletion(
        Set,
        EX_KEY_RESERVED, // Key,
        &pOverlapped->ov,
        0 // BytesTransfered
        );
}


void
ExAttachHandle(
    _In_ ExSetHandle_t Set,
    _In_ HANDLE Handle,
    _In_opt_ EX_PROCESSOR_KEYS Key
    )
{
    MPIU_Assert(IsValidSet(Set));
    MPIU_Assert(s_processors[Key] != nullptr);

    for(;;)
    {
        HANDLE hPort;
        hPort = CreateIoCompletionPort(
                    Handle, // FileHandle
                    Set,    // ExistingCompletionPort
                    Key,    // CompletionKey
                    0       // NumberOfConcurrentThreads
                    );

        if(hPort != nullptr)
        {
            MPIU_Assert(hPort == Set);
            return;
        }

        MPIU_Assert(GetLastError() == ERROR_NO_SYSTEM_RESOURCES);
        Sleep(10);
    }
}
