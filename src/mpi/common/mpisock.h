// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#if !defined(MPIDU_SOCK_H_INCLUDED)
#define MPIDU_SOCK_H_INCLUDED

#include "mpidef.h"
#include "ex.h"


#define MPIDU_SOCK_INVALID_SOCK   NULL
#define MPIDU_SOCK_INFINITE_TIME   INFINITE

typedef SOCKET MPIDU_SOCK_NATIVE_FD;


//
// Allocated per request
//
typedef struct sock_read_context
{
    WSABUF tmpiov;
    WSABUF *iov;
    MPIU_Bsize_t total;
    MPIU_Bsize_t min_recv;
    int iovlen;

} sock_read_context;


typedef struct sock_write_context
{
    WSABUF tmpiov;
    WSABUF *iov;
    MPIU_Bsize_t total;
    int iovlen;

} sock_write_context;


struct sock_state_t;


typedef struct sock_accept_context
{
    sock_state_t* accept_state;
    char accept_buffer[sizeof(struct sockaddr_in)*2+32];

} sock_accept_context;


#define SOCKI_DESCRIPTION_LENGTH    256
typedef struct sock_connect_context
{
    HANDLE retry_timer;
    const char* cur_host;
    int error;
    int port;
    int retry_count;
    char host_description[SOCKI_DESCRIPTION_LENGTH];

} sock_connect_context;


typedef struct sock_close_context
{
    int closectx;

} sock_close_context;


typedef struct sock_overlapped_s
{
    EXOVERLAPPED exov;
    sock_state_t* sock;

    union
    {
        sock_read_context read;
        sock_write_context write;
        sock_accept_context accept;
        sock_connect_context connect;
        sock_close_context close;
    };

} sock_overlapped_t;


typedef void (*sock_close_routine)(
    _Inout_ sock_state_t* sock,
    _Inout_ sock_overlapped_t* pov
    );


//
// Allocated per socket
//
typedef struct sock_connect_state_t
{
    sock_overlapped_t* pov;

} sock_connect_state_t;


typedef struct sock_state_t
{
    sock_close_routine pfnClose;
    SOCKET sock;
    ExSetHandle_t set;
    int closing;
    sock_connect_state_t connect;

} sock_state_t;


typedef struct MPIDU_Sock_context_t
{

    //
    // Caller Executive overlapped.
    // * The success/failure callback functions will be invoked on Scok async
    //   operation completion when MPIDU_Sock_wait is called.
    // * The total number of bytes transfered in a successful read/write
    //   operation is in uov.ov.InernalHigh field of the OVERLAPPED strcture.
    // * The Sock MPI error value is in uov.ov.Internal
    //
    EXOVERLAPPED uov;

    //
    // Sock private context
    //
    sock_overlapped_t sov;

} MPIDU_Sock_context_t;


//
// Parses the netmask from a given environment variable.
//
HRESULT ParseNetmask( _In_z_ PCWSTR szNetmaskEnv, _Out_ IN_ADDR* pAddr, _Out_ IN_ADDR* pMask );


/*@
MPIDU_Sock_init - initialize the Sock communication library

Return value: MPI error code
. MPI_SUCCESS - initialization completed successfully

Notes:
The Sock module may be initialized multiple times.  The implementation should perform reference counting if necessary.

Module:
Utility-Sock
@*/

_Success_(return==MPI_SUCCESS)
int MPIDU_Sock_init(void);


/*@
MPIDU_Sock_finalize - shutdown the Sock communication library

Return value: MPI error code
. MPI_SUCCESS - shutdown completed successfully

Notes:
<BRT> What are the semantics of finalize?  Is it responsible for releasing any resources (socks and sock sets) that the calling
code(s) leaked?  Should it block until all OS resources are released?

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int MPIDU_Sock_finalize(void);


/*@
MPIDU_Sock_get_host_description - obtain a description of the host's
communication capabilities

Input Parameters:
. host_description - character array in which the function can store a string
  describing the communication capabilities of the host
- len - length of the character array

Return value: MPI error code
. MPI_SUCCESS - description successfully obtained and placed in host_description

Notes:
The host description string returned by the function is defined by the
implementation and should not be interpreted by the
application.  This string is to be supplied to MPIDU_Sock_post_connect() when
one wishes to form a connection with this host.

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_get_host_description(
    _Out_writes_z_(len) char* host_description,
    _In_             int   len
    );

/*@
MPIDU_Sock_hostname_to_host_description - convert a host name to a description of the host's communication capabilities

Input Parameters:
+ hostname - host name string
. host_description - character array in which the function can store a string describing the communication capabilities of the host
- len - length of host_description

Return value: MPI error code
. MPI_SUCCESS - description successfully obtained and placed in host_description

Notes:
The host description string returned by the function is defined by the implementation and should not be interpreted by the
application.  This string is to be supplied to MPIDU_Sock_post_connect() when one wishes to form a connection with the host
specified by hostname.

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_hostname_to_host_description(
    _In_z_ const char* hostname,
    _Out_writes_z_(len) char* host_description,
    _In_ int len
    );


/*@
MPIDU_Sock_create_native_fd - create a new native socket descriptor/handle

Output Parameter:
. fd - pointer to the new socket handle

Return value: MPI error code
. MPI_SUCCESS - new sock set successfully create

Notes:

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_create_native_fd(
    _Out_ MPIDU_SOCK_NATIVE_FD* fd
    );


/*@
MPIDU_Sock_native_to_sock - convert a native file descriptor/handle to a sock object

Input Parameters:
+ set - sock set to which the new sock should be added
. fd - native file descriptor

Output Parameter:
. sock - new sock object

Return value: MPI error code
. MPI_SUCCESS - sock successfully created

Notes:
The constraints on which file descriptors/handles may be converted to a sock object are defined by the implementation.
It is possible, however, that the conversion of an inappropriate descriptor/handle may complete successfully but the
sock object may not function properly.

Thread safety:
The addition of a new sock object to the sock set may occur while other threads are performing operations on the same sock set.
Thread safety of simultaneously operations on the same sock set must be guaranteed by the Sock implementation.

@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_native_to_sock(
    _In_ ExSetHandle_t set,
    _In_ MPIDU_SOCK_NATIVE_FD fd,
    _Outptr_ sock_state_t **ppSock
    );


/*@
MPIDU_Sock_listen - establish a listener sock

Input Parameters:
+ set - sock set to which the listening sock should be added
- port - desired port (or zero if a specific port is not desired)

Output Parameters:
+ port - port assigned to the listener
- sock - new listener sock

Return value: MPI error code
. MPI_SUCCESS - listener sock successfully established

Notes:
Use the established listener socket to call MPIDU_Sock_post_accept

The environment variable MPICH_PORTRANGE=min:max may be used to restrict the ports mpich processes listen on.

Thread safety:
The addition of the listener sock object to the sock set may occur while other threads are performing operations on the same sock
set.  Thread safety of simultaneously operations on the same sock set must be guaranteed by the Sock implementation.

Module:
Utility-Sock
@*/

_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_listen(
    _In_ ExSetHandle_t set,
    _In_ unsigned long addr,
    _Inout_ int *port,
    _Outptr_ sock_state_t **ppSock
    );


/*@
MPIDU_Sock_post_accept - request that a new connection would be accepted

Input Parameters:
+ listener_sock - listener sock object from which to obtain the new connection
- pov - user context associated with the accept request

Output Parameter:
. pSock - sock object for the new connection

Return value: MPI error code
. MPI_SUCCESS - new connection successfully established and associated with new sock objecta

Notes:
In the event of a connection failure, MPIDU_Sock_post_accept() may fail to acquire and return a new sock despite any
MPIDU_SOCK_OP_ACCEPT event notification.  On the other hand, MPIDU_Sock_post_accept() may return a sock for which the underlying
connection has already failed.  (The Sock implementation may be unaware of the failure until read/write operations are performed.)

Thread safety:
The addition of the new sock object to the sock set may occur while other threads are performing operations on the same sock set.
Thread safety of simultaneously operations on the same sock set must be guaranteed by the Sock implementation.

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_accept(
    _In_ sock_state_t *listener_sock,
    _Outptr_ sock_state_t **ppSock,
    _In_ MPIDU_Sock_context_t* psc
    );


/*@
MPIDU_Sock_post_connect - request that a new connection be formed

Input Parameters:
+ set - sock set to which the new sock object should be added
. host_description - string containing the communication capabilities of the listening host
+ port - port number of listener sock on the listening host
. pov - user context associated with the connect request

Output Parameter:
. sock - new sock object associated with the connection request

Return value: MPI error code
. MPI_SUCCESS - request to form new connection successfully posted

Notes:
The host description of the listening host is supplied MPIDU_Sock_get_host_description().  The intention is that the description
contain an enumeration of interface information so that the MPIDU_Sock_connect() can try each of the interfaces until it succeeds
in forming a connection.  Having a complete set of interface information also allows a particular interface be used selected by the
user at runtime using the MPICH_NETMASK.  <BRT> The name of the environment variable seems wrong.  Perhaps MPICH_INTERFACE?  We
should ask the Systems group.

Thread safety:
The addition of the new sock object to the sock set may occur while other threads are performing operations on the same sock set.
Thread safety of simultaneously operations on the same sock set must be guaranteed by the Sock implementation.

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_connect(
    _In_    ExSetHandle_t         set,
    _In_z_  const char*           host_description,
    _In_    int                   port,
    _Outptr_ sock_state_t**       ppSock,
    _In_    int                   usemask,
    _Inout_ MPIDU_Sock_context_t* psc
    );


/*@
MPIDU_Sock_post_close - request that an existing connection be closed

Input Parameter:
. sock - sock object to be closed
- pov - user context associated with the close request

Return value: MPI error code
. MPI_SUCCESS - request to close the connection was successfully posted

Notes:
If any other operations are posted on the specified sock, they will be terminated.  An appropriate event will be generated for each
terminated operation.  All such events will be delivered by MPIDU_Sock_wait() prior to the delivery of the MPIDU_SOCK_OP_CLOSE
event.

The sock object is destroyed just prior to the MPIDU_SOCK_OP_CLOSE event being returned by MPIDU_Sock_wait().  Any oustanding
references to the sock object held by the application should be considered invalid and not used again.

Thread safety:
MPIDU_Sock_post_close() may be called while another thread is calling or blocking in MPIDU_Sock_wait() specifying the same sock set
to which this sock belongs.  If another thread is blocking MPIDU_Sock_wait() and the close operation causes the sock set to become
empty, then MPIDU_Sock_wait() will return with an error.

Calling any of the immediate or post routines during or after the call to MPIDU_Sock_post_close() is consider an application error.
The result of doing so is undefined.  The application should coordinate the closing of a sock with the activities of other threads
to ensure that simultaneous calls do not occur.

Module:
Utility-Sock
@*/

void
MPIDU_Sock_post_close(
    _Inout_ sock_state_t *sock,
    _Inout_ MPIDU_Sock_context_t* psc
    );

/*@
MPIDU_Sock_post_read - request that data be read from a sock

Input Parameters:
+ sock - sock object from which data is to be read
. buf - buffer into which the data should be placed
. len - number of bytes to read
. minbr - the async operation can return with number of bytes read greater or
          equal to minbr (min bar) before the entire buffer is read.
. pov - user context associated with the read request

Return value: MPI error code
. MPI_SUCCESS - request to read was successfully posted

Notes:
Only one read operation may be posted at a time.  Furthermore, an immediate read may not be performed while a posted write is
outstanding.  This is considered to be an application error, and the results of doing so are undefined.

If MPIDU_Sock_post_close() is called before the posted read operation completes, the read operation will be terminated.

Thread safety:
MPIDU_Sock_post_read() may be called while another thread is attempting to perform an immediate write or post a write operation on
the same sock.  MPIDU_Sock_post_read() may also be called while another thread is calling or blocking in MPIDU_Sock_wait() on the
same sock set to which the specified sock belongs.

MPIDU_Sock_post_write() may not be called while another thread is performing an immediate read on the same sock.  This is
considered to be an application error, and the results of doing so are undefined.

Calling MPIDU_Sock_post_read() during or after the call to MPIDU_Sock_post_close() is consider an application error.  The result of
doing so is undefined.  The application should coordinate the closing of a sock with the activities of other threads to ensure that
one thread is not attempting to post a new operation while another thread is attempting to close the sock.

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_read(
    _Inout_ sock_state_t *sock,
    _Out_writes_bytes_(len) void * buf,
    _In_ MPIU_Bsize_t len,
    _In_ MPIU_Bsize_t minbr,
    _Inout_ MPIDU_Sock_context_t* psc
    );

/*@
MPIDU_Sock_post_readv - request that a vector of data be read from a sock

Input Parameters:
+ sock - sock object from which the data is to read
. iov - I/O vector describing buffers into which the data is placed
. iov_n - number of elements in I/O vector (must be 1 currently)
. minbr - the async operation can return with number of bytes read greater or
          equal to minbr (min bar) before the entire buffer is read.
. pov - user context associated with the readv request

Return value: MPI error code
. MPI_SUCCESS - request to read was successfully posted

Notes:
Only one read operation may be posted at a time.  Furthermore, an immediate read may not be performed while a posted write is
outstanding.  This is considered to be an application error, and the results of doing so are undefined.

If MPIDU_Sock_post_close() is called before the posted read operation completes, the read operation will be terminated.

Thread safety:
MPIDU_Sock_post_readv() may be called while another thread is attempting to perform an immediate write or post a write operation on
the same sock.  MPIDU_Sock_post_readv() may also be called while another thread is calling or blocking in MPIDU_Sock_wait() on the
same sock set to which the specified sock belongs.

MPIDU_Sock_post_readv() may not be called while another thread is performing an immediate read on the same sock.  This is
considered to be an application error, and the results of doing so are undefined.

Calling MPIDU_Sock_post_readv() during or after the call to MPIDU_Sock_post_close() is consider an application error.  The result
of doing so is undefined.  The application should coordinate the closing of a sock with the activities of other threads to ensure
that one thread is not attempting to post a new operation while another thread is attempting to close the sock.

Module:
Utility-Sock
@*/

_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_readv(
    _Inout_ sock_state_t *sock,
    _In_reads_(iov_n)WSABUF * iov,
    _In_ int iov_n,
    _In_ MPIU_Bsize_t minbr,
    _Inout_ MPIDU_Sock_context_t* psc
    );

/*@
MPIDU_Sock_post_write - request that data be written to a sock

Input Parameters:
+ sock - sock object which the data is to be written
. buf - buffer containing the data
. len - number of bytes to write
. pov - user context associated with the write request

Return value: MPI error code
. MPI_SUCCESS - request to write was successfully posted

Notes:
Only one write operation may be posted at a time.  Furthermore, an immediate write may not be performed while a posted write is
outstanding.  This is considered to be an application error, and the results of doing so are undefined.

If MPIDU_Sock_post_close() is called before the posted write operation completes, the write operation will be terminated.

Thread safety:
MPIDU_Sock_post_write() may be called while another thread is attempting to perform an immediate read or post a read operation on
the same sock.  MPIDU_Sock_post_write() may also be called while another thread is calling or blocking in MPIDU_Sock_wait() on the
same sock set to which the specified sock belongs.

MPIDU_Sock_post_write() may not be called while another thread is performing an immediate write on the same sock.  This is
considered to be an application error, and the results of doing so are undefined.

Calling MPIDU_Sock_post_write() during or after the call to MPIDU_Sock_post_close() is consider an application error.  The result
of doing so is undefined.  The application should coordinate the closing of a sock with the activities of other threads to ensure
that one thread is not attempting to post a new operation while another thread is attempting to close the sock.  <BRT> Do we really
need this flexibility?

Module:
Utility-Sock
@*/

_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_write(
    _Inout_ sock_state_t *sock,
    _In_reads_bytes_(min) const void* buf,
    _In_ MPIU_Bsize_t min,
    _Inout_ MPIDU_Sock_context_t* psc
    );

/*@
MPIDU_Sock_post_writev - request that a vector of data be written to a sock

Input Parameters:
+ sock - sock object which the data is to be written
. iov - I/O vector describing buffers of data to be written
. iov_n - number of elements in I/O vector
. pov - user context associated with the writev request

Return value: MPI error code
. MPI_SUCCESS - request to write was successfully posted

Notes:
Only one write operation may be posted at a time.  Furthermore, an immediate write may not be performed while a posted write is
outstanding.  This is considered to be an application error, and the results of doing so are undefined.

If MPIDU_Sock_post_close() is called before the posted write operation completes, the write operation will be terminated.

Thread safety:
MPIDU_Sock_post_writev() may be called while another thread is attempting to perform an immediate read or post a read operation on
the same sock.  MPIDU_Sock_post_writev() may also be called while another thread is calling or blocking in MPIDU_Sock_wait() on the
same sock set to which the specified sock belongs.

MPIDU_Sock_post_writev() may not be called while another thread is performing an immediate write on the same sock.  This is
considered to be an application error, and the results of doing so are undefined.

Calling MPIDU_Sock_post_writev() during or after the call to MPIDU_Sock_post_close() is consider an application error.  The result
of doing so is undefined.  The application should coordinate the closing of a sock with the activities of other threads to ensure
that one thread is not attempting to post a new operation while another thread is attempting to close the sock.

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_post_writev(
    _Inout_ sock_state_t *sock,
    _In_reads_(iov_n) WSABUF* iov,
    _In_ int iov_n,
    _Inout_ MPIDU_Sock_context_t* psc
    );

/*@
MPIDU_Sock_close - perform an immediate hard close

Input Parameter:
. sock - sock object to be closed

Return value: none

Notes:
If any other operations are posted on the specified sock, they will be terminated.  An appropriate event will be generated for each
terminated operation.  All such events will be delivered by MPIDU_Sock_wait(). No MPIDU_SOCK_OP_CLOSE is generated.
event.

The sock object is destroyed immediately.  Any oustanding references to the sock object held by the application should be considered
invalid and not used again.

Thread safety:
MPIDU_Sock_close() may be called while another thread is calling or blocking in MPIDU_Sock_wait() specifying the same sock set
to which this sock belongs.  If another thread is blocking MPIDU_Sock_wait() and the close operation causes the sock set to become
empty, then MPIDU_Sock_wait() will return with an error.

Calling any of the immediate or post routines during or after the call to MPIDU_Sock_post_close() is consider an application error.
The result of doing so is undefined.  The application should coordinate the closing of a sock with the activities of other threads
to ensure that simultaneous calls do not occur.

Module:
Utility-Sock
@*/

void
MPIDU_Sock_close(
    _In_ _Post_invalid_ sock_state_t *sock
    );


/*@
MPIDU_Sock_writev - perform an immediate vector write

Input Parameters:
+ sock - sock object to which data is to be written
. iov - I/O vector describing buffers of data to be written
- iov_n - number of elements in I/O vector

Output Parameter:
. num_written - actual number of bytes written

Return value: MPI error code
. MPI_SUCCESS - no error encountered during the write operation

Notes:
An immediate write may not be performed while a posted write is outstanding on the same sock.  This is considered to be an
application error, and the results of doing so are undefined.

Thread safety:
MPIDU_Sock_write() may be called while another thread is attempting to perform an immediate read or post a read operation on the
same sock.  MPIDU_Sock_write() may also be called while another thread is calling or blocking in MPIDU_Sock_wait() on the same sock
set to which the specified sock belongs.

A immediate write may not be performed if another thread is performing an immediate write on the same sock.  This is considered to
be an application error, and the results of doing so are undefined.

Calling MPIDU_Sock_write() during or after the call to MPIDU_Sock_post_close() is consider to be an application error.  The result
of doing so is undefined.  The application should coordinate the closing of a sock with the activities of other threads to ensure
that one thread is not attempting to perform an immediate write while another thread is attempting to close the sock.

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_writev(
    _In_ const sock_state_t * const sock,
    _In_reads_(iov_n) WSABUF * iov,
    _In_ int iov_n,
    _Out_ MPIU_Bsize_t * num_written
    );


/*@r
MPIDU_Sock_get_sock_id - get an integer identifier for a sock object

Input Parameter:
. sock - sock object

Return value: an integer that uniquely identifies the sock object

Notes:
The integer is unique relative to all other open sock objects in the local process.  The integer may later be reused for a
different sock once the current object is closed and destroyed.

This function does not return an error code.  Passing in an invalid sock object has undefined results (garbage in, garbage out).

Module:
Utility-Sock
@*/

_Success_(return >=0)
int
MPIDU_Sock_get_sock_id(
    _In_ const sock_state_t * const sock
    );

/*@
MPIDU_Sock_keepalive - enable connection keep-alive protocol

Input Parameter:
. sock - sock object

Return value: an MPI error code

Module:
Utility-Sock
@*/
_Success_(return==MPI_SUCCESS)
int
MPIDU_Sock_keepalive(
    _In_ const sock_state_t * const sock
    );

//
// Utility function to retrieve the ip address string associated with a socket.
// This is the address the socket is connected to on the remote host.
//
_Success_(*pPort != 0)
void
get_sock_peer_address(
    _In_ _Pre_satisfies_(0 != pSock->sock) sock_state_t* pSock,
    _Out_writes_bytes_(addrLen) char* pAddr,
    _In_ int addrLen,
    _Out_ int* pPort
    );

#endif /* !defined(MPIDU_SOCK_H_INCLUDED) */
