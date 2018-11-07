// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

typedef SRWLOCK             MPI_RWLOCK;
typedef CRITICAL_SECTION    MPI_LOCK;

#ifdef __cplusplus
extern "C" {
#endif

VOID
MpiLockInitializeSingleThreadMode();

VOID
MpiLockInitialize(
    _Out_ MPI_LOCK*   Lock
    );

VOID
MpiLockDelete(
    _Inout_ MPI_LOCK*     Lock
    );

VOID
MpiLockEnter(
    _Inout_ MPI_LOCK*  Lock
    );

VOID
MpiLockLeave(
    _Inout_ MPI_LOCK*  Lock
    );

#define MPI_RWLOCK_INIT     SRWLOCK_INIT

VOID
MpiRwLockInitialize(
    _Out_ MPI_RWLOCK*   RwLock
    );

VOID
MpiRwLockAcquireExclusive(
    _Inout_ MPI_RWLOCK* RwLock
    );

VOID
MpiRwLockReleaseExclusive(
    _Inout_ MPI_RWLOCK* RwLock
    );

VOID
MpiRwLockAcquireShared(
    _Inout_ MPI_RWLOCK* RwLock
    );

VOID
MpiRwLockReleaseShared(
    _Inout_ MPI_RWLOCK* RwLock
    );


#ifdef __cplusplus
} //extern "C" {
#endif