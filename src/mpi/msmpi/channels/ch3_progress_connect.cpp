// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


int MPIDI_CH3_Connection_terminate(MPIDI_VC_t * vc)
{
    int mpi_errno = MPI_SUCCESS;

    switch( vc->ch.channel )
    {
    case MPIDI_CH3I_CH_TYPE_SHM:
        /* There is no post_close for shm connections so handle them as closed immediately. */
        vc->ch.state = MPIDI_CH3I_VC_STATE_UNCONNECTED;
        MPIDI_CH3U_Handle_connection(vc, MPIDI_VC_EVENT_TERMINATED);
        break;

    case MPIDI_CH3I_CH_TYPE_ND:
        MPIDI_CH3I_Nd_disconnect(vc);
        break;

    case MPIDI_CH3I_CH_TYPE_NDv1:
        MPIDI_CH3I_Ndv1_disconnect(vc);
        break;

    case MPIDI_CH3I_CH_TYPE_SOCK:
        mpi_errno = MPIDI_CH3I_Post_close_connection(vc->ch.conn);
        break;

    default:
        __assume(0);
    }

    return mpi_errno;
}

static void MPIDI_CH3I_SHM_Remove_vc_read_references(MPIDI_VC_t *vc)
{
    MPIDI_VC_t *iter, *trailer;
    /* remove vc from the reading list */
    iter = trailer = MPIDI_CH3I_Process.shm_reading_list;
    while (iter != NULL)
    {
        if (iter == vc)
        {
            /* Mark the connection as NOT read connected so it won't be mistaken for active */
            /* Since shm connections are uni-directional the following three states are considered active:
             * MPIDI_VC_STATE_ACTIVE + ch.shm.recv.connected = 0 - active in the write direction
             * MPIDI_VC_STATE_INACTIVE + ch.shm.recv.connected = 1 - active in the read direction
             * MPIDI_VC_STATE_ACTIVE + ch.shm.recv.connected = 1 - active in both directions
             */
            vc->ch.shm.recv.connected = 0;
            if (trailer != iter)
            {
                /* remove the vc from the list */
                trailer->ch.shm.recv.next_vc = iter->ch.shm.recv.next_vc;
            }
            else
            {
                /* remove the vc from the head of the list */
                MPIDI_CH3I_Process.shm_reading_list = MPIDI_CH3I_Process.shm_reading_list->ch.shm.recv.next_vc;
            }
        }
        if (trailer != iter)
            trailer = trailer->ch.shm.recv.next_vc;
        iter = iter->ch.shm.recv.next_vc;
    }
}

static void MPIDI_CH3I_SHM_Remove_vc_write_references(const MPIDI_VC_t *vc)
{
    MPIDI_VC_t *iter, *trailer;

    /* remove the vc from the writing list */
    iter = trailer = MPIDI_CH3I_Process.shm_writing_list;
    while (iter != NULL)
    {
        if (iter == vc)
        {
            if (trailer != iter)
            {
                /* remove the vc from the list */
                trailer->ch.shm.send.next_vc = iter->ch.shm.send.next_vc;
            }
            else
            {
                /* remove the vc from the head of the list */
                MPIDI_CH3I_Process.shm_writing_list = MPIDI_CH3I_Process.shm_writing_list->ch.shm.send.next_vc;
            }
        }
        if (trailer != iter)
            trailer = trailer->ch.shm.send.next_vc;
        iter = iter->ch.shm.send.next_vc;
    }
}

void MPIDI_CH3I_SHM_Remove_vc_references(MPIDI_VC_t *vc)
{
    MPIDI_CH3I_SHM_Remove_vc_read_references(vc);
    MPIDI_CH3I_SHM_Remove_vc_write_references(vc);
}

void MPIDI_CH3I_SHM_Add_to_reader_list(MPIDI_VC_t *vc)
{
    MPIDI_CH3I_SHM_Remove_vc_read_references(vc);
    vc->ch.shm.recv.next_vc = MPIDI_CH3I_Process.shm_reading_list;
    MPIDI_CH3I_Process.shm_reading_list = vc;
}

void MPIDI_CH3I_SHM_Add_to_writer_list(MPIDI_VC_t *vc)
{
    MPIDI_CH3I_SHM_Remove_vc_write_references(vc);
    vc->ch.shm.send.next_vc = MPIDI_CH3I_Process.shm_writing_list;
    MPIDI_CH3I_Process.shm_writing_list = vc;
}


static inline void init_shm_buffer(MPIDI_CH3I_SHM_Queue_t* p)
{
    p->pg_id = MPIDI_Process.my_pg->id;
    p->ver = MSMPI_VER_EX;
    p->pg_rank = MPIDI_Process.my_pg_rank;
    p->recv.index = 0;
    p->recv.notify_index = 0;
    p->recv.notify_capture = 0;
    p->send.index = 0;
    p->send.notify_index = 0;
    p->send.notify_capture = 0;
}


int MPIDI_CH3I_Shm_connect(MPIDI_VC_t* vc, const char* business_card, int* flag)
{
    int rc;
    char hostname[256];
    char queue_name[100];
    MPIDI_CH3I_SHM_send_t* p = &vc->ch.shm.send;

    *flag = FALSE;

    if(MPIDI_CH3I_Process.disable_shm)
        return MPI_SUCCESS;

    /* get the host and queue from the business card */
    /* If there is no shm host key, assume that we can't use shared memory */
    rc = MPIU_Str_get_string_arg(business_card, MPIDI_CH3I_SHM_HOST_KEY, hostname, _countof(hostname));
    if (rc != MPIU_STR_SUCCESS)
        return MPI_SUCCESS;

    //
    // Compare our hostname with the business card's hostname. If they
    // are different, there cannot be a shm connection.
    //
    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        MPIDI_CH3U_Hostname_sshm(),
                        -1,
                        hostname,
                        -1 ) != CSTR_EQUAL )
    {
        return MPI_SUCCESS;
    }

    if(vc->ch.shm.queue == NULL)
    {
        rc = MPIU_Str_get_string_arg(business_card, MPIDI_CH3I_SHM_QUEUE_KEY, queue_name, _countof(queue_name));
        if (rc != MPIU_STR_SUCCESS)
        {
            Trace_SHMEM_Error_MPIDI_CH3I_Shm_connect(Shm_connectConnectQueueName, rc, business_card, vc, business_card, flag);
            return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**argstr_shmq");
        }

        rc = MPIDI_CH3I_BootstrapQ_attach(queue_name, &vc->ch.shm.queue);
        if (rc != MPI_SUCCESS)
        {
            Trace_SHMEM_Error_MPIDI_CH3I_Shm_connect(Shm_connectConnectQueueAttach, rc, business_card, vc, business_card, flag);
            return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**boot_attach %s", queue_name);
        }
    }

    /* create the write queue */
    rc = MPIDI_CH3I_SHM_Get_mem(sizeof(MPIDI_CH3I_SHM_Queue_t), &p->shm_info);
    if (rc != MPI_SUCCESS)
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Shm_connect(Shm_connectWriteQueue, rc, business_card, vc, business_card, flag);
        rc = MPIU_ERR_FATAL_GET(rc, MPI_ERR_OTHER, "**shmconnect_getmem");
        goto fn_fail1;
    }

    p->shm = p->shm_info.addr;
    init_shm_buffer(p->shm);

    /* send the queue connection information */
    rc = MPIDI_CH3I_Notify_connect(vc->ch.shm.queue, p->shm_info.hShm, GetCurrentProcessId());
    if (rc != MPI_SUCCESS)
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Shm_connect(Shm_connectNotifyConnect, rc, business_card, vc, business_card, flag);
        rc = MPIU_ERR_FATAL_GET(rc, MPI_ERR_OTHER, "**boot_send");
        goto fn_fail2;
    }

    vc->eager_max_msg_sz = MPIDI_CH3I_Process.shm_eager_limit;
    *flag = TRUE;
    Trace_SHMEM_Info_MPIDI_CH3I_Shm_connect_Success(hostname, business_card);
    return MPI_SUCCESS;

fn_fail2:
    MPIDI_CH3I_SHM_Unlink_mem(&p->shm_info);
    MPIDI_CH3I_SHM_Release_mem(&p->shm_info);
fn_fail1:
    MPIDI_CH3I_BootstrapQ_detach(vc->ch.shm.queue);
    return rc;

}


int MPIDI_CH3I_Accept_shm_connection(DWORD pid, PVOID p)
{
    OACR_USE_PTR( p );
    int rc;
    int pg_rank;
    int shm_id = PtrToUlong(p);
    MPIDI_CH3I_SHM_info_t info;
    const MPIDI_CH3I_SHM_Queue_t* recvq;
    char queue_name[100];
    char bizcard[MPIDI_MAX_KVS_VALUE_LEN];

    rc = MPIDI_CH3I_SHM_Attach_to_mem(pid, shm_id, &info);
    if (rc != MPI_SUCCESS || info.size < sizeof(MPIDI_CH3I_SHM_Queue_t))
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Accept_shm_connection(Shm_acceptQueueAttach, pid, p);
        return MPIU_ERR_FATAL_GET(rc, MPI_ERR_OTHER, "**attach_to_mem");
    }

    recvq = info.addr;

    if (recvq->ver != MSMPI_VER_EX)
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Accept_shm_connection(Shm_acceptMismatchedVersion, pid, p);
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**version %d %d %d %d %d %d",
            MSMPI_VER_MAJOR( recvq->ver ),
            MSMPI_VER_MINOR( recvq->ver ),
            MSMPI_VER_BUILD( recvq->ver ),
            MSMPI_VER_MAJOR( MSMPI_VER_EX ),
            MSMPI_VER_MINOR( MSMPI_VER_EX ),
            MSMPI_VER_BUILD( MSMPI_VER_EX )
            );
    }

    MPIDI_PG_t* pg = MPIDI_PG_Find(recvq->pg_id);
    if(pg == NULL)
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Accept_shm_connection(Shm_acceptPGFind, pid, p);
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**pglookup %g", recvq->pg_id);
    }

    pg_rank = recvq->pg_rank;
    if((pg_rank < 0) || (pg_rank >= pg->size))
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Accept_shm_connection(Shm_acceptRank, pid, p);
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**rank %d %d", pg_rank, pg->size);
    }

    MPIDI_VC_t* vc = MPIDI_PG_Get_vc(pg, pg_rank);
    MPIU_Assert(vc != NULL);
    MPIU_Assert(vc->pg_rank == pg_rank);

    HANDLE hProcess;
    hProcess = OpenProcess(
                PROCESS_VM_READ,
                FALSE,  // bInheritHandle
                pid
                );
    if(hProcess == NULL)
    {
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**OpenProcess %d %d", pg_rank, ::GetLastError());
    }

    vc->ch.shm.recv.hProcess = hProcess;

    vc->ch.shm.recv.connected = 1;
    vc->ch.channel = MPIDI_CH3I_CH_TYPE_SHM;
    vc->ch.shm.recv.shm_info = info;
    vc->ch.shm.recv.shm = info.addr;

    /* add this VC to the global list to be shm_waited on */
    MPIDI_CH3I_SHM_Add_to_reader_list(vc);

    if(vc->ch.shm.queue != NULL)
    {
        Trace_SHMEM_Info_MPIDI_CH3I_Accept_shm_connection_Success(Mpi.CommWorld->rank, pg_rank);
        return MPI_SUCCESS;
    }

    rc = MPIDI_PG_GetConnString(vc->pg, vc->pg_rank, bizcard, _countof(bizcard));
    if(rc != MPI_SUCCESS)
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Accept_shm_connection(Shm_acceptGetConnStringFailed, pid, p);
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**getConnStringFailed");
    }

    rc = MPIU_Str_get_string_arg(bizcard, MPIDI_CH3I_SHM_QUEUE_KEY, queue_name, _countof(queue_name));
    if (rc != MPIU_STR_SUCCESS)
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Accept_shm_connection(Shm_acceptGetStringArgFailed, pid, p);
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**argstr_shmq");
    }

    rc = MPIDI_CH3I_BootstrapQ_attach(queue_name, &vc->ch.shm.queue);
    if (rc != MPI_SUCCESS)
    {
        Trace_SHMEM_Error_MPIDI_CH3I_Accept_shm_connection(Shm_acceptBootstrapQueueAttach, pid, p);
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**boot_attach %s", queue_name);
    }

    Trace_SHMEM_Info_MPIDI_CH3I_Accept_shm_connection_SuccessAfterAttach(Mpi.CommWorld->rank, pg_rank);
    return MPI_SUCCESS;
}


static
int
MPIDI_CH3I_Sock_connect(
    _In_   MPIDI_VC_t*               vc,
    _In_z_ const char*               host_description,
    _In_   int                       port,
    _In_z_ const char*               business_card,
    _Out_  MPIDI_CH3I_Connection_t** conn_ptr
    )
{
    int mpi_errno;
    MPIDI_CH3I_Connection_t * conn;

    if(*conn_ptr != NULL)
    {
        //
        // Connection is being established by the other side and we already accepted it
        // waiting for the second connection. use the existing connection.
        //
        return MPI_SUCCESS;
    }

    mpi_errno = MPIDI_CH3I_Connection_alloc(&conn);
    ON_ERROR_FAIL(mpi_errno);

    conn->vc = vc;
    *conn_ptr = conn;

    mpi_errno = MPIDI_CH3I_Post_connect(conn, host_description, port);
    if (mpi_errno != MPI_SUCCESS)
    {
        MPIDI_CH3I_Connection_free(conn);
        *conn_ptr = NULL;
        Trace_SOCKETS_Error_MPIDI_CH3I_Sock_connect_PostFailed(Mpi.CommWorld->rank, vc->pg_rank, business_card);
        mpi_errno = MPIU_ERR_FATAL_GET(mpi_errno, MPI_ERR_OTHER, "**ch3|sock|postconnect %d %d %s", Mpi.CommWorld->rank, vc->pg_rank, business_card);
        goto fn_fail;
    }

    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}


MPI_RESULT MPIDI_CH3I_VC_start_connect(_In_ MPIDI_VC_t * vc, _In_ MPID_Request * sreq)
{
    int mpi_errno = MPI_SUCCESS;
    char val[MPIDI_MAX_KVS_VALUE_LEN];
    char host_description[MAX_HOST_DESCRIPTION_LEN];
    int port;
    int connected;

    MPIU_Assert(vc->ch.state == MPIDI_CH3I_VC_STATE_UNCONNECTED);

    /* get the business card */

    mpi_errno = MPIDI_PG_GetConnString(vc->pg, vc->pg_rank, val, _countof(val));
    ON_ERROR_FAIL(mpi_errno);

    /* attempt to connect through shared memory */
    connected = FALSE;
    mpi_errno = MPIDI_CH3I_Shm_connect(vc, val, &connected);
    ON_ERROR_FAIL(mpi_errno);

    if (connected)
    {
        MPIDI_CH3I_SHM_Add_to_writer_list(vc);

        vc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTED;
        vc->ch.channel = MPIDI_CH3I_CH_TYPE_SHM;
        TraceSendShm_Connect(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
        goto fn_exit;
    }

    //
    // Attempt connecting using network direct
    // Force if sockets are disabled so that we get useful error messages.
    //
    mpi_errno = MPIDI_CH3I_Nd_connect(vc, val, MPIDI_CH3I_Process.disable_sock, &connected);
    ON_ERROR_FAIL(mpi_errno);

    if (connected)
    {
        TraceSendNd_Connect(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
        goto fn_exit;
    }

    /* attempt to connect through sockets */
    mpi_errno = MPIDU_Sock_get_conninfo_from_bc( val, host_description, _countof(host_description), &port );
    ON_ERROR_FAIL(mpi_errno);

    if(MPIDI_CH3I_Process.disable_sock)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**sock_connect %s %d %s", host_description, port, "the socket interconnect is disabled");
        goto fn_fail;
    }

    vc->eager_max_msg_sz = MPIDI_CH3I_Process.sock_eager_limit;

    MPIU_Assert(vc->ch.conn == NULL);

    mpi_errno = MPIDI_CH3I_Sock_connect(vc, host_description, port, val, &vc->ch.conn);
    ON_ERROR_FAIL(mpi_errno);

    vc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTING;
    vc->ch.channel = MPIDI_CH3I_CH_TYPE_SOCK;
    TraceSendSock_Connect(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);

 fn_exit:
    return mpi_errno;

fn_fail:
    vc->ch.state = MPIDI_CH3I_VC_STATE_FAILED;
    sreq->status.MPI_ERROR = mpi_errno;
    sreq->signal_completion();
    goto fn_exit;
}


MPI_RESULT MPIDI_CH3I_VC_post_connect(_In_ MPIDI_VC_t * vc, _In_ MPID_Request * sreq)
{
    if (Mpi.IsMultiThreaded() == false ||
        Mpi.IsCurrentThreadMakingProgress())
    {
        return MPIDI_CH3I_VC_start_connect(vc, sreq);
    }

    int mpi_errno = MPIDI_CH3I_DeferConnect(vc, sreq);
    if (mpi_errno != MPI_SUCCESS)
    {
        sreq->signal_completion();
        sreq->status.MPI_ERROR = mpi_errno;
    }
    return mpi_errno;
}
