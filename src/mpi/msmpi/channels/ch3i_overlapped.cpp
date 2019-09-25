// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"
#include "ch3i_overlapped.h"

static inline ch3i_overlapped_t* ch3i_create_overlapped()
{
    return (ch3i_overlapped_t*)malloc(sizeof(ch3i_overlapped_t));
}


int
ch3i_post_close(
    MPIDI_CH3I_Connection_t* conn,
    ExCompletionRoutine pfn
    )
{
    ch3i_overlapped_t* pov = ch3i_create_overlapped();
    if(pov == NULL)
        return MPI_ERR_NO_MEM;

    ch3i_post_close_ex(conn, pov, pfn);
    return MPI_SUCCESS;

}


int
ch3i_post_write(
    MPIDI_CH3I_Connection_t* conn,
    const void* buf,
    MPIU_Bsize_t size,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    int result;
    ch3i_overlapped_t* pov = ch3i_create_overlapped();
    if(pov == NULL)
        return MPI_ERR_NO_MEM;

    result = ch3i_post_write_ex(conn, buf, size, pov, pfnSuccess, pfnFailure);
    if(result != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
    }

    return result;
}


int
ch3i_post_writev(
    MPIDI_CH3I_Connection_t* conn,
    MPID_IOV * iov,
    int iov_n,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    int result;
    ch3i_overlapped_t* pov = ch3i_create_overlapped();
    if(pov == NULL)
        return MPI_ERR_NO_MEM;

    result = ch3i_post_writev_ex(conn, iov, iov_n, pov, pfnSuccess, pfnFailure);
    if(result != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
    }

    return result;
}


int
ch3i_post_read_min(
    _Inout_                  MPIDI_CH3I_Connection_t* conn,
    _Out_writes_bytes_(size) void* buf,
    _In_                     MPIU_Bsize_t size,
    _In_                     MPIU_Bsize_t minbr,
    _In_                     ExCompletionRoutine pfnSuccess,
    _In_                     ExCompletionRoutine pfnFailure
    )
{
    int result;
    ch3i_overlapped_t* pov = ch3i_create_overlapped();
    if(pov == NULL)
        return MPI_ERR_NO_MEM;

    result = ch3i_post_read_min_ex(conn, buf, size, minbr, pov, pfnSuccess, pfnFailure);
    if(result != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
    }

    return result;
}


int
ch3i_post_connect(
    _In_   MPIDI_CH3I_Connection_t* conn,
    _In_z_ const char*              host_description,
    _In_   int                      port,
    _In_   ExCompletionRoutine      pfnSuccess,
    _In_   ExCompletionRoutine      pfnFailure
    )
{
    int result;
    ch3i_overlapped_t* pov = ch3i_create_overlapped();
    if(pov == NULL)
        return MPI_ERR_NO_MEM;

    result = ch3i_post_connect_ex(conn, host_description, port, pov, pfnSuccess, pfnFailure);
    if(result != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
    }

    return result;
}


int
ch3i_post_accept(
    MPIDI_CH3I_Connection_t* conn,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    int result;
    ch3i_overlapped_t* pov = ch3i_create_overlapped();
    if(pov == NULL)
        return MPI_ERR_NO_MEM;

    result = ch3i_post_accept_ex(conn, pov, pfnSuccess, pfnFailure);
    if(result != MPI_SUCCESS)
    {
        ch3i_free_overlapped(pov);
    }

    return result;
}
