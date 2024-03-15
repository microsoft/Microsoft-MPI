// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "hwtree.h"
#include "util.h"

//
// Summary:
//  Utility function to initialize the specified node within the view.
//
// Parameters:
//  pView           - pointer to the view to manipulate
//  index           - index of the node with in the tree
//  type            - the type to set on the node
//  parent          - the parent ID of the node
//  previous        - the previous node
//  treeNodeId      - id of the node in the original tree
//
static void
HwViewInitializeNode(
    _Inout_ HWVIEW*                 pView,
    _In_ UINT32                     index,
    _In_ HWNODE_TYPE                type,
    _In_ UINT32                     parent,
    _In_ UINT32                     previous,
    _In_ UINT32                     treeNodeId
    )
{
    pView->Nodes[index].Type            = type;
    pView->Nodes[index].Parent          = parent;
    pView->Nodes[index].FirstChild      = HWNODEID_NONE;
    pView->Nodes[index].LastChild       = HWNODEID_NONE;
    pView->Nodes[index].NextSibling     = HWNODEID_NONE;
    pView->Nodes[index].PrevSibling     = previous;

    pView->Nodes[parent].LastChild      = index;

    if( previous == HWNODEID_NONE  ||
        pView->Nodes[previous].Parent != parent )
    {
        pView->Nodes[parent].FirstChild = index;
    }
    else
    {
        pView->Nodes[previous].NextSibling = index;
    }

    pView->Nodes[index].NodeId = treeNodeId;
}

//
// Summary:
//  Initialize the subtree elements of the view
//
// Parameters:
//  pView           - pointer to the view to initialize
//  pStrides        - array of offsets for each level in the view
//  pEnds           - array of end offsets for each level in the view
//  parentId        - the node id of the parent to add children for
//
static HRESULT
HwViewInitializeSubTree(
    _Inout_ HWVIEW*                      pView,
    _Inout_updates_(HWNODE_MAX_DEPTH) UINT32  pStrides[],
    _Inout_updates_(HWNODE_MAX_DEPTH) UINT32  pEnds[],
    _In_ UINT32                          parentId,
    _In_ const HWSUMMARY*                pSummary
    )
{
    UINT32 last = HWNODEID_NONE;
    UINT32 current = pView->Tree->Nodes[parentId].FirstChild;
    UINT32 end = pView->Tree->Nodes[parentId].LastChild;

    HWNODE_TYPE depth = pView->Tree->Nodes[current].Type;

    Assert(depth >= HWNODE_TYPE_GROUP);
    Assert( nullptr != pSummary );

    while( current <= end )
    {
        if( pStrides[depth] >= pEnds[depth] )
        {
            return E_UNEXPECTED;
        }

        //
        // Verify that the current element has not been filtered by the HWSUMMARY.
        //
        Assert( pSummary->Count >= (UINT32)pView->Tree->Nodes[ current ].Affinity.GroupId );
        if( 0 == ( pView->Tree->Nodes[current].Affinity.Mask & pSummary->Infos[pView->Tree->Nodes[ current ].Affinity.GroupId].ActiveMask ) )
        {
            current++;
            continue;
        }

        HwViewInitializeNode(
            pView,
            pStrides[depth],
            depth,
            pStrides[depth - 1],
            last,
            current
            );

        if( depth + 1 < HWNODE_MAX_DEPTH )
        {
            HRESULT hr = HwViewInitializeSubTree( pView, pStrides, pEnds, current, pSummary );
            if( FAILED( hr ) )
            {
                return hr;
            }
        }
        last = pStrides[depth];
        pStrides[depth]++;
        pView->Counts[depth]++;
        current++;
    }
    return S_OK;
}


//
// Summary:
//  Utility function to collect the node counts for each depth and the total count
//      for the specified set of machines in the tree.
//
// Paramters:
//  pTree           - pointer to the tree to scan
//  pMachines       - The linked list of machine information
//  pCounts         - array to recieve the counts at each depth
//
// Returns:
//  The total number of nodes required in the View
//
static UINT32
HwViewCalculateCountNodes(
    _In_ const HWTREE*                      pTree,
    _Out_writes_(HWNODE_MAX_DEPTH) UINT32   pCounts[]
    )
{
    pCounts[HWNODE_TYPE_MACHINE]    = 0;
    pCounts[HWNODE_TYPE_GROUP]      = 0;
    pCounts[HWNODE_TYPE_NUMA]       = 0;
    pCounts[HWNODE_TYPE_PCORE]      = 0;
    pCounts[HWNODE_TYPE_LCORE]      = 0;

    UINT32 first;
    UINT32 last;
    UINT32 depth;
    UINT32 machineId;

    machineId = HWMACHINEID_SELF;

    Assert(machineId >= pTree->Strides[HWNODE_TYPE_MACHINE] );
    Assert(machineId < pTree->Strides[HWNODE_TYPE_MACHINE] + pTree->Counts[HWNODE_TYPE_MACHINE] );
    Assert(pTree->Nodes[machineId].Type == HWNODE_TYPE_MACHINE);

    depth = static_cast<UINT32>( HWNODE_TYPE_GROUP );
    first = pTree->Nodes[machineId].FirstChild;
    last = pTree->Nodes[machineId].LastChild;

    do
    {
        Assert( first != HWNODEID_NONE );
        Assert( last != HWNODEID_NONE );

        pCounts[depth] +=  last - first + 1;
        first = pTree->Nodes[first].FirstChild;
        last = pTree->Nodes[last].LastChild;

    } while( ++depth < HWNODE_MAX_DEPTH );

    pCounts[HWNODE_TYPE_MACHINE]++;

    //
    // Include 1 extra for the WORLD node.
    //
    return   1 +
             pCounts[HWNODE_TYPE_MACHINE] +
             pCounts[HWNODE_TYPE_GROUP] +
             pCounts[HWNODE_TYPE_NUMA] +
             pCounts[HWNODE_TYPE_PCORE] +
             pCounts[HWNODE_TYPE_LCORE];

}


//
// Summary:
//  Initialize a HWVIEW from an HWTREE
//
// Parameters:
//  pcbView         - On input, current size of pView
//                    On output, the size of the buffer used.
//                    if return code is HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
//                    the required size will be written here.
//  pView           - pointer to HWVIEW buffer
//  pTree           - Pointer to tree to base view on
//  pMachines       - The linked list of machine information
//
HRESULT
HwViewInitialize(
    _Inout_     UINT32*                         pcbView,
    _Out_writes_to_(*pcbView,*pcbView) HWVIEW*  pView,
    _In_  const HWTREE*                         pTree,
    _In_  const HWSUMMARY*                      pSummary
    )
{
    UINT32  machineId;
    UINT32  strides[HWNODE_MAX_DEPTH];
    UINT32  ends[HWNODE_MAX_DEPTH];
    UINT32  last = HWNODEID_NONE;

    UINT32 nNodes = HwViewCalculateCountNodes( pTree, ends );

    UINT32 cb = sizeof(*pView) - sizeof(pView->Nodes) + ( sizeof(pView->Nodes[0]) * nNodes );
    if( cb > *pcbView )
    {
        *pcbView = cb;
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    //
    // Update the stride table.
    //
    strides[HWNODE_TYPE_MACHINE]= 1;
    strides[HWNODE_TYPE_GROUP]  = strides[HWNODE_TYPE_MACHINE] +  ends[HWNODE_TYPE_MACHINE];
    strides[HWNODE_TYPE_NUMA]   = strides[HWNODE_TYPE_GROUP] +  ends[HWNODE_TYPE_GROUP];
    strides[HWNODE_TYPE_PCORE]  = strides[HWNODE_TYPE_NUMA] +  ends[HWNODE_TYPE_NUMA];
    strides[HWNODE_TYPE_LCORE]  = strides[HWNODE_TYPE_PCORE] +  ends[HWNODE_TYPE_PCORE];

    pView->Strides[HWNODE_TYPE_MACHINE] = strides[HWNODE_TYPE_MACHINE];
    pView->Strides[HWNODE_TYPE_GROUP]   = strides[HWNODE_TYPE_GROUP];
    pView->Strides[HWNODE_TYPE_NUMA]    = strides[HWNODE_TYPE_NUMA];
    pView->Strides[HWNODE_TYPE_PCORE]   = strides[HWNODE_TYPE_PCORE];
    pView->Strides[HWNODE_TYPE_LCORE]   = strides[HWNODE_TYPE_LCORE];

    pView->Counts[HWNODE_TYPE_MACHINE]  = 0;
    pView->Counts[HWNODE_TYPE_GROUP]    = 0;
    pView->Counts[HWNODE_TYPE_NUMA]     = 0;
    pView->Counts[HWNODE_TYPE_PCORE]    = 0;
    pView->Counts[HWNODE_TYPE_LCORE]    = 0;

    //
    // update the "ends" to be offset from the strides so we don't
    //  have to track the start of the chain through the iteration.
    //
    ends[HWNODE_TYPE_MACHINE]   += strides[HWNODE_TYPE_MACHINE];
    ends[HWNODE_TYPE_GROUP]     += strides[HWNODE_TYPE_GROUP];
    ends[HWNODE_TYPE_NUMA]      += strides[HWNODE_TYPE_NUMA];
    ends[HWNODE_TYPE_PCORE]     += strides[HWNODE_TYPE_PCORE];
    ends[HWNODE_TYPE_LCORE]     += strides[HWNODE_TYPE_LCORE];

    pView->Nodes[HWNODEID_WORLD].Type           = HWNODE_TYPE_WORLD;
    pView->Nodes[HWNODEID_WORLD].Parent         = HWNODEID_NONE;
    pView->Nodes[HWNODEID_WORLD].NextSibling    = HWNODEID_NONE;
    pView->Nodes[HWNODEID_WORLD].FirstChild     = HWNODEID_NONE;
    pView->Nodes[HWNODEID_WORLD].PrevSibling    = HWNODEID_NONE;
    pView->Nodes[HWNODEID_WORLD].LastChild      = HWNODEID_NONE;

    pView->Tree = pTree;

    if( strides[HWNODE_TYPE_MACHINE] >= ends[HWNODE_TYPE_MACHINE] )
    {
        return E_UNEXPECTED;
    }

    machineId = HWMACHINEID_SELF;

    HwViewInitializeNode(
        pView,
        strides[HWNODE_TYPE_MACHINE],
        HWNODE_TYPE_MACHINE,
        HWNODEID_WORLD,
        last,
        machineId
        );


    HRESULT hr = HwViewInitializeSubTree(pView, strides, ends, machineId, pSummary);
    if( FAILED( hr ) )
    {
        return hr;
    }
    last = strides[HWNODE_TYPE_MACHINE];
    strides[HWNODE_TYPE_MACHINE]++;
    pView->Counts[HWNODE_TYPE_MACHINE]++;

    *pcbView = cb;
    return S_OK;
}