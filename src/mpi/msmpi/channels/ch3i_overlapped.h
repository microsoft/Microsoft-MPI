// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef CH3I_OVERLAPPED_H
#define CH3I_OVERLAPPED_H

struct ch3i_overlapped_t
{
    MPIDU_Sock_context_t ov;
    MPIDI_CH3I_Connection_t* conn;
};


static
inline
ch3i_overlapped_t*
ch3i_ov_from_exov(
    EXOVERLAPPED* pexov
    )
{
    return CONTAINING_RECORD(pexov, ch3i_overlapped_t, ov.uov);
}


int
ch3i_post_close(
    MPIDI_CH3I_Connection_t* conn,
    ExCompletionRoutine pfn
    );

int
ch3i_post_write(
    MPIDI_CH3I_Connection_t* conn,
    const void* buf,
    MPIU_Bsize_t size,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    );

int
ch3i_post_writev(
    MPIDI_CH3I_Connection_t* conn,
    MPID_IOV* iov,
    int iov_n,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    );

int
ch3i_post_read_min(
    _Inout_                  MPIDI_CH3I_Connection_t* conn,
    _Out_writes_bytes_(size) void* buf,
    _In_                     MPIU_Bsize_t size,
    _In_                     MPIU_Bsize_t minbr,
    _In_                     ExCompletionRoutine pfnSuccess,
    _In_                     ExCompletionRoutine pfnFailure
    );

int
ch3i_post_connect(
    _In_   MPIDI_CH3I_Connection_t* conn,
    _In_z_ const char*              host_description,
    _In_   int                      port,
    _In_   ExCompletionRoutine      pfnSuccess,
    _In_   ExCompletionRoutine      pfnFailure
    );

int
ch3i_post_accept(
    MPIDI_CH3I_Connection_t* conn,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    );


static
inline
void
ch3i_init_overlapped(
    ch3i_overlapped_t* pov,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure,
    MPIDI_CH3I_Connection_t* conn
    )
{
    ASSERT(conn != NULL);
    ASSERT(pfnSuccess != NULL);
    ASSERT(pfnFailure != NULL);

    ExInitOverlapped(&pov->ov.uov, pfnSuccess, pfnFailure);
    pov->conn = conn;
}


static
inline
void
ch3i_free_overlapped(
    ch3i_overlapped_t* pov
    )
{
    free(pov);
}


static
inline
void
ch3i_post_close_ex(
    MPIDI_CH3I_Connection_t* conn,
    ch3i_overlapped_t* pov,
    ExCompletionRoutine pfn
    )
{
    ch3i_init_overlapped(pov, pfn, pfn, conn);
    MPIDU_Sock_post_close(conn->sock, &pov->ov);
}


static
inline
int
ch3i_post_write_ex(
    MPIDI_CH3I_Connection_t* conn,
    const void* buf,
    MPIU_Bsize_t size,
    ch3i_overlapped_t* pov,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    ch3i_init_overlapped(pov, pfnSuccess, pfnFailure, conn);
    return MPIDU_Sock_post_write(conn->sock, buf, size, &pov->ov);
}


static
inline
int
ch3i_post_writev_ex(
    MPIDI_CH3I_Connection_t* conn,
    MPID_IOV* iov,
    int iov_n,
    ch3i_overlapped_t* pov,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    ch3i_init_overlapped(pov, pfnSuccess, pfnFailure, conn);
    return MPIDU_Sock_post_writev(conn->sock, iov, iov_n, &pov->ov);
}

static
inline
int
ch3i_post_read_min_ex(
    _Inout_ MPIDI_CH3I_Connection_t* conn,
    _Out_writes_bytes_(size) void* buf,
    _In_ MPIU_Bsize_t size,
    _In_ MPIU_Bsize_t minbr,
    _In_ ch3i_overlapped_t* pov,
    _In_ ExCompletionRoutine pfnSuccess,
    _In_ ExCompletionRoutine pfnFailure
    )
{
    ch3i_init_overlapped(pov, pfnSuccess, pfnFailure, conn);
    return MPIDU_Sock_post_read(conn->sock, buf, size, minbr, &pov->ov);
}


static
inline
int
ch3i_post_read_ex(
    _Inout_ MPIDI_CH3I_Connection_t* conn,
    _Out_writes_bytes_(size) void* buf,
    _In_ MPIU_Bsize_t size,
    _In_ ch3i_overlapped_t* pov,
    _In_ ExCompletionRoutine pfnSuccess,
    _In_ ExCompletionRoutine pfnFailure
    )
{
    return ch3i_post_read_min_ex(conn, buf, size, MPIU_BSIZE_MAX, pov, pfnSuccess, pfnFailure);
}


static
inline
int
ch3i_post_readv_min_ex(
    MPIDI_CH3I_Connection_t* conn,
    MPID_IOV* iov,
    int iov_n,
    MPIU_Bsize_t minbr,
    ch3i_overlapped_t* pov,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    ch3i_init_overlapped(pov, pfnSuccess, pfnFailure, conn);
    return MPIDU_Sock_post_readv(conn->sock, iov, iov_n, minbr, &pov->ov);
}


static
inline
int
ch3i_post_readv_ex(
    MPIDI_CH3I_Connection_t* conn,
    MPID_IOV* iov,
    int iov_n,
    ch3i_overlapped_t* pov,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    return ch3i_post_readv_min_ex(conn, iov, iov_n, MPIU_BSIZE_MAX, pov, pfnSuccess, pfnFailure);
}


static
inline
int
ch3i_post_connect_ex(
    _Inout_ MPIDI_CH3I_Connection_t* conn,
    _In_z_  const char*              host_description,
    _In_    int                      port,
    _Out_   ch3i_overlapped_t*       pov,
    _In_    ExCompletionRoutine      pfnSuccess,
    _In_    ExCompletionRoutine      pfnFailure
    )
{
    ch3i_init_overlapped(pov, pfnSuccess, pfnFailure, conn);
    return MPIDU_Sock_post_connect(MPIDI_CH3I_set, host_description, port, &conn->sock, 1, &pov->ov);
}


static
inline
int
ch3i_post_accept_ex(
    MPIDI_CH3I_Connection_t* conn,
    ch3i_overlapped_t* pov,
    ExCompletionRoutine pfnSuccess,
    ExCompletionRoutine pfnFailure
    )
{
    ch3i_init_overlapped(pov, pfnSuccess, pfnFailure, conn);
    return MPIDU_Sock_post_accept(conn->sock, &conn->aux_sock, &pov->ov);
}

#endif /* CH3I_OVERLAPPED_H */
