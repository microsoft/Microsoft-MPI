// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "DynProcAlloc.h"
#include "DynProcServer.h"
#include "DynProc.h"
#include "ndsupport.h"
#include <ws2tcpip.h>

DynProcServer g_AcceptServer;

DynProcPort::DynProcPort():
    m_refcount(0),
    m_isClosed(false),
    m_pendingReqs(0)
{}


void
DynProcPort::Close()
{
    g_AcceptServer.RemovePort( this );
    m_isClosed = true;

    //
    // At this point no further pending requests can be made
    // because GetPortByTag (which is the only method that will
    // increment the pending request count) will not return
    // a closed port. It is now safe to loop until all pending
    // requests are serviced.
    //
    while( !HasNoPendingRequest() )
    {
        MPID_Progress_wait();
    }
}


DynProcServer::DynProcServer():
    m_tag( 0 ),
    m_bListening( false )
{}


//
// Summary:
// Start the Accept server to listen for Connect requests
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
//
MPI_RESULT
DynProcServer::Start()
{
    MPIU_Assert( !m_bListening );

    char  endpointStr[_countof("65535") + 1];
    const char* pEndpoint;
    int lowPort;
    int highPort;

    if( env_to_range(
            L"MSMPI_ACCEPT_PORT",
            0,
            65535,
            TRUE,
            &lowPort,
            &highPort ) == FALSE )
    {
        pEndpoint = nullptr;
    }
    else
    {

        lowPort = FindNextOpenPort( lowPort );
        if( lowPort > highPort )
        {
            return MPIU_ERR_CREATE(
                MPI_ERR_PORT,
                "**dynamicStartFailedEnv" );        
        }
        MPIU_Snprintf(
            endpointStr,
            _countof(endpointStr),
            "%d",
            lowPort );
        pEndpoint = endpointStr;
    }
    
    //
    // Start the RPC Server
    //
    int err = StartRpcServer( "ncacn_ip_tcp",
                              pEndpoint,
                              RpcSrvDynProc_v1_0_s_ifspec,
                              NULL );
    if( err != NOERROR )
    {
        if( err == RPC_S_INVALID_ENDPOINT_FORMAT )
        {
            return MPIU_ERR_CREATE( MPI_ERR_PORT,
                                    "**dynamicStartFailedEnv" );
        }

        return MPIU_ERR_CREATE( MPI_ERR_PORT,
                                "**dynamicStartFailed %d",
                                err );
    }

    //
    // Obtain the Progress Engine handle and register the connection
    // request handler. This will be used as the communication
    // mechanism between the RPC thread and the main thread
    //
    hExSet       = MPIDI_CH3I_set;

    //
    // Register completion processors for processing connection request
    // from the client
    //
    ExRegisterCompletionProcessor( EX_KEY_CONNECT_REQUEST, DynConnReqHandler );

    return MPI_SUCCESS;
}


//
// Summary:
// Stop the Accept server that listens for Connect requests
//
void
DynProcServer::Stop()
{
    MPIU_Assert( m_portList.empty() );
    //
    // Stop the server
    //
    StopRPCServer( RpcSrvDynProc_v1_0_s_ifspec );

    ExUnregisterCompletionProcessor( EX_KEY_CONNECT_REQUEST );

    m_bListening = false;
}

//
// Summary:
// Start the RPC accept server
//
// In:
// pProtSeq           : The protocol sequence
// pEndpoint          : The endpoint (port). If NULL, we use dynamic port
// rpcInterface       : The RPC interface handle
// pSecurityCallbackFn: The security callback function
//
// Return:
// NOERROR on success, other errors otherwise
//
int
DynProcServer::StartRpcServer(
    _In_z_     const char*         pProtSeq,
    _In_opt_z_ const char*         pEndpoint,
    _In_       RPC_IF_HANDLE       rpcInterface,
    _In_opt_   RPC_IF_CALLBACK_FN* pSecurityCallbackFn
    )
{
    RPC_STATUS          status;

    GUID lrpcEp;
    status = UuidCreate( &lrpcEp );
    if( status != RPC_S_OK )
    {
        return status;
    }

    char lrpcEpStr[GUID_STRING_LENGTH + 1];
    GuidToStr( lrpcEp, lrpcEpStr, _countof( lrpcEpStr ) );
    
    //
    // Enable LRPC
    //
    status = RpcServerUseProtseqEpA(
        reinterpret_cast<RPC_CSTR>( "ncalrpc" ),
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        reinterpret_cast<RPC_CSTR>( lrpcEpStr ),
        NULL
        );
    if( status != RPC_S_OK )
    {
        return status;
    }


    //
    // Dynamic and static endpoints are setup through different RPC APIs
    //
    if( pEndpoint == NULL )
    {
        //
        // Specify that we will be using TCP socket with dynamic endpoint
        //
        status = RpcServerUseProtseqA(
            reinterpret_cast<RPC_CSTR>( const_cast<char*>(pProtSeq) ),
            RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
            NULL
            );
    }
    else
    {
        //
        // Specify that we will be using TCP socket with static endpoint
        //
        status = RpcServerUseProtseqEpA(
            reinterpret_cast<RPC_CSTR>( const_cast<char*>(pProtSeq) ),
            RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
            reinterpret_cast<RPC_CSTR>( const_cast<char*>(pEndpoint) ),
            NULL
            );
    }
    if( status != RPC_S_OK )
    {
        return status;
    }

    status = RpcServerRegisterAuthInfoA( NULL,
                                         RPC_C_AUTHN_WINNT,
                                         NULL,
                                         NULL );
    if( status != RPC_S_OK )
    {
        return status;
    }

    //
    // Register the interface
    //
    status = RpcServerRegisterIfEx( rpcInterface,
                                    NULL,
                                    NULL,
                                    RPC_IF_AUTOLISTEN,
                                    RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                                    pSecurityCallbackFn );
    if( status != RPC_S_OK )
    {
        return status;
    }

    m_bListening = true;

    //
    // Obtain the binding string
    //
    DWORD err = GetRpcBindingString( lrpcEpStr, pEndpoint );
    if( err != NOERROR )
    {
        Stop();
        return MPIU_ERR_CREATE( MPI_ERR_PORT,
                                "**dynamicStartFailed %d",
                                err );
    }

    return NOERROR;
}


//
// Summary:
// In Azure environment, replace the reddog hostname with the
// friendly name or the IP address so that the node can be recognized
// by other other Azure nodes in the same deployment.
//
// In:
// bindingStr: The original null terminated binding string

// Out:
// pAzureBindingStr: The binding string with the reddog hostname
//                   replaced. If not on Azure, this will be set to nullptr
//
// Return: NOERROR if the operation succeeds
//         Other NT error otherwise
//
_Success_(return == NOERROR)
static
int
AdjustBindingHost(
    _In_  RPC_CSTR            bindingStr,
    _Out_ RPC_CSTR*           pAdjustedBindingStr
    )
{
    RPC_CSTR   azureBindingStr;
    RPC_STATUS status;
    HRESULT    hr;

    char  friendlyName[MAX_COMPUTERNAME_LENGTH];
    friendlyName[0] = '\0';
    DWORD err = MPIU_Getenv(
        "MSMPI_ACCEPT_HOST",
        friendlyName,
        _countof(friendlyName) );
    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        if( get_azure_node_logical_name( nullptr, 0 ) == false )
        {
            *pAdjustedBindingStr = nullptr;
            return NOERROR;
        }

        //
        // Attempt to fetch the friendly name from environment variable.
        // The default environment variable we check is MSMPI_NODE_NAME.
        // If it does not exist, we check for HPC_NODE_NAME. Nodes deployed
        // by HPC Pack should have HPC_NODE_NAME set.
        //
        err = MPIU_Getenv(
            "MSMPI_NODE_NAME",
            friendlyName,
            _countof(friendlyName) );
        if( err == ERROR_ENVVAR_NOT_FOUND )
        {
            err = MPIU_Getenv(
                "HPC_NODE_NAME",
                friendlyName,
                _countof(friendlyName) );
        }
    }
    
    if( err != NOERROR && err != ERROR_ENVVAR_NOT_FOUND )
    {
        return err;
    }


    //
    // Parse the binding so we can replace the existing
    // hostname with a friendly name (or IP address)
    //
    RPC_CSTR objUuidStr;
    RPC_CSTR protSeqStr;
    RPC_CSTR networkAddrStr;
    RPC_CSTR endpointStr;
    RPC_CSTR networkOptionsStr;
    status = RpcStringBindingParseA(
        bindingStr,
        &objUuidStr,
        &protSeqStr,
        &networkAddrStr,
        &endpointStr,
        &networkOptionsStr );
    if( status != RPC_S_OK )
    {
        return status;
    }

    if( friendlyName[0] == '\0' )
    {
        //
        // The user did not set MSMPI_ACCEPT_HOST. Furthermore,
        // both MSMPI_NODE_NAME and HPC_NODE_NAME do not exist.
        // We need to resolve the ip addresses of the hostname
        // from the binding
        //
        addrinfo* pAddrInfos;
        addrinfo  hints;
        ZeroMemory( &hints, sizeof(hints) );

        //
        // RPC can support IpV6, but since the rest of MPI does not
        // we will limit to IpV4 for now to make things simple
        //
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        WSADATA wsaData;
        int ret = WSAStartup( MAKEWORD(2,0), &wsaData );
        if( ret != 0 )
        {
            return ret;
        }

        status = GetAddrInfoA(
            reinterpret_cast<char*>( networkAddrStr ), nullptr, &hints, &pAddrInfos );
        if( status != 0 )
        {
            WSACleanup();
            goto FreeRpcStringsAndReturn;
        }

        if( pAddrInfos == nullptr )
        {
            WSACleanup();
            status = ERROR_UNEXP_NET_ERR;
            goto FreeRpcStringsAndReturn;
        }

        //
        // If there are more than one network interfaces that support IP,
        // we will just pick the first one that does not have ND enabled.
        //
        // TODO: We should actually look at the registry
        // HKLM\System\CurrentControlSet\Services\Rpc\Linkage\Bind
        // to determine whether the system restricts RPC to bind to certain
        // interface, and match it up against pAddrInfos.
        // It's quite rare that this registry is ever set, but it is possible.
        //

        addrinfo* pAddrInfo;
        for( pAddrInfo = pAddrInfos;
             pAddrInfo != nullptr;
             pAddrInfo = pAddrInfo->ai_next )
        {
            hr = NdCheckAddress( pAddrInfo->ai_addr, pAddrInfo->ai_addrlen );
            if( FAILED( hr ) )
            {
                //
                // TODO: What if the user disable ND by ndinstall -r ?
                //
                break;
            }
        }

        if( pAddrInfo == nullptr )
        {
            WSACleanup();
            status = ERROR_CLUSTER_NETWORK_NOT_FOUND;
            goto FreeRpcStringsAndReturn;
        }

        sockaddr_in* sockaddr = reinterpret_cast<sockaddr_in*>(
            pAddrInfo->ai_addr );
        const char* addr = inet_ntoa(sockaddr->sin_addr);
        hr = StringCchCopyA( friendlyName,
                             _countof(friendlyName),
                             addr );
        FreeAddrInfoA( pAddrInfos );
        WSACleanup();
        if( FAILED( hr ) )
        {
            status = ERROR_BUFFER_OVERFLOW;
            goto FreeRpcStringsAndReturn;
        }
     }

    MPIU_Assert( friendlyName[0] != '\0' );

    //
    // Compose a string binding using the friendly name.
    // We will make a copy of it and free the string returned by RPC
    // so that it is easier to manage the lifetime of it
    //
    status = RpcStringBindingComposeA(
        objUuidStr,
        protSeqStr,
        reinterpret_cast<RPC_CSTR>( friendlyName ),
        endpointStr,
        networkOptionsStr,
        &azureBindingStr );
    if( status != RPC_S_OK )
    {
        goto FreeRpcStringsAndReturn;
    }

    *pAdjustedBindingStr = azureBindingStr;
    status = NOERROR;

FreeRpcStringsAndReturn:
    RpcStringFreeA( &objUuidStr );
    RpcStringFreeA( &protSeqStr );
    RpcStringFreeA( &networkAddrStr );
    RpcStringFreeA( &endpointStr );
    RpcStringFreeA( &networkOptionsStr );

    return status;
}


//
// Summary:
// Get the RPC binding string for this server
//
// Remarks:
// This function populates the m_bindingStr member with the LRPC
// and the TCP binding.
//
// Returns:
// NOERROR on success, other errors otherwise
//
_Success_(return == NOERROR)
int
DynProcServer::GetRpcBindingString(
    _In_z_     const char* lrpcEpStr,
    _In_opt_z_ const char* tcpEpStr
    )
{
    RPC_BINDING_VECTOR* pBindingVector;

    //
    // Get the server binding handle vector. This vector has information
    // about which protocols the server will support.
    //
    RPC_STATUS status = RpcServerInqBindings( &pBindingVector );
    if( status != RPC_S_OK )
    {
        return status;
    }

    int numWritten = 0;
    for( unsigned int i = 0; i < pBindingVector->Count; ++i )
    {
        RPC_CSTR bindingStr;
        status = RpcBindingToStringBindingA(
            pBindingVector->BindingH[i],
            &bindingStr );
        if( status != RPC_S_OK )
        {
            goto CleanUpBindingVector;
        }

        RPC_CSTR protSeqStr;
        RPC_CSTR endpointStr;
        status = RpcStringBindingParseA(
            bindingStr,
            nullptr,
            &protSeqStr,
            nullptr,
            &endpointStr,
            nullptr );
        if( status != RPC_S_OK )
        {
            RpcStringFree( &bindingStr );
            goto CleanUpBindingVector;
        }

        if( CompareStringA( LOCALE_INVARIANT,
                            0,
                            reinterpret_cast<char*>(protSeqStr),
                            -1,
                            "ncalrpc",
                            -1 ) == CSTR_EQUAL )
        {
            if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                reinterpret_cast<char*>(endpointStr),
                                -1,
                                lrpcEpStr,
                                -1 ) == CSTR_EQUAL )
            {
                numWritten += MPIU_Snprintf( m_bindingStr + numWritten,
                                             _countof(m_bindingStr) - numWritten,
                                             "%s ",
                                             bindingStr );
            }
        }
        else if( CompareStringA( LOCALE_INVARIANT,
                                 0,
                                 reinterpret_cast<char*>(protSeqStr),
                                 -1,
                                 "ncacn_ip_tcp",
                                 -1 ) == CSTR_EQUAL )
        {
            if( tcpEpStr == nullptr  || 
                CompareStringA( LOCALE_INVARIANT,
                                0,
                                reinterpret_cast<char*>(endpointStr),
                                -1,
                                tcpEpStr,
                                -1 ) == CSTR_EQUAL )
            {
                RPC_CSTR adjustedBinding;
                status = AdjustBindingHost( bindingStr,
                                            &adjustedBinding );
                if( status != RPC_S_OK )
                {
                    RpcStringFree( &protSeqStr );
                    RpcStringFree( &endpointStr );
                    RpcStringFree( &bindingStr );
                    goto CleanUpBindingVector;
                }
                numWritten += MPIU_Snprintf( m_bindingStr + numWritten,
                                             _countof(m_bindingStr) - numWritten,
                                             "%s ",
                                             adjustedBinding == nullptr ? bindingStr : adjustedBinding );

                if( adjustedBinding != nullptr )
                {
                    RpcStringFree( &adjustedBinding );
                }
            }
        }

        RpcStringFree( &protSeqStr );
        RpcStringFree( &endpointStr );
        RpcStringFree( &bindingStr );
    }

    
CleanUpBindingVector:
    //
    // Ignore the status of RpcBindingVectorFree, there is no possible recovery
    //
    RpcBindingVectorFree( &pBindingVector );

    return status;
}


//
// Summary:
// Stop this RPC server
//
// In:
// rpcInterface    : The RPC interface handle
//
int
DynProcServer::StopRPCServer(
    _In_       RPC_IF_HANDLE       rpcInterface
        )
{
    return RpcServerUnregisterIf( rpcInterface, nullptr, FALSE );
}


//
// Summary:
// Allocate a new port that can be used for accept/connect
//
// Return:
// The pointer to the allocated port, NULL if such a port cannot be allocated
//
// Remarks:
// If the RPC server is already running, we basically allocate a new port
// give it an incremented tag and return the new name
//
DynProcPort*
DynProcServer::GetNewPort()
{
    if( !m_bListening )
    {
        return NULL;
    }

    DynProcPort* pPort = new DynProcPort();
    if( pPort == NULL )
    {
        return NULL;
    }

    pPort->m_tag = GetNextTag();

    //
    // Tag is an unsigned so when it wraps around to 0 we are out of tags
    //
    if( pPort->m_tag == 0 )
    {
        delete pPort;
        return NULL;
    }

    HRESULT hr = StringCchPrintfA( pPort->m_name,
                                   _countof(pPort->m_name),
                                   "%s%u",
                                   m_bindingStr,
                                   pPort->m_tag );

    if (S_OK != hr)
    {
        MPIU_Assert( false );
    }

    CS lock( m_PortLock );
    m_portList.push_back( *pPort );

    return pPort;
}


//
// Summary:
// Given a port name, retrieve the port
//
// In:
// pPortName       : The port name to retrieve the port
//
// Return:
// The port associated with this port name.
// NULL if the port does not exist
//
DynProcPort*
DynProcServer::GetPortByName(
    _In_z_ const char* pPortName
    )
{
    List<DynProcPort>::iterator itr_end = m_portList.end();

    CS lock( m_PortLock );
    for( List<DynProcPort>::iterator itr = m_portList.begin();
         itr != itr_end;
         ++itr )
    {
        if( CompareStringA( LOCALE_INVARIANT,
                            0,
                            pPortName,
                            -1,
                            itr->m_name,
                            -1 ) == CSTR_EQUAL )
        {
            if( itr->IsClosed() )
            {
                return NULL;
            }

            itr->AddRef();
            return &*itr;
        }
    }
    return NULL;
}


//
// Summary:
// Given a port tag, retrieve the port
//
// In:
// tag            : The port tag to retrieve the port
//
// Return:
// The port associated with this port tag
// NULL if the port does not exist
//
DynProcPort*
DynProcServer::GetPortByTag(
    _In_ unsigned int tag
    )
{
    CS lock( m_PortLock );
    List<DynProcPort>::iterator itr_end = m_portList.end();
    for( List<DynProcPort>::iterator itr = m_portList.begin();
         itr != itr_end;
         ++itr )
    {
        if( tag == itr->m_tag )
        {
            if( itr->IsClosed() )
            {
                return NULL;
            }

            itr->AddRef();
            itr->IncrementPendingReqs();
            return &*itr;
        }
    }
    return NULL;
}


//
// Summary:
// Remove a port
//
// In:
// pPort      : The pointer to the port to be removed
//
void
DynProcServer::RemovePort(
    _In_   DynProcPort* pPort
    )
{
    {
        CS lock( m_PortLock );
        m_portList.remove( *pPort );
    }

    //
    // If the code becomes multi-threaded, the locking mechanism
    // here needs to be rewritten because another thread can
    // call MPI_Open_port at anytime
    //
    if( m_portList.empty() )
    {
        Stop();
    }
}


//
// Summary:
// This is the server side implementation of the RPC AsyncExchangeConnInfo
// routine. The client calls into this routine to send PG info to the server
// and retrieve the PG infos from the server
//
// In:
// tag          : The tag that indentifies the port
// pContextId   : The remote side's context id
// pCount       : The number of processes in the remote side
// pInConnInfos : The array containing the remote side's PG infos
//
// Out:
// pContextId   : The pointer to receive the context id from the local group
// pCount       : The pointer indicating the number of processes in the local group
// ppOutConnInfos: The pointer to receive the array containing the PG infos of the local group
//
_Use_decl_annotations_
void __stdcall
RpcSrvAsyncExchangeConnInfo(
    /*_In_*/    PRPC_ASYNC_STATE      pAsync,
    /*_In_*/    handle_t              hBinding,
    /*_In_*/    unsigned int          tag,
    /*_Out_*/   int*                  pRoot,
    /*_Inout_*/ MPI_CONTEXT_ID*       pContextId,
    /*_Inout_*/ unsigned int*         pCount,
    /*_In_*/    CONN_INFO_TYPE*       pInConnInfos,
    /*_Out_*/   CONN_INFO_TYPE**      ppOutConnInfos
    )
{
    UNREFERENCED_PARAMETER( hBinding );

    DWORD reply;
    ConnRequest* pConnReq = new ConnRequest;
    if( pConnReq == NULL )
    {
        reply = ERROR_OUTOFMEMORY;
        goto CompleteAsyncWithErr;
    }

    //
    // Verify whether this client was connecting to a valid port
    //
    pConnReq->pPort = g_AcceptServer.GetPortByTag( tag );
    if( pConnReq->pPort == NULL )
    {
        reply = ERROR_INVALID_PARAMETER;
        goto FreeAndCompleteAsyncWithErr;
    }

    //
    // Save all the input and output pointers so we can update it later
    // when we process the connection request
    //
    pConnReq->pAsync         = pAsync;
    pConnReq->pRoot          = pRoot;
    pConnReq->pContextId     = pContextId;
    pConnReq->pCount         = pCount;
    pConnReq->pInConnInfos   = pInConnInfos;
    pConnReq->ppOutConnInfos = ppOutConnInfos;

    if( PostQueuedCompletionStatus( g_AcceptServer.hExSet,
                                    sizeof( ConnRequest* ),
                                    EX_KEY_CONNECT_REQUEST,
                                    reinterpret_cast<LPOVERLAPPED>(
                                        pConnReq ) ) == FALSE )
    {
        pConnReq->pPort->DecrementPendingReqs();
        pConnReq->pPort->Release();

        reply = ERROR_NOT_READY;
        goto FreeAndCompleteAsyncWithErr;
    }

    return;

FreeAndCompleteAsyncWithErr:
    delete pConnReq;
CompleteAsyncWithErr:
    *ppOutConnInfos = NULL;
    OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
    RpcAsyncCompleteCall( pAsync, &reply );
    OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
}

//
// Summary:
// This is the handler to process connection request from the client
// The connection request is queued to the progress engine IOCP from the
// RPC thread.
// The progress engine then call this function when it
// sees the connection request
//
MPI_RESULT WINAPI
DynProcServer::DynConnReqHandler(
    _In_ DWORD                     bytesTransferred,
    _In_ VOID*                     pOverlapped
    )
{
    UNREFERENCED_PARAMETER( bytesTransferred );

    ConnRequest* pConnReq = reinterpret_cast<ConnRequest*>( pOverlapped );
    DynProcPort* pPort = pConnReq->pPort;

    MPIU_Assert( pPort != NULL );

    pPort->m_connRequestQueue.push_back( *pConnReq );

    pPort->DecrementPendingReqs();

    Mpi.SignalProgress();
    return MPI_SUCCESS;
}
