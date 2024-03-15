/*++

 Copyright (c) Microsoft Corporation. All rights reserved.
 Licensed under the MIT License.

mpi_api.h - Defines MPI API support routines, used internally to the API layer,
            such as error checking, etc.

--*/

#pragma once

#ifndef _MPI_API_
#define _MPI_API_

#include <oacr.h>

#include "mpi.h"
#include "mpierrs.h"


//
// Special case for "is initialized".
// This should be used in cases where there is no
// additional error checking
//
inline void MpiaIsInitializedOrExit()
{
    if(Mpi.IsReady() == false)
    {
        MPIR_Err_preOrPostInit();
    }
}


//
// Once we get rid of NMPI stuff, we can eliminate the following.
//
__forceinline void MpiaEnter()
{
    Mpi.CallState->nest_count++;
}


__forceinline void MpiaExit()
{
    Mpi.CallState->nest_count--;
}


__forceinline
MPI_RESULT
MpiaDatatypeValidateHandle(
    _In_ MPI_Datatype hType,
    _In_ PCSTR typeParamName,
    _Out_ TypeHandle* pDatatype
    )
{
    if( hType == MPI_DATATYPE_NULL )
    {
        return MPIU_ERR_CREATE( MPI_ERR_TYPE, "**dtypenull %s", typeParamName );
    }

    MPID_Datatype* pType = TypePool::Lookup( hType );
    if( pType == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_TYPE,"**dtype");
    }

    pDatatype->Init( pType );
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaDatatypeValidateCommitted(
    _In_ MPI_Datatype hType,
    _In_ PCSTR typeParamName,
    _Out_ TypeHandle* pDatatype
    )
{
    if( hType == MPI_DATATYPE_NULL )
    {
        return MPIU_ERR_CREATE( MPI_ERR_TYPE, "**dtypenull %s", typeParamName );
    }

    MPID_Datatype* pType = TypePool::Lookup( hType );
    if( pType == nullptr )
    {
        return MPIU_ERR_CREATE( MPI_ERR_TYPE,"**dtype" );
    }

    if( HANDLE_GET_TYPE(hType) != HANDLE_TYPE_BUILTIN )
    {
        if( pType->is_committed == false )
        {
            return MPIU_ERR_CREATE( MPI_ERR_TYPE, "**dtypecommit" );
        }
        pDatatype->Init( pType );
    }
    else
    {
        *pDatatype = g_hBuiltinTypes.Get( hType );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaDatatypeValidate(
    _Pre_maybenull_ const void* buffer,
    _In_ int count,
    _In_ MPI_Datatype hType,
    _In_ PCSTR typeParamName,
    _Out_ TypeHandle* pDatatype
    )
{
    if( count == 0 )
    {
        //
        // return a valid handle so future references work.
        //
        *pDatatype = g_hBuiltinTypes.MPI_Byte;
        return MPI_SUCCESS;
    }

    if( count < 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", count );
    }

    if( hType == MPI_DATATYPE_NULL )
    {
        return MPIU_ERR_CREATE( MPI_ERR_TYPE, "**dtypenull %s", typeParamName );
    }

    MPID_Datatype* pType = TypePool::Lookup( hType );
    if( pType == nullptr )
    {
        return MPIU_ERR_CREATE( MPI_ERR_TYPE,"**dtype" );
    }

    if( HANDLE_GET_TYPE(hType) != HANDLE_TYPE_BUILTIN )
    {
        if( pType->is_committed == false )
        {
            return MPIU_ERR_CREATE( MPI_ERR_TYPE, "**dtypecommit" );
        }

        if( buffer == nullptr && pType->true_lb == 0 && pType->size > 0 )
        {
            return MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**bufnull" );
        }
        pDatatype->Init( pType );
    }
    else
    {
        if( buffer == nullptr )
        {
            return MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**bufnull" );
        }
        *pDatatype = g_hBuiltinTypes.Get( hType );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaDatatypeValidateNotPermanent(
    _In_ TypeHandle hType
    )
{
    MPI_Datatype datatype = hType.GetMpiHandle();

    if( HANDLE_GET_TYPE( datatype ) == HANDLE_TYPE_BUILTIN ||
        datatype == MPI_FLOAT_INT ||
        datatype == MPI_DOUBLE_INT ||
        datatype == MPI_LONG_INT ||
        datatype == MPI_SHORT_INT ||
        datatype == MPI_LONG_DOUBLE_INT)
    {
        return MPIU_ERR_CREATE( MPI_ERR_TYPE, "**dtypeperm" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaDatatypeValidateBuffer(
    _In_ TypeHandle hType,
    _Pre_maybenull_ const void* buffer,
    _In_ int count
    )
{
    bool contig;
    MPI_Aint lb;
    size_t size = hType.GetSizeAndInfo( count, &contig, &lb );

    if( buffer == nullptr && lb == 0 && size > 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**bufnull" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaSendTagValidate(
    _In_ int tag
    )
{
    if( tag < 0 || tag > Mpi.Attributes.tag_ub )
    {
        return MPIU_ERR_CREATE(MPI_ERR_TAG, "**tag %d", tag);
    }
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaRecvTagValidate(
    _In_ int tag
    )
{
    if( tag < MPI_ANY_TAG || tag > Mpi.Attributes.tag_ub )
    {
        return MPIU_ERR_CREATE(MPI_ERR_TAG, "**tag %d", tag);
    }
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaBufferValidateAliasing(
    _In_opt_ const void* sendbuf,
    _In_ MPI_Datatype sendtype,
    _In_ const void* recvbuf,
    _In_ MPI_Datatype recvtype,
    _In_ int count
    )
{
    //
    // If we had a user-specified level of strictness for error checking
    // we could do more here.
    //
    if( count > 0 && sendtype == recvtype && sendbuf == recvbuf )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_BUFFER,
            "**bufalias %s %s",
            "sendbuf",
            "recvbuf"
            );
    }
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaRequestValidate(
    _In_ MPI_Request hReq,
    _Outptr_result_nullonfailure_ MPID_Request** ppReq
    )
{
    if( HANDLE_GET_MPI_KIND( hReq ) != MPID_REQUEST ||
        HANDLE_GET_TYPE( hReq ) == HANDLE_TYPE_INVALID )
    {
        *ppReq = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_REQUEST, "**request" );
    }

    /* Convert MPI object handles to object pointers */
    MPID_Request_get_ptr( hReq, *ppReq );
    if( *ppReq == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**nullptrtype %s", "request" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaMessageValidate(
    _In_ MPI_Message hMsg,
    _Outptr_result_maybenull_ MPID_Request** ppReq
    )
{
    if( HANDLE_GET_MPI_KIND( hMsg ) != MPID_MESSAGE ||
        HANDLE_GET_TYPE( hMsg ) == HANDLE_TYPE_INVALID )
    {
        *ppReq = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_ARG, "**message" );
    }

    if( hMsg == MPI_MESSAGE_NO_PROC )
    {
        *ppReq = nullptr;
        return MPI_SUCCESS;
    }

    //
    // Convert MPI object handles to object pointers.
    //
    MPID_Request_get_ptr( HANDLE_SET_MPI_KIND( hMsg, MPID_REQUEST ), *ppReq );
    if( *ppReq == nullptr )
    {
        return MPIU_ERR_CREATE( MPI_ERR_REQUEST, "**nullptrtype %s", "request" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaCommValidateHandle(
    _In_ MPI_Comm hComm,
    _Outptr_result_nullonfailure_ MPID_Comm** ppComm
    )
{
    MPID_Comm* pComm = CommPool::Lookup( hComm );
    if( pComm == nullptr )
    {
        *ppComm = nullptr;
        return MPIU_ERR_CREATE(MPI_ERR_COMM,"**comm");
    }

    *ppComm = pComm;
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaCommValidateIntracomm(
    _In_ MPI_Comm hComm,
    _Outptr_result_nullonfailure_ MPID_Comm** ppComm
    )
{
    MPID_Comm* pComm = CommPool::Lookup( hComm );
    if( pComm == nullptr )
    {
        *ppComm = nullptr;
        return MPIU_ERR_CREATE(MPI_ERR_COMM,"**comm");
    }

    if( pComm->comm_kind == MPID_INTERCOMM )
    {
        *ppComm = nullptr;
        return MPIU_ERR_CREATE(MPI_ERR_COMM,"**commnotintra");
    }

    *ppComm = pComm;
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaCommValidateRoot(
    _In_ const MPID_Comm *pComm,
    _In_ int root
    )
{
    if( root < 0 || root >= pComm->remote_size )
    {
        return MPIU_ERR_CREATE( MPI_ERR_ROOT, "**root %d", root );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaCommValidateRank(
    _In_ const MPID_Comm* pComm,
    _In_ int rank
    )
{
    if( rank < 0 || rank >= pComm->remote_size )
    {
        return MPIU_ERR_CREATE( MPI_ERR_RANK, "**rank %d %d", rank, pComm->remote_size );
    }
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaCommValidateSendRank(
    _In_ const MPID_Comm* pComm,
    _In_ int rank
    )
{
    if( rank < MPI_PROC_NULL || rank >= pComm->remote_size )
    {
        return MPIU_ERR_CREATE( MPI_ERR_RANK, "**rank %d %d", rank, pComm->remote_size );
    }
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaCommValidateRecvRank(
    _In_ const MPID_Comm* pComm,
    _In_ int rank
    )
{
    if( rank < MPI_ANY_SOURCE || rank >= pComm->remote_size )
    {
        return MPIU_ERR_CREATE(MPI_ERR_RANK, "**rank %d %d", rank, pComm->remote_size );
    }
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaCommValidateTopology(
    _In_ const MPID_Comm* pComm,
    _In_ int topoKind,
    _Outptr_result_nullonfailure_ MPIR_Topology** ppTopo )
{
    MPIR_Topology* pTopo = MPIR_Topology_get( pComm );
    if( pTopo == nullptr )
    {
        *ppTopo = nullptr;
        return MPIU_ERR_CREATE(MPI_ERR_TOPOLOGY,"**notopology");
    }

    if( pTopo->kind != topoKind )
    {
        *ppTopo = nullptr;
        if( topoKind == MPI_CART )
        {
            return MPIU_ERR_CREATE(MPI_ERR_TOPOLOGY, "**notcarttopo");
        }
        else
        {
            return MPIU_ERR_CREATE( MPI_ERR_TOPOLOGY, "**notgraphtopo" );
        }
    }

    *ppTopo = pTopo;
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaKeyvalValidate(
    _In_ int keyval,
    _In_ MPID_Object_kind kind,
    _Out_ KeyHandle* phKey
    )
{
    if( keyval == MPI_KEYVAL_INVALID )
    {
        return MPIU_ERR_CREATE( MPI_ERR_KEYVAL, "**keyvalinvalid" );
    }

    if( HANDLE_GET_MPI_KIND(keyval) != MPID_KEYVAL )
    {
        return MPIU_ERR_CREATE( MPI_ERR_KEYVAL, "**keyval" );
    }

    if( MPID_Keyval::GetKind( keyval ) != kind )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_KEYVAL,
            "**keyvalobj %s",
            MPID_Object_kind_names[kind]
            );
    }

    if( HANDLE_GET_TYPE(keyval) == HANDLE_TYPE_BUILTIN )
    {
        if( MPID_Keyval::GetKind( keyval ) != kind )
        {
            return MPIU_ERR_CREATE(
                MPI_ERR_KEYVAL,
                "**keyvalobj %s",
                MPID_Object_kind_names[kind]
                );
        }
    }
    else
    {
        MPID_Keyval* pKeyval;
        MPID_Keyval_get_ptr( keyval, pKeyval );
        if( pKeyval == nullptr )
        {
            return MPIU_ERR_CREATE(MPI_ERR_KEYVAL, "**nullptrtype %s", "Keyval" );
        }

        if( pKeyval->kind != kind )
        {
            return MPIU_ERR_CREATE(
                MPI_ERR_KEYVAL,
                "**keyvalobj %s",
                MPID_Object_kind_names[kind]
                );
        }
    }

    *phKey = KeyHandle( keyval );
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaKeyvalValidateNotPermanent(
    _In_ KeyHandle hKey
    )
{
    if( HANDLE_GET_TYPE( hKey.GetMpiHandle() ) == HANDLE_TYPE_BUILTIN )
    {
        return MPIU_ERR_CREATE( MPI_ERR_KEYVAL, "**permattr" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaInfoValidateHandle(
    _In_ MPI_Info hInfo,
    _Outptr_result_nullonfailure_ MPID_Info** ppInfo
    )
{
    if( HANDLE_GET_MPI_KIND( hInfo ) != MPID_INFO ||
        HANDLE_GET_TYPE( hInfo ) == HANDLE_TYPE_INVALID )
    {
        *ppInfo = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_INFO, "**info" );
    }

    MPID_Info_get_ptr( hInfo, *ppInfo );
    if( *ppInfo == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_INFO, "**nullptrtype %s", "Info" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaInfoValidateHandleOrNull(
    _In_ MPI_Info hInfo,
    _Outptr_result_maybenull_ MPID_Info** ppInfo
    )
{
    if( hInfo == MPI_INFO_NULL )
    {
        *ppInfo = nullptr;
        return MPI_SUCCESS;
    }

    return MpiaInfoValidateHandle( hInfo, ppInfo );
}


__forceinline
MPI_RESULT
MpiaInfoValidateKey(
    _In_ PCSTR key
    )
{
    if( key == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_INFO_KEY, "**infokeynull");
    }

    size_t keylen = MPIU_Strlen( key, MPI_MAX_INFO_KEY + 1 );
    if(keylen > MPI_MAX_INFO_KEY)
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_INFO_KEY,
            "**infokeylong %s %d %d",
            key,
            static_cast<int>(keylen),
            MPI_MAX_INFO_KEY
            );
    }

    if(keylen == 0)
    {
        return MPIU_ERR_CREATE(MPI_ERR_INFO_KEY, "**infokeyempty");
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaErrhandlerValidateHandle(
    _In_ MPI_Errhandler hErrHandler,
    _Outptr_result_nullonfailure_ MPID_Errhandler** ppErrHandler
    )
{
    if( HANDLE_GET_MPI_KIND( hErrHandler ) != MPID_ERRHANDLER ||
        HANDLE_GET_TYPE( hErrHandler ) == HANDLE_TYPE_INVALID )
    {
        *ppErrHandler = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_ARG, "**errhandler" );
    }

    MPID_Errhandler_get_ptr( hErrHandler, *ppErrHandler );
    if( *ppErrHandler == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_INFO, "**nullptrtype %s", "Errhandler" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaWinValidateHandle(
    _In_ MPI_Win hWin,
    _Outptr_result_nullonfailure_ MPID_Win** ppWin
    )
{
    if( HANDLE_GET_MPI_KIND( hWin ) != MPID_WIN ||
        HANDLE_GET_TYPE( hWin ) == HANDLE_TYPE_INVALID )
    {
        *ppWin = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_WIN, "**win" );
    }

    MPID_Win_get_ptr( hWin, *ppWin );
    if( *ppWin == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_WIN, "**nullptrtype %s", "Window" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaFileValidateHandle(
    _In_ MPI_File hFile,
    _Outptr_result_nullonfailure_ ADIOI_FileD** ppFile
    )
{
    if( hFile == MPI_FILE_NULL )
    {
        *ppFile = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_ARG, "**iobadfh" );
    }

    *ppFile = hFile;
    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaGroupValidateHandle(
    _In_ MPI_Group hGroup,
    _Outptr_result_nullonfailure_ MPID_Group** ppGroup
    )
{
    if( HANDLE_GET_MPI_KIND( hGroup ) != MPID_GROUP ||
        HANDLE_GET_TYPE( hGroup ) == HANDLE_TYPE_INVALID )
    {
        *ppGroup = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_GROUP, "**group" );
    }

    MPID_Group_get_ptr( hGroup, *ppGroup );
    if( *ppGroup == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_GROUP, "**nullptrtype %s", "Group" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaOpValidateHandle(
    _In_ MPI_Op hOp,
    _Outptr_result_nullonfailure_ MPID_Op** ppOp
    )
{
    if( HANDLE_GET_MPI_KIND( hOp ) != MPID_OP ||
        HANDLE_GET_TYPE( hOp ) == HANDLE_TYPE_INVALID )
    {
        *ppOp = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_OP, "**op" );
    }

    *ppOp = OpPool::Lookup( hOp );
    if( *ppOp == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**nullptrtype %s", "Op" );
    }

    return MPI_SUCCESS;
}


__forceinline
MPI_RESULT
MpiaOpValidate(
    _In_ MPI_Op hOp,
    _In_ MPI_Datatype hType,
    _In_ bool rmaOp,
    _Outptr_result_nullonfailure_ MPID_Op** ppOp
    )
{
    if( HANDLE_GET_MPI_KIND( hOp ) != MPID_OP ||
        HANDLE_GET_TYPE( hOp ) == HANDLE_TYPE_INVALID )
    {
        *ppOp = nullptr;
        return MPIU_ERR_CREATE( MPI_ERR_OP, "**op" );
    }

    *ppOp = OpPool::Lookup( hOp );
    if( *ppOp == nullptr )
    {
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**nullptrtype %s", "Op" );
    }

    if ( !rmaOp )
    {
        if ( (*ppOp)->kind == MPID_OP_REPLACE )
        {
            *ppOp = nullptr;
            return MPIU_ERR_CREATE(MPI_ERR_OP, "**replacenotallowed");
        }
        if ( (*ppOp)->kind == MPID_OP_NOOP )
        {
            *ppOp = nullptr;
            return MPIU_ERR_CREATE(MPI_ERR_OP, "**noopnotallowed");
        }
        if ( HANDLE_GET_TYPE(hOp) == HANDLE_TYPE_BUILTIN )
        {
            int mpi_errno = MPIR_Op_check_dtype_table[hOp % 16 - 1](hType);
            if (mpi_errno != MPI_SUCCESS)
            {
                *ppOp = nullptr;
                return mpi_errno;
            }
        }
    }

    return MPI_SUCCESS;
}

#endif /* _MPI_API_ */
