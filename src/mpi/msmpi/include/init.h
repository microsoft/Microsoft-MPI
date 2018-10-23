// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef INIT_H
#define INIT_H

#include "colltunersettings.h"
#include "collutil.h"
#include "kernel32util.h"

//
// MPI Initialization state
//
enum MSMPI_STATE
{
    MSMPI_STATE_UNINITIALIZED=0,
    MSMPI_STATE_INITIALIZING,
    MSMPI_STATE_INITIALIZED,
    MSMPI_STATE_FINALIZED
};


/* Bindings for internal routines */
/*@ MPIR_Add_finalize - Add a routine to be called when MPI_Finalize is invoked

+ routine - Routine to call
. extra   - Void pointer to data to pass to the routine
- priority - Indicates the priority of this callback and controls the order
  in which callbacks are executed.  Use a priority of zero for most handlers;
  higher priorities will be executed first.

Notes:
  The routine 'MPID_Finalize' is executed with priority
  'MPIR_FINALIZE_CALLBACK_PRIO' (currently defined as 5).  Handlers with
  a higher priority execute before 'MPID_Finalize' is called; those with
  a lower priority after 'MPID_Finalize' is called.
@*/
void MPIR_Add_finalize( int (*routine)( void * ), void *extra, unsigned int priority );

#define MPIR_FINALIZE_CALLBACK_PRIO 5
#define MPIR_FINALIZE_CALLBACK_MAX_PRIO 10


MPI_RESULT MPID_Get_processor_name(_Out_cap_(namelen) char * name, int namelen, int * resultlen);

void MPIR_Call_finalize_callbacks(_In_ unsigned int min_prio);


class MpiProcess
{
private:
    DWORD               MasterThread;
    volatile long       InitState;

    volatile long       DeferredRequestCounter;
    volatile long       ProgressCounter;
    volatile long       CurrentProgressThread;
    MPI_LOCK            CommLock;
    bool                CommLockInitialized;
public:
    int                 ThreadLevel;
    struct MPID_Comm*   CommWorld;
    struct MPID_Comm*   CommSelf;
    struct MPID_Comm*   CommParent;
    PreDefined_attrs    Attributes;            /* Predefined attribute values */
    HANDLE              Heap;

    CollectiveSwitchoverSettings    SwitchoverSettings;
    CollectiveTunerSettings         TunerSettings;

    Tls<MpiCallState>   CallState;

    bool                ForceAsyncWorkflow;

public:
    MpiProcess();
    ~MpiProcess();

public:

    inline bool IsMultiThreaded()
    {
        return ThreadLevel >= MPI_THREAD_MULTIPLE;
    }

    inline bool TestState(MSMPI_STATE state)
    {
        return InitState == state;
    }

    inline bool IsReady()
    {
        return InitState == MSMPI_STATE_INITIALIZED;
    }

    inline bool IsFinalized()
    {
        return InitState >= MSMPI_STATE_FINALIZED;
    }

    inline bool IsInitialized()
    {
        return InitState >= MSMPI_STATE_INITIALIZED;
    }

    inline bool IsCurrentThreadMaster()
    {
        return MasterThread == ::GetCurrentThreadId();
    }


    MPI_RESULT Initialize(
        _In_ int requested_level,
        _Out_opt_ int* provided_level
        );

    MPI_RESULT Finalize();
    void PostFinalize();

    void CompleteFinalize()
    {
        ::InterlockedExchange(&InitState, MSMPI_STATE_FINALIZED);
    }

    //
    // Progress functions
    //
    inline void IncrementDeferredRequestCounter()
    {
        InterlockedIncrement(&DeferredRequestCounter);
    }

    inline void DecrementDeferredRequestCounter()
    {
        InterlockedDecrement(&DeferredRequestCounter);
    }

    inline bool HasDeferredRequests()
    {
        return DeferredRequestCounter > 0;
    }

    inline long GetProgressCookie()
    {
        return ProgressCounter;
    }

    inline bool HasProgressCountChanged(long cookie)
    {
        return cookie != ProgressCounter;
    }

    inline void WakeProgress(bool signal = true)
    {
        ExPostCompletion(
            MPIDI_CH3I_set,
            EX_KEY_PROGRESS_WAKEUP,
            NULL,
            signal ? TRUE : FALSE
            );
    }


    inline void SignalProgress()
    {
        ::InterlockedIncrement(&ProgressCounter);

        //
        // If this signal is coming from outside the progress loop, then
        // we will signal the progress loop in MT mode to wake up if it
        // is waiting for a completion, else we will wake up the waiting threads
        // so they can make sure they check if it was their progress that was made
        //
        if (IsMultiThreaded())
        {
            if (IsCurrentThreadMakingProgress())
            {
                WakeWaitingThreads();
            }
            else
            {
                WakeProgress(false);
            }
        }
    }


    //
    // MT Progress Lock functions
    //

    inline bool IsCurrentThreadMakingProgress()
    {
        return (long)::GetCurrentThreadId() == CurrentProgressThread;
    }

    inline bool TryAcquireProgressLock()
    {
        MPIU_Assert(IsMultiThreaded());
        return 0 == ::InterlockedCompareExchange(
                        &CurrentProgressThread,
                        ::GetCurrentThreadId(),
                        0);
    }

    inline void ReleaseProgressLock()
    {
        MPIU_Assert(IsMultiThreaded());
        MPIU_Assert(IsCurrentThreadMakingProgress());

        ::InterlockedExchange(&CurrentProgressThread, 0);

        //
        //  When we release the lock, we must increment the completion counter
        //   to deal with the following race condition.
        //  Thread0 - Enter Progress loop
        //  Thread1 - Try Enter Progress loop, but fails
        //  Thread0 - Exit progress loop
        //  Thread1 - Enters waitfor progress loop
        //  Thread0 - Never calls back in
        //
        //  This situation causes a deadlock.  To deal with this, we increment
        //  the progress counter, release the progress lock, then signal all
        //  waiting threads.
        //
        ::InterlockedIncrement(&ProgressCounter);
        WakeWaitingThreads();
    }

    inline void WakeWaitingThreads()
    {
        MPIU_Assert(IsMultiThreaded());
        MPIU_Assert(g_IsWin8OrGreater);
        Kernel32::Methods.WakeByAddressAll((void*)&ProgressCounter);
    }

    inline MPI_RESULT WaitForProgress(long cookie, DWORD timeout = 0)
    {
        MPIU_Assert(IsMultiThreaded());
        MPIU_Assert(g_IsWin8OrGreater);

        MPI_RESULT result = MPI_SUCCESS;
        if (FALSE == Kernel32::Methods.WaitOnAddress(
                        &ProgressCounter,
                        &cookie,
                        sizeof(cookie),
                        timeout))
        {
            result = HRESULT_FROM_WIN32(GetLastError());
        }
        return result;
    }

    //
    // Comm Creation lock
    //
    _Acquires_lock_(this->CommLock)
    inline void AcquireCommLock()
    {
        MpiLockEnter(&CommLock);
    }

    _Releases_lock_(this->CommLock)
    inline void ReleaseCommLock()
    {
        MpiLockLeave(&CommLock);
    }

private:
    MPI_RESULT InitializeCore();
    MPI_RESULT ReadSmpEnvironmentSettings();
    void InitCollectiveSwitchPoints();
    MPI_RESULT TuneCollectives();

private:
    MpiProcess(const MpiProcess& other);
    MpiProcess& operator = (const MpiProcess& other);
};

extern MpiProcess Mpi;


#endif // INIT_H
