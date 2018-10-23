// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiexec.h"

void smpd_fix_up_host_tree(smpd_host_t* host)
{
    smpd_host_t *cur, *iter;
    BOOL left_found;

    cur = host;
    while (cur != NULL)
    {
        left_found = FALSE;
        iter = static_cast<smpd_host_t*>( cur->Next );
        while (iter != NULL)
        {
            if(iter->parent == cur->HostId)
            {
                if(left_found)
                {
                    cur->right = iter;
                    break;
                }
                cur->left = iter;
                left_found = TRUE;
            }
            iter = static_cast<smpd_host_t*>( iter->Next );
        }
        cur = static_cast<smpd_host_t*>( cur->Next );
    }
}


smpd_host_t*
smpd_get_host_id(
    _In_ PCWSTR host_name
    )
{
    smpd_host_t* host;
    int bit, mask, temp;

    /* look for the host in the list */
    host = smpd_process.host_list;
    while (host)
    {
        if( CompareStringW( LOCALE_INVARIANT,
                            0,
                            host->name,
                            -1,
                            host_name,
                            -1 ) == CSTR_EQUAL )
        {
            return host;
        }
        if(host->Next == NULL)
            break;
        host = static_cast<smpd_host_t*>( host->Next );
    }

    /* allocate a new host */
    if(host != NULL)
    {
        host->Next = new smpd_host_t;
        host = static_cast<smpd_host_t*>( host->Next );
    }
    else
    {
        host = new smpd_host_t;
        smpd_process.host_list = host;
    }
    if(host == NULL)
    {
        smpd_err_printf(L"malloc failed to allocate a host node structure\n");
        return nullptr;
    }

    MPIU_Strcpy( host->name, _countof(host->name), host_name );

    //
    // nameA will be used to store UTF-8 representation of the host name
    // It will only be necessary if we need to collect the hostname
    // for the Debug Interface.
    //
    host->nameA = nullptr;
    host->parent = smpd_process.mpiexec_parent_id;
    host->HostId = smpd_process.mpiexec_tree_id;
    host->connected = FALSE;
    host->collected = FALSE;
    host->nproc = -1;
    host->Next = nullptr;
    host->left = nullptr;
    host->right = nullptr;
    host->Summary = nullptr;
    host->hwTree = nullptr;
    host->nodeProcCount = 0;
    host->pLaunchBlockList = nullptr;
    host->pBlockOptions = nullptr;

    /* move to the next id and parent */
    smpd_process.mpiexec_tree_id++;

    temp = smpd_process.mpiexec_tree_id >> 2;
    bit = 1;
    while (temp)
    {
        bit <<= 1;
        temp >>= 1;
    }
    mask = bit - 1;
    smpd_process.mpiexec_parent_id = bit | (smpd_process.mpiexec_tree_id & mask);

    return host;
}


smpd_rank_data_t*
next_rank_data(
    _In_opt_ smpd_rank_data_t* pNode,
    _In_ const smpd_host_t* pHost )
{
    if( pNode == nullptr )
    {
        return pHost->pLaunchBlockList->pRankData;
    }
    else
    {
        if( pNode->next == nullptr )
        {
            if( pNode->parent->next != nullptr )
            {
                return pNode->parent->next->pRankData;
            }
        }

        return pNode->next;
    }
}


smpd_launch_node_t *next_launch_node(smpd_launch_node_t *node, int id)
{
    while (node)
    {
        if(node->host_id == id)
            return node;

        node = node->next;
    }
    return NULL;
}


smpd_launch_node_t *prev_launch_node(smpd_launch_node_t *node, int id)
{
    while (node)
    {
        if(node->host_id == id)
            return node;

        node = node->prev;
    }
    return NULL;
}
