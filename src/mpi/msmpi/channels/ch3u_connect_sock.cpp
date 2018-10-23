// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"

/* Private packet types used only within this file */
struct MPIDI_CH3I_Pkt_sc_conn_accept_t
{
    MPIDI_CH3_Pkt_type_t type;
    int port_name_tag;
};
C_ASSERT(sizeof(MPIDI_CH3I_Pkt_sc_conn_accept_t) <= sizeof(MPIDI_CH3_Pkt_t));

/* FIXME: Describe what these routines do */

/* FIXME: Clean up use of private packets (open/accept) */

/* Partial description:
   This file contains the routines that are used to create socket connections,
   including the routines used to encode/decode the description of a connection
   into/out of the "business card".

   ToDo: change the "host description" to an "interface address" so that
   socket connections are targeted at particularly interfaces, not
   compute nodes, and that the address is in ready-to-use IP address format,
   and does not require a gethostbyname lookup.  - Partially done
 */

/*
 * Manage the connection information that is exported to other processes
 *
 */
#define MPIDI_CH3I_HOST_DESCRIPTION_KEY  "description"
#define MPIDI_CH3I_PORT_KEY              "port"
#define MPIDI_CH3I_IFNAME_KEY            "ifname"

/*
 * Routines for establishing a listener socket on the socket set that
 * is used for all communication.  These should be called from the
 * channel init and finalize routines.
 */
static int MPIDI_CH3I_listener_port = 0;
static MPIDI_CH3I_Connection_t * MPIDI_CH3I_listener_conn = NULL;


int MPIDU_CH3I_SetupListener( ExSetHandle_t sock_set )
{
    int mpi_errno;

    mpi_errno = MPIDI_CH3I_Connection_alloc(&MPIDI_CH3I_listener_conn);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDU_Sock_listen(sock_set, INADDR_ANY, &MPIDI_CH3I_listener_port, &MPIDI_CH3I_listener_conn->sock);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_CH3I_Post_accept(MPIDI_CH3I_listener_conn);
    ON_ERROR_FAIL(mpi_errno);

fn_exit:
    return mpi_errno;

fn_fail:
    goto fn_exit;
}


void MPIDU_CH3I_ShutdownListener( void )
{
    if( MPIDI_CH3I_listener_conn == NULL )
    {
        return;
    }

    MPIDU_Sock_close(MPIDI_CH3I_listener_conn->sock);
    MPIDI_CH3I_Connection_free(MPIDI_CH3I_listener_conn);
    MPIDI_CH3I_listener_conn = NULL;
    MPIDI_CH3I_listener_port = 0;
}


int MPIDI_CH3I_Connection_alloc(MPIDI_CH3I_Connection_t ** connp)
{
    MPIDI_CH3I_Connection_t * conn = MPIU_Malloc_obj(MPIDI_CH3I_Connection_t);
    if( conn == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    conn->vc = NULL;
    conn->recv_active = NULL;
    conn->recv_buff_end = 0;
    conn->disconnect = FALSE;
    *connp = conn;

    return MPI_SUCCESS;
}


void MPIDI_CH3I_Connection_free(MPIDI_CH3I_Connection_t * conn)
{
    MPIU_Free(conn);
}

/* ------------------------------------------------------------------------- */
/* Business card management.  These routines insert or extract connection
   information when using sockets from the business card */
/* ------------------------------------------------------------------------- */

/* FIXME: These are small routines; we may want to bring them together
   into a more specific post-connection-for-sock */

/* The host_description should be of length MAX_HOST_DESCRIPTION_LEN */
MPI_RESULT
MPIDU_Sock_get_conninfo_from_bc(
    _In_z_              const char* bc,
    _Out_z_cap_(maxlen) char*       host_description,
    _In_                size_t      maxlen,
    _Out_               int*        port
    )
{
    int mpi_errno = MPI_SUCCESS;
    int str_errno;

    str_errno = MPIU_Str_get_string_arg(bc, MPIDI_CH3I_HOST_DESCRIPTION_KEY,
                                 host_description, maxlen);
    if (str_errno != MPIU_STR_SUCCESS)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER,"**argstr_missinghost");
        goto fn_fail;
    }

    str_errno = MPIU_Str_get_int_arg(bc, MPIDI_CH3I_PORT_KEY, port);
    if (str_errno != MPIU_STR_SUCCESS)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER,"**argstr_missingport");
        goto fn_fail;
    }

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


/*  MPIDI_CH3U_Get_business_card_sock - does socket specific portion of
 *  setting up a business card
 *
 *  Parameters:
 *     bc_val_p     - business card value buffer pointer, updated to the next
 *                    available location or freed if published.
 *     val_max_sz_p - ptr to maximum value buffer size reduced by the number
 *                    of characters written
 *
 */

int
MPIDI_CH3U_Get_business_card_sock(
    _Deref_out_z_cap_c_(*val_max_sz_p) char** bc_val_p,
    _Inout_ int*                              val_max_sz_p
    )
{
    int mpi_errno = MPI_SUCCESS;
    int port;
    char host_description[MAX_HOST_DESCRIPTION_LEN];

    mpi_errno = MPIDU_Sock_get_host_description( host_description,
                                                 _countof(host_description) );
    ON_ERROR_FAIL(mpi_errno);

    port = MPIDI_CH3I_listener_port;
    mpi_errno = MPIU_Str_add_int_arg(bc_val_p, val_max_sz_p,
                                     MPIDI_CH3I_PORT_KEY, port);
    if (mpi_errno != MPIU_STR_SUCCESS)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER,"**buscard");
        goto fn_fail;
    }

    mpi_errno = MPIU_Str_add_string_arg(bc_val_p, val_max_sz_p,
                           MPIDI_CH3I_HOST_DESCRIPTION_KEY, host_description);
    if (mpi_errno != MPIU_STR_SUCCESS)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER,"**buscard");
        goto fn_fail;
    }

    /* Look up the interface address cooresponding to this host description */
    /* FIXME: We should start switching to getaddrinfo instead of
       gethostbyname */
    /* FIXME: We don't make use of the ifname in Windows in order to
       provide backward compatibility with the (undocumented) host
       description string used by the socket connection routine
       MPIDU_Sock_post_connect.  We need to change to an interface-address
       (already resolved) based description for better scalability and
       to eliminate reliance on fragile DNS services. Note that this is
       also more scalable, since the DNS server may serialize address
       requests.  On most systems, asking for the host info of yourself
       is resolved locally (i.e., perfectly parallel).
    */

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}
