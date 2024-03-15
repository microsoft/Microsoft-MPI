// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpisock.h"
#include <mswsock.h>    // acceptex
#include <mstcpip.h>    // keep-alive definitions
#include "mpitrace.h"
#include <winternl.h>
#include <mstcpip.h>

#define SOCKI_TCP_BUFFER_SIZE       32*1024

//
// Shorthand macros
//
#define MPIU_E_FAIL_GLE2(gle_) \
    MPIU_ERR_CREATE(MPI_ERR_OTHER, "**fail %s %d", get_error_string(gle_), gle_)


void sock_hard_close_socket(
    _Inout_ sock_state_t* sock,
    _Inout_ sock_overlapped_t* pov
    );

void sock_graceful_close_socket(
    _Inout_ sock_state_t *sock,
    _Inout_ sock_overlapped_t* pov
    );

static int g_socket_rbuffer_size = SOCKI_TCP_BUFFER_SIZE;
static int g_socket_sbuffer_size = SOCKI_TCP_BUFFER_SIZE;
static int g_init_called = 0;
static int g_min_port = 0;
static int g_max_port = 0;
static struct in_addr g_netmask = { 0 };
static struct in_addr g_netaddr = { 0 };
static int g_connect_retries = MSMPI_DEFAULT_CONNECT_RETRIES;


#ifndef STATUS_CANCELLED
#define STATUS_CANCELLED ((DWORD)0xC0000120L)
#endif


//
// Utility function to return the IPv4 address string of the remote host
// connect to this socket.
//
_Success_(*pPort != 0)
void
get_sock_peer_address(
    _In_ _Pre_satisfies_(0 != pSock->sock) sock_state_t* pSock,
    _Out_writes_bytes_(addrLen) char* pAddr,
    _In_ int addrLen,
    _Out_ int* pPort
    )
{
    struct sockaddr_in info;
    int len = sizeof(struct sockaddr_in);

    MPIU_Assert(nullptr != pAddr);
    MPIU_Assert(nullptr != pSock);
    MPIU_Assert(0 != pSock->sock);
    MPIU_Assert(nullptr != pPort);

    int status = getpeername(
        pSock->sock,
        reinterpret_cast<struct sockaddr*>(&info),
        &len
        );

    if(0 != status)
    {
        *pPort = 0;
    }
    else
    {
        MPIU_Strcpy( pAddr, addrLen, inet_ntoa( info.sin_addr ) );
        *pPort = info.sin_port;
    }
}


_Success_(return == NOERROR)
static int
sock_get_overlapped_result(
    _In_ sock_overlapped_t* pov
    )
{
    int gle = NO_ERROR;
    DWORD Flags;
    DWORD BytesTransferred;

    //
    // Special check for error codes returned when the socket get closed.
    // Cannot retrive the error with an invalid socket; 'manually' translate
    // to winsock error code.
    //
    NTSTATUS status = ExGetStatus(&pov->exov);
    if(status == STATUS_CANCELLED)
    {
        Trace_SOCKETS_Error_sock_get_overlapped_result();
        return WSA_OPERATION_ABORTED;
    }

    MPIU_Assert(pov->sock->sock != INVALID_SOCKET);
    if(!WSAGetOverlappedResult(pov->sock->sock, &pov->exov.ov, &BytesTransferred, FALSE, &Flags))
    {
        gle = WSAGetLastError();
        Trace_SOCKETS_Error_sock_get_overlapped_result_Failed(gle, get_error_string(gle));
    }
    MPIU_Assert(gle != WSAENOTSOCK);
    return gle;
}


static void
TrimIOV(
    _Inout_ _Outptr_ WSABUF** piov,
    _Inout_ int* piovlen,
    _In_ MPIU_Bsize_t num_bytes
    )
{
    WSABUF* iov = *piov;
    int iovlen = *piovlen;

    while (num_bytes != 0)
    {
        MPIU_Assert(num_bytes > 0);
        MPIU_Assert(iovlen > 0);

        if(iov[0].len <= num_bytes)
        {
            num_bytes -= iov[0].len;
            iov++;
            iovlen--;
        }
        else
        {
            iov[0].len -= num_bytes;
            iov[0].buf += num_bytes;
            num_bytes = 0;
        }
    }

    *piov = iov;
    *piovlen = iovlen;
}


_Success_(return==NO_ERROR)
static int
sock_safe_send(
    _In_ SOCKET sock,
    _Inout_ WSABUF* iov,
    _In_ int iovlen,
    _Out_ DWORD* pBytesSent,
    _Inout_opt_ OVERLAPPED* ov)
{
    int rc;
    WSABUF tmp = iov[0];

    for(;;)
    {
        MPIU_Assert(iov[0].len > 0);
        if(WSASend(sock, iov, iovlen, pBytesSent, 0, ov, nullptr) != SOCKET_ERROR)
            return NO_ERROR;

        rc = WSAGetLastError();
        if(rc == WSA_IO_PENDING)
            return NO_ERROR;

        //
        // An error is returned for nonblocking sockets (w/o) overlapped.
        // The caller should handle that error.
        //
        MPIU_Assert((rc != WSAEWOULDBLOCK) || (ov == nullptr));
        MPIU_Assert(rc != WSAESHUTDOWN);

        if(rc != WSAENOBUFS)
        {
            Trace_SOCKETS_Error_sock_safe_send(rc, get_error_string(rc));
            return rc;
        }

        //
        // No buffers for send, use the temporary overlapped to send
        // the first buffer only.
        //
        iov = &tmp;
        iovlen = 1;

        //
        // Reduce the buffer size, don't let the size go down to zero. Assume
        // that eventually winsock will return success or a different error than
        // WSAENOBUFFS.
        //
        tmp.len = tmp.len / 2 + 1;
        Sleep(0);
    }
}


_Success_(return==NO_ERROR)
static int
sock_safe_receive(
    _In_ SOCKET sock,
    _In_ const WSABUF* iov,
    _In_opt_ int /*iovlen*/,
    _Inout_opt_ OVERLAPPED* ov
    )
{
    int rc;
    WSABUF tmp = iov[0];
    DWORD Flags;

    //
    // Cap the size to receive to avoid excessive probe and lock in the kernel.
    // N.B. The excessive probe and lock happens because of partial receives of large buffers.
    //      The buffer is repeatedly re-posted for receive and being probed and locked again.
    //      It's better to use MSG_WAITALL, but unfortonatly it can not be mixed with
    //      non-blocking buffers
    //
    tmp.len = min(tmp.len, 1024 * 1024);

    for(;;)
    {
        Flags = 0;
        if(WSARecv(sock, &tmp, 1, nullptr, &Flags, ov, nullptr) != SOCKET_ERROR)
            return NO_ERROR;

        rc = WSAGetLastError();
        if(rc == WSA_IO_PENDING)
            return NO_ERROR;

        //
        // An error is returned for nonblocking sockets (w/o) overlapped.
        // The caller should handle that error.
        //
        MPIU_Assert((rc != WSAEWOULDBLOCK) || (ov == nullptr));
        MPIU_Assert(rc != WSAESHUTDOWN);

        if(rc != WSAENOBUFS)
        {
            Trace_SOCKETS_Error_sock_safe_receive(rc, get_error_string(rc));
            return rc;
        }

        //
        // Reduce the buffer size, don't let the size go down to zero. Assume
        // that eventually winsock will return success or a different error than
        // WSAENOBUFFS.
        //
        tmp.len = tmp.len / 2 + 1;
        Sleep(0);
    }
}


/* sock functions */
_Success_(return!=nullptr)
static sock_state_t*
sock_create_state(
    _In_ sock_close_routine pfnClose
    )
{
    sock_state_t *p = MPIU_Malloc_obj(sock_state_t);
    if(p == nullptr)
        return nullptr;

    memset(p, 0, sizeof(*p));
    p->pfnClose = pfnClose;
    p->sock = INVALID_SOCKET;
    p->set = EX_INVALID_SET;
    return p;
}


static inline void
sock_free_state(
    _In_ _Post_ptr_invalid_ sock_state_t *p
    )
{
    MPIU_Free(p);
}


static
inline
void
sock_init_overlapped(
    _Inout_ sock_overlapped_t* p,
    _In_ sock_state_t *sock,
    _In_ ExCompletionRoutine pfnSuccess,
    _In_ ExCompletionRoutine pfnFailure
    )
{
    ExInitOverlapped(&p->exov, pfnSuccess, pfnFailure);
    p->sock = sock;
}


static inline sock_overlapped_t*
sock_ov_from_exov(
    _In_ EXOVERLAPPED* pexov
    )
{
    return CONTAINING_RECORD(pexov, sock_overlapped_t, exov);
}


_Success_(return==MPI_SUCCESS)
static int
CallbackFailure(
    _In_ sock_overlapped_t* pov,
    _In_ int error)
{
    MPIDU_Sock_context_t* psc = CONTAINING_RECORD(pov, MPIDU_Sock_context_t, sov);
    return ExCallFailure(&psc->uov, error, 0 /*BytesTransferred*/);
}


_Success_(return==MPI_SUCCESS)
static int
CallbackSuccess(
    _In_ sock_overlapped_t* pov,
    _In_ MPIU_Bsize_t num_bytes)
{
    MPIDU_Sock_context_t* psc = CONTAINING_RECORD(pov, MPIDU_Sock_context_t, sov);
    return ExCallSuccess(&psc->uov, MPI_SUCCESS, num_bytes);
}


static void set_port_range(void)
{
   if( env_to_range_ex(
            L"MSMPI_PORT_RANGE",
            L"MPICH_PORT_RANGE",
            0,
            65535,
            FALSE,
            &g_min_port,
            &g_max_port ) == FALSE )
    {
        g_min_port = g_max_port = 0;
    }
}


HRESULT ParseNetmask( _In_z_ PCWSTR szNetmaskEnv, _Out_ IN_ADDR* pAddr, _Out_ IN_ADDR* pMask )
{
    //
    // Max of "xxx.xxx.xxx.xxx/xxx.xxx.xxx.xxx" plus null is 32
    //
    WCHAR env[32];
    ULONG ret = MPIU_Getenv( szNetmaskEnv,
                             env,
                             _countof(env) );
    if( ret != NOERROR )
    {
        return HRESULT_FROM_WIN32(ret);
    }

    PCWSTR delim;
    ret = RtlIpv4StringToAddressW( env, TRUE, &delim, pAddr );
    if( FAILED( ret ) )
    {
        return ret;
    }

    if( *delim != L'/' )
    {
        return E_INVALIDARG;
    }

    PCWSTR szMask = delim + 1;
    ret = RtlIpv4StringToAddressW( szMask, TRUE, &delim, pMask );
    if( ret == STATUS_INVALID_PARAMETER )
    {
        int bits = _wtol( szMask );
        if( bits <= 0 || bits > 31 )
        {
            return E_INVALIDARG;
        }

        //
        // The addresses are stored in network byte order, so we must form the mask
        // and swap it.  Simply creating a mask in the lower bits doesn't work, for
        // example a 12 bit mask shoudl result in s_addr == 0x0000F0FF (0xFFF00000 in
        // host order).
        //
        // We perform a left shift to clear the lower bits, leaving only the desired
        // number of most-significant bits set, then swap that to network order.
        //
        pMask->s_addr = _byteswap_ulong( ~0UL << (32 - bits) );
    }
    else if( FAILED( ret ) )
    {
        return ret;
    }

    pAddr->s_addr &= pMask->s_addr;
    return S_OK;
}


static void set_netmask()
{
    if( FAILED( ParseNetmask( L"MPICH_NETMASK", &g_netaddr, &g_netmask ) ) )
    {
        g_netaddr.s_addr = 0;
        g_netmask.s_addr = 0;
    }
}


_Success_(return==MPI_SUCCESS)
int MPIDU_Sock_init()
{
    int v;
    WSADATA wsaData;

    if (g_init_called)
    {
        g_init_called++;
        return MPI_SUCCESS;
    }

    /* Start the Winsock dll */
    if ((v = WSAStartup(MAKEWORD(2, 0), &wsaData)) != 0)
    {
        Trace_SOCKETS_Error_MPIDU_Sock_init(v, get_error_string(v));
        return MPIU_E_FAIL_GLE2(v);
    }

    /* get the socket buffers size */
    v = env_to_int(L"MPICH_SOCKET_BUFFER_SIZE", SOCKI_TCP_BUFFER_SIZE, 0);
    g_socket_rbuffer_size = env_to_int(L"MPICH_SOCKET_RBUFFER_SIZE", v, 0);
    g_socket_sbuffer_size = env_to_int(L"MPICH_SOCKET_SBUFFER_SIZE", v, 0);

    /* get the connect max retry count */
    g_connect_retries = env_to_int_ex(
        L"MSMPI_CONNECT_RETRIES",
        L"MPICH_CONNECT_RETRIES",
        MSMPI_DEFAULT_CONNECT_RETRIES,
        0
        );

    /* check to see if a port range was specified */
    set_port_range();

    /* check to see if a subnet was specified through the environment */
    set_netmask();

    g_init_called = 1;

    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
int MPIDU_Sock_finalize()
{
    MPIU_Assert(g_init_called);

    g_init_called--;
    if (g_init_called == 0)
    {
        WSACleanup();
    }
    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
static int
add_host_description(
    _In_z_ const char* host,
    _Inout_ _Outptr_result_buffer_(*plen) PSTR* phost_description,
    _Inout_ int* plen
    )
{
    int str_errno = MPIU_Str_add_string(phost_description, plen, host);
    if (str_errno != MPIU_STR_SUCCESS)
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**desc_len");

    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
static int
socki_get_host_list(
    _In_z_ const char* hostname,
    _Out_writes_z_(len) char* host_description,
    _In_ int len
    )
{
    int mpi_errno;
    char** p;
    struct hostent* res;

    MPIU_Assert(len > 0);

    *host_description = '\0';

    res = gethostbyname(hostname);
    if((res == nullptr) || (res->h_addr_list == nullptr))
    {
        int gle = WSAGetLastError();
        Trace_SOCKETS_Error_socki_get_host_list(gle, get_error_string(gle), hostname);
        return MPIU_E_FAIL_GLE2(gle);
    }

    /* add the ip addresses */
    for(p = res->h_addr_list; *p != nullptr; p++)
    {
        mpi_errno = add_host_description(inet_ntoa(*(struct in_addr*)*p), &host_description, &len);
        if (mpi_errno != MPI_SUCCESS)
        {
            Trace_SOCKETS_Error_socki_get_host_list_AddIp(mpi_errno);
            return mpi_errno;
        }
    }

    /* add the hostname to the end of the list */
    mpi_errno = add_host_description(hostname, &host_description, &len);
    if (mpi_errno != MPI_SUCCESS)
    {
        Trace_SOCKETS_Error_socki_get_host_list_AddHostname(mpi_errno);
        return mpi_errno;
    }

    return MPI_SUCCESS;
}

_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_hostname_to_host_description(
    _In_z_ const char* hostname,
    _Out_writes_z_(len) char* host_description,
    _In_ int len
    )
{
    int mpi_errno;

    MPIU_Assert(g_init_called);

    mpi_errno = socki_get_host_list(hostname, host_description, len);
    if (mpi_errno != MPI_SUCCESS)
        return MPIU_ERR_FAIL(mpi_errno);

    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_get_host_description(
    _Out_writes_z_(len) char* host_description,
    _In_             int   len
    )
{
    int mpi_errno;
    char hostname[100];

    MPIU_Assert(g_init_called);

    if (gethostname(hostname, _countof(hostname)) == SOCKET_ERROR)
    {
        int gle = WSAGetLastError();
        Trace_SOCKETS_Error_MPIDU_Sock_get_host_description(gle, get_error_string(gle));
        return MPIU_E_FAIL_GLE2(gle);
    }

    mpi_errno = MPIDU_Sock_hostname_to_host_description(hostname, host_description, len);
    if (mpi_errno != MPI_SUCCESS)
    {
        return MPIU_ERR_FAIL(mpi_errno);
    }
    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_native_to_sock(
    _In_ ExSetHandle_t set,
    _In_ MPIDU_SOCK_NATIVE_FD fd,
    _Outptr_ sock_state_t **ppSock
    )
{
    sock_state_t *sock_state;

    MPIU_Assert(g_init_called);

    /* setup the structures */
    sock_state = sock_create_state(sock_graceful_close_socket);
    if (sock_state == nullptr)
        return MPIU_ERR_NOMEM();

    sock_state->sock = fd;
    sock_state->set = set;

    /* associate the socket with the completion port */
    ExAttachHandle(set, (HANDLE)sock_state->sock);

    *ppSock = sock_state;

    return MPI_SUCCESS;
}


static void
set_socket_options(
    _In_ SOCKET sock
    )
{
    BOOL nodelay = TRUE;

    /* set the socket buffers */
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&g_socket_rbuffer_size, sizeof(g_socket_rbuffer_size));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&g_socket_sbuffer_size, sizeof(g_socket_sbuffer_size));

    /* disable nagling */
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_create_native_fd(
    _Out_ MPIDU_SOCK_NATIVE_FD* fd
    )
{
    SOCKET s = WSASocketW(PF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (s == INVALID_SOCKET)
    {
        int gle = WSAGetLastError();
        Trace_SOCKETS_Error_MPIDU_Sock_create_native_fd(gle, get_error_string(gle));
        return MPIU_E_FAIL_GLE2(gle);
    }

    set_socket_options(s);

    SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);

    *fd = s;
    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
static int
easy_create_ranged(
    _Out_ SOCKET *sock,
    _In_ int port,
    _In_ unsigned long addr
    )
{
    int mpi_errno;
    SOCKET temp_sock;
    SOCKADDR_IN sockAddr;
    int use_range = 0;

    /* create the socket */
    mpi_errno = MPIDU_Sock_create_native_fd(&temp_sock);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    if (port == 0 && g_min_port != 0 && g_max_port != 0)
    {
        use_range = 1;
        port = g_min_port;
    }

    memset(&sockAddr,0,sizeof(sockAddr));

    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = addr;
    sockAddr.sin_port = _byteswap_ushort(static_cast<u_short>(port));

    for (;;)
    {
        if (bind(temp_sock, (const SOCKADDR*)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR)
        {
            if (use_range)
            {
                port++;
                if (port > g_max_port)
                {
                    Trace_SOCKETS_Error_easy_create_ranged_Port(port, g_max_port);
                    return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**sock|getport");
                }

                sockAddr.sin_port = _byteswap_ushort(static_cast<u_short>(port));
            }
            else
            {
                int gle = WSAGetLastError();
                Trace_SOCKETS_Error_easy_create_ranged(gle, get_error_string(gle));
                return MPIU_E_FAIL_GLE2(gle);
            }
        }
        else
        {
            break;
        }
    }

    *sock = temp_sock;
    return MPI_SUCCESS;
}


static inline int
get_socket_port(
    _In_ SOCKET sock
    )
{
    struct sockaddr_in addr;
    int name_len = sizeof(addr);

    getsockname(sock, (struct sockaddr*)&addr, &name_len);
    return ntohs(addr.sin_port);
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_listen(
    _In_ ExSetHandle_t set,
    _In_ unsigned long addr,
    _Inout_ int *port,
    _Outptr_ sock_state_t **ppSock
    )
{
    int mpi_errno;
    sock_state_t *listen_state;

    MPIU_Assert(g_init_called);

    listen_state = sock_create_state(sock_hard_close_socket);
    if(listen_state == nullptr)
        return MPIU_ERR_NOMEM();

    //
    // Get a bound socket to a port in the range range specified by the
    // MPICH_PORT_RANGE env var.
    //
    mpi_errno = easy_create_ranged(&listen_state->sock, *port, addr);
    if (mpi_errno != MPI_SUCCESS)
        return MPIU_ERR_FAIL(mpi_errno);

    if (listen(listen_state->sock, SOMAXCONN) == SOCKET_ERROR)
    {
        int gle = WSAGetLastError();
        Trace_SOCKETS_Error_MPIDU_Sock_listen(gle, get_error_string(gle));
        return MPIU_E_FAIL_GLE2(gle);
    }

    ExAttachHandle(set, (HANDLE)listen_state->sock);

    *port = get_socket_port(listen_state->sock);
    listen_state->set = set;

    *ppSock = listen_state;
    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
static inline int
post_next_accept(
    _In_ SOCKET listen_sock,
    _Out_ SOCKET* pAcceptSock,
    _Inout_ sock_overlapped_t* pov
    )
{
    int mpi_errno;
    DWORD nBytesReceived;
    SOCKET sock;
    mpi_errno = MPIDU_Sock_create_native_fd(&sock);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    *pAcceptSock = sock;

    if (!AcceptEx(
            listen_sock,
            sock,
            pov->accept.accept_buffer,
            0,
            sizeof(pov->accept.accept_buffer)/2,
            sizeof(pov->accept.accept_buffer)/2,
            &nBytesReceived,
            &pov->exov.ov))
    {
        int gle = WSAGetLastError();
        if (gle == ERROR_IO_PENDING)
            return MPI_SUCCESS;

        Trace_SOCKETS_Error_post_next_accept(gle, get_error_string(gle));

        MPIU_Assert( sock != INVALID_SOCKET);
        closesocket(sock);
        *pAcceptSock = INVALID_SOCKET;
        return MPIU_E_FAIL_GLE2(gle);
    }
    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
static int AcceptFailed(
    _Inout_ EXOVERLAPPED* pexov
    )
{

    int mpi_errno;
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    //
    // The overlapped structure might get freed in the callback, so save
    // any context we need so we don't deref it after the callback.
    //
    sock_state_t* accept_state = pov->accept.accept_state;

    MPIU_Assert( accept_state->sock != INVALID_SOCKET );
    closesocket( accept_state->sock );
    accept_state->sock = INVALID_SOCKET;

    int gle = sock_get_overlapped_result(pov);

    if( gle == WSAECONNRESET )
    {
         mpi_errno = post_next_accept(pov->sock->sock, &pov->accept.accept_state->sock, pov);
         if(mpi_errno == MPI_SUCCESS)
         {
             Trace_SOCKETS_Info_AcceptFailed_ResetPosted();
             return MPI_SUCCESS;
         }
         Trace_SOCKETS_Error_AcceptFailed_ResetPostFailed(mpi_errno);
    }
    else
    {
        Trace_SOCKETS_Error_AcceptFailed(gle, get_error_string(gle));
        mpi_errno = CallbackFailure(pov, gle);
    }

    sock_free_state( accept_state );
    return mpi_errno;
}


static void
sock_finish_accept(
    _Inout_ sock_state_t *accept_state,
    _Inout_ sock_state_t *listener_sock
    )
{
    u_long optval;

    Trace_SOCKETS_Info_sock_finish_accept();

    /* finish the accept */
    setsockopt(accept_state->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char *)&listener_sock->sock, sizeof(listener_sock->sock));

    /* set the socket to non-blocking */
    optval = TRUE;
    ioctlsocket(accept_state->sock, FIONBIO, &optval);

    /* set the socket buffers */
    set_socket_options(accept_state->sock);

    /* associate the socket with the completion port */
    ExAttachHandle(listener_sock->set, (HANDLE)accept_state->sock);

    accept_state->set = listener_sock->set;
    accept_state->pfnClose = sock_graceful_close_socket;
}


_Success_(return==MPI_SUCCESS)
static int
AcceptSucceeded(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    sock_finish_accept(pov->accept.accept_state, pov->sock);
    return CallbackSuccess(pov, 0);
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_accept(
    _In_ sock_state_t *listener_sock,
    _Outptr_ sock_state_t **ppSock,
    _In_ MPIDU_Sock_context_t* psc
    )
{
    int mpi_errno;
    sock_state_t *accept_state;
    sock_overlapped_t* pov = &psc->sov;

    MPIU_Assert(g_init_called);

    accept_state = sock_create_state(sock_hard_close_socket);
    if (accept_state == nullptr)
        return MPIU_ERR_NOMEM();

    sock_init_overlapped(pov, listener_sock, AcceptSucceeded, AcceptFailed);
    pov->accept.accept_state = accept_state;
    *ppSock = accept_state;
    mpi_errno = post_next_accept(listener_sock->sock, &accept_state->sock, pov);
    if (mpi_errno != MPI_SUCCESS)
    {
        *ppSock = nullptr;
        sock_free_state(accept_state);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    return MPI_SUCCESS;
}


static const GUID xGuidConnectEx = WSAID_CONNECTEX;


_Success_(return==MPI_SUCCESS)
static int
gle_connect_ex(
    _In_ SOCKET Socket,
    _In_reads_bytes_(namelen) const struct sockaddr* name,
    _In_ int namelen,
    _Inout_opt_ OVERLAPPED* ov)
{
    int rc;
    BOOL fSucc;
    DWORD BytesReturned;
    LPFN_CONNECTEX pfnConnectEx;

    //
    // Query the entry point every time since different providers
    // have different entry points
    //
    rc = WSAIoctl(
                 Socket,
                 SIO_GET_EXTENSION_FUNCTION_POINTER,
                 (LPVOID)&xGuidConnectEx,
                 sizeof(xGuidConnectEx),
                 (LPVOID)&pfnConnectEx,
                 sizeof(pfnConnectEx),
                 &BytesReturned,
                 nullptr,
                 nullptr
                 );

    if(rc == SOCKET_ERROR)
    {
        int gle = WSAGetLastError();
        Trace_SOCKETS_Error_gle_connect_ex_WSAIoctlSocketError(gle, get_error_string(gle));
        return gle;
    }

    fSucc = pfnConnectEx(
                Socket,
                name,
                namelen,
                nullptr,
                0,
                &BytesReturned,
                ov
                );

    if(fSucc)
    {
        struct sockaddr_in* pSockName = reinterpret_cast<struct sockaddr_in*>(const_cast<struct sockaddr*>(name));
OACR_WARNING_SUPPRESS(26500, "Suppress false anvil warning.")
        Trace_SOCKETS_Info_gle_connect_ex_Succeeded(inet_ntoa(pSockName->sin_addr), pSockName->sin_port);
        return NO_ERROR;
    }

    rc = WSAGetLastError();
    if(rc != WSA_IO_PENDING)
    {
        Trace_SOCKETS_Error_gle_connect_ex_pfnConnectEx(rc, get_error_string(rc));
        return rc;
    }

    return NO_ERROR;
}


_Success_(return==MPI_SUCCESS)
static int
gle_connect_host(
    _Inout_ sock_overlapped_t* pov
    )
{
    const sock_connect_context* scc = &pov->connect;

    struct sockaddr_in sockAddr;
    memset(&sockAddr,0,sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(scc->cur_host);
    sockAddr.sin_port = _byteswap_ushort(static_cast<u_short>(scc->port));
    return gle_connect_ex(pov->sock->sock, (const SOCKADDR*)&sockAddr, sizeof(sockAddr), &pov->exov.ov);
}


#ifndef STATUS_TIMER_EXPIRED
#define STATUS_TIMER_EXPIRED ((DWORD)0xC0000001L)
#endif


static void CALLBACK
ConnectTimerCallback(
    _In_ void* p,
    _In_opt_ BOOLEAN /*TimerFired*/
    )
{
    //
    // *** This function is called on the timer thread. ***
    //

    //
    // Queue the timer expiration callback back to the Executive thread.
    // Post the overlapped with an error status value to invoke the ConnectFailed routine
    // on the Executive thread.
    //
    sock_overlapped_t* pov = (sock_overlapped_t*)p;
    ExPostOverlappedResult(pov->sock->set, &pov->exov, STATUS_TIMER_EXPIRED, 0);
}


_Success_(return==NO_ERROR)
static int
gle_postpone_retry_connect(
    _Inout_ sock_overlapped_t* pov,
    _In_ DWORD DueTime
    )
{
    BOOL fSucc;

    fSucc = CreateTimerQueueTimer(
                &pov->connect.retry_timer,
                nullptr,
                ConnectTimerCallback,
                pov,
                DueTime,
                0,
                WT_EXECUTEDEFAULT
                );

    if(!fSucc)
    {
        int gle = GetLastError();
        Trace_SOCKETS_Error_gle_postpone_retry_connect(gle, get_error_string(gle));
        return gle;
    }

    return NO_ERROR;
}


//
// sock_cancel_inprogress_connect
//
// Helper function to cancel an in progress connect with an outstanding timer.
//
static void
sock_cancel_inprogress_connect(
    _Inout_ sock_state_t *sock
    )
{
    HANDLE Timer;
    sock_overlapped_t* pov = sock->connect.pov;

    struct sockaddr_in addr;
    int name_len = sizeof(addr);

    getsockname(sock->sock, (struct sockaddr*)&addr, &name_len);
    Trace_SOCKETS_Info_sock_cancel_inprogress_connect(inet_ntoa(addr.sin_addr), addr.sin_port);

    if(pov == nullptr)
        return;

    //
    // Set the timer nullptr to help ConnectFailed to identify that close was called
    // and the timer has already been deleted.
    //
    Timer = pov->connect.retry_timer;
    MPIU_Assert(Timer != nullptr);
    pov->connect.retry_timer = nullptr;
    sock->connect.pov = nullptr;

    //
    // Delete the timer and wait for the callback to complete its execution.
    // N.B. The callback function execution is guaranteed, either the timer expires
    //      or the delete timer function runs it down.
    //
    OACR_WARNING_SUPPRESS(RETVAL_IGNORED_FUNC_COULD_FAIL, "Call blocks until timer is deleted.");
    DeleteTimerQueueTimer(nullptr, Timer, INVALID_HANDLE_VALUE);
}


_Success_(return==MPI_SUCCESS)
static int
ConnectFailed(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    int gle;
    int mpi_errno;
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    sock_connect_context* scc = &pov->connect;
    NTSTATUS status = ExGetStatus(&pov->exov);

    if(status == STATUS_TIMER_EXPIRED)
    {
        //
        // The timer has expired; verify race condition with close (the timer set to nullptr).
        //
        if(scc->retry_timer == nullptr)
        {
            //
            // The socket was closed while the timer was armed; return with 'abort' error code.
            // Don't use sock_get_overlapped_result to get the error code, the socket is invalid.
            //
            gle = WSA_OPERATION_ABORTED;
            Trace_SOCKETS_Error_ConnectFailed(ConnectFailedEnumAbortedBeforeTimeout, status, pexov, scc->cur_host, scc->port);
            goto fn_fail_gle;
        }

        //
        // Delete the timer (no wait) and go try connecting again. No need to update the
        // retry count, it was incremented when the timer was armed down below.
        //
        Trace_SOCKETS_Info_ConnectFailed(ConnectFailedEnumTimeout, status, pexov, scc->cur_host, scc->port);
        BOOL timer_marked_for_deletion = DeleteTimerQueueTimer(nullptr, pov->connect.retry_timer, nullptr);
        while(!timer_marked_for_deletion)
        {
            DWORD last_error = GetLastError();
            if(last_error == ERROR_IO_PENDING)
            {
                break;
            }
            else
            {
                timer_marked_for_deletion = DeleteTimerQueueTimer(nullptr, pov->connect.retry_timer, nullptr);
            }
        }
        scc->retry_timer = nullptr;
        pov->sock->connect.pov = nullptr;
        goto fn_connect;
    }

    //
    // Check if this sock is closed or is being closed.
    //
    // N.B. Accessing the sock object here is safe only with asynchronous close which guarantee
    //      that all async completion routines are executed before the the sock object is deleted.
    //      Using sync close while connect is in progress will result in AV or memory corruption.
    //
    if(pov->sock->closing)
    {
        //
        // The sock is closing do not retry to connect again.
        //
        gle = WSA_OPERATION_ABORTED;
        Trace_SOCKETS_Error_ConnectFailed(ConnectFailedEnumAbortedClosing, status, pexov, scc->cur_host, scc->port);
        goto fn_fail_gle;
    }

    scc->retry_count++;
    gle = sock_get_overlapped_result(pov);
    MPIU_Assert(gle != WSA_OPERATION_ABORTED);
    if((gle == WSAECONNREFUSED || gle == WSAETIMEDOUT) && scc->retry_count <= g_connect_retries)
    {
        //
        // Connection was refused, wait and retry.
        //
        DWORD t = scc->retry_count * (rand() % 256 + 16);
        gle = gle_postpone_retry_connect(pov, t);
        if(gle != NO_ERROR)
            goto fn_fail_gle;

        //
        // Save the overlapped to be used by close while the timer has not expired.
        //
        Trace_SOCKETS_Info_ConnectFailed(ConnectFailedEnumRefused, gle, pexov, scc->cur_host, scc->port);
        pov->sock->connect.pov = pov;
        return MPI_SUCCESS;
    }

    //
    // Capture the connect error
    //
    Trace_SOCKETS_Info_ConnectFailed(ConnectFailedEnumError, gle, pexov, scc->cur_host, scc->port);
    scc->error = MPIU_ERR_GET(scc->error, "**sock_connect %s %d %s %d", scc->cur_host, scc->port, get_error_string(gle), gle);

    //
    // Move to the next host in the list
    //
    scc->retry_count = 0;
    scc->cur_host = scc->cur_host + MPIU_Strlen( scc->cur_host ) + 1;
    if(*scc->cur_host == '\0')
    {
        Trace_SOCKETS_Error_ConnectFailed(ConnectFailedEnumExhausted, gle, pexov, scc->cur_host, scc->port);
        mpi_errno = MPIU_ERR_GET(scc->error, "**sock_connect %s %d %s", scc->host_description, scc->port, "exhausted all endpoints");
        return CallbackFailure(pov, mpi_errno);
    }

fn_connect:
    gle = gle_connect_host(pov);
    if(gle != NO_ERROR)
        goto fn_fail_gle;

    return MPI_SUCCESS;

fn_fail_gle:
    Trace_SOCKETS_Error_ConnectFailed(ConnectFailedEnumFail, gle, pexov, scc->cur_host, scc->port);
    mpi_errno = MPIU_E_FAIL_GLE2(gle);
    return CallbackFailure(pov, mpi_errno);
}


static void
sock_finish_connect(
    _In_ sock_state_t *connect_state
    )
{
    DWORD sockoptval;
    u_long optval;

    struct sockaddr_in addr;
    int name_len = sizeof(addr);

    getsockname(connect_state->sock, (struct sockaddr*)&addr, &name_len);
    Trace_SOCKETS_Info_sock_finish_connect(inet_ntoa(addr.sin_addr), addr.sin_port);

    /* update winsock connect context */
    sockoptval = 1;
    setsockopt(connect_state->sock, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, (const char*)&sockoptval, sizeof(DWORD));

    /* set the socket to non-blocking */
    optval = TRUE;
    ioctlsocket(connect_state->sock, FIONBIO, &optval);

    connect_state->pfnClose = sock_graceful_close_socket;
}


_Success_(return==MPI_SUCCESS)
static int
ConnectSucceeded(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    //
    // Check to see if the socket has been closed.
    //
    if(!pov->sock->closing)
    {
        sock_finish_connect(pov->sock);
    }
    return CallbackSuccess(pov, 0);
}


_Success_(return==NO_ERROR)
static int
gle_bind_any(
    _In_ SOCKET Socket
    )
{
    int rc;

    struct sockaddr_in sockAddr;
    memset(&sockAddr,0,sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = INADDR_ANY;
    sockAddr.sin_port = 0;

    rc = bind(Socket, (const SOCKADDR*)&sockAddr, sizeof(sockAddr));
    if(rc == SOCKET_ERROR)
        return WSAGetLastError();

    return NO_ERROR;
}


//
// This function saves the valid hosts lists from the host_description into a save
// hosts string. Each name ends with a '\0' the last entry ends with a double '\0'.
// N.B. that if the host_description is empty the output would be a single '\0'.
//
_Success_(return==MPI_SUCCESS)
static int
save_valid_endpoints(
    _In_z_ const char* host_description,
    _Out_writes_z_(len) char* hosts,
    _In_ size_t len,
    _In_ int port,
    _In_ int usemask
    )
{
    size_t n;
    int str_errno;
    int address_saved = 0;
    int address_valid = 0;
    struct hostent* lphost;
    struct in_addr addr;
    int mpi_errno = MPI_SUCCESS;

    const char* p = host_description;

    for(;;)
    {
        *hosts = '\0';

        str_errno = MPIU_Str_get_string(&p, hosts, len);
        if (str_errno != MPIU_STR_SUCCESS)
            return MPIU_ERR_GET(mpi_errno, "**fail %d", str_errno);

        n = MPIU_Strlen( hosts, len );
        if(n == 0)
        {
            char no_endpoint[128];
            const char* msg = no_endpoint;

            if(address_saved > 0)
                return MPI_SUCCESS;

            if(address_valid == 0)
            {
                msg = "no endpoints";
            }
            else
            {
                MPIU_Szncpy(no_endpoint, "no endpoint matches the netmask ");
                MPIU_Sznapp(no_endpoint, inet_ntoa(g_netaddr));
                MPIU_Sznapp(no_endpoint, "/");
                MPIU_Sznapp(no_endpoint, inet_ntoa(g_netmask));
                msg = no_endpoint;
            }

            return MPIU_ERR_GET(mpi_errno, "**sock_connect %s %d %s", host_description, port, msg);
        }


        addr.s_addr = inet_addr(hosts);

        if (addr.s_addr == INADDR_NONE || addr.s_addr == 0)
        {
            lphost = gethostbyname(hosts);
            if (lphost != nullptr)
            {
                const char* s = inet_ntoa( *((struct in_addr*)lphost->h_addr) );
                n = MPIU_Strlen( s, _countof("xxx.xxx.xxx.xxx") );
                MPIU_Strncpy(hosts, s, len);
            }
            else
            {
                //
                // Because the detailed error message calls get_error_string, the last error
                // value could be overwritten, so we save it here so that it is preserved
                // properly.
                //
                int gle = WSAGetLastError();
                mpi_errno = MPIU_ERR_GET(mpi_errno, "**gethostbyname %s %d", get_error_string(gle), gle);
                continue;
            }
        }

        address_valid++;

        /* if a subnet was specified, make sure the currently extracted ip falls in the subnet */
        if (usemask)
        {
            if ((addr.s_addr & g_netmask.s_addr) != g_netaddr.s_addr)
            {
                /* this ip does not match, move to the next */
                continue;
            }
        }

        address_saved++;

        //
        // Adjust hosts buffer and the len left in that buffer
        //
        hosts += n + 1;
        len -= n + 1;
    }
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_connect(
    _In_    ExSetHandle_t         set,
    _In_z_  const char*           host_description,
    _In_    int                   port,
    _Outptr_ sock_state_t**       ppSock,
    _In_    int                   usemask,
    _Inout_ MPIDU_Sock_context_t* psc
    )
{
    int mpi_errno;
    DWORD gle;
    sock_state_t *connect_state;
    sock_overlapped_t* pov = &psc->sov;

    MPIU_Assert(ppSock);
    MPIU_Assert(g_init_called);

    *ppSock = nullptr;

    if( MPIU_Strlen( host_description, SOCKI_DESCRIPTION_LENGTH + 1 ) == SIZE_MAX )
    {
        return MPIU_ERR_NOMEM();
    }

    /* setup the structures */
    connect_state = sock_create_state(sock_hard_close_socket);
    if (connect_state == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    connect_state->set = set;

    sock_init_overlapped(pov, connect_state, ConnectSucceeded, ConnectFailed);

    mpi_errno = save_valid_endpoints(host_description, pov->connect.host_description, _countof(pov->connect.host_description), port, usemask);
    if (mpi_errno != MPI_SUCCESS)
    {
        Trace_SOCKETS_Error_MPIDU_Sock_post_connect_endpoints(mpi_errno, host_description, port);
        sock_free_state(connect_state);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    /* create a socket */
    mpi_errno = MPIDU_Sock_create_native_fd(&connect_state->sock);
    if (mpi_errno != MPI_SUCCESS)
    {
        sock_free_state(connect_state);
        return MPIU_ERR_FAIL(mpi_errno);
    }

    //
    // ConnectEx requires the socket to be bound
    //
    gle = gle_bind_any(connect_state->sock);
    if(gle != NO_ERROR)
    {
        *ppSock = nullptr;
        Trace_SOCKETS_Error_MPIDU_Sock_post_connect_gle_bind_any(gle, get_error_string(gle));
        sock_free_state(connect_state);
        return MPIU_E_FAIL_GLE2(gle);
    }
    MPIU_Assert(connect_state->sock != INVALID_SOCKET);
    /* associate the socket with the completion port */
    ExAttachHandle(set, (HANDLE)connect_state->sock);

    pov->connect.error = MPI_SUCCESS;
    pov->connect.port = port;
    pov->connect.retry_count = 0;
    pov->connect.retry_timer = nullptr;
    pov->connect.cur_host = pov->connect.host_description;

    gle = gle_connect_host(pov);
    if(gle != NO_ERROR)
    {
        sock_free_state(connect_state);
        return MPIU_E_FAIL_GLE2(gle);
    }

    *ppSock = connect_state;

    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
static int HardCloseComplete(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    sock_free_state(pov->sock);
    return CallbackSuccess(pov, 0);
}


void
sock_hard_close_socket(
    _Inout_ sock_state_t *sock,
    _Inout_ sock_overlapped_t* pov
    )
{
    sock_cancel_inprogress_connect(sock);
    MPIU_Assert( sock->sock != INVALID_SOCKET );
    closesocket(sock->sock);
    sock->sock = INVALID_SOCKET;

    sock_init_overlapped(pov, sock, HardCloseComplete, HardCloseComplete);
    ExPostOverlappedResult(sock->set, &pov->exov, 0, 0);
}


_Success_(return==MPI_SUCCESS)
static int
GracefulCloseFailed(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    sock_state_t *sock = pov->sock;

    //
    // Nothing to do here, receive failed to read the FD_CLOSE signal (zero bytes on receive)
    // close the socket the hard way.
    //
    MPIU_Assert( sock->sock != INVALID_SOCKET );
    if (closesocket(sock->sock) == SOCKET_ERROR)
    {
        int gle = WSAGetLastError();
        struct sockaddr_in addr;
        int len = sizeof(addr);
        getsockname(sock->sock, reinterpret_cast<struct sockaddr*>(&addr), &len);
        Trace_SOCKETS_Error_GracefulCloseFailed(gle, get_error_string(gle), inet_ntoa(addr.sin_addr), addr.sin_port);
        int mpi_errno = MPIU_E_FAIL_GLE2(gle);
        return CallbackFailure(pov, mpi_errno);
    }
    sock->sock = INVALID_SOCKET;
    sock_free_state(sock);
    return CallbackSuccess(pov, 0);
}


_Success_(return==MPI_SUCCESS)
static int
GracefulDummyRecv(
    _In_ const sock_state_t *sock,
    _Inout_ sock_overlapped_t* pov)
{
    static char dummy_read_buffer[16];
    static WSABUF dummy_iov = { sizeof(dummy_read_buffer), dummy_read_buffer };

    return sock_safe_receive(sock->sock, &dummy_iov, 1, &pov->exov.ov);
}


_Success_(return==MPI_SUCCESS)
static int
GracefulCloseSucceeded(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    DWORD num_bytes = ExGetBytesTransferred(pexov);
    sock_state_t *sock = pov->sock;

    if(num_bytes == 0)
    {
        //
        // This is a graceful shutdown, FD_CLOSE received. Free the socket
        //
        Trace_SOCKETS_Info_GracefulCloseSucceeded();

        closesocket(sock->sock);
        sock->sock = INVALID_SOCKET;
        sock_free_state(sock);
        return CallbackSuccess(pov, 0);
    }

    //
    // The other side is still sending data; repost the dummy receive to
    // identify the FD_CLOSE signal.
    //
    if(GracefulDummyRecv(sock, pov) == NO_ERROR)
        return MPI_SUCCESS;

    return GracefulCloseFailed(&pov->exov);
}


void
sock_graceful_close_socket(
    _Inout_ sock_state_t *sock,
    _Inout_ sock_overlapped_t* pov
    )
{
    /* Mark the socket as non-writable */
    if (shutdown(sock->sock, SD_SEND) == SOCKET_ERROR)
    {
        sock_hard_close_socket(sock, pov);
        return;
    }

    //
    // Post a dummy receive to identify the FD_CLOSE signal
    //
    sock_init_overlapped(pov, sock, GracefulCloseSucceeded, GracefulCloseFailed);
    if(GracefulDummyRecv(sock, pov) == NO_ERROR)
        return;

    sock_hard_close_socket(sock, pov);
}


void
MPIDU_Sock_post_close(
    _Inout_ sock_state_t *sock,
    _Inout_ MPIDU_Sock_context_t* psc
    )
{
    sock_overlapped_t* pov = &psc->sov;

    MPIU_Assert(g_init_called);
    MPIU_Assert(!sock->closing);
    sock->closing = TRUE;

    sock->pfnClose(sock, pov);
}


_Success_(return==MPI_SUCCESS)
static int
ReadFailed(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    int mpi_errno = MPIU_E_FAIL_GLE2(sock_get_overlapped_result(pov));
    return CallbackFailure(pov, mpi_errno);
}


_Success_(return==MPI_SUCCESS)
static int
ReadSucceeded(
    _Inout_ EXOVERLAPPED* pexov)
{
    int gle;
    int mpi_errno;
    DWORD num_bytes = ExGetBytesTransferred(pexov);
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);

    if(num_bytes == 0)
    {
        //
        // Use MPI_SUCCESS as the error *class* to indicate graceful close
        //
        Trace_SOCKETS_Error_ReadSucceeded_ConnectionClosed();
        mpi_errno = MPIU_ERR_CREATE(MPI_SUCCESS, "**sock|connclosed");
        return CallbackFailure(pov, mpi_errno);
    }

    pov->read.total += num_bytes;
    TrimIOV(&pov->read.iov, &pov->read.iovlen, num_bytes);

    if((pov->read.iovlen == 0) || (pov->read.total >= pov->read.min_recv))
        return CallbackSuccess(pov, pov->read.total);

    /* post a read of the remaining data */
    gle = sock_safe_receive(pov->sock->sock, pov->read.iov, pov->read.iovlen, &pov->exov.ov);
    if(gle == NO_ERROR)
        return MPI_SUCCESS;

    Trace_SOCKETS_Error_ReadSucceeded_Error(gle, get_error_string(gle));

    mpi_errno = MPIU_E_FAIL_GLE2(gle);
    return CallbackFailure(pov, mpi_errno);
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_read(
    _Inout_ sock_state_t *sock,
    _Out_writes_bytes_(len) void * buf,
    _In_ MPIU_Bsize_t len,
    _In_ MPIU_Bsize_t minbr,
    _Inout_ MPIDU_Sock_context_t* psc
    )
{
    sock_overlapped_t* pov = &psc->sov;
    pov->read.tmpiov.len = len;
    pov->read.tmpiov.buf = static_cast<CHAR*>( buf );
    return MPIDU_Sock_post_readv(sock, &pov->read.tmpiov, 1, minbr, psc);
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_readv(
    _Inout_ sock_state_t *sock,
    _In_reads_(iov_n)WSABUF * iov,
    _In_ int iov_n,
    _In_ MPIU_Bsize_t minbr,
    _Inout_ MPIDU_Sock_context_t* psc
    )
{
    int gle;
    sock_overlapped_t* pov = &psc->sov;

    MPIU_Assert(g_init_called);

    sock_init_overlapped(pov, sock, ReadSucceeded, ReadFailed);

    /* strip any trailing empty buffers */
    while (iov_n && iov[iov_n-1].len == 0)
    {
        iov_n--;
    }

    pov->read.iov = iov;
    pov->read.iovlen = iov_n;
    pov->read.min_recv = minbr;
    pov->read.total = 0;

    gle = sock_safe_receive(sock->sock, pov->read.iov, iov_n, &pov->exov.ov);
    if(gle != NO_ERROR)
        return MPIU_E_FAIL_GLE2(gle);

    return MPI_SUCCESS;
}


_Success_(return==MPI_SUCCESS)
static int
WriteFailed(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);
    int mpi_errno = MPIU_E_FAIL_GLE2(sock_get_overlapped_result(pov));
    return CallbackFailure(pov, mpi_errno);
}


_Success_(return==MPI_SUCCESS)
static int
WriteSucceeded(
    _Inout_ EXOVERLAPPED* pexov
    )
{
    int gle;
    int mpi_errno;
    DWORD nBytesSent;
    DWORD num_bytes = ExGetBytesTransferred(pexov);
    sock_overlapped_t* pov = sock_ov_from_exov(pexov);

    pov->write.total += num_bytes;
    TrimIOV(&pov->write.iov, &pov->write.iovlen, num_bytes);

    if (pov->write.iovlen == 0)
        return CallbackSuccess(pov, pov->write.total);

    /* post a write of the remaining data */
    gle = sock_safe_send(pov->sock->sock, pov->write.iov, pov->write.iovlen, &nBytesSent, &pov->exov.ov);
    if(gle == NO_ERROR)
        return MPI_SUCCESS;

    mpi_errno = MPIU_E_FAIL_GLE2(gle);
    return CallbackFailure(pov, mpi_errno);
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_write(
    _Inout_ sock_state_t *sock,
    _In_reads_bytes_(min) const void* buf,
    _In_ MPIU_Bsize_t min,
    _Inout_ MPIDU_Sock_context_t* psc
    )
{
    sock_overlapped_t* pov = &psc->sov;
    pov->write.tmpiov.len = min;
    pov->read.tmpiov.buf = static_cast<CHAR*>( const_cast<void*>( buf ) );
    return MPIDU_Sock_post_writev(sock, &pov->write.tmpiov, 1, psc);
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_writev(
    _Inout_ sock_state_t *sock,
    _In_reads_(iov_n) WSABUF* iov,
    _In_ int iov_n,
    _Inout_ MPIDU_Sock_context_t* psc
    )
{
    int gle;
    DWORD nBytesSent;
    sock_overlapped_t* pov = &psc->sov;

    sock_init_overlapped(pov, sock, WriteSucceeded, WriteFailed);

    /* strip any trailing empty buffers */
    while (iov_n && iov[iov_n-1].len == 0)
    {
        iov_n--;
    }

    pov->write.iov = iov;
    pov->write.iovlen = iov_n;
    pov->write.total = 0;

    gle = sock_safe_send(sock->sock, pov->write.iov, iov_n, &nBytesSent, &pov->exov.ov);
    if(gle != NO_ERROR)
        return MPIU_E_FAIL_GLE2(gle);

    return MPI_SUCCESS;
}


void
MPIDU_Sock_close(
    _In_ _Post_invalid_ sock_state_t *sock
    )
{
    MPIU_Assert(g_init_called);
    MPIU_Assert(!sock->closing);

    //
    // Synchronous close can not be used while connect is in progress becaues the sock state
    // is being freed here. Doing so will result in Access Violation or Memroy Corruption. (see
    // comment in ConnectFailed). The assertion below is only a partial check to see that no
    // connect timer is active. There is no validation when the a connect is actually posted.
    //
    MPIU_Assert(sock->connect.pov == nullptr);

    MPIU_Assert( sock->sock != INVALID_SOCKET );
    CancelIoEx( reinterpret_cast<HANDLE>(sock->sock), nullptr );
    closesocket(sock->sock);
    sock->sock = INVALID_SOCKET;
    sock_free_state(sock);
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_writev(
    _In_ const sock_state_t * const sock,
    _In_reads_(iov_n) WSABUF * iov,
    _In_ int iov_n,
    _Out_ MPIU_Bsize_t * num_written
    )
{
    DWORD num_written_local;
    int gle = sock_safe_send(sock->sock, iov, iov_n, &num_written_local, nullptr /*overlapped*/);
    if(gle == NO_ERROR)
    {
        *num_written = num_written_local;
        return MPI_SUCCESS;
    }

    *num_written = 0;

    if(gle == WSAEWOULDBLOCK)
        return MPI_SUCCESS;

    return MPIU_E_FAIL_GLE2(gle);
}


_Success_(return >=0)
int
MPIDU_Sock_get_sock_id(
    _In_ const sock_state_t * const sock
    )
{
    if (sock == MPIDU_SOCK_INVALID_SOCK)
        return -1;

    return (int)sock->sock;
}


_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_keepalive(
    _In_ const sock_state_t * const sock
    )
{
    int rc;
    DWORD nbytes;

    //
    // After 5 minutes of no network activity send up to ten keep-alive packets,
    // while waiting 10 seconds between successive unacknowledged packets.
    //
    struct tcp_keepalive ka;
    ka.onoff = 1;
    ka.keepalivetime = 5*60*1000;
    ka.keepaliveinterval = 10*1000;

    rc = WSAIoctl(sock->sock, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &nbytes, nullptr, nullptr);
    if(rc == NO_ERROR)
        return MPI_SUCCESS;

    rc = WSAGetLastError();
    Trace_SOCKETS_Error_MPIDU_Sock_keepalive(rc, get_error_string(rc));
    return MPIU_E_FAIL_GLE2(rc);
}
