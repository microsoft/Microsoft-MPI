// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "ch3_compression.h"

#include <stdio.h>
#include <windows.h>

typedef NTSTATUS (FN_RtlCompressBuffer)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG,
                                        ULONG, PULONG, PVOID);
typedef NTSTATUS (FN_RtlGetCompressionWorkSpaceSize)(USHORT, PULONG, PULONG);
typedef NTSTATUS (FN_RtlDecompressBuffer)(USHORT, PUCHAR, ULONG, PUCHAR,
                                          ULONG, PULONG);
typedef NTSTATUS (FN_RtlDecompressFragment)(USHORT, PUCHAR, ULONG, PUCHAR,
                                            ULONG, ULONG, PULONG, PVOID);

struct CompressionState
{
    unsigned short CompressionFormatAndEngine;
    unsigned long CompressionWorkSpaceSize;
    unsigned long FragmentWorkSpaceSize;
    unsigned long UncompressedChunkSize;
    void* pCompressionWorkSpace;
    void* pFragmentWorkSpace;
    FN_RtlGetCompressionWorkSpaceSize* pRtlGetCompressionWorkSpaceSize;
    FN_RtlCompressBuffer* pRtlCompressBuffer;
    FN_RtlDecompressBuffer* pRtlDecompressBuffer;
    FN_RtlDecompressFragment* pRtlDecompressFragment;
};

static CompressionState s_CompressionState;

#define MSMPI_MINIMUM_COMPRESSION_THRESHOLD 512
#define MSMPI_DEFAULT_COMPRESSION_THRESHOLD 16384
#define MSMPI_DEFAULT_COMPRESSION_CHUNK_SIZE 4096

int g_CompressionThreshold;

MPI_RESULT InitializeCompression()
{
    g_CompressionThreshold = env_to_int(L"MSMPI_SOCK_COMPRESSION_THRESHOLD",
                                     MSMPI_COMPRESSION_OFF,
                                     MSMPI_COMPRESSION_OFF);

    if(g_CompressionThreshold != MSMPI_COMPRESSION_OFF)
    {
        s_CompressionState.UncompressedChunkSize = static_cast<unsigned long>(
                 env_to_int(L"MSMPI_COMPRESSION_CHUNK_SIZE",
                            MSMPI_DEFAULT_COMPRESSION_CHUNK_SIZE,
                            MSMPI_MINIMUM_COMPRESSION_THRESHOLD) );
        if((s_CompressionState.UncompressedChunkSize != 512  &&
            s_CompressionState.UncompressedChunkSize != 1024 &&
            s_CompressionState.UncompressedChunkSize != 2048 &&
            s_CompressionState.UncompressedChunkSize != 4096))
        {
            //
            //Default to something reasonable.
            //
            s_CompressionState.UncompressedChunkSize = MSMPI_DEFAULT_COMPRESSION_CHUNK_SIZE;
        }

        if(g_CompressionThreshold <
           static_cast<long>(s_CompressionState.UncompressedChunkSize))
        {
            //
            //Round up to at least the default chunk size.
            //
            g_CompressionThreshold = s_CompressionState.UncompressedChunkSize;
        }

        s_CompressionState.CompressionFormatAndEngine =
            COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_STANDARD;

        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if(ntdll == NULL)
        {
            return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**unableToLoadDLL");
        }

        s_CompressionState.pRtlCompressBuffer =
            reinterpret_cast<FN_RtlCompressBuffer *>(
                GetProcAddress(ntdll, "RtlCompressBuffer")
                );
        s_CompressionState.pRtlGetCompressionWorkSpaceSize =
            reinterpret_cast<FN_RtlGetCompressionWorkSpaceSize *>(
                GetProcAddress(ntdll, "RtlGetCompressionWorkSpaceSize")
                );
        s_CompressionState.pRtlDecompressBuffer =
            reinterpret_cast<FN_RtlDecompressBuffer *>(
                GetProcAddress(ntdll, "RtlDecompressBuffer")
                );
        s_CompressionState.pRtlDecompressFragment =
            reinterpret_cast<FN_RtlDecompressFragment *>(
                GetProcAddress(ntdll, "RtlDecompressFragment")
                );

        if( s_CompressionState.pRtlCompressBuffer              == NULL ||
            s_CompressionState.pRtlGetCompressionWorkSpaceSize == NULL ||
            s_CompressionState.pRtlDecompressBuffer            == NULL ||
            s_CompressionState.pRtlDecompressFragment          == NULL )
        {
            return MPIU_ERR_CREATE(
                MPI_ERR_OTHER,
                "**failureGetProcAddress %d",
                GetLastError()
                );
        }

        NTSTATUS compressionStatus =
            s_CompressionState.pRtlGetCompressionWorkSpaceSize(
                s_CompressionState.CompressionFormatAndEngine,
                &s_CompressionState.CompressionWorkSpaceSize,
                &s_CompressionState.FragmentWorkSpaceSize
                );
        if(compressionStatus != STATUS_SUCCESS)
        {
            return MPIU_ERR_CREATE(
                MPI_ERR_OTHER,
                "**failureCompressionWorkSpace %d",
                GetLastError()
                );
        }
        s_CompressionState.pCompressionWorkSpace = MPIU_Malloc(
                static_cast<size_t>(s_CompressionState.CompressionWorkSpaceSize)
                );
        if(s_CompressionState.pCompressionWorkSpace == NULL)
        {
            return MPIU_ERR_CREATE(MPI_ERR_NO_MEM, "**nomem");
        }
        s_CompressionState.pFragmentWorkSpace = MPIU_Malloc(
                static_cast<size_t>(s_CompressionState.FragmentWorkSpaceSize)
                );
        if(s_CompressionState.pFragmentWorkSpace == NULL)
        {
            MPIU_Free(s_CompressionState.pCompressionWorkSpace);
            return MPIU_ERR_CREATE(MPI_ERR_NO_MEM, "**nomem");
        }
    }
    else
    {
        s_CompressionState.pCompressionWorkSpace = NULL;
        s_CompressionState.pFragmentWorkSpace = NULL;
        s_CompressionState.CompressionWorkSpaceSize = 0;
        s_CompressionState.FragmentWorkSpaceSize = 0;
    }
    return MPI_SUCCESS;
}

static inline NTSTATUS CompressBuffer(
    _Out_cap_(compressedBufferLength) unsigned char* pCompressedBuffer,
    unsigned long  compressedBufferLength,
    _In_count_(uncompressedBufferLength) unsigned char* pUncompressedBuffer,
    unsigned long  uncompressedBufferLength,
    _Out_cap_(1) unsigned long* pCountCompressed
    )
{
    return s_CompressionState.pRtlCompressBuffer(
        s_CompressionState.CompressionFormatAndEngine,
        pUncompressedBuffer,
        uncompressedBufferLength,
        pCompressedBuffer,
        compressedBufferLength,
        s_CompressionState.UncompressedChunkSize,
        pCountCompressed,
        s_CompressionState.pCompressionWorkSpace
        );
}

static inline MPI_RESULT DecompressBuffer(
    _Out_cap_(decompressedBufferLength) unsigned char* pDecompressedBuffer,
    unsigned long  decompressedBufferLength,
    _In_count_(compressedBufferLength) unsigned char* pCompressedBuffer,
    unsigned long  compressedBufferLength,
    _Out_cap_(1) unsigned long* pCountDecompressed
    )
{
    NTSTATUS status = s_CompressionState.pRtlDecompressBuffer(
        s_CompressionState.CompressionFormatAndEngine,
        pDecompressedBuffer,
        decompressedBufferLength,
        pCompressedBuffer,
        compressedBufferLength,
        pCountDecompressed
        );

    if(status != STATUS_SUCCESS)
    {
        //
        //Decompression of the entire buffer failed because the target buffer
        //was not large enough.  Decompress just a fragment of the buffer into
        //the target buffer instead.
        //
        status = s_CompressionState.pRtlDecompressFragment(
            s_CompressionState.CompressionFormatAndEngine,
            pDecompressedBuffer,
            decompressedBufferLength,
            pCompressedBuffer,
            compressedBufferLength,
            0, // FragmentOffset
            pCountDecompressed,
            s_CompressionState.pFragmentWorkSpace
            );
        if(status == STATUS_SUCCESS)
        {
            return MPIU_ERR_CREATE(MPI_ERR_TRUNCATE, "**truncate");
        }
    }

    if(status != STATUS_SUCCESS)
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**decompressFailure %d",
            GetLastError()
            );
    }

    return MPI_SUCCESS;
}

static inline void CompressSendBuffer(_Inout_ MPID_Request* pReq)
{
    bool datatypeIsContiguous;
    MPI_Aint datatypeTrueLowerBound;
    MPIDI_msg_sz_t dataSize;
    unsigned long compressedSendSize;
    NTSTATUS status;

    dataSize = fixme_cast<MPIDI_msg_sz_t>(
        pReq->dev.datatype.GetSizeAndInfo(
            pReq->dev.user_count,
            &datatypeIsContiguous,
            &datatypeTrueLowerBound
            )
        );

    MPIU_Assert(pReq->dev.pCompressionBuffer == NULL);

    //
    //Todo: handle discontiguous datatypes.
    //Don't create an error in the error stack here because we ignore data
    //payloads that are discontiguous or below the threshold in size.
    //
    if( datatypeIsContiguous &&
        dataSize >= static_cast<MPIDI_msg_sz_t>( g_CompressionThreshold )
        )
     {
        MPIU_Assert(datatypeTrueLowerBound == 0);
        MPIU_Assert(pReq->dev.user_buf != NULL);

        pReq->dev.pCompressionBuffer = static_cast<CompressionBuffer*>(
            MPIU_Malloc(dataSize + sizeof(MPIDI_msg_sz_t))
            );

        if(pReq->dev.pCompressionBuffer == NULL)
        {
            //
            //Just send the uncompressed message.
            //
            return;
        }

        status = CompressBuffer(
            pReq->dev.pCompressionBuffer->data,
            dataSize,
            static_cast<unsigned char*>( pReq->dev.user_buf ),
            dataSize,
            &compressedSendSize
            );

        if( FAILED(status) )
        {
            if(status == STATUS_BUFFER_ALL_ZEROS)
            {
                pReq->dev.pkt.base.flags |= MPIDI_CH3_PKT_FLAG_COMPRESSED_ALL_ZEROS;
            }
            MPIU_Free(pReq->dev.pCompressionBuffer);
            pReq->dev.pCompressionBuffer = NULL;
        }
        else
        {
            //
            //Update the flags to indicate the data is compressed.
            //
            pReq->dev.pkt.base.flags |= MPIDI_CH3_PKT_FLAG_COMPRESSED;

            MPIU_Assert(compressedSendSize <= dataSize);

            //
            //Record the size of the uncompressed buffer in the first
            //integer after the packet header.
            //
            pReq->dev.pCompressionBuffer->size = compressedSendSize + sizeof(MPIDI_msg_sz_t);
        }
    }
}


MPI_RESULT DecompressRequest(_Inout_ MPID_Request* pReq)
{
    MPI_RESULT mpi_errno;
    MPIDI_msg_sz_t recvBufferSize;
    unsigned long decompressedMessageSize = 0;
    unsigned char* pTempBuffer;
    CompressionBuffer* pCompressedBuffer;

    MPIU_Assert( g_CompressionThreshold != MSMPI_COMPRESSION_OFF );
    MPIU_Assert( pReq->status.count <= UINT_MAX );

    recvBufferSize = static_cast<MPIDI_msg_sz_t>( pReq->status.count );

    pCompressedBuffer = reinterpret_cast<CompressionBuffer*>(pReq->dev.tmpbuf);

    pTempBuffer = static_cast<unsigned char*>( MPIU_Malloc(recvBufferSize) );

    if(pTempBuffer == NULL)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_NO_MEM, "**nomem");
        goto fn_fail;
    }

    mpi_errno = DecompressBuffer(
        pTempBuffer,
        recvBufferSize,
        static_cast<unsigned char*>(pCompressedBuffer->data),
        pReq->dev.recv_data_sz - sizeof(MPIDI_msg_sz_t),
        &decompressedMessageSize
        );

    if( mpi_errno != MPI_SUCCESS && mpi_errno != MPI_ERR_TRUNCATE)
    {
        //
        //Decompression failed.
        //
        MPIU_Free(pTempBuffer);
        goto fn_fail;
    }
    else
    {
        MPIU_Assert( mpi_errno == MPI_ERR_TRUNCATE ||
            decompressedMessageSize >= static_cast<unsigned long>(g_CompressionThreshold)
            );

        //
        //Decompression is assumed successful, copy the pTempBuffer
        //buffer onto the input buffer.
        //
        MPIU_Free(pReq->dev.tmpbuf);

        pReq->dev.tmpbuf = pTempBuffer;
        pReq->dev.recv_data_sz = decompressedMessageSize;
    }

    pReq->dev.pkt.eager_send.flags = MPIDI_CH3_PKT_FLAGS_CLEAR;

    pReq->status.count = decompressedMessageSize;

fn_fail:
    pReq->status.MPI_ERROR = mpi_errno;
    return mpi_errno;
}

MPI_RESULT CompressRequest(_Inout_ MPID_Request* pReq)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPIU_Assert(g_CompressionThreshold != MSMPI_COMPRESSION_OFF);

    switch(pReq->dev.pkt.type)
    {
        case MPIDI_CH3_PKT_EAGER_SEND:
        case MPIDI_CH3_PKT_EAGER_SYNC_SEND:
        case MPIDI_CH3_PKT_READY_SEND:
            {
                if(pReq->dev.user_buf == NULL)
                {
                    break;
                }

                CompressSendBuffer(pReq);

                //
                //Replace the user_buf with the pCompressionBuffer as the iov
                //to be sent.
                //
                MPIU_Assert((pReq->dev.pkt.base.flags & MPIDI_CH3_PKT_IS_COMPRESSED)
                    != MPIDI_CH3_PKT_IS_COMPRESSED);
                if(pReq->dev.pkt.base.flags & MPIDI_CH3_PKT_FLAG_COMPRESSED)
                {
                    //
                    //The compressed buffer has data to send.
                    //
                    pReq->dev.iov[1].buf = reinterpret_cast<PCHAR>( pReq->dev.pCompressionBuffer );
                    pReq->dev.iov[1].len = pReq->dev.pCompressionBuffer->size;
                }
                else if(pReq->dev.pkt.base.flags & MPIDI_CH3_PKT_FLAG_COMPRESSED_ALL_ZEROS)
                {
                    //
                    //The user's buffer was all zeros.  Send the packet only.
                    //
                    pReq->dev.iov[1].buf = NULL;
                    pReq->dev.iov[1].len = 0;
                    pReq->dev.iov_count = 1;
                }

                break;
            }
        case MPIDI_CH3_PKT_RNDV_REQ_TO_SEND:
            {
                //
                //Compress the send buffer of the request associated with
                //the send.
                //
                MPID_Request* pPayloadRequest;
                MPID_Request_get_ptr(pReq->dev.pkt.eager_send.sender_req_id, pPayloadRequest);
                if( pPayloadRequest == nullptr )
                {
                    mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**nullPayloadRequest");
                    break;
                }
                if(pPayloadRequest->dev.user_buf == NULL)
                {
                    break;
                }
                MPIU_Assert(pReq->dev.pCompressionBuffer == NULL);
                CompressSendBuffer(pPayloadRequest);
                break;
            }
        case MPIDI_CH3_PKT_RNDV_SEND:
            {
                MPIU_Assert((pReq->dev.pkt.base.flags & MPIDI_CH3_PKT_IS_COMPRESSED)
                    != MPIDI_CH3_PKT_IS_COMPRESSED);
                if(pReq->dev.pkt.rndv_send.flags & MPIDI_CH3_PKT_FLAG_COMPRESSED)
                {
                    MPIU_Assert(pReq->dev.pCompressionBuffer != NULL);
                    MPIU_Assert(pReq->dev.pCompressionBuffer->size > 0);
                    MPIU_Assert(pReq->dev.user_buf != NULL);
                    //
                    //Fix up the iov to send the compression buffer instead of
                    //the user_buf in the rndv_send packet.
                    //
                    pReq->dev.iov[1].buf = reinterpret_cast<PCHAR>( pReq->dev.pCompressionBuffer );
                    pReq->dev.iov[1].len = pReq->dev.pCompressionBuffer->size;
                }
                else if(pReq->dev.pkt.rndv_send.flags & MPIDI_CH3_PKT_FLAG_COMPRESSED_ALL_ZEROS)
                {
                    MPIU_Assert(pReq->dev.pCompressionBuffer == NULL);
                    MPIU_Assert(pReq->dev.user_buf != NULL);
                    pReq->dev.iov[1].buf = NULL;
                    pReq->dev.iov[1].len = 0;
                    pReq->dev.iov_count = 1;
                }
                break;
            }
        default:
            //
            // Do nothing as there is no payload to compress.
            //
            break;
    }
    return mpi_errno;
}
