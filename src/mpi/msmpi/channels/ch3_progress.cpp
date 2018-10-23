// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"
#include <oacr.h>

/*
 * This file contains the progress routine implementation
 */

ExSetHandle_t MPIDI_CH3I_set = EX_INVALID_SET;

extern volatile LONG s_LockQueueDepth;

//
// The min value the adaptive spin will spin down to
//
#ifndef MPIDI_CH3I_SPIN_COUNT_MIN
#define MPIDI_CH3I_SPIN_COUNT_MIN 0x40
#endif

//
// The max value the adaptive spin will spin up to
//
#ifndef MPIDI_CH3I_SPIN_COUNT_MAX
#define MPIDI_CH3I_SPIN_COUNT_MAX 0x10000
#endif

//
// The ticks for performing slow operations while spining
//
#ifndef MPIDI_CH3I_SPIN_COUNT_SLOW_TICK
#define MPIDI_CH3I_SPIN_COUNT_SLOW_TICK  128
#endif


//
// Spin count management functions
//

static unsigned int s_progress_spin_max = MPIDI_CH3I_SPIN_COUNT_MIN;

void MPIDI_CH3I_Progress_spin_up(void)
{
    MPIU_Assert(Mpi.IsCurrentThreadMakingProgress() || false == Mpi.IsMultiThreaded());

    if(MPIDI_CH3I_Process.progress_fixed_spin)
        return;

    if(s_progress_spin_max < MPIDI_CH3I_SPIN_COUNT_MAX/2)
    {
        s_progress_spin_max *= 2;
    }
}


void MPIDI_CH3I_Progress_spin_down(void)
{
    MPIU_Assert(Mpi.IsCurrentThreadMakingProgress() || false == Mpi.IsMultiThreaded());

    if(MPIDI_CH3I_Process.progress_fixed_spin)
    {
        s_progress_spin_max = MPIDI_CH3I_Process.progress_fixed_spin;
        return;
    }

    //
    // Decay the max spin count by a quater
    //
    s_progress_spin_max = (s_progress_spin_max + MPIDI_CH3I_SPIN_COUNT_MIN)/2;
}


static inline BOOL spin_done(unsigned int spin)
{
    return ((spin % s_progress_spin_max) == 0);
}


static inline BOOL spin_slow_tick(unsigned int spin)
{
    return ((spin % MPIDI_CH3I_SPIN_COUNT_SLOW_TICK) == MPIDI_CH3I_SPIN_COUNT_SLOW_TICK - 1);
}


static int MPIDI_CH3I_Progress_shm(BOOL* pfProgress)
{
    int rc;

    MPIU_Assert(Mpi.IsCurrentThreadMakingProgress() || false == Mpi.IsMultiThreaded());

    if (MPIDI_CH3I_Process.shm_reading_list != NULL)
    {
        rc = MPIDI_CH3I_SHM_read_progress(MPIDI_CH3I_Process.shm_reading_list, pfProgress);
        ON_ERROR_FAIL(rc);
    }

    if (MPIDI_CH3I_Process.shm_writing_list != NULL)
    {
        rc = MPIDI_CH3I_SHM_write_progress(MPIDI_CH3I_Process.shm_writing_list, pfProgress);
        ON_ERROR_FAIL(rc);
    }

    return MPI_SUCCESS;

fn_fail:
    return rc;
}


int MPIDI_CH3I_Accept_shm_message(DWORD /*d1*/, PVOID /*p1*/)
{
    MPIU_Assert(Mpi.IsCurrentThreadMakingProgress() || false == Mpi.IsMultiThreaded());

    MPIDI_CH3I_Progress_spin_up();
    return MPI_SUCCESS;
}


//
// enable_shm_notifications
//
// Enable notification on shm send/recv. Walk the vc's lists and update the notify_index.
// Check whether the other side update their index; in that case a notification migh be missed, and
// enable is not posible. (the progress engine will retry recv/send in that case)
//
static BOOL enable_shm_notifications()
{
    MPIDI_VC_t* vc;
    MPIDI_CH3I_SHM_Queue_t* shm;

    MPIU_Assert(Mpi.IsCurrentThreadMakingProgress() || false == Mpi.IsMultiThreaded());

    for(vc = MPIDI_CH3I_Process.shm_reading_list; vc != NULL; vc = vc->ch.shm.recv.next_vc)
    {
        //
        // Make sure that the receiver notify_index is visible to the sender process before reading the
        // sender index. The sender first updates its index before reading the receiver notify_index.
        // Which guarantee that this code detects any skipped send notification.
        //
        // Check shm_is_full() for correctness; opt to spin more by checking !shm_is_empty().
        //
        shm = vc->ch.shm.recv.shm;
        shm->recv.notify_index++;
        MPID_READ_WRITE_BARRIER();
        if(!shm_is_empty(shm))
            goto fn_spinup;
    }

    for(vc = MPIDI_CH3I_Process.shm_writing_list; vc != NULL; vc = vc->ch.shm.send.next_vc)
    {
        if (MPIDI_CH3I_SendQ_empty(vc))
            continue;

        //
        // Make sure that the sender notify_index is visible to the receiver process before reading the
        // receiver index. The receiver first updates its index before reading the sender notify_index.
        // Which guarantee that this code detects any skipped receive notification.
        //
        // Check shm_is_empty() for correctness; opt to spin more for non rma by checking !shm_is_full().
        //
        shm = vc->ch.shm.send.shm;
        shm->send.notify_index++;
        MPID_READ_WRITE_BARRIER();

        if(shm_is_empty(shm))
            goto fn_spinup;

        if(!vc->ch.shm.send.wait_for_rma && !shm_is_full(shm))
            goto fn_spinup;
    }

    return TRUE;

fn_spinup:
    MPIDI_CH3I_Progress_spin_up();
    return FALSE;
}


static int MPIDI_CH3I_Progress_until(DWORD completions, DWORD timeout, bool interruptible)
{
    int mpi_errno = MPI_SUCCESS;
    MPIU_Assert(Mpi.IsCurrentThreadMakingProgress() || false == Mpi.IsMultiThreaded());

    do
    {
        unsigned int spin = 0;

        for( ;; )
        {
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }


            BOOL fProgress = FALSE;
            mpi_errno = MPIDI_CH3I_Progress_shm(&fProgress);
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }

            if(Mpi.HasProgressCountChanged(completions))
            {
                return mpi_errno;
            }

            if (Mpi.HasDeferredRequests())
            {
                //
                // If there are deferred connections/writes that were posted
                //  we will pick them up here before we perform normal ND progress.
                //  With ND, a recv handled by the progress engine could also process
                //  send request that was queued as deferred.  If this occurs, a reference
                //  to request could be held indefinately until the progress engine actually
                //  hits the completion port.
                //
                mpi_errno = ExProcessCompletions(MPIDI_CH3I_set, 0);
                if(mpi_errno != MPI_SUCCESS)
                {
                    return mpi_errno;
                }

                if(Mpi.HasProgressCountChanged(completions))
                {
                    return mpi_errno;
                }
            }

            mpi_errno = MPIDI_CH3I_Nd_progress(&fProgress);
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }

            if(Mpi.HasProgressCountChanged(completions))
            {
                return mpi_errno;
            }

            if( timeout == 0 )
            {
                break;
            }

            if( interruptible == true && s_LockQueueDepth > LONG_MIN + 1)
            {
                return MPI_ERR_PENDING;
            }

            if(spin_slow_tick(spin))
            {
                //
                // Test the IOCP for completions so we make progress on sockets
                // and connection establishment.
                //
                mpi_errno = ExProcessCompletions(MPIDI_CH3I_set, 0);
                if(mpi_errno != MPI_SUCCESS)
                {
                    return mpi_errno;
                }

                if(Mpi.HasProgressCountChanged(completions))
                {
                    return mpi_errno;
                }
            }

            if(fProgress)
            {
                spin = 1;
                continue;
            }

            if( !spin_done(++spin) )
            {
                continue;
            }

            if( !enable_shm_notifications() )
            {
                //
                //TODO: If we get here, then spin_done returned true, but
                // we're going to loop.  We incremented spin, though, so the
                // next time through the loop spin_done will return false.
                // Is that by design?
                //
                continue;
            }

            mpi_errno = MPIDI_CH3I_Nd_enable_notification(&fProgress);
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
            if(fProgress == TRUE)
            {
                //
                // We found an ND completion - no need to wait for the CQ Notify
                // completion, just go back to polling.
                //
                spin = 1;
                continue;
            }
            break;
        }

        if(timeout != 0)
        {
            MPIDI_CH3I_Progress_spin_down();
        }

        mpi_errno = ExProcessCompletions(MPIDI_CH3I_set, timeout, interruptible);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

    } while (timeout != 0 && false == Mpi.HasProgressCountChanged(completions));

    return mpi_errno;
}


int MPIDI_CH3I_Progress(DWORD timeout, bool interruptible)
{
    MPI_RESULT mpi_errno;
    long cookie = Mpi.GetProgressCookie();


    //
    // For ST cases, we simply invoke the progress loop directly.
    //

    if (false == Mpi.IsMultiThreaded())
    {
        return MPIDI_CH3I_Progress_until(cookie, timeout, interruptible);
    }

    // For MT cases, the wait system is not interuptible
    //

    if (interruptible)
    {
        return MPIU_ERR_CREATE(MPI_ERR_INTERN, "**interuptiblenotsupported");
    }

    do
    {
        //
        // We can get nested calls when routines like MPIU_Sleep are used.
        //
        // NOTE: the ProgressLock is not recursive, so we must test to
        //  see if we already hold the progress lock before trying to take it.
        //
        bool nestedCall = Mpi.IsCurrentThreadMakingProgress();
        if (nestedCall || Mpi.TryAcquireProgressLock())
        {
            mpi_errno = MPIDI_CH3I_Progress_until(cookie, timeout, false);

            if (nestedCall == false)
            {
                Mpi.ReleaseProgressLock();
            }
            return mpi_errno;
        }

        //
        // we didn't take the progress lock, so another thread is currently
        //  pumping progress.
        //
        MPIU_Assert(false == Mpi.IsCurrentThreadMakingProgress());

        OACR_WARNING_DISABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from HRESULT to MPI_RESULT OK.");
        mpi_errno = Mpi.WaitForProgress(cookie, timeout);
        if (MPI_SUCCESS != mpi_errno)
        {
            if (timeout == 0 && mpi_errno == HRESULT_FROM_WIN32(ERROR_TIMEOUT))
            {
                mpi_errno = MPI_SUCCESS;
            }
            return mpi_errno;
        }
        OACR_WARNING_ENABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from HRESULT to MPI_RESULT OK.");

        //
        // We can loop here if the thread cookie still matches,
        //  meaning no completions have occurred yet.
        //
    } while(timeout != 0 && false == Mpi.HasProgressCountChanged(cookie));

    return mpi_errno;
}


OACR_WARNING_DISABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from HRESULT to MPI_RESULT OK.");
int MPIU_Sleep( DWORD timeout )
{
    ULONGLONG timeout_time = GetTickCount64() + timeout;

    for( ULONGLONG current_time = GetTickCount64();
        current_time < timeout_time;
        current_time = GetTickCount64() )
    {
        int mpi_errno = MPIDI_CH3I_Progress(
            static_cast<DWORD>(timeout_time - current_time),
            false
            );
        if( mpi_errno == HRESULT_FROM_WIN32(WAIT_TIMEOUT) )
        {
            return MPI_SUCCESS;
        }
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    return MPI_SUCCESS;
}
OACR_WARNING_ENABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from HRESULT to MPI_RESULT OK.");



int WINAPI progress_wakeup( DWORD enableSignal, void* )
{
    if (enableSignal != FALSE)
    {
        Mpi.SignalProgress();
    }
    return MPI_SUCCESS;
}


//
// Structure used to hold the information required to perform
//  an action later on inside the progress loop, rather than inline.
//
struct DeferRequest
{
    struct MPIDI_VC_t*   Vc;
    struct MPID_Request* Request;

public:
    DeferRequest(_In_ MPIDI_VC_t* vc, _In_ MPID_Request* sreq)
        :  Vc(vc)
        , Request(sreq)
    {
        Request->AddRef();

        //
        // We don't need to reference the VC.  The VCT owns the reference to
        //  the VC, which is referenced by the COMM, which is referenced by the
        //  Request.
        //
    }

    ~DeferRequest()
    {
        Request->Release();
    }

private:
    DeferRequest( const DeferRequest& other);
    DeferRequest& operator =( const DeferRequest& other);
};


//
// Callback Routine for processing deferred connect operations.
//  This is invoked during progress loop in response to a call to
//  MPIDI_CH3I_DeferConnect.
//
static int WINAPI
progress_defer_write(
    _In_ DWORD,
    _In_ VOID* pOverlapped
    )
{
    StackGuardPointer<DeferRequest> defer(static_cast<DeferRequest*>(pOverlapped));
    MPIDI_VC_t* vc = defer->Vc;
    int mpi_errno = MPI_SUCCESS;
    {
        //
        // This is under a new scope to ensure that the lock is release
        //  before we wakeup other threads as the thread may need this lock.
        //
        SendQLock lock(vc);

        //
        // because ND channel can be processed in the bg, a close could have occured
        //  before this completion is action handled.  So only if the vc->state is still
        //  still valid, we will dispatch pending sends on the VC
        //
        if (vc->state < MPIDI_VC_STATE_CLOSE_ACKED)
        {
            switch( vc->ch.channel )
            {
            case MPIDI_CH3I_CH_TYPE_ND:
                mpi_errno = MPIDI_CH3I_Nd_write_progress(vc);
                break;
            case MPIDI_CH3I_CH_TYPE_NDv1:
                mpi_errno = MPIDI_CH3I_Ndv1_write_progress(vc);
                break;
            case MPIDI_CH3I_CH_TYPE_SOCK:
                mpi_errno = MPIDI_CH3I_SOCK_write_progress(vc);
                break;
            default:
                MPIU_Assert(FALSE);
                break;
            }
        }
    }
    Mpi.DecrementDeferredRequestCounter();
    Mpi.SignalProgress();
    return mpi_errno;
}


MPI_RESULT MPIDI_CH3I_DeferWrite(_In_ MPIDI_VC_t* vc, _In_ MPID_Request* sreq)
{
    DeferRequest* defer = new DeferRequest(vc, sreq);
    if (defer == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    switch( vc->ch.channel )
    {
    case MPIDI_CH3I_CH_TYPE_ND:
    case MPIDI_CH3I_CH_TYPE_NDv1:
        TraceDeferNd_Write(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
        break;

    case MPIDI_CH3I_CH_TYPE_SOCK:
        TraceDeferSock_Write(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
        break;
    default:
        __assume(0);
    }

    ExPostCompletion(
            MPIDI_CH3I_set,
            EX_KEY_DEFER_WRITE,
            defer,
            0
            );
    Mpi.IncrementDeferredRequestCounter();

    return MPI_SUCCESS;
}


//
// Callback Routine for processing deferred connect operations.
//  This is invoked during progress loop in response to a call to
//  MPIDI_CH3I_DeferConnect.
//
static int WINAPI
progress_defer_connect(
    _In_ DWORD,
    _In_ VOID* pOverlapped
    )
{
    StackGuardPointer<DeferRequest> defer(static_cast<DeferRequest*>(pOverlapped));
    MPID_Request* sreq = defer->Request;
    MPIDI_VC_t* vc = defer->Vc;
    int mpi_errno = MPI_SUCCESS;

    switch( vc->ch.state )
    {
    case MPIDI_CH3I_VC_STATE_UNCONNECTED:
        mpi_errno = MPIDI_CH3I_VC_start_connect(vc, sreq);

        if (mpi_errno != MPI_SUCCESS)
        {
            //
            // The connect logic doesn't complete the request on failure,
            //  so when it fails, we must ensure it is not in the sendq
            //  and complete the request.
            //
            MPIR_SENDQ_FORGET(sreq);
            sreq->Release();
        }
        break;
    default:
        break;
    }

    Mpi.DecrementDeferredRequestCounter();
    Mpi.SignalProgress();
    return mpi_errno;
}


//
// This routine will defer the establishment of a connection until
//  the progress loop, rather than performing any VC state transitions
//  inline in the current execution context.  It does this by posting
//  a Completion to the CP, which will be handled by the next call that
//  makes progress.
//
MPI_RESULT MPIDI_CH3I_DeferConnect(_In_ MPIDI_VC_t* vc, _In_ MPID_Request* sreq)
{
    DeferRequest* defer = new DeferRequest(vc, sreq);
    if (defer == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    switch( vc->ch.channel )
    {
    case MPIDI_CH3I_CH_TYPE_SHM:
        TraceDeferShm_Connect(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
        break;

    case MPIDI_CH3I_CH_TYPE_ND:
    case MPIDI_CH3I_CH_TYPE_NDv1:
        TraceDeferNd_Connect(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
        break;

    case MPIDI_CH3I_CH_TYPE_SOCK:
        TraceDeferSock_Connect(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
        break;
    default:
        __assume(0);
    }


    ExPostCompletion(
            MPIDI_CH3I_set,
            EX_KEY_DEFER_CONNECT,
            defer,
            0
            );

    Mpi.IncrementDeferredRequestCounter();
    return MPI_SUCCESS;
}


int MPIDI_CH3I_Progress_init()
{
    /* create sock set */
    MPIDI_CH3I_set = ExCreateSet();
    if (MPIDI_CH3I_set == EX_INVALID_SET)
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // Register a completion processor for MPIDI_CH3_Progress_wakeup.
    //
    ExRegisterCompletionProcessor( EX_KEY_PROGRESS_WAKEUP, progress_wakeup );
    ExRegisterCompletionProcessor( EX_KEY_DEFER_CONNECT, progress_defer_connect );
    ExRegisterCompletionProcessor( EX_KEY_DEFER_WRITE, progress_defer_write );

    return MPI_SUCCESS;
}

int MPIDI_CH3I_Progress_finalize()
{
    int mpi_errno = MPI_SUCCESS;

    ExUnregisterCompletionProcessor(EX_KEY_DEFER_WRITE);
    ExUnregisterCompletionProcessor(EX_KEY_DEFER_CONNECT);
    ExUnregisterCompletionProcessor(EX_KEY_PROGRESS_WAKEUP);

    ExCloseSet(MPIDI_CH3I_set);

    return mpi_errno;
}


/* FIXME: This is a temp to free memory allocated in this file */
void MPIDI_CH3U_Finalize_ssm_memory( void )
{
    /* Free resources allocated in CH3_Init() */
    while (MPIDI_CH3I_Process.shm_reading_list)
    {
        MPIDI_CH3I_SHM_Release_mem(&MPIDI_CH3I_Process.shm_reading_list->ch.shm.recv.shm_info);
        MPIDI_CH3I_Process.shm_reading_list = MPIDI_CH3I_Process.shm_reading_list->ch.shm.recv.next_vc;
    }
    while (MPIDI_CH3I_Process.shm_writing_list)
    {
        MPIDI_CH3I_SHM_Release_mem(&MPIDI_CH3I_Process.shm_writing_list->ch.shm.send.shm_info);
        MPIDI_CH3I_Process.shm_writing_list = MPIDI_CH3I_Process.shm_writing_list->ch.shm.send.next_vc;
    }
}
