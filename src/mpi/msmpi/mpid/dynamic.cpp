// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "namepub.h"
#include "pmi.h"
#include "DynProc.h"
#include "DynProcAlloc.h"
#include "DynProcServer.h"


//
// These values (in milliseconds) are used for the clients
// to backoff in case of the accept server is being congested
//
#define DYNPROC_BACKOFF_INTERVAL 100
#define DYNPROC_MAX_BACKOFF      10000


#define SOCKET_EINTR        WSAEINTR

static int
MPIR_fd_send(
    _In_ SOCKET fd,
    _In_reads_(length) const char* buffer,
    _In_ int length
    )
{
    int result, num_bytes;

    while (length)
    {
        num_bytes = send(fd, buffer, length, 0);
        if (num_bytes == -1)
        {
            result = WSAGetLastError();
            if (result == SOCKET_EINTR)
                continue;
            else
                return result;
        }
        else
        {
            length -= num_bytes;
            buffer = (char*)buffer + num_bytes;
        }
    }
    return 0;
}

static int
MPIR_fd_recv(
    _In_ SOCKET fd,
    _Out_writes_(length) char* buffer,
    _In_ int length
    )
{
    int result, num_bytes;

    while (length)
    {
        num_bytes = recv(fd, buffer, length, 0);
        if (num_bytes == -1)
        {
            result = WSAGetLastError();
            if (result == SOCKET_EINTR)
                continue;
            else
                return result;
        }
        else
        {
            length -= num_bytes;
            buffer = (char*)buffer + num_bytes;
        }
    }
    return 0;
}


/*@
   MPI_Comm_join - Create a communicator by joining two processes connected by
     a socket.

   Input Parameter:
. fd - socket file descriptor

   Output Parameter:
. intercomm - new intercommunicator (handle)

 Notes:
  The socket must be quiescent before 'MPI_COMM_JOIN' is called and after
  'MPI_COMM_JOIN' returns. More specifically, on entry to 'MPI_COMM_JOIN', a
  read on the socket will not read any data that was written to the socket
  before the remote process called 'MPI_COMM_JOIN'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
MPI_RESULT
MPID_Comm_join(_In_ SOCKET fd, _Out_ MPI_Comm *intercomm)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int err;
    StackGuardArray<char> local_port;
    char *remote_port;

    local_port = new char[MPI_MAX_PORT_NAME * 2];
    if( local_port == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }
    remote_port = local_port + MPI_MAX_PORT_NAME;

    mpi_errno = NMPI_Open_port(MPI_INFO_NULL, local_port);
    ON_ERROR_FAIL(mpi_errno);

    err = MPIR_fd_send(fd, local_port, MPI_MAX_PORT_NAME);
    if(err != 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**join_send %d", err);
        goto fn_fail;
    }

    err = MPIR_fd_recv(fd, remote_port, MPI_MAX_PORT_NAME);
    if(err != 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**join_recv %d", err);
        goto fn_fail;
    }

    int cmpLocalRemote = CompareStringA( LOCALE_INVARIANT,
                                         0,
                                         local_port,
                                         MPI_MAX_PORT_NAME,
                                         remote_port,
                                         MPI_MAX_PORT_NAME );
    if( cmpLocalRemote == CSTR_EQUAL )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**join_portname %s %s", (char *)local_port, remote_port);
        goto fn_fail;
    }

    if( cmpLocalRemote == CSTR_LESS_THAN )
    {
        mpi_errno = NMPI_Comm_accept(local_port, MPI_INFO_NULL, 0,
                                     MPI_COMM_SELF, intercomm);
    }
    else
    {
        mpi_errno = NMPI_Comm_connect(remote_port, MPI_INFO_NULL, 0,
                                     MPI_COMM_SELF, intercomm);
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = NMPI_Close_port(local_port);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

  fn_fail:
    return mpi_errno;
}


/* Define the name service handle */
#define MPID_MAX_NAMEPUB 64

MPI_RESULT MPID_NS_Create( const MPID_Info* /*info_ptr*/, MPID_NS_t **handle_ptr )
{
    static struct MPID_NS_t nsHandleWithNoData;

    /* MPID_NS_Create() should always create a valid handle */
    *handle_ptr = &nsHandleWithNoData;  /* The name service needs no local data */
    return MPI_SUCCESS;
}

MPI_RESULT MPID_NS_Publish( const MPID_NS_t* /*handle*/, const MPID_Info* /*info_ptr*/,
                     const char service_name[], const char port[] )
{
    MPI_RESULT rc = PMI_Publish_name( service_name, port );
    if (rc != MPI_SUCCESS)
    {
        /*printf( "PMI_Publish_name failed for %s\n", service_name );*/
        return MPIU_ERR_CREATE(MPI_ERR_NAME, "**namepubnotpub %s", service_name );
    }

    return MPI_SUCCESS;
}

MPI_RESULT MPID_NS_Lookup( _In_opt_ const MPID_NS_t* /*handle*/, _In_opt_ const MPID_Info* /*info_ptr*/,
                    _In_z_ const char service_name[], _Out_writes_z_( MPI_MAX_PORT_NAME ) char port[] )
{
    MPI_RESULT rc = PMI_Lookup_name( service_name, port );
    if (rc != MPI_SUCCESS)
    {
        /*printf( "PMI_Lookup_name failed for %s\n", service_name );*/
        return MPIU_ERR_CREATE(MPI_ERR_NAME, "**namepubnotfound %s", service_name );
    }

    return MPI_SUCCESS;
}

MPI_RESULT MPID_NS_Unpublish( const MPID_NS_t* /*handle*/, const MPID_Info* /*info_ptr*/,
                       const char service_name[] )
{
    MPI_RESULT rc = PMI_Unpublish_name( service_name );
    if (rc != MPI_SUCCESS)
    {
        /*printf( "PMI_Unpublish_name failed for %s\n", service_name );*/
        return MPIU_ERR_CREATE(MPI_ERR_NAME, "**namepubnotunpub %s", service_name );
    }

    return MPI_SUCCESS;
}

MPI_RESULT MPID_NS_Free( MPID_NS_t** /*handle_ptr*/ )
{
    return MPI_SUCCESS;
}


/*
 * MPID_Open_port()
 */
MPI_RESULT MPID_Open_port(
    _In_opt_                       MPID_Info*,
    _Out_z_cap_(MPI_MAX_PORT_NAME) char *port_name )
{
    int mpi_errno = MPI_SUCCESS;
    if( !g_AcceptServer.IsListening() )
    {
        mpi_errno = g_AcceptServer.Start();
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    DynProcPort* pPort = g_AcceptServer.GetNewPort();
    if( pPort == NULL )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**dynamicNewPortFailed"
            );
    }

    HRESULT hr = StringCchCopyA( port_name, MPI_MAX_PORT_NAME, pPort->m_name );
    if( FAILED( hr ) )
    {
        MPID_Close_port( port_name );
        return MPIU_ERR_NOMEM();
    }

    return MPI_SUCCESS;

}


MPI_RESULT MPID_Close_port(
    _In_z_ const char *port_name )
{
    DynProcPort* pPort = g_AcceptServer.GetPortByName( port_name );
    if( pPort == NULL )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**dynamicInvalidPort %s",
            port_name
            );
    }

    pPort->Close();
    pPort->Release();

    return MPI_SUCCESS;
}


//
// Summary:
// Generate a random number within a given range
//
// In:
// min           : The lower bound (inclusive)
// max           : The upper bound (exclusive)
//
// Return
// A random number within [min,max)
//
// Remarks:
// This is a util. function that is used for our backoff logic
// when the RPC server is too busy
//
static int
GetARandomNumber(
    int min,
    int max
    )
{
    MPIU_Assert( min < max );
    int ret = static_cast<int>(
        static_cast<double>(rand()) / (RAND_MAX + 1) * ( max - min ) + min );

    return ret;
}


//
// Summary:
// The progress engine calls this handler when it receives the completion
// notification from the RPC layer. This function will set the boolean
// pointer which will be checked by the client to know that its Async RPC
// request has been satisfied by the server
//
MPI_RESULT
WINAPI
RpcAsyncNotificationHandler(
    _In_ DWORD,
    _In_ VOID*                 pOverlapped
    )
{
    Mpi.SignalProgress();

    //
    // The root of the MPI_Comm_connect passes in this pointer to wait
    // for the notification of async completion from the RPC layer.
    // It will loop and wait in the progress engine until this becomes 1.
    //
    *(reinterpret_cast<ULONG*>(pOverlapped)) = 1;

    return MPI_SUCCESS;
}


// Summary:
// Given a port from the server, parse it into a port string
// and a tag
//
// In:
// portName         : Port name as given by the accept server
// szPortStr        : Size of the buffer to hold the port string
//
// Out:
// portStr          : Output string to hold the port string
// pTag             : Pointer to receive the port tag
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
static MPI_RESULT
ParsePortName(
    _In_z_       const char*        portName,
    _Out_z_cap_(szPortStr) char*    portStr,
    _In_         size_t             szPortStr,
    _Out_        unsigned int*      pTag
    )
{
    HRESULT hr = StringCchCopyA( portStr, szPortStr, portName );
#if DBG
    const char* pEnd = portStr + szPortStr;
#endif
    if( FAILED( hr ) )
    {
        goto fn_fail;
    }

    //
    // Find the end of the first binding
    //
    char* p = strchr( portStr, ' ');
    if( p == NULL )
    {
        goto fn_fail;
    }

    //
    // Advance p to the second binding, this is safe
    // because the port is NULL terminated
    //
    ++p;

#if DBG
    MPIU_Assert( p < pEnd );
#endif

    //
    // Find the end of the next binding
    //
    p = strchr( p, ' ');
    if( p == NULL )
    {
        goto fn_fail;
    }

    //
    // Null terminate the string here so we can extract the port
    //
    *p = '\0';

#if DBG
    MPIU_Assert( p < pEnd );
#endif

    int tag = atoi( ++p );

    if( tag <= 0 )
    {
        goto fn_fail;
    }

    *pTag = static_cast<unsigned int>( tag );

    return MPI_SUCCESS;

fn_fail:
    return MPIU_ERR_CREATE(
            MPI_ERR_PORT,
            "**dynamicInvalidPort %s",
            portName);
}


//
// Summary:
// Create the RPC binding from a binding string
//
// In:
// pBindingStr     : The RPC binding string received from the server
//
//
// Out:
// hBindings       : The array of RPC binding handles that can be used to
//                   talk to the server
// numBindings     : The number of RPC binding handles
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
static MPI_RESULT
CreateRpcBinding(
    _In_z_  const char*         pBindingStr,
    _Out_   handle_t*           hBindings,
    _Out_   unsigned int*       numBindings
    )
{
    RPC_STATUS  status;
    char        tempStr[MPI_MAX_PORT_NAME];
    int         nBindings = 0;

    HRESULT hr = StringCchCopyA( tempStr, _countof(tempStr), pBindingStr );
    if( FAILED(hr) )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_PORT,
            "**dynamicInvalidBindingString %s",
            pBindingStr );
    }

    char* p = strchr( tempStr, ' ' );
    if( p == NULL )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_PORT,
            "**dynamicInvalidBindingString %s",
            pBindingStr );
    }
    *p = '\0';
    ++p;

    status = RpcBindingFromStringBindingA(
        reinterpret_cast<RPC_CSTR>( const_cast<char*>(tempStr) ),
        &hBindings[nBindings] );
    if( status == RPC_S_OK )
    {
        nBindings++;
    }
    else
    {
        //
        // Local RPC will fail for remote host
        //
        MPIU_Assert( status == RPC_S_INVALID_NET_ADDR );
    }

    status = RpcBindingFromStringBindingA(
        reinterpret_cast<RPC_CSTR>( const_cast<char*>( p ) ),
        &hBindings[nBindings] );
    if( status != RPC_S_OK )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**dynamicBindingFromStringFailed %s %d",
            pBindingStr,
            status
            );
    }
    nBindings++;

    MPIU_Assert( nBindings <= 2 );
    for( int i = 0; i < nBindings; i++ )
    {
        status = RpcBindingSetAuthInfoA(
            hBindings[i],
            NULL,
            RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_AUTHN_WINNT,
            NULL,
            0
            );
        if( status != RPC_S_OK )
        {
            RpcBindingFree( &hBindings[i] );

            //
            // Free all the other bindings that we allocated
            //
            for( int j = 0; j < i; j++ )
            {
                RpcBindingFree( &hBindings[j] );
            }

            return MPIU_ERR_CREATE(
                MPI_ERR_OTHER,
                "**dynamicBindingSetAuthFailed %d",
                status
                );
        }
    }

    *numBindings = nBindings;
    return MPI_SUCCESS;
}


//
// Summary:
// Send the local group info to the remote side and receive
// the remote group info from the remote side
//
// In:
// pRoot          : Pointer to the rank of the root on the remote side
// pGroupData     : The pointer to the metadata of the remote group
// pConnInfos     : Pointer to local group's PG infos
//
// Out:
// pRoot          : Pointer to receive the remote group's root's rank
// pGroupData     : Pointer to receive the metadata of the remote group
// ppOutConnInfos : Pointer to an array to receive remote group's PG infos
//
// Returns:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
// The root on the connect side uses this routine to connect to the RPC server
// of the accept side and exchange information.
//
static MPI_RESULT
ExchangePGInfoWithServer(
    _In_    const char*          port_name,
    _Inout_ int*                 pRoot,
    _Inout_ RemoteGroupMetadata* pGroupData,
    _In_    CONN_INFO_TYPE*      pConnInfos,
    _Out_   CONN_INFO_TYPE**     ppOutConnInfos
    )
{
    unsigned int tag;
    char bindingStr[MPI_MAX_PORT_NAME];

    MPI_RESULT mpi_errno = ParsePortName( port_name,
                                          bindingStr,
                                          _countof( bindingStr ),
                                          &tag );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    RPC_ASYNC_STATE RpcAsyncState;
    RPC_STATUS      status = RPC_S_INTERNAL_ERROR;

    //
    // Max number of bindings supported is 2: LRPC and TCP
    //
    handle_t     hBindings[2];

    unsigned int numBindings;
    mpi_errno = CreateRpcBinding( bindingStr, hBindings, &numBindings );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Make the async call and wait for completion.
    // We try all the bindings available until one succeeds.
    //
    ULONG AsyncCompleted = 0;
    for( unsigned int i = 0; i < numBindings; i++ )
    {
        status = RpcAsyncInitializeHandle( &RpcAsyncState,
                                           sizeof( RPC_ASYNC_STATE ) );
        if( status != RPC_S_OK )
        {
            //
            // This is a severe error, trying another binding will not help, best
            // to abort here
            //
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_INTERN,
                "**dynamicInitializeAsyncFailed %d",
                status );
            goto FreeBindingAndReturn;
        }

        //
        // Setup the completion port for the async handle
        //
        AsyncCompleted = 0;
        RpcAsyncState.NotificationType      = RpcNotificationTypeIoc;
        RpcAsyncState.u.IOC.hIOPort         = MPIDI_CH3I_set;
        RpcAsyncState.u.IOC.dwCompletionKey = EX_KEY_CONNECT_COMPLETE;
        RpcAsyncState.u.IOC.dwNumberOfBytesTransferred = sizeof( ULONG );
        RpcAsyncState.u.IOC.lpOverlapped    = reinterpret_cast<LPOVERLAPPED>(
            &AsyncCompleted ) ;

        //
        // The RPC layer requires this to be set to NULL.
        //
        *ppOutConnInfos = NULL;

        int backOff = 1;
        for(;;)
        {
            RpcTryExcept
            {
                RpcCliAsyncExchangeConnInfo(
                    &RpcAsyncState,
                    hBindings[i],
                    tag,
                    pRoot,
                    &pGroupData->remoteContextId,
                    &pGroupData->remoteCommSize,
                    pConnInfos,
                    ppOutConnInfos );
            }
            RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
            {
                status = RpcExceptionCode();
            }
            RpcEndExcept;

            if( status == RPC_S_SERVER_TOO_BUSY )
            {
                if( backOff > DYNPROC_MAX_BACKOFF / DYNPROC_BACKOFF_INTERVAL )
                {
                    backOff = DYNPROC_MAX_BACKOFF / DYNPROC_BACKOFF_INTERVAL;
                }

                Sleep(
                    DYNPROC_BACKOFF_INTERVAL * GetARandomNumber( 0, backOff )
                    );

                backOff = backOff << 1;
                continue;
            }
            else
            {
                //
                // Break out of the backoff loop
                //
                break;
            }
        }

        if( status == RPC_S_OK )
        {
            //
            // Wait for RPC Async completion
            //
            while( AsyncCompleted != 1 )
            {
                mpi_errno = MPID_Progress_wait();
                if( mpi_errno != MPI_SUCCESS )
                {
                    //
                    // Cancel and wait for the server to confirm receipt of cancellation.
                    // Intentionally ignore the return value of RpcAsyncCancelCall since
                    // there is no possible recovery at this point.
                    //
                    OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
                        "Intentionally ignore the return value of RpcAsyncCancelCall since there is no possible recovery.");
                    RpcAsyncCancelCall( &RpcAsyncState, FALSE );
                    OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
                        "Intentionally ignore the return value of RpcAsyncCancelCall since there is no possible recovery.");
                    goto FreeBindingAndReturn;
                }
            }

            //
            // Finish the async call
            //
            int s;

            //
            // Note that this call cannot be retried unless the status
            // is RPC_S_ASYNC_PENDING, which is not possible because we
            // received a completion notification.
            //
            // RPC_S_SERVER_IS_BUSY is possible, but we would need to
            // retry the whole async operation. This will need coordination
            // with the server because the server is already processing
            // the connection request.
            //
            status = RpcAsyncCompleteCall( &RpcAsyncState, &s );

            if( status != RPC_S_OK )
            {
                continue;
            }

            if( s != NOERROR )
            {
                //
                // The call succeeded as far as the RPC layer is concerned.
                // However, the server indicated an error. No need to retry
                // in this case
                //
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_INTERN,
                    "**dynamicExchangeInfoFailed %d",
                    s );
                goto FreeBindingAndReturn;
            }

            //
            // Everything succeeded, no need to try another binding
            //
            break;
        }
    }

    if( status == RPC_S_SERVER_UNAVAILABLE )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_PORT,
            "**dynamicInvalidPort %s",
            port_name );
    }
    else if( status != RPC_S_OK)
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_INTERN,
            "**dynamicCompleteAsyncFailed %d",
            status );
    }

FreeBindingAndReturn:
    for( unsigned int i = 0; i < numBindings; i++ )
    {
        RpcBindingFree( &hBindings[i] );
        hBindings[i] = NULL;
    }
    return mpi_errno;

}


//
// Summary:
// This implements the connect routine for the root of MPI_Comm_connect
//
// In:
// port_name       : The port given by the server's MPI_Open_port
// info            : The MPI_Info structure given by the user
// comm_ptr        : The intracomm of the processes participate in this call
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
MPI_RESULT
MPID_Comm_connect_root(
    _In_z_   const char*        port_name,
    _In_opt_ const MPID_Info*,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    //
    // Gather process group information from the local group
    //
    CONN_INFO_TYPE* pLocalConnInfos = ConnInfoAlloc( comm_ptr->remote_size );
    if( pLocalConnInfos == NULL )
    {
        mpi_errno = MPIU_ERR_NOMEM();
    }

    //
    // If the previous allocation fails, the root will use this function
    // to notify other non-root of this operation of the alloc error
    // so that we can fail gracefully.
    //
    int mpi_errno2 = ReceiveLocalPGInfo( comm_ptr, pLocalConnInfos, mpi_errno );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( mpi_errno2 != MPI_SUCCESS )
    {
        ConnInfoFree( pLocalConnInfos );
        mpi_errno = mpi_errno2;
    }

    //
    // For intercomm, each side generates its own receiving context id
    // and sends the generated context id to the other side. This
    // process will expect to receive all future messages on this
    // context id
    //
    MPI_CONTEXT_ID localContextId;
    mpi_errno2 = MPIR_Get_contextid( comm_ptr,
                                     &localContextId,
                                     mpi_errno != MPI_SUCCESS );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( mpi_errno2 != MPI_SUCCESS )
    {
        return mpi_errno2;
    }

    //
    // Information to be collected by the root from the remote side
    // and broadcast to the local group.
    //
    //
    // Using these fields as temporary storage to send the local
    // group information to the remote side. These are [in/out],
    // they will be read and then overwritten after the call to
    // ExchangePGInfoWithServer
    //
    // Arbitrarily pick is_low_group = 0 for accept side
    // and is_low_group = 1 for connect side
    //
    //
    RemoteGroupMetadata groupData        = {localContextId, static_cast<unsigned int>(comm_ptr->remote_size), 1};
    int                 remoteRoot       = comm_ptr->rank;
    CONN_INFO_TYPE*     pRemoteConnInfos = NULL;

    //
    // Call into the accept server to exchange PG information
    //
    mpi_errno = ExchangePGInfoWithServer( port_name,
                                          &remoteRoot,
                                          &groupData,
                                          pLocalConnInfos,
                                          &pRemoteConnInfos );
    ConnInfoFree( pLocalConnInfos );
    if( mpi_errno != MPI_SUCCESS )
    {
        //
        // When the non-root receives 0 as the context id, it will
        // know that the root is indicating an error condition.
        //
        groupData.remoteContextId = GUID_NULL;
    }

    //
    // Distribute the remote PG infos to the non-root processes
    //
    mpi_errno2 = SendRemoteConnInfo( comm_ptr,
                                     &groupData,
                                     pRemoteConnInfos );
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = mpi_errno2;
    }
    if( mpi_errno == MPI_SUCCESS )
    {
        //
        // All processes setup the new intercomm using
        // the connection infos obtained from the remote side
        //
        mpi_errno = MPIR_Intercomm_create( comm_ptr,
                                           localContextId,
                                           groupData,
                                           pRemoteConnInfos,
                                           newcomm );

    }

    //
    // The RPC layer allocated the memory for the root but we have to
    // free it
    //
    ConnInfoFree( pRemoteConnInfos );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Synchronize with local group. This is necessary before the root
    // of MPI_Comm_connect sends the ACK to the server side, otherwise
    // the non-root processes might not be ready to receive incoming
    // messages from the remote side
    //
    mpi_errno = MPIR_Barrier_intra( comm_ptr );
    if( mpi_errno == MPI_SUCCESS )
    {
        //
        // Send an Ack to let the remote side server
        // know we have finished setting up the intercomm
        //
        mpi_errno = MPIC_Send( NULL,
                               0,
                               g_hBuiltinTypes.MPI_Int,
                               remoteRoot,
                               MPIR_TAG_CONNECT_ACCEPT_SYNC,
                               *newcomm );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        MPIR_Comm_release( *newcomm, 0 );
    }

    return mpi_errno;
}


//
// Summary:
// This implements the connect routine for the non_root
// processes of MPID_Comm_connect
//
// In:
// port_name       : The port given by the server's MPI_Open_port
// info            : The MPI_Info structure given by the user
// root            : The root of the MPID_Comm_connect operation
// comm_ptr        : The intracomm of the processes participate in this call
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
static MPI_RESULT
MPID_Comm_connect_non_root(
    _In_z_   const char*,
    _In_     int                root,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    )
{
    //
    // Send process group information to the root
    //
    MPI_RESULT mpi_errno = SendLocalPGInfo( comm_ptr, root );

    //
    // For intercomm, each side generates its own receiving context id
    // and sends the generated context id to the other side. This
    // process will expect to receive all future messages on this
    // context id
    //
    MPI_CONTEXT_ID localContextId;
    MPI_RESULT mpi_errno2 = MPIR_Get_contextid( comm_ptr,
                                                &localContextId,
                                                mpi_errno != MPI_SUCCESS );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( mpi_errno2 != MPI_SUCCESS )
    {
        return mpi_errno2;
    }

    //
    // Information about the remote group that will be broadcast
    // to this process from the local root
    //
    RemoteGroupMetadata groupData;
    CONN_INFO_TYPE* pRemoteConnInfos = NULL;

    //
    // This call will allocate memory to hold the remote connection
    // infos for non-root processes, which will need to be freed
    // after setting up the intercomm
    //
    mpi_errno = ReceiveRemoteConnInfo( comm_ptr,
                                       root,
                                       &groupData,
                                       &pRemoteConnInfos );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // All processes (root and non-root) setup the new intercomm using
    // the connection infos obtained from the remote side
    //
    mpi_errno = MPIR_Intercomm_create( comm_ptr,
                                       localContextId,
                                       groupData,
                                       pRemoteConnInfos,
                                       newcomm );

    ConnInfoFree( pRemoteConnInfos );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Synchronize with local group's root. This is necessary before
    // the root of MPI_Comm_connect sends the ACK to the server side,
    // otherwise the non-root processes might not be ready to receive
    // incoming messages from the remote side.
    //
    mpi_errno = MPIR_Barrier_intra( comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        MPIR_Comm_release( *newcomm, false );
    }

    return mpi_errno;
}


//
// Summary:
// This implements the connect routine of MPI_Comm_accept/connect
//
// In:
// port_name       : The port given by the server's MPI_Open_port
// info            : The MPI_Info structure given by the user
// root            : The root process of this operation
// comm_ptr        : The intracomm of the processes participate in this call
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
MPI_RESULT
MPID_Comm_connect(
    _In_z_   const char*        port_name,
    _In_opt_ const MPID_Info*   info,
    _In_     int                root,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    )
{
    if( comm_ptr->rank == root )
    {
        return MPID_Comm_connect_root( port_name, info, comm_ptr, newcomm );
    }
    else
    {
        return MPID_Comm_connect_non_root( port_name, root, comm_ptr, newcomm );
    }
}


//
// Summary:
// The accept routine for the root of MPI_Comm_accept
//
// In:
// port_name       : The port given by the server
// info            : The MPI_Info structure given by the user
// comm_ptr        : The intracomm of the processes participate in this call
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
static MPI_RESULT
MPID_Comm_accept_root(
    _In_z_   const char*        port_name,
    _In_opt_ const MPID_Info*,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    )
{
    RPC_STATUS      status;
    MPI_RESULT      mpi_errno = MPI_SUCCESS;
    CONN_INFO_TYPE* pLocalConnInfos = NULL;
    DynProcPort*    pPort = g_AcceptServer.GetPortByName( port_name );
    if( pPort == NULL )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_PORT,
            "**dynamicInvalidPort %s",
            port_name);
    }
    else
    {
        pLocalConnInfos = ConnInfoAlloc( comm_ptr->remote_size );

        if( pLocalConnInfos == NULL )
        {
            pPort->Release();
            mpi_errno = MPIU_ERR_NOMEM();
        }
    }

    MPI_RESULT mpi_errno2 = ReceiveLocalPGInfo( comm_ptr, pLocalConnInfos, mpi_errno );
    if( mpi_errno != MPI_SUCCESS || mpi_errno2 != MPI_SUCCESS )
    {
        ConnInfoFree( pLocalConnInfos  );
        if( pPort != NULL )
        {
            pPort->Release();
        }

        return mpi_errno == MPI_SUCCESS ? mpi_errno2 : mpi_errno;
    }

    while( pPort->m_connRequestQueue.empty() )
    {
        mpi_errno = MPID_Progress_wait();
        if( mpi_errno != MPI_SUCCESS )
        {
            ConnInfoFree( pLocalConnInfos );
            pPort->Release();
            break;
        }
    }

    //
    // Information to be collected by the root from the remote side
    // and broadcast to the local group.
    //
    ConnRequest*        pConnReq = NULL;
    CONN_INFO_TYPE*     pRemoteConnInfos = NULL;
    RemoteGroupMetadata groupData;

    if( mpi_errno == MPI_SUCCESS )
    {
        pConnReq = &pPort->m_connRequestQueue.front();
        pPort->m_connRequestQueue.pop_front();
        pPort->Release();

        pRemoteConnInfos          = pConnReq->pInConnInfos;
        groupData.remoteContextId = *pConnReq->pContextId;
        groupData.remoteCommSize  = *pConnReq->pCount;

        //
        // Arbitrarily pick is_low_group = 0 for accept side
        // and is_low_group = 1 for connect side
        //
        groupData.is_low_group    = 0;

        //
        // Verify if this connection request is still good.  If the
        // client experienced an error after it initiated the async
        // call, it will attempt to cancel the async.
        //
        status = RpcServerTestCancel( RpcAsyncGetCallHandle(pConnReq->pAsync) );
        if( status == RPC_S_OK )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_INTERN,
                                         "**dynamicClientRPCFailed" );
            //
            // When the non-root receives GUID_NULL as the context id, it will
            // know that the root is indicating an error condition.
            //
            groupData.remoteContextId = GUID_NULL;
        }
        else
        {
            MPIU_Assert( status == RPC_S_CALL_IN_PROGRESS );
        }
    }

    //
    // Distribute the remote PG infos to the non-root processes.
    //
    mpi_errno2 = SendRemoteConnInfo( comm_ptr,
                                     &groupData,
                                     pRemoteConnInfos );
    if( mpi_errno != MPI_SUCCESS )
    {
        if( pConnReq != NULL )
        {
            //
            // This means the server detected a cancellation from the client
            //
            goto CompleteRpcAsyncAndFail;
        }
        else
        {
            return mpi_errno;
        }
    }

    if( mpi_errno2 != MPI_SUCCESS )
    {
        mpi_errno = mpi_errno2;
        goto CompleteRpcAsyncAndFail;
    }

    //
    // For intercomm, each side generates its own receiving context id
    // and sends it to the other side. This process will expect to
    // receive all future messages on this context id
    //
    MPI_CONTEXT_ID localContextId;
    mpi_errno = MPIR_Get_contextid( comm_ptr, &localContextId, false );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto CompleteRpcAsyncAndFail;
    }

    //
    // All processes (root and non-root) setup the new intercomm using
    // the connection infos obtained from the remote side
    //
    mpi_errno = MPIR_Intercomm_create( comm_ptr,
                                       localContextId,
                                       groupData,
                                       pRemoteConnInfos,
                                       newcomm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto CompleteRpcAsyncAndFail;
    }

    //
    // Save the root to receive the ack later
    //
    int remoteRoot = *pConnReq->pRoot;

    //
    // Complete the Async Call
    //
    *pConnReq->pRoot           = comm_ptr->rank;
    *pConnReq->pCount          = comm_ptr->remote_size;
    *pConnReq->pContextId      = localContextId;
    *pConnReq->ppOutConnInfos  = pLocalConnInfos;

    //
    // Waiting for everybody to finish setting up the intercomm before
    // signaling the clients
    //
    mpi_errno = MPIR_Barrier_intra( comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto CompleteRpcAsyncAndFail;
    }

    //
    // The RPC layer now takes ownership of ppOutConnInfos and will free it
    // when the async operation finishes (whether success or failure)
    //
    pLocalConnInfos = NULL;

    int reply = NOERROR;
    status = RpcAsyncCompleteCall( pConnReq->pAsync, &reply );
    MPIU_Assert( status == RPC_S_OK );
    delete pConnReq;

    //
    // Receive a zero byte ack so we know that the client side
    // have setup their intercomm successfully
    //
    mpi_errno = MPIC_Recv(
        NULL,
        0,
        g_hBuiltinTypes.MPI_Int,
        remoteRoot,
        MPIR_TAG_CONNECT_ACCEPT_SYNC,
        *newcomm,
        MPI_STATUS_IGNORE );

    //
    // Synchronize with local group
    //
    mpi_errno2 = MPIR_Bcast_intra( &mpi_errno, 1, g_hBuiltinTypes.MPI_Int, comm_ptr->rank, comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    return mpi_errno2;

CompleteRpcAsyncAndFail:
    ConnInfoFree( pLocalConnInfos );

    pConnReq->pRoot           = NULL;
    pConnReq->pCount          = NULL;
    pConnReq->pContextId      = NULL;
    pConnReq->ppOutConnInfos  = NULL;

    int ret = E_UNEXPECTED;
    OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
    RpcAsyncCompleteCall( pConnReq->pAsync, &ret );
    OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
    return mpi_errno;
}


//
// Summary:
// The accept routine for the non-root processes of MPI_Comm_accept
//
// In:
// port_name       : The port given by the server
// root            : The root of this operation
// comm_ptr        : The intracomm of the processes participate in this call
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
static MPI_RESULT
MPID_Comm_accept_non_root(
    _In_z_   const char*,
    _In_     int                root,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    )
{
    MPI_RESULT mpi_errno = SendLocalPGInfo( comm_ptr, root );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Information to be collected by the root from the remote side
    // and broadcast to the local group.
    //
    RemoteGroupMetadata groupData;
    CONN_INFO_TYPE* pRemoteConnInfos = NULL;

    //
    // This call will allocate memory to hold the remote connection
    // info for non-root processes, which will need to be free
    // after setting up the new intercomm
    //
    mpi_errno = ReceiveRemoteConnInfo( comm_ptr,
                                       root,
                                       &groupData,
                                       &pRemoteConnInfos );

    //
    // For intercomm, each side generates its own receiving context id
    // and send it to the other side. This process will expect to
    // receive all future messages on this context id
    //
    MPI_CONTEXT_ID localContextId;
    int mpi_errno2 = MPIR_Get_contextid( comm_ptr,
                                         &localContextId,
                                         mpi_errno != MPI_SUCCESS );
    if( mpi_errno != MPI_SUCCESS || mpi_errno2 != MPI_SUCCESS )
    {
        return mpi_errno == MPI_SUCCESS ? mpi_errno2 : mpi_errno;
    }

    //
    // All processes (root and non-root) setup the new intercomm using
    // the connection infos obtained from the remote side
    //
    mpi_errno = MPIR_Intercomm_create( comm_ptr,
                                       localContextId,
                                       groupData,
                                       pRemoteConnInfos,
                                       newcomm );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    ConnInfoFree( pRemoteConnInfos );

    //
    // Synchronize with the local root to inform root that we
    // have finished setting up the intercomm.
    //
    // MPI_Reduce and MPI_Gather can also work here.
    //
    mpi_errno = MPIR_Barrier_intra( comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Receive notification from root that the other side has
    // completed setting up their intercomms as well.
    //
    mpi_errno2 = MPIR_Bcast_intra( &mpi_errno, 1, g_hBuiltinTypes.MPI_Int, root, comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_CREATE( MPI_ERR_INTERN,
                                "**dynamicRootConnectAcceptFailed" );
    }

    return mpi_errno2;
}


//
// Summary:
// This implements the accept routine of MPI_Comm_accept/connect
//
// In:
// port_name       : The port given by the root's MPI_Open_port
// info            : The MPI_Info structure given by the user
// root            : The root process of this operation
// comm_ptr        : The intracomm of the processes participate in this call
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
MPI_RESULT
MPID_Comm_accept(
    _In_z_   const char*        port_name,
    _In_opt_ const MPID_Info*   info,
    _In_     int                root,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    )
{
    if( comm_ptr->rank == root )
    {
        return MPID_Comm_accept_root( port_name, info, comm_ptr, newcomm );
    }
    else
    {
        return MPID_Comm_accept_non_root( port_name, root, comm_ptr, newcomm );
    }
}


//
// Summary:
// Non-root process uses this routine to send business card to
// the root process
//
// In:
// comm_ptr  : The pointer to the communicator
// root      : The root process
//
// Return:
// MPI_SUCCESS if the function succeeds, other errors otherwise.
//
// Remarks: None
//
MPI_RESULT
SendLocalPGInfo(
    _In_      const MPID_Comm*    comm_ptr,
    _In_      int                 root
    )
{
    //
    // The root informs the non-root whether it should send the business cards or not
    //
    int bclen;

    MPID_Request* creq_ptr;
    MPI_RESULT mpi_errno = MPIR_Iscatter_intra(
        NULL,
        1,
        g_hBuiltinTypes.MPI_Int,
        &bclen,
        1,
        g_hBuiltinTypes.MPI_Int,
        root,
        const_cast<MPID_Comm*>( comm_ptr ),
        MPIR_SCATTER_TAG,
        &creq_ptr
        );
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = MPIR_Wait( creq_ptr );
        creq_ptr->Release();
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // The root indicated that it had experienced an error
    //
    if( bclen < 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_INTERN,
                                "**dynamicRootConnectAcceptFailed" );
    }

    if( bclen > 0 )
    {
        bclen = static_cast<int>(MPIU_Strlen(
            MPIDI_Process.my_businesscard,
            _countof(MPIDI_Process.my_businesscard) ) + 1);

        //
        // Non-root processes send the business cards to the root
        //
        return MPIC_Send( MPIDI_Process.my_businesscard,
                          bclen,
                          g_hBuiltinTypes.MPI_Char,
                          root,
                          MPIR_TAG_CONNECT_ACCEPT_BIZCARD,
                          comm_ptr );
    }

    return MPI_SUCCESS;
}


//
// Summary:
// Root of MPI_Comm_connect/accept uses this routine to
// receive PG's from the local group of a communicator.
//
// In:
// comm_ptr  : The pointer to the communicator
// prevStatus : Previous error leading to the function
//
// Out:
// pConnInfos: Array of PG info, only populated for root process
//             This parameter is ignored for the non-root
//
// Returns:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks: None
//
MPI_RESULT
ReceiveLocalPGInfo(
    _In_                         const MPID_Comm*    comm_ptr,
    _When_(prevStatus == MPI_SUCCESS, _Out_writes_(comm_ptr->remote_size))
                                 CONN_INFO_TYPE      pConnInfos[],
    _In_                         int                 prevStatus
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    const char*  business_card = MPIDI_Process.my_businesscard;
    int bclen;
    MPID_Request* creq_ptr;
    //
    // Root process populate connection infos which include the following
    // 1) pg_id          - obtained from comm_ptr
    // 2) pg_size        - obtained from comm_ptr
    // 3) pg_rank        - obtained from comm_ptr
    // 4) business cards - gathered from all processes in comm_ptr
    //

    //
    // These are the count and displacement arrays so that we
    // gather the business cards into the right slot.
    //
    int* recvcnts = new int[comm_ptr->remote_size];
    if( recvcnts == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    if( prevStatus != MPI_SUCCESS )
    {
        goto ScatterErrorAndFail;
    }

    //
    // The pg_id, pg_size and pg_rank can be collected locally off
    // the comm_ptr. The business cards will be "gathered" from
    // the processes if the root does not already have it.
    //
    for( int i = 0;  i< comm_ptr->remote_size; i++ )
    {
        MPIDI_PG_t* pg_ptr = comm_ptr->vcr[i]->pg;
        int pg_rank        = comm_ptr->vcr[i]->pg_rank;

        pConnInfos[i].pg_id = pg_ptr->id;
        pConnInfos[i].pg_size = pg_ptr->size;
        pConnInfos[i].pg_rank = pg_rank;

        //
        // If we already have the business card of a process,
        // we just copy it locally and mark the recvcnts entry
        // for that process with a 0-count.
        //
        if( pg_ptr->pBusinessCards != NULL &&
            pg_ptr->pBusinessCards[pg_rank] != NULL )
        {
            HRESULT hr = StringCchCopyA( pConnInfos[i].business_card,
                                         _countof( pConnInfos[i].business_card ),
                                         pg_ptr->pBusinessCards[pg_rank] );
            if( FAILED( hr ) )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_INTERN,
                                        "**dynamicInternalFailure" );
                goto ScatterErrorAndFail;
            }
            recvcnts[i] = 0;
        }
        else
        {
            recvcnts[i] = _countof( pConnInfos[i].business_card );
        }
    }

    //
    // The root informs the non-root whether it should send the
    // business cards or not
    //
    mpi_errno = MPIR_Iscatter_intra(
        recvcnts,
        1,
        g_hBuiltinTypes.MPI_Int,
        &bclen,
        1,
        g_hBuiltinTypes.MPI_Int,
        comm_ptr->rank,
        const_cast<MPID_Comm*>( comm_ptr ),
        MPIR_SCATTER_TAG,
        &creq_ptr
        );
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = MPIR_Wait( creq_ptr );
        creq_ptr->Release();
    }

    if( mpi_errno == MPI_SUCCESS )
    {
        for( int i = 0; i < comm_ptr->remote_size; i++ )
        {
            if( i == comm_ptr->rank )
            {
                HRESULT hr = StringCchCopyA(
                    pConnInfos[i].business_card,
                    _countof(pConnInfos[i].business_card),
                    business_card );
                if( FAILED( hr ) )
                {
                    return MPIU_ERR_CREATE( MPI_ERR_INTERN,
                                            "**dynamicInternalFailure" );
                }
                continue;
            }
            if( recvcnts[i] > 0 )
            {
                mpi_errno = MPIC_Recv( pConnInfos[i].business_card,
                                       _countof( pConnInfos[i].business_card ),
                                       g_hBuiltinTypes.MPI_Char,
                                       i,
                                       MPIR_TAG_CONNECT_ACCEPT_BIZCARD,
                                       comm_ptr,
                                       MPI_STATUS_IGNORE );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
            }
        }
    }

    delete [] recvcnts;
    return mpi_errno;

ScatterErrorAndFail:
    for( int i = 0; i < comm_ptr->remote_size; i++ )
    {
        recvcnts[i] = -1;
    }

    MPI_RESULT mpi_errno2 = MPIR_Iscatter_intra(
        recvcnts,
        1,
        g_hBuiltinTypes.MPI_Int,
        &bclen,
        1,
        g_hBuiltinTypes.MPI_Int,
        comm_ptr->rank,
        const_cast<MPID_Comm*>( comm_ptr ),
        MPIR_SCATTER_TAG,
        &creq_ptr
        );
    if( mpi_errno2 == MPI_SUCCESS )
    {
        mpi_errno2 = MPIR_Wait( creq_ptr );
        creq_ptr->Release();
    }

    delete [] recvcnts;
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }
    return mpi_errno2;
}


//
// Summary:
// Distribute the remote group's PG info to the local group
//
// In:
// comm_ptr        : The intracomm of the local group
// pGroupData      : The pointer to the metadata of the remote group
// pRemoteConnInfos: The array of the remote PG infos
//
//
// Returns:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks: None
//
MPI_RESULT
SendRemoteConnInfo(
    _In_     const MPID_Comm*           comm_ptr,
    _In_     const RemoteGroupMetadata* pGroupData,
    _In_opt_ const CONN_INFO_TYPE*      pRemoteConnInfos
    )
{
    //
    // If there was an error leading to this function, we only
    // do one single Bcast to inform non-root processes of the
    // failure.
    //
    MPI_RESULT mpi_errno = MPIR_Bcast_intra( const_cast<RemoteGroupMetadata*>(pGroupData),
                                             sizeof(RemoteGroupMetadata),
                                             g_hBuiltinTypes.MPI_Byte,
                                             comm_ptr->rank,
                                             comm_ptr );
    if( mpi_errno != MPI_SUCCESS ||
        pGroupData->remoteContextId == GUID_NULL )
    {
        return mpi_errno;
    }

    return MPIR_Bcast_intra( const_cast<CONN_INFO_TYPE*>( pRemoteConnInfos ),
                             pGroupData->remoteCommSize * sizeof( CONN_INFO_TYPE ),
                             g_hBuiltinTypes.MPI_Byte,
                             comm_ptr->rank,
                             comm_ptr );
}


//
// Summary:
// Receive the remote group's PG info from the local root
//
// In:
// comm_ptr          : The intracomm of the local group
// root              : The root process
//
// Out:
// pGroupData        : The pointer to the metadata of the remote group
// ppRemoteConnInfos : The pointer to the array of the remote PG's
//
// Returns:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks: ppRemoteConnInfos should be freed by non-root processes
// when it's no longer used
//
MPI_RESULT
ReceiveRemoteConnInfo(
    _In_    const MPID_Comm*          comm_ptr,
    _In_    int                       root,
    _Out_   RemoteGroupMetadata*      pGroupData,
    _Inout_ _Outptr_ CONN_INFO_TYPE** ppRemoteConnInfos
    )
{
    MPIU_Assert( comm_ptr->rank != root );
    MPI_RESULT mpi_errno = MPIR_Bcast_intra( pGroupData,
                                             sizeof(RemoteGroupMetadata),
                                             g_hBuiltinTypes.MPI_Byte,
                                             root,
                                             comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( pGroupData->remoteContextId == GUID_NULL )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_INTERN,
            "**dynamicRootConnectAcceptFailed"
            );
    }

    CONN_INFO_TYPE* pRemoteConnInfos = ConnInfoAlloc( pGroupData->remoteCommSize );
    if( pRemoteConnInfos == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    mpi_errno = MPIR_Bcast_intra( pRemoteConnInfos,
                                  pGroupData->remoteCommSize * sizeof( CONN_INFO_TYPE ),
                                  g_hBuiltinTypes.MPI_Byte,
                                  root,
                                  comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        ConnInfoFree( pRemoteConnInfos );
        return mpi_errno;
    }

    *ppRemoteConnInfos = pRemoteConnInfos;

    return MPI_SUCCESS;
}


/*@
   MPID_Comm_disconnect - Disconnect a communicator

   Arguments:
.  comm_ptr - communicator

   Notes:

.N Errors
.N MPI_SUCCESS
@*/
MPI_RESULT MPID_Comm_disconnect(MPID_Comm *comm_ptr)
{
    MPI_RESULT mpi_errno;

    /* Before releasing the communicator, we need to ensure that all VCs are
       in a stable state.  In particular, if a VC is still in the process of
       connecting, complete the connection before tearing it down */
    /* FIXME: How can we get to a state where we are still connecting a VC but
       the MPIR_Comm_release will find that the ref count decrements to zero
       (it may be that some operation fails to increase/decrease the reference
       count.  A patch could be to increment the reference count while
       connecting, then decrement it.  But the increment in the reference
       count should come
       from the step that caused the connection steps to be initiated.
       Possibility: if the send queue is not empty, the ref count should
       be higher.  */
    /* FIXME: This doesn't work yet */
    /*
    mpi_errno = MPIDI_CH3U_Comm_FinishPending( comm_ptr );
    */

    /*
     * Since outstanding I/O bumps the reference count on the communicator,
     * we wait until we hold the last reference count to
     * ensure that all communication has completed.  The reference count
     * is 1 when the communicator is created, and it is incremented
     * only for pending communication operations (and decremented when
     * those complete).
     */
    while (comm_ptr->Busy() )
    {
        mpi_errno = MPID_Progress_wait();
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
    }

    /* it's more than a comm_release, but ok for now */
    /* FIXME: Describe what more might be required */
    mpi_errno = MPIR_Comm_release(comm_ptr,1);
    /* If any of the VCs were released by this Comm_release, wait
     for those close operations to complete */
    MPIDI_CH3U_VC_WaitForClose();


    return mpi_errno;
}


static
inline
void
free_keyval_array(
    _In_ PMI_keyval_t* pKv,
    _In_ size_t count
    )
{
    size_t n;
    for (n = 0; n < count; n++)
    {
        MPIU_Free(pKv[n].key);
        MPIU_Free(pKv[n].val);
    }

    MPIU_Free(pKv);
}


static
inline
void
free_pmi_keyvals(
    _In_ PMI_keyval_t **ppKv,
    _In_ int size,
    _In_ int *counts
    )
{
    int i;
    for (i = 0; i < size; i++)
    {
        free_keyval_array(ppKv[i], counts[i]);
    }
}


static
inline
MPID_Info*
copy_info_keyval_node(
    _In_ MPID_Info* info,
    _In_ PMI_keyval_t* keyval
    )
{
    keyval->key = MPIU_Strdup(info->key);
    if (keyval->key == nullptr)
    {
        return nullptr;
    }

    keyval->val = MPIU_Strdup(info->value);
    if (keyval->val == nullptr)
    {
        MPIU_Free(keyval->key);
        return nullptr;
    }

    return info;
}


static
inline
int
get_info_count(
    _In_ MPID_Info* info
    )
{
    int n = 0;

    info = MPIU_Info_head(info);
    while (info != nullptr)
    {
        info = MPIU_Info_next(info);
        n++;
    }

    return n;
}


static
inline
MPI_RESULT 
copy_info_keyvals(
    _In_     MPID_Info* info,
    _Outptr_ PMI_keyval_t** pKeyvals,
    _Out_    int* pSize
    )
{
    int n = get_info_count(info);
    PMI_keyval_t* kv = MPIU_Malloc_objn(n, PMI_keyval_t);
    if (kv == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    *pKeyvals = kv;

    info = MPIU_Info_head(info);
    while (info != nullptr)
    {
        if (copy_info_keyval_node(info, kv++) == nullptr)
        {
            free_keyval_array(*pKeyvals, kv - *pKeyvals - 1);
            return MPIU_ERR_NOMEM();
        }
        info = MPIU_Info_next(info);
    }
    (*pSize) = n;
    return MPI_SUCCESS;
}


static
MPI_RESULT ConvertAppName(
    _In_z_             const char*  app,
    _Outptr_result_z_  wchar_t**    appW
    )
{
    wchar_t* tempAppW;
    DWORD len = static_cast<DWORD>(strlen(app) + 1);

    tempAppW = new wchar_t[len];
    if (tempAppW == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    if (MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        app,
        -1,
        tempAppW,
        len) == 0)
    {
        delete[] tempAppW;
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", app);
    }

    (*appW) = tempAppW;
    return MPI_SUCCESS;
}


static
MPI_RESULT ConvertArgs(
    _In_z_             char**     argv,
    _Outptr_result_z_  wchar_t**  argvW
    )
{
    if (argv != nullptr)
    {
        wchar_t* tempArgvWs = new wchar_t[UNICODE_STRING_MAX_CHARS];
        if (tempArgvWs == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }

        tempArgvWs[0] = L'\0';
        DWORD len;

        int curr = 0;
        while (argv[curr] != nullptr)
        {
            len = static_cast<DWORD>(strlen(argv[curr]) + 1);
            wchar_t* tempArgvW = new wchar_t[len];
            if (tempArgvW == nullptr)
            {
                delete[] tempArgvWs;
                return MPIU_ERR_NOMEM();
            }
            if (MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                argv[curr],
                -1,
                tempArgvW,
                len) == 0)
            {
                delete[] tempArgvWs;
                delete[] tempArgvW;
                return MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", argv[curr]);
            }
            //
            // enclose individual args into "*" to preserve spaces
            //
            MPIU_Strnapp(tempArgvWs, L" \"", UNICODE_STRING_MAX_CHARS);
            MPIU_Strnapp(tempArgvWs, tempArgvW, UNICODE_STRING_MAX_CHARS);
            MPIU_Strnapp(tempArgvWs, L"\"", UNICODE_STRING_MAX_CHARS);
            delete[] tempArgvW;
            curr++;
        }
        (*argvW) = tempArgvWs;
    }
    else
    {
        wchar_t* tempArgvW = new wchar_t[1];
        if (tempArgvW == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        tempArgvW[0] = L'\0';
        (*argvW) = tempArgvW;
    }
    return MPI_SUCCESS;
}


static
MPI_RESULT
MPIDI_Comm_spawn_multiple_root(
    _In_range_(>=, 0)               int          count,
    _In_reads_(count)               char**       commands,
    _In_reads_(count)               char***      arguments,
    _In_count_(count)               const int*   maxprocs,
    _In_reads_(count)               MPID_Info**  info_ptrs,
    _Out_                           int*         errcodes,
    _Out_z_cap_(MPI_MAX_PORT_NAME)  char*        port_name
    )
{
    int i;
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    StackGuardArray<int> pmi_errcodes;
    int total_num_processes = 0;
    wchar_t** appWs = nullptr;
    wchar_t** argvWs = nullptr;

    PMI_keyval_t** info_keyval_vectors = nullptr;
    int* info_keyval_sizes = nullptr;

    //
    // translate the arguments to wide char
    //
    appWs = new wchar_t*[count];
    if (appWs == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    argvWs = new wchar_t*[count];
    if (argvWs == nullptr)
    {
        delete[] appWs;
        return MPIU_ERR_NOMEM();
    }

    ZeroMemory(appWs, count * sizeof(wchar_t*));
    ZeroMemory(argvWs, count * sizeof(wchar_t*));

    for (int i = 0; i < count; i++)
    {
        mpi_errno = ConvertAppName(commands[i], &(appWs[i]));
        if (mpi_errno != MPI_SUCCESS)
        {
            goto CleanUp;
        }
    }

    if (arguments == nullptr)
    {
        for (int m = 0; m < count; m++)
        {
            mpi_errno = ConvertArgs(nullptr, &(argvWs[m]));
            if (mpi_errno != MPI_SUCCESS)
            {
                goto CleanUp;
            }
        }
    }
    else
    {
        for (int m = 0; m < count; m++)
        {
            mpi_errno = ConvertArgs(arguments[m], &(argvWs[m]));
            if (mpi_errno != MPI_SUCCESS)
            {
                goto CleanUp;
            }
        }
    }

    info_keyval_vectors = new PMI_keyval_t*[count];
    if (info_keyval_vectors == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto CleanUp;
    }

    info_keyval_sizes = new int[count];
    if (info_keyval_vectors == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto CleanUp;
    }
    ZeroMemory(info_keyval_sizes, count*sizeof(int));

    if (info_ptrs != nullptr)
    {
        for (i = 0; i < count; i++)
        {
            if (info_ptrs[i] != nullptr)
            {
                mpi_errno = copy_info_keyvals(info_ptrs[i], &info_keyval_vectors[i], &info_keyval_sizes[i]);
                if (mpi_errno != MPI_SUCCESS)
                {
                    goto CleanUp;
                }
            }
            else
            {
                info_keyval_vectors[i] = nullptr;
            }
        }
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            info_keyval_sizes[i] = 0;
            info_keyval_vectors[i] = nullptr;
        }
    }

    /* create an array for the pmi error codes */
    for (i = 0; i < count; i++)
    {
        total_num_processes += maxprocs[i];
    }

    pmi_errcodes = new int[total_num_processes];
    if (pmi_errcodes == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto CleanUp;
    }

    /* initialize them to MPI_SUCCESS */
    for (i = 0; i < total_num_processes; i++)
    {
        pmi_errcodes[i] = MPI_SUCCESS;
    }

    /* Open a port for the spawned processes to connect to */
    /* FIXME: info may be needed for port name */
    mpi_errno = MPID_Open_port(NULL, port_name);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto CleanUp;
    }

    /* Spawn the processes */
    mpi_errno = PMI_Spawn_multiple(
        count,
        appWs,
        argvWs,
        maxprocs,
        info_keyval_sizes,
        info_keyval_vectors,
        port_name,
        pmi_errcodes
        );

    if (mpi_errno != MPI_SUCCESS)
    {
        goto CleanUp;
    }

    if (errcodes != MPI_ERRCODES_IGNORE)
    {
        for (i = 0; i<total_num_processes; i++)
        {
            /* FIXME: translate the pmi error codes here */
            errcodes[i] = pmi_errcodes[i];
        }
    }

CleanUp:
    for (int i = 0; i < count; i++)
    {
        delete[] appWs[i];
        delete[] argvWs[i];
    }
    delete[] appWs;
    delete[] argvWs;

    if (info_keyval_sizes != nullptr)
    {
        free_pmi_keyvals(info_keyval_vectors, count, info_keyval_sizes);
    }

    delete[] info_keyval_vectors;
    delete[] info_keyval_sizes;

    return mpi_errno;
}


MPI_RESULT
MPID_Comm_spawn_multiple(
    _In_   int              count,
    _In_z_ char**           commands,
    _In_z_ char***          arguments,
    _In_   const int*       maxprocs,
    _In_   MPID_Info**      info_ptrs,
    _In_   int              root,
    _In_   MPID_Comm*       comm_ptr,
    _Out_  MPID_Comm**      intercomm,
    _Out_  int*             errcodes
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    char port_name[MPI_MAX_PORT_NAME];

    if (comm_ptr->rank == root)
    {
        mpi_errno = MPIDI_Comm_spawn_multiple_root(
            count,
            commands,
            arguments,
            maxprocs,
            info_ptrs,
            errcodes,
            port_name
            );
        ON_ERROR_FAIL(mpi_errno);
    }

    mpi_errno = MPID_Comm_accept(port_name, NULL, root, comm_ptr, intercomm);
    ON_ERROR_FAIL(mpi_errno);

    if (errcodes != MPI_ERRCODES_IGNORE)
    {
        mpi_errno = NMPI_Bcast(errcodes, count, MPI_INT, root, comm_ptr->handle);
        ON_ERROR_FAIL(mpi_errno);
    }

fn_fail:
    return mpi_errno;
}
