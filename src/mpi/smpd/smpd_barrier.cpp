// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"


static DWORD
smpd_send_barrier_result(
    _In_ smpd_overlapped_t* pov,
    _In_ const GUID& kvs,
    _In_ INT16 dest
    )
{
    smpd_init_result_command( pov->pRes, dest );

    wchar_t name[GUID_STRING_LENGTH + 1];
    GuidToStr( kvs, name, _countof(name) );
    smpd_dbg_printf(L"sending reply to barrier command '%s'.\n", name);

    return RpcAsyncCompleteCall( pov->pAsync, nullptr );
}


static smpd_barrier_node_t*
smpd_alloc_barrier(
    _In_ const GUID& kvs,
    _In_ UINT16 count
    )
{
    smpd_barrier_node_t* node;
    node = new smpd_barrier_node_t;
    if( node == nullptr )
    {
        return nullptr;
    }

    node->in_array = new smpd_barrier_in_t[count];
    if( node->in_array == nullptr )
    {
        delete node;
        return nullptr;
    }

    node->kvs = kvs;

    node->numExpected = count;
    node->numReached = 0;
    node->next = nullptr;
    return node;
}


static void
smpd_free_barrier(
    _In_ _Post_ptr_invalid_ smpd_barrier_node_t* node
    )
{
    delete[] node->in_array;
    delete node;
}


static smpd_barrier_node_t**
smpd_find_barrier_node(
    _In_ const GUID& kvs
    )
{
    smpd_barrier_node_t** ppb;
    for(ppb = &smpd_process.barrier_list; *ppb != nullptr; ppb = &(*ppb)->next)
    {
        if( IsEqualGUID( (*ppb)->kvs, kvs ) )
        {
            break;
        }
    }

    return ppb;
}


static void
smpd_release_barrier(
    _Inout_ smpd_barrier_node_t** ppBarrierNode,
    _In_ DWORD rc
    )
{
    smpd_barrier_node_t* pBarrierNode = *ppBarrierNode;
    for(unsigned i = 0; i < pBarrierNode->numExpected; i++)
    {
        reinterpret_cast<SmpdResHdr*>(pBarrierNode->in_array[i].pov->pRes)->err = rc;

        DWORD rc = smpd_send_barrier_result(
            pBarrierNode->in_array[i].pov,
            pBarrierNode->kvs,
            pBarrierNode->in_array[i].dest );
        if( rc != NOERROR )
        {
            smpd_err_printf(
                L"Failed to send barrier result back to smpd=%hd ctx_key=%hu error %u \n",
                pBarrierNode->in_array[i].dest,
                pBarrierNode->in_array[i].ctx_key,
                rc );
        }
    }

    *ppBarrierNode = pBarrierNode->next;
    smpd_free_barrier( pBarrierNode );
}


void
smpd_handle_barrier_result(
    _In_ smpd_overlapped_t* pov
    )
{
    DWORD rc = reinterpret_cast<SmpdResHdr*>(pov->pRes)->err;

    const SmpdBarrierCmd* pBarrierCmd = &pov->pCmd->BarrierCmd;
    smpd_barrier_node_t** ppb = smpd_find_barrier_node( pBarrierCmd->kvs );

    //
    // We should not have sent a request for barrier unless
    // we have collected all the barrier requests from the children
    //
    MPIU_Assert( *ppb != nullptr && (*ppb)->numReached == (*ppb)->numExpected );

    smpd_release_barrier( ppb, rc );

    //
    // If there are errors we expect that the PMI clients
    // will abort very soon after this.
    //
}


static DWORD
smpd_send_barrier_command(
    _In_ const GUID& kvs
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_BARRIER,
        smpd_process.tree_id,
        smpd_process.parent_id );
    if(pCmd == nullptr)
    {
        smpd_err_printf(L"unable to create a 'close' command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pCmd->BarrierCmd.kvs = kvs;
    pCmd->BarrierCmd.header.ctx_key = USHRT_MAX;

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        smpd_handle_barrier_result );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for 'close' command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD rc = smpd_post_command(
        smpd_process.parent_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"failed to post close command error %lu\n", rc);
    }
    return rc;

}


static
UINT16
GetNumBarrierChildren(GUID kvs)
{
    UINT16 count = 0;

    smpd_numchildren_t* numChildrenNode = smpd_process.num_children_list;
    while (numChildrenNode != nullptr)
    {
        if (IsEqualGUID(numChildrenNode->kvs, kvs))
        {
            break;
        }
        numChildrenNode = numChildrenNode->next;
    }
    if (numChildrenNode != nullptr)
    {
        count = numChildrenNode->numChildren;
    }

    smpd_barrier_forward_t* ppbf = smpd_process.barrier_forward_list;
    while (ppbf != nullptr)
    {
        if (IsEqualGUID(ppbf->kvs, kvs))
        {
            break;
        }
        ppbf = ppbf->next;
    }

    if (ppbf != nullptr)
    {
        count += ppbf->left;
        count += ppbf->right;
    }

    return count;
}


//
// Summary:
//  Handler for the barrier command from the MPI process
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_barrier_command(
    _In_ smpd_overlapped_t* pov
    )
{
    const SmpdBarrierCmd* pBarrierCmd = &pov->pCmd->BarrierCmd;

    smpd_dbg_printf(L"Handling %s src=%hd ctx_key=%hu\n",
                    CmdTypeToString(pBarrierCmd->header.cmdType),
                    pBarrierCmd->header.src,
                    pBarrierCmd->header.ctx_key );
    smpd_barrier_node_t** ppb = smpd_find_barrier_node( pBarrierCmd->kvs );
    if(*ppb == nullptr)
    {
        //
        // This is the first guy in so create a new barrier structure
        // and add it to the list
        //
        UINT16 count = GetNumBarrierChildren(pBarrierCmd->kvs);

        wchar_t name[GUID_STRING_LENGTH + 1];
        GuidToStr( pBarrierCmd->kvs, name, _countof(name) );
        smpd_dbg_printf(L"initializing barrier(%s): in=1 size=%hu\n", name, count);

        *ppb = smpd_alloc_barrier( pBarrierCmd->kvs, count);
        if(*ppb == nullptr)
        {
            smpd_post_abort_command(L"insufficient memory for a barrier object.\n");
            reinterpret_cast<SmpdResHdr*>(pov->pRes)->err = ERROR_NOT_ENOUGH_MEMORY;

            smpd_send_barrier_result(
                pov,
                pBarrierCmd->kvs,
                pBarrierCmd->header.src );

            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    smpd_barrier_node_t* iter = *ppb;
    iter->in_array[iter->numReached].pov = pov;
    iter->in_array[iter->numReached].dest = pBarrierCmd->header.src;
    iter->in_array[iter->numReached].ctx_key = pBarrierCmd->header.ctx_key;

    wchar_t name[GUID_STRING_LENGTH + 1];
    GuidToStr( iter->kvs, name, _countof(name) );

    smpd_dbg_printf(L"incrementing barrier(%s) incount from %hu to %hu out of %hu\n",
                    name,
                    iter->numReached,
                    iter->numReached + 1U,
                    iter->numExpected );

    iter->numReached++;
    if( iter->numReached < iter->numExpected )
    {
        return NOERROR;
    }

    if( smpd_process.tree_id == SMPD_IS_TOP )
    {
        smpd_dbg_printf(L"all in barrier, release the barrier.\n");
        smpd_release_barrier( ppb, NOERROR );

        return NOERROR;
    }

    smpd_dbg_printf(L"all in barrier, sending barrier to parent.\n");

    DWORD rc = smpd_send_barrier_command( iter->kvs );
    if( rc != NOERROR )
    {
        //
        // Release the PMI clients as well SMPD children
        //
        smpd_release_barrier( ppb, rc );
    }

    //
    // We take no action here. When we receive the barrier result
    // from the parent we will release the barrier
    //
    return NOERROR;
}
