// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "MpiLock.h"
//
// This file defines function pointers to the core Win32 lock functions
//  that are used by the MSMPI stack.  We store these as function pointers
//  so that we can replace them with "NOP" functions when we run in single
//  threaded mode.
//



//
// Prototype functions for the platform locking functions we wrap.
//

typedef void WINAPI
FN_InitializeCriticalSection(
    _Out_ LPCRITICAL_SECTION lpCriticalSection
    );

typedef void WINAPI
FN_DeleteCriticalSection(
    _Inout_ LPCRITICAL_SECTION lpCriticalSection
    );

typedef void WINAPI
FN_EnterCriticalSection(
    _Inout_ LPCRITICAL_SECTION CriticalSection
    );

typedef void WINAPI
FN_LeaveCriticalSection(
    _Inout_ LPCRITICAL_SECTION CriticalSection
    );

typedef VOID WINAPI
FN_InitializeSRWLock(
    _Out_ PSRWLOCK SRWLock
    );

typedef VOID WINAPI
FN_AcquireSRWLockExclusive(
    _Inout_ PSRWLOCK SRWLock
    );

typedef VOID WINAPI
FN_AcquireSRWLockShared(
    _Inout_ PSRWLOCK SRWLock
    );

typedef VOID WINAPI
FN_ReleaseSRWLockExclusive(
    _Inout_ PSRWLOCK SRWLock
    );

typedef VOID WINAPI
FN_ReleaseSRWLockShared(
    _Inout_ PSRWLOCK SRWLock
    );


#ifdef __cplusplus
extern "C" {
#endif

//
// Summary:
//  Empty lock function for SingleThread mode
//
static void WINAPI
MpiLockNoThread(
    _Inout_ LPCRITICAL_SECTION
    )
{
}


//
// Summary:
//  Empty read/writer lock function for SingleThread mode
//
static VOID WINAPI
MpiRwLockNoThread(
    _Inout_ PSRWLOCK
    )
{
}


//
// Global list of all Locking functions MPI will use in
//  both ST and MT scenarios (IE, Shared code paths).
//
static struct _MpiLockFunctions
{

    FN_InitializeCriticalSection*   LockInitialize;
    FN_DeleteCriticalSection*       LockDelete;
    FN_EnterCriticalSection*        LockEnter;
    FN_LeaveCriticalSection*        LockLeave;


    FN_InitializeSRWLock*           RwLockInitialize;
    FN_AcquireSRWLockExclusive*     RwLockAcquireExclusive;
    FN_ReleaseSRWLockExclusive*     RwLockReleaseExclusive;

    FN_AcquireSRWLockShared*        RwLockAcquireShared;
    FN_ReleaseSRWLockShared*        RwLockReleaseShared;

} MpiLockFunctions =
//
// NOTE: We set the default locking behavior to be ON
//  this means that all locks function as expected by default
//  and can be disabled with a call to MpiLockInitializeSingleThreadMode
//
{
    InitializeCriticalSection,
    DeleteCriticalSection,
    EnterCriticalSection,
    LeaveCriticalSection,
    InitializeSRWLock,
    AcquireSRWLockExclusive,
    ReleaseSRWLockExclusive,
    AcquireSRWLockShared,
    ReleaseSRWLockShared
};


//
// Summary:
//  Initialize the local process to skip all locking functions.
//
VOID
MpiLockInitializeSingleThreadMode()
{
    MpiLockFunctions.LockInitialize         = MpiLockNoThread;
    MpiLockFunctions.LockDelete             = MpiLockNoThread;
    MpiLockFunctions.LockEnter              = MpiLockNoThread;
    MpiLockFunctions.LockLeave              = MpiLockNoThread;
    MpiLockFunctions.RwLockInitialize       = MpiRwLockNoThread;
    MpiLockFunctions.RwLockAcquireExclusive = MpiRwLockNoThread;
    MpiLockFunctions.RwLockReleaseExclusive = MpiRwLockNoThread;
    MpiLockFunctions.RwLockAcquireShared    = MpiRwLockNoThread;
    MpiLockFunctions.RwLockReleaseShared    = MpiRwLockNoThread;
}


//
// Summary:
//  Initializes a MPI_LOCK (CRITICAL_SECTION)
//
VOID
MpiLockInitialize(
    _Out_ MPI_LOCK*  Lock
    )
{
    MpiLockFunctions.LockInitialize(Lock);
}


//
// Summary:
//  Deletes a MPI_LOCK (CRITICAL_SECTION)
//
VOID
MpiLockDelete(
    _Inout_ MPI_LOCK*  Lock
    )
{
    MpiLockFunctions.LockDelete(Lock);
}


//
// Summary:
//  Enters a MPI_LOCK (CRITICAL_SECTION)
//
VOID
MpiLockEnter(
    _Inout_ MPI_LOCK*  Lock
    )
{
    MpiLockFunctions.LockEnter(Lock);
}


//
// Summary:
//  Leaves a MPI_LOCK (CRITICAL_SECTION)
//
VOID
MpiLockLeave(
    _Inout_ MPI_LOCK*  Lock
    )
{
    MpiLockFunctions.LockLeave(Lock);
}


//
// Summary:
//  Initialize a MPI_RWLOCK (SRWLOCK)
//
VOID
MpiRwLockInitialize(
    _Out_ MPI_RWLOCK* RwLock
    )
{
    MpiLockFunctions.RwLockInitialize(RwLock);
}


//
// Summary:
//  Acquire Exclusive access for a MPI_RWLOCK (SRWLOCK)
//
VOID
MpiRwLockAcquireExclusive(
    _Inout_ MPI_RWLOCK* RwLock
    )
{
    MpiLockFunctions.RwLockAcquireExclusive(RwLock);
}


//
// Summary:
//  Release Exclusive access for a MPI_RWLOCK (SRWLOCK)
//
VOID
MpiRwLockReleaseExclusive(
    _Inout_ MPI_RWLOCK* RwLock
    )
{
    MpiLockFunctions.RwLockReleaseExclusive(RwLock);
}


//
// Summary:
//  Acquire Shared access for a MPI_RWLOCK (SRWLOCK)
//
VOID
MpiRwLockAcquireShared(
    _Inout_ MPI_RWLOCK* RwLock
    )
{
    MpiLockFunctions.RwLockAcquireShared(RwLock);
}


//
// Summary:
//  Release Shared access for a MPI_RWLOCK (SRWLOCK)
//
VOID
MpiRwLockReleaseShared(
    _Inout_ MPI_RWLOCK* RwLock
    )
{
    MpiLockFunctions.RwLockReleaseShared(RwLock);
}


#ifdef __cplusplus
} //extern "C" {
#endif
