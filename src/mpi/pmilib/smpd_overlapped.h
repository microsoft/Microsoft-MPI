// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef SMPD_OVERLAPPED_H
#define SMPD_OVERLAPPED_H


typedef
void
(WINAPI * smpd_state_completion_routine)(
    struct EXOVERLAPPED* pOverlapped
    );

struct smpd_overlapped_t
{
    //
    // Caller Executive overlapped.
    // * The success/failure callback functions will be invoked on Sock async
    //   operation completion when MPIDU_Sock_wait is called.
    // * The total number of bytes transfered in a successful read/write
    //   operation is in uov.ov.InernalHigh field of the OVERLAPPED strcture.
    // * The Sock MPI error value is in uov.ov.Internal
    //
    EXOVERLAPPED     uov;

    SmpdCmd*         pCmd;
    SmpdRes*         pRes;
    RPC_ASYNC_STATE* pAsync;
    smpd_context_t*  pContext;
    char*            pPmiTmpBuffer;
    DWORD*           pPmiTmpErr;
};


static
inline
smpd_overlapped_t*
smpd_ov_from_exov(
    EXOVERLAPPED* pexov
    )
{
    return CONTAINING_RECORD(pexov, smpd_overlapped_t, uov);
}


inline
smpd_overlapped_t*
smpd_create_overlapped()
{
    smpd_overlapped_t* pov = new smpd_overlapped_t;
    if( pov == nullptr )
    {
        return nullptr;
    }

    //
    // SMPD redirect stdin/out also uses smpd_overlapped
    // but does not need to use RPC Async structure so we
    // delay the async struct creation.
    //
    pov->pAsync = nullptr;

    return pov;
}


static
inline
int
smpd_get_overlapped_result(
    const smpd_overlapped_t* pov
    )
{
    return ExGetStatus(&pov->uov);
}


static
inline
MPIU_Bsize_t
smpd_get_bytes_transferred(
    const smpd_overlapped_t* pov
    )
{
    return ExGetBytesTransferred(&pov->uov);
}


static
inline
void
smpd_init_overlapped(
    smpd_overlapped_t* pov,
    smpd_state_completion_routine pfn,
    smpd_context_t* pContext
    )
{
    ExInitOverlapped(&pov->uov, (ExCompletionRoutine)pfn, (ExCompletionRoutine)pfn);
    pov->pContext = pContext;
    pov->pPmiTmpBuffer = nullptr;
    pov->pPmiTmpErr = nullptr;
}


static
inline
void
smpd_free_overlapped(
    smpd_overlapped_t* pov
    )
{
    delete pov;
}


#endif /* SMPD_OVERLAPPED_H */
