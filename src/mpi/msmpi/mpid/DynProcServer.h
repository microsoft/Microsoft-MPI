// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "rpc.h"
#include "list.h"
#include "cs.h"
#include "DynProcTypes.h"


class DynProcPort;

//
// This struct reprensents a connection request from the client
// of Connect/Accept.
// The RPC layer allocates this structure and setup the pointers
// to point to the [in] data from the client. When the MPI process
// processes this request from the Accept routine, it reads the [in]
// data and point the pointers to the [out] data and uses the RPC
// async handle to complete the RPC async call.
//
// pAsync : Pointer to the RPC Async structure
// pRoot  : Pointer to receive/send the root's rank of both side
// pCount : Pointer to receive/send the number of connection infos
// ppOutConnInfos: Pointer to send the array of connection infos
// pInConnInfos:   The array of connection infos from the remote side
// pContextId  :   Pointer to receive/send the context_id
// pPort       :   A pointer to quickly retrieve the port the request was made on
//
struct ConnRequest
{
    LIST_ENTRY               m_link;
    RPC_ASYNC_STATE*         pAsync;
    int*                     pRoot;
    unsigned int*            pCount;
    CONN_INFO_TYPE**         ppOutConnInfos;
    CONN_INFO_TYPE*          pInConnInfos;
    MPI_CONTEXT_ID*          pContextId;
    DynProcPort*             pPort;
};


//
// This class represents a dynamic port opened for receiving
// MPI_Comm_connect request. The ports are chained in a linked
// list maintained by the acceptServer. Each port has its name
// and tag that are given by the acceptServer. In addition,
// each port maintains a linked list of incoming connection requests
//
// Fields:
// name            : Name of the port
// tag             : A unique number (per server) that identifies this port
// connRequestQueue: Queue of connection requests received by the progress engine
// isClosed        : Whether the port has been marked closed by MPI_Close_port
// m_pendingReqs   : Number of pending requests (can be more than size of queue
//                   because the RPC layer will increase this count whenver
//                   it receives an incoming connection request)
//
class DynProcPort
{
    friend class DynProcServer;

public:
    LIST_ENTRY               m_link;
    char                     m_name[MPI_MAX_PORT_NAME];
    unsigned int             m_tag;
    List<ConnRequest>        m_connRequestQueue;

private:
    bool                     m_isClosed;
    LONG volatile            m_refcount;
    LONG volatile            m_pendingReqs;

public:
    DynProcPort();

    ~DynProcPort(){}

    void AddRef()
    {
        InterlockedIncrement( &m_refcount );
    }


    void Release()
    {
        if( InterlockedDecrement( &m_refcount ) == 0 )
        {
            delete this;
        }
    }


    void IncrementPendingReqs()
    {
        InterlockedIncrement( &m_pendingReqs );
    }


    void DecrementPendingReqs()
    {
        InterlockedDecrement( &m_pendingReqs );
    }


    void Close();


    bool IsClosed() const
    {
        return (m_isClosed == true);
    }


    const bool HasNoPendingRequest()
    {
        return (m_pendingReqs == 0);
    }

};


//
// This class represents the accept server.
//
// Fields:
// m_bindingStr      : The RPC Server's binding string
// m_portList        : The list of currently active ports
// m_PortLock        : Lock to serialize access to m_portList
// m_tag             : Existing counter for tag (each port has a unique tag)
// m_bListening      : Is the RPC server up or not?
//
class DynProcServer
{
private:
    char               m_bindingStr[MPI_MAX_PORT_NAME];
    List<DynProcPort>  m_portList;
    CriticalSection    m_PortLock;
    unsigned int       m_tag;
    bool               m_bListening;

public:

    //
    // This is the handle to the progress engine IOCP
    //
    ExSetHandle_t    hExSet;
    DynProcServer();

    ~DynProcServer()
    { //MPIU_Assert( m_bListening == false && m_refcount == 0 );
    }

    //
    // Start the Accept server to listen for Connect requests
    //
    MPI_RESULT Start();


    //
    // Stop the Accept server that listens for Connect requests
    //
    void Stop();


    //
    // Return whether the Rpc Server is listening for connection
    //
    bool IsListening() const { return m_bListening; }


    //
    // Return a new port that can be used for Connect request
    //
    DynProcPort* GetNewPort();


    //
    // Given a name, find the port object associated with this name.
    // This will add a reference to the port that will need to be released
    // when the ref is not used anymore.
    //
    DynProcPort* GetPortByName(
        _In_z_ const char*  pPortName
        );


    //
    // Given a tag, find the port object associated with this tag
    // This will add a reference to the port that will need to be released
    // when the ref is not used anymore.
    //
    DynProcPort* GetPortByTag(
        _In_ unsigned int tag
        );


    //
    // Remove the port from the list of ports
    //
    void RemovePort(
        _In_   DynProcPort* pPort
        );


    //
    // Summary:
    // This is the handler to process connection request from the client
    // The connection request is queued to the progress engine IOCP from the
    // RPC thread.
    // The progress engine then call this function when it
    // sees the connection request
    //
    static MPI_RESULT WINAPI DynProcServer::DynConnReqHandler(
        _In_ DWORD bytesTransferred,
        _In_ VOID* pOverlapped
        );


private:

    //
    // Disable copy and assignment
    //
    DynProcServer& operator=( const DynProcServer& /*otherServer*/ ){}
    DynProcServer( const DynProcServer& /*otherServer*/ ){}

    //
    // Get the next tag
    //
    unsigned int GetNextTag(){ return ++m_tag; }

    //
    // Start the RPC Server
    //
    int StartRpcServer(
        _In_z_     const char*         pProtSeq,
        _In_opt_z_ const char*         pEndpoint,
        _In_       RPC_IF_HANDLE       rpcInterface,
        _In_opt_   RPC_IF_CALLBACK_FN* pSecurityCallbackFn
        );

    //
    // Obtain the binding string for this server
    //
    _Success_(return == NOERROR)
    int
    GetRpcBindingString(
        _In_z_     const char* lrpcEpStr,
        _In_opt_z_ const char* tcpEpStr
        );

    //
    // Stop the RPC Server
    //
    int StopRPCServer(
        _In_       RPC_IF_HANDLE       rpcInterface
        );

};

extern DynProcServer g_AcceptServer;
