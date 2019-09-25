// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <wmistr.h>
#include <evntrace.h>
#include "evntprov.h"

#ifndef _In_reads_
#  define _In_reads_(max_count)
#endif

//
//Enums to identify call sites.
//

//used for events in ch3u_nd_adapter.cpp
enum AdapterInit{
    AdapterInitOpen = 1,
    AdapterInitQuery,
    AdapterInitCQDepth,
    AdapterInitInitiatorQDepth,
    AdapterInitRecvQDepth,
    AdapterInitCreateOverlapped
};

enum AdapterListen{
    AdapterListenCreateListener = 1,
    AdapterListenBind,
    AdapterListenGetLocalAddress,
    AdapterListenListen
};

enum AdapterGetConnectionRequest{
    GetConnectionRequestCreateConnector = 1,
    GetConnectionRequestGetConnectionRequest
};

enum AdapterCreateConector{
    AdapterCreateConectorCreateConnector = 1,
    AdapterCreateConectorBind
};

enum AdapterGetConnSucceeded{
    AdapterGetConnSucceededInvalidBufferSize = 1,
    AdapterGetConnSucceededAbortOrInvalid,
    AdapterGetConnSucceededReject,
    AdapterGetConnSucceededMismatchedVersion,
    AdapterGetConnSucceededPGFind,
    AdapterGetConnSucceededRank,
    AdapterGetConnSucceededHeadToHeadReject,
    AdapterGetConnSucceededHeadToHeadShutdown,
    AdapterGetConnSucceededAdapterShutdown,
    AdapterGetConnSucceededDefaultReject,
    AdapterGetConnSucceededSuccess
};

//used for events in ch3u_nd_endpoint.cpp
enum Endpoint{
    EndpointCompleteConnectBufferSize = 1,
    EndpointCompleteConnectDefault,
    EndpointCompleteConnectPending,
    EndpointConnReqFailedPassive,
    EndpointConnReqFailedCanceled,
    EndpointConnReqFailedFailed,
    EndpointConnCompleted,
    EndpointConnFailedRetry,
    EndpointConnFailedFail,
    EndpointAcceptPending,
    EndpointPrepostReceivesFailed,
    EndpointAcceptCompleted,
    EndpointAcceptFailedAbortedOrTimeout,
    EndpointAcceptFailedFailed,
    EndpointDisconnect,
    EndpointConnect,
    EndpointAccept,
    EndpointHandleTimeoutConnectTimeout,
    EndpointCompleteConnectAbortedOrInvalid,
    EndpointCompleteConnectCompleteConnect,
    EndpointHandleTimeoutConnect
};

//used for events in ch3u_nd_env.cpp
enum EnvironmentListen{
    EnvironmentListenNoNDv2Providers = 1,
    EnvironmentListenQueryAddressListForSizeFailed,
    EnvironmentListenQueryAddressListFailed
};

enum EnvironmentConnect{
    EnvironmentConnectGetAddrsFromBc = 1,
    EnvironmentConnectNoLocalNoRemoteForce,
    EnvironmentConnectNoLocalForce,
    EnvironmentConnectNoLocalNoFallback,
    EnvironmentConnectNoLocalNoFallbackForce,
    EnvironmentConnectNoRemoteForce,
    EnvironmentConnectNoRemoteNoFallback,
    EnvironmentConnectNoPathForce,
    EnvironmentConnectNoPathNoFallback,
    EnvironmentConnectNoLocalFallback,
    EnvironmentConnectNoRemoteFallback,
    EnvironmentConnectNoPathFallback
};

//used for events in ch3_progress_connect.cpp
enum Shm_connect{
    Shm_connectConnectQueueName = 1,
    Shm_connectConnectQueueAttach,
    Shm_connectWriteQueue,
    Shm_connectNotifyConnect
};

enum Shm_accept{
    Shm_acceptQueueAttach = 1,
    Shm_acceptMismatchedVersion,
    Shm_acceptPGFind,
    Shm_acceptRank,
    Shm_acceptGetConnStringFailed,
    Shm_acceptGetStringArgFailed,
    Shm_acceptBootstrapQueueAttach
};

//used for events in ch3_progress_sock.c
enum RecvOpenRequestSucceeded{
    RecvOpenRequestSucceededUnexpectedControl = 1,
    RecvOpenRequestSucceededMismatchedVersion,
    RecvOpenRequestSucceededInternal,
    RecvOpenRequestSucceededSuccess
};

//used for events in sock.c
enum ConnectFailedEnum{
    ConnectFailedEnumAbortedBeforeTimeout = 1,
    ConnectFailedEnumTimeout,
    ConnectFailedEnumAbortedClosing,
    ConnectFailedEnumRefused,
    ConnectFailedEnumError,
    ConnectFailedEnumExhausted,
    ConnectFailedEnumFail
};


//
// Mpi specific wrappers around trace functions
//
ULONG MpiTraceError(
    REGHANDLE RegHandle,
    PCEVENT_DESCRIPTOR EventDescriptor,
    int ErrorCode
    );


#define SENTINEL_MASK               ((ULONG_PTR)0x01)

#define IS_SENTINEL(p_)             (0 == (((ULONG_PTR)p_) & (~SENTINEL_MASK)))

#define SENTINEL_SAFE_SIZE(p_)      (IS_SENTINEL(p_)?0:sizeof(*p_))

#define SENTINEL_SAFE_COUNT(p_,c_)  (IS_SENTINEL(p_)?0:c_)

//
// This is a generated header.
//
#include "MpiTraceEvents.h"


#ifndef MAX_TRACE_ARRAY_VALUE_COUNT
#define MAX_TRACE_ARRAY_VALUE_COUNT 1
#endif

#ifndef TraceArrayLength
#define TraceArrayLength(_c_) ((BYTE)(_c_<=MAX_TRACE_ARRAY_VALUE_COUNT?_c_:MAX_TRACE_ARRAY_VALUE_COUNT))
#endif


//
// Conditional trace macros.  They will only trace if enabled.  The generated header
//  contains the macros and values we use for these tests.  See that header for more deatils.
//

#define TraceError(_fn_,_errorcode_) \
        MCGEN_ENABLE_CHECK(MICROSOFT_HPC_MPI_PROVIDER_Context, EVENT_Error_##_fn_) ?\
        MpiTraceError(Microsoft_HPC_MPIHandle, &EVENT_Error_##_fn_,_errorcode_) \
        : ERROR_SUCCESS

