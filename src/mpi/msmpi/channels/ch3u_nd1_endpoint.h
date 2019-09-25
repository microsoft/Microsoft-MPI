// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_endpoint.h - Network Direct MPI CH3 Channel endpoint object

--*/

#pragma once

#ifndef CH3U_NDV1_ENDPOINT_H
#define CH3U_NDV1_ENDPOINT_H

#define BCOPY_BUFFER_SIZE_V1 2048

namespace CH3_ND
{
namespace v1
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
    // When we format the src avail message, we put the MW descriptor at the end
    // of the buffer to allow us to copy data that preceeded the buffer being
    // advertised.  That is, we start with a "standard" buffer and start down the
    // bCopy path until we get to a buffer that is above the zCopy threshold.  We then
    // register it, bind it to a MW, and put the MW descriptor at the end of the
    // buffer.  Because our zCopy threshold is at least a whole private buffer, we are
    // guaranteed that any SrcAvail message carries a full payload.
    //
    struct nd_src_avail_t
    {
        UINT8 Data[BCOPY_BUFFER_SIZE_V1 - sizeof(nd_hdr_t) - sizeof(ND_MW_DESCRIPTOR)];
        ND_MW_DESCRIPTOR MwDescriptor;
    };

    //
    // Message structure for all messages put on the wire.  For NdMsgTypeData messages,
    // the Data member is used.  For NdMsgTypeSrcAvail, the SrcAvail member is used.
    //
    struct nd_msg_t : public nd_hdr_t
    {
        union
        {
            UINT8 Data[BCOPY_BUFFER_SIZE_V1 - sizeof(nd_hdr_t)];
            nd_src_avail_t SrcAvail;
        };
    };
    C_ASSERT( sizeof(nd_msg_t) == BCOPY_BUFFER_SIZE_V1 );
    #pragma pack( pop )


class CEpOverlapped;


class CEndpoint
{
private:
    struct nd_endpoint_result_t : public nd_result_t
    {
        CEndpoint* pEp;
    };

    struct nd_send_result_t : public nd_endpoint_result_t
    {
    };

    struct nd_recv_result_t : public nd_endpoint_result_t
    {
    };

    struct nd_read_result_t : public nd_endpoint_result_t
    {
        CMr* pMr;
    };

private:
    CEndpoint();

    int Init(
        __inout CAdapter* pAdapter,
        __in INDConnector* pIConnector,
        __in const struct sockaddr_in& DestAddr,
        __inout MPIDI_VC_t* pVc,
        __in SIZE_T Ird,
        __in SIZE_T Ord
        );

    int Reinit();

protected:
    ~CEndpoint();

public:
    static
    MPI_RESULT
    Create(
        _Inout_ CAdapter* pAdapter,
        _In_ INDConnector* pIConnector,
        _In_ const struct sockaddr_in& DestAddr,
        _Inout_ MPIDI_VC_t* pVc,
        _In_ SIZE_T Ird,
        _In_ SIZE_T Ord,
        _Outptr_ CEndpoint** ppEp
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

    int ConnReqSucceeded( __in HRESULT hr );

    int ConnReqFailed( __in HRESULT hr );

    int ConnCompleted();

    int ConnFailed( __in HRESULT hr );

private:
    static
    int
    WINAPI
    ConnReqSucceeded(
        __in EXOVERLAPPED* pOverlapped
        );

    static
    int
    WINAPI
    ConnReqFailed(
        __in EXOVERLAPPED* pOverlapped
        );

    static
    int
    WINAPI
    ConnCompleted(
        __in EXOVERLAPPED* pOverlapped
        );

    static
    int
    WINAPI
    ConnFailed(
        __in EXOVERLAPPED* pOverlapped
        );

    //
    // Passive side connection establishment functions:
    //
public:
    int Accept( __in const UINT8 SendCredits );

    int AcceptFailed( __in HRESULT hr );

    void Abandon( __in MPIDI_CH3I_VC_state_t State );

private:
    static
    int
    WINAPI
    AcceptSucceeded(
        __in EXOVERLAPPED* pOverlapped
        );

    static
    int
    WINAPI
    AcceptFailed(
        __in EXOVERLAPPED* pOverlapped
        );

    //
    // Connection teardown:
    //
public:
    void Disconnect();

private:
    void DoDisconnect();

private:
    static
    int
    WINAPI
    DisconnectSucceeded(
        __in EXOVERLAPPED* pOverlapped
        );

    static
    int
    WINAPI
    DisconnectFailed(
        __in EXOVERLAPPED* pOverlapped
        );

    //
    // Send functions:
    //
public:
    int
    Send(
        __inout const MPID_IOV* pIov,
        __in int nIov,
        __out MPIU_Bsize_t* pnBytes
        );

    int ProcessSendsUnsafe( _Inout_ bool* pfMpiRequestDone );

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
        if( m_nSends == NDv1_SENDQ_SIZE )
            return false;

        //
        // Don't overrun the remote peer's receive queue.
        //
        if( m_nSendCredits == 0 )
            return false;

        //
        // Can't use the last send credit without giving at least one credit.
        //
        if( m_nSendCredits == 1 && m_nCreditsToGive == 0 )
            return false;

        return true;
    }

    int Progress( __inout bool* pfMpiRequestDone );

    int SendFlowControlMsg();

    inline int SendMsg( __in nd_msg_type_t Type, __in SIZE_T Length );

private:
    static
    int
    SendSucceeded(
        __in nd_result_t* pResult,
        __out bool* pfMpiRequestDone
        );

    static
    MPI_RESULT
    SendFailed(
        _In_ nd_result_t* pResult,
        _Out_ bool* pfMpiRequestDone
        );

    //
    // Receive functions:
    //
private:
    int ProcessReceives( __out bool* pfMpiRequestDone );

    inline int ProcessFlowControlData( __in UINT8 iRecv );

    MPI_RESULT
    ProcessDataMsg(
        _In_ UINT8 iRecv,
        _Out_ bool* pfMpiRequestDone
        );

    MPI_RESULT
    ProcessSrcAvailMsg(
        _In_ UINT8 iRecv,
        _Out_ bool* pfRepost
        );

private:
    static
    int
    RecvSucceeded(
        __in nd_result_t* pResult,
        __out bool* pfMpiRequestDone
        );

    static
    int
    RecvFailed(
        _In_ nd_result_t* pResult,
        bool* pfMpiRequestDone
        );

    //
    // MW Bind functions:
    //
private:
    int
    BindMw(
        __in CMr* pMr,
        __in const void* pBuf,
        __in SIZE_T Length,
        __out ND_MW_DESCRIPTOR* pMwDesc
        );

private:
    static
    int
    BindSucceeded(
        _In_ nd_result_t* pResult,
        bool* pfMpiRequestDone
        );

    static
    int
    BindFailed(
        _In_ nd_result_t* pResult,
        bool* pfMpiRequestDone
        );

    //
    // MW Invalidate functions:
    //
private:
    inline int InvalidateMw();

private:
    static
    int
    InvalidateSucceeded(
        __in nd_result_t* pResult,
        __out bool* pfMpiRequestDone
        );

    static
    int
    InvalidateFailed(
        _In_ nd_result_t* pResult,
        bool* pfMpiRequestDone
        );

    //
    // RDMA Read functions:
    //
private:
    int
    Read(
        _In_ const ND_MW_DESCRIPTOR& MwDescriptor,
        _In_reads_(nDst) char* pDst,
        _In_ MPIU_Bsize_t nDst,
        _Inout_ MPIU_Bsize_t* pcbRead
        );

    int
    ReadToIov(
        _In_ const ND_MW_DESCRIPTOR& MwDescriptor,
        _Out_ bool* pfRepost
        );

private:
    static
    int
    ReadSucceeded(
        __in nd_result_t* pResult,
        __out bool* pfMpiRequestDone
        );

    static
    int
    ReadFailed(
        _In_ nd_result_t* pResult,
        bool* pfMpiRequestDone
        );

    //
    // Helpers:
    //
public:
    INDEndpoint* IEndpoint(){ return m_pIEndpoint.get(); }

private:
    inline HRESULT GetOverlappedResult()
    {
        SIZE_T BytesRet;
        return m_pIConnector->GetOverlappedResult( &m_ConnectorOv.ov, &BytesRet, FALSE );
    }

    void SetState( __in MPIDI_CH3I_VC_state_t State )
    {
        MPIU_Assert( m_pVc );
        MPIU_Assert( State != MPIDI_CH3I_VC_STATE_CONNECTED ||
            m_pVc->state == MPIDI_VC_STATE_ACTIVE );

        m_pVc->ch.state = State;
    }

    int PrepostReceives();

private:
    MPIDI_VC_t* m_pVc;
    StackGuardRef<INDConnector> m_pIConnector;
    StackGuardRef<INDEndpoint> m_pIEndpoint;
    StackGuardRef<CCq> m_pCq;

    struct sockaddr_in m_DestAddr;

    volatile LONG m_nRef;

    nd_send_result_t m_SendResults[NDv1_SENDQ_SIZE];
    nd_read_result_t m_ReadResults[NDv1_READ_LIMIT];
    nd_recv_result_t m_RecvResults[NDv1_RECVQ_SIZE];

    struct
    {
        nd_msg_t Send[NDv1_SENDQ_SIZE];
        nd_msg_t Recv[NDv1_RECVQ_SIZE];
    } m_Buffers;
    ND_MR_HANDLE m_hPrivateBufferMr;

    //
    // Index of the next free send.
    //
    UINT8 m_iSend;
    //
    // Number of outstanding sends.
    //
    UINT8 m_nSends;

    //
    // Index of the next free RDMA Read.
    //
    UINT8 m_iRead;
    //
    // Number of outstanding RDMA Reads.
    //
    UINT8 m_nReads;
    //
    // Offset into the memory window being consumed.  The memory window
    // descriptor is in the message at 'm_iRead'.
    //
    UINT64 m_ReadOffset;
    //
    // Number of unadvertised complete RDMA reads.
    //
    UINT8 m_nRdComplToGive;

    //
    // Index of the first queued receive message.
    //
    UINT8 m_iRecv;
    //
    // Number of queued receive messages.
    //
    UINT8 m_nRecvs;

    //
    // Number of receive credits the remote peer gave us.
    //
    UINT8 m_nSendCredits;
    //
    // Number of unadvertised credits.
    //
    UINT8 m_nCreditsToGive;

    MPID_Request* m_pRecvActive;

    List<CMw> m_MwList;
    List<CMw> m_MwInvalidatingList;

    bool m_fDisconnect;
    EXOVERLAPPED m_ConnectorOv;
};

}   // namespace v1
}   // namespace CH3_ND


#endif //CH3U_NDV1_ENDPOINT_H
