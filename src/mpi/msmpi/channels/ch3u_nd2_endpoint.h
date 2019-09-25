// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_endpoint.h - Network Direct MPI CH3 Channel endpoint object

--*/

#pragma once

#ifndef CH3U_ND_ENDPOINT_H
#define CH3U_ND_ENDPOINT_H

#define BCOPY_BUFFER_SIZE_V2 8096

namespace CH3_ND
{
    enum nd_msg_type_t
    {
        NdMsgTypeData,
        NdMsgTypeFlowControl,
        NdMsgTypeSrcAvail,
    };

    #pragma pack( push, 1 )
    struct nd_hdr_t
    {
        nd_msg_type_t Type;
        UINT8 Credits;
        UINT8 nRdCompl;
        UINT8 Pad[2];
    };

    //
    // When we format the src avail message, we put the data for the remote memory
    // at the end of the buffer to allow us to copy data that preceeded the buffer
    // being advertised.  That is, we start with a "standard" buffer and start down
    // the bCopy path until we get to a buffer that is above the zCopy threshold.
    // We then register it, bind it to a MW, and put the MW descriptor at the end
    // of the buffer.  Because our zCopy threshold is at least a whole private
    // buffer, we are guaranteed that any SrcAvail message carries a full payload.
    //
    // The data necessary to perform an RDMA read or write is:
    //  - Memory region remote token
    //  - Address within the memory region at which to start the transfer
    //  - Length of the buffer within the memory region
    //
    // In NDSPIv1, these fields used to be in the ND_MW_DESCRIPTOR structure.
    // There is no similar structure defined for NDSPIv2, so we define these
    // fields explicitly.
    //
    struct nd_src_avail_t
    {
        UINT8 Data[BCOPY_BUFFER_SIZE_V2 - sizeof(nd_hdr_t) - sizeof(UINT64) - sizeof(UINT64) - sizeof(UINT32)];
        UINT32 Token;
        UINT64 Buffer;
        UINT64 Length;
    };

    //
    // Message structure for all messages put on the wire.  For NdMsgTypeData messages,
    // the Data member is used.  For NdMsgTypeSrcAvail, the SrcAvail member is used.
    //
    struct nd_msg_t : public nd_hdr_t
    {
        union
        {
            UINT8 Data[BCOPY_BUFFER_SIZE_V2 - sizeof(nd_hdr_t)];
            nd_src_avail_t SrcAvail;
        };
    };

    C_ASSERT( sizeof(nd_msg_t) == BCOPY_BUFFER_SIZE_V2 );

    //
    // Ensure Token, Buffer, and Length are properly aligned.
    //
#define C_ASSERT_ALIGN( s, m ) \
    C_ASSERT( offsetof(s,m) % RTL_FIELD_SIZE(s,m) == 0 )

    C_ASSERT_ALIGN( nd_msg_t, SrcAvail.Token );
    C_ASSERT_ALIGN( nd_msg_t, SrcAvail.Buffer );
    C_ASSERT_ALIGN( nd_msg_t, SrcAvail.Length );

    #pragma pack( pop )


class Endpoint
{
private:
    MPIDI_VC_t* m_pVc;
    StackGuardRef<IND2Connector> m_pIConnector;
    StackGuardRef<IND2QueuePair> m_pIEndpoint;
    StackGuardRef<Cq> m_pCq;

    struct sockaddr_in m_DestAddr;

    volatile LONG m_nRef;

    ULONG* m_cbRecv;

    struct
    {
        nd_msg_t* Send;
        nd_msg_t* Recv;
    } m_Buffers;
    IND2MemoryRegion* m_pIMr;
    UINT32 m_MrToken;

    //
    // Index of the next free send.
    //
    UINT8 m_iSend;
    UINT8 m_SendQueueMask;
    //
    // Number of outstanding sends.
    //
    UINT8 m_nSends;
    //
    // Send queue depth.
    //
    UINT8 m_SendQueueDepth;

    //
    // Offset into the memory window being consumed.  The memory window
    // descriptor is in the message at 'm_iRecv'.
    //
    ULONG m_ReadOffset;
    //
    // Index of the next free RDMA Read.
    //
    UINT8 m_iRead;
    //
    // Number of outstanding RDMA Reads.
    //
    UINT8 m_nReads;
    //
    // Number of unadvertised complete RDMA reads.
    //
    UINT8 m_nRdComplToGive;
    //
    // Number of RdCompl messages we need to receive to continue processing sends.
    //
    UINT8 m_nRdComplToRecv;

    //
    // Index of the first queued receive message.
    //
    UINT8 m_iRecv;
    UINT8 m_RecvQueueMask;
    //
    // Number of queued receive messages.
    //
    UINT8 m_nRecvs;
    //
    // Receive queue depth.
    //
    UINT8 m_RecvQueueDepth;

    //
    // Number of receive credits the remote peer gave us.
    //
    UINT8 m_nSendCredits;
    //
    // Number of unadvertised credits.
    //
    UINT8 m_nCreditsToGive;

    //
    // Number of connection retries left, for non-timeout failures.
    //
    UINT16 m_nConnectRetries;

    MPID_Request* m_pRecvActive;

    bool m_fDisconnect;
    ND_OVERLAPPED m_ConnectorOv;

private:
    Endpoint();
    Endpoint( const Endpoint& rhs );
    Endpoint(
        _In_ IND2Connector& pIConnector,
        _In_ const struct sockaddr_in& destAddr
        );

    int Init(
        _Inout_ Adapter& adapter,
        _Inout_ MPIDI_VC_t* pVc
        );

    int Reinit();
    Endpoint& operator = ( const Endpoint& rhs );

protected:
    ~Endpoint();

public:
    static
    MPI_RESULT
    Create(
        _Inout_ Adapter& adapter,
        _In_ IND2Connector* pIConnector,
        _In_ const struct sockaddr_in& destAddr,
        _Inout_ MPIDI_VC_t* pVc,
        _Outptr_ Endpoint** ppEp
        );

public:
    LONG AddRef()
    {
        return ::InterlockedIncrement(&m_nRef);
    }

    void Release();

    //
    // Active side connection establishment functions:
    //
public:
    int Connect();

private:
    int CompleteConnect();

    int HandleTimeout();

    int ConnReqFailed( __in HRESULT hr );

    int ConnCompleted();

    int ConnFailed( __in HRESULT hr );

private:
    static ND_OVERLAPPED::FN_CompletionRoutine ConnectRetryHandler;
    static ND_OVERLAPPED::FN_CompletionRoutine ConnectHandler;
    static ND_OVERLAPPED::FN_CompletionRoutine CompleteConnectHandler;

    //
    // Passive side connection establishment functions:
    //
public:
    int Accept( __in const UINT8 nSendCredits );

    void Abandon( __in MPIDI_CH3I_VC_state_t state );

private:
    int AcceptCompleted();

    int AcceptFailed( __in HRESULT hr );

private:
    static ND_OVERLAPPED::FN_CompletionRoutine AcceptHandler;

    //
    // Connection teardown:
    //
public:
    void Disconnect();

private:
    void DoDisconnect();

private:
    static ND_OVERLAPPED::FN_CompletionRoutine DisconnectHandler;

    //
    // Send functions:
    //
public:
    int
    Send(
        __inout const MPID_IOV* pIov,
        __in int nIov,
        __out MPIU_Bsize_t* pcbSent
        );

    int ProcessSendsUnsafe( __inout bool* pfMpiRequestDone );

    inline int ProcessSends( __inout bool* pfMpiRequestDone )
    {
        SendQLock lock(m_pVc);
        return ProcessSendsUnsafe(pfMpiRequestDone);
    }

private:
    inline bool OkToSend() const
    {
        //
        // Don't overrun the send or completion queues.
        //
        if( m_nSends == m_SendQueueDepth )
        {
            return false;
        }

        //
        // Don't overrun the remote peer's receive queue.
        //
        if( m_nSendCredits == 0 )
        {
            return false;
        }

        //
        // Can't use the last send credit without giving at least one credit.
        //
        if( m_nSendCredits == 1 && m_nCreditsToGive == 0 )
        {
            return false;
        }

        return true;
    }

    int Progress( __inout bool* pfMpiRequestDone );

    int SendFlowControlMsg();

    inline int SendNextMsg( __in nd_msg_type_t type, __in ULONG cbSend );

    int
    SendCompletion(
        __in const ND2_RESULT* pResult,
        __out bool* pfMpiRequestDone
        );

    //
    // Receive functions:
    //
    int ProcessReceives( _Out_ bool* pfMpiRequestDone );

    inline int ProcessFlowControlData( __in UINT8 iRecv, __out bool* pfMpiRequestDone );

    int ProcessReadComplete(
        __in UINT8 nRdCompl,
        __out bool* pfMpiRequestDone
        );

    int ProcessDataMsg( _In_ UINT8 iRecv, _Inout_ bool* pfMpiRequestDone );

    int ProcessSrcAvailMsg( __in UINT8 iRecv, __out bool* pfRepost );

    int
    RecvCompletion(
        _In_ const ND2_RESULT* pResult,
        _Out_ bool* pfMpiRequestDone
        );

    //
    // RDMA Read functions:
    //
    int
    Read(
        _In_  const nd_src_avail_t& srcAvail,
        _In_reads_(nDst) char* pDst,
        _In_ MPIU_Bsize_t nDst,
        _Inout_ MPIU_Bsize_t* pcbRead
        );

    int
    ReadToIov(
        _In_ const nd_src_avail_t& srcAvail,
        _Out_ bool* pfRepost
        );

    int
    ReadCompletion(
        __in ND2_RESULT* pResult,
        __out bool* pfMpiRequestDone
        );

public:
    static
    int
    CompletionHandler(
        __in ND2_RESULT* pResult,
        __out bool* pfMpiRequestDone
        );

    //
    // Helpers:
    //
public:
    IND2QueuePair* IEndpoint(){ return m_pIEndpoint.get(); }

private:
    inline HRESULT GetOverlappedResult()
    {
        return m_pIConnector->GetOverlappedResult( &m_ConnectorOv, FALSE );
    }

    void SetState( __in MPIDI_CH3I_VC_state_t state )
    {
        MPIU_Assert( m_pVc );
        MPIU_Assert( state != MPIDI_CH3I_VC_STATE_CONNECTED ||
            m_pVc->state == MPIDI_VC_STATE_ACTIVE );

        m_pVc->ch.state = state;
    }

    int PrepostReceives();
};

}   // namespace CH3_ND


#endif //CH3U_ND_ENDPOINT_H
