// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "hwtree.h"
#include "util.h"

//
// List of masks index by count - 1 of sequential bits set starting at bit 0.
//
static const UINT64 WidthMasks[] =
{
    0x0000000000000001L,0x0000000000000003L,0x0000000000000007L,0x000000000000000FL,
    0x000000000000001FL,0x000000000000003FL,0x000000000000007FL,0x00000000000000FFL,
    0x00000000000001FFL,0x00000000000003FFL,0x00000000000007FFL,0x0000000000000FFFL,
    0x0000000000001FFFL,0x0000000000003FFFL,0x0000000000007FFFL,0x000000000000FFFFL,
    0x000000000001FFFFL,0x000000000003FFFFL,0x000000000007FFFFL,0x00000000000FFFFFL,
    0x00000000001FFFFFL,0x00000000003FFFFFL,0x00000000007FFFFFL,0x0000000000FFFFFFL,
    0x0000000001FFFFFFL,0x0000000003FFFFFFL,0x0000000007FFFFFFL,0x000000000FFFFFFFL,
    0x000000001FFFFFFFL,0x000000003FFFFFFFL,0x000000007FFFFFFFL,0x00000000FFFFFFFFL,
    0x00000001FFFFFFFFL,0x00000003FFFFFFFFL,0x00000007FFFFFFFFL,0x0000000FFFFFFFFFL,
    0x0000001FFFFFFFFFL,0x0000003FFFFFFFFFL,0x0000007FFFFFFFFFL,0x000000FFFFFFFFFFL,
    0x000001FFFFFFFFFFL,0x000003FFFFFFFFFFL,0x000007FFFFFFFFFFL,0x00000FFFFFFFFFFFL,
    0x00001FFFFFFFFFFFL,0x00003FFFFFFFFFFFL,0x00007FFFFFFFFFFFL,0x0000FFFFFFFFFFFFL,
    0x0001FFFFFFFFFFFFL,0x0003FFFFFFFFFFFFL,0x0007FFFFFFFFFFFFL,0x000FFFFFFFFFFFFFL,
    0x001FFFFFFFFFFFFFL,0x003FFFFFFFFFFFFFL,0x007FFFFFFFFFFFFFL,0x00FFFFFFFFFFFFFFL,
    0x01FFFFFFFFFFFFFFL,0x03FFFFFFFFFFFFFFL,0x07FFFFFFFFFFFFFFL,0x0FFFFFFFFFFFFFFFL,
    0x1FFFFFFFFFFFFFFFL,0x3FFFFFFFFFFFFFFFL,0x7FFFFFFFFFFFFFFFL,0xFFFFFFFFFFFFFFFFL,
};


//
// Summary:
//  Utility function to initialize the specified node within the tree.
//
// Parameters:
//  pTree           - pointer to the tree to manipulate
//  index           - index of the node within the tree
//  type            - the type to set on the node
//  parent          - the parent ID of the node
//  previous        - the previous node
//
static void
HwTreeInitializeNode(
    _Inout_ HWTREE*                 pTree,
    _In_ UINT32                     index,
    _In_ HWNODE_TYPE                type,
    _In_ UINT32                     parent,
    _In_ UINT32                     previous
    )
{
    pTree->Nodes[index].Type            = type;
    pTree->Nodes[index].Parent          = parent;
    pTree->Nodes[index].FirstChild      = HWNODEID_NONE;
    pTree->Nodes[index].LastChild       = HWNODEID_NONE;
    pTree->Nodes[index].NextSibling     = HWNODEID_NONE;
    pTree->Nodes[index].PrevSibling     = previous;

    pTree->Nodes[parent].LastChild      = index;

    if( previous == HWNODEID_NONE  ||
        pTree->Nodes[previous].Parent != parent )
    {
        pTree->Nodes[parent].FirstChild = index;
    }
    else
    {
        pTree->Nodes[previous].NextSibling = index;
    }
}


//
// Summary:
//  Utility function to initialize the specified node within the tree with affinity data.
//
// Parameters:
//  pTree           - pointer to the tree to manipulate
//  index           - index of the node within the tree
//  type            - the type to set on the node
//  parent          - the parent ID of the node
//  previous        - the previous node
//  group           - the processor group to set in the affinity value
//  mask            - the affinity mask to set
//
static void
HwTreeInitializeAffinityNode(
    _Inout_ HWTREE*                 pTree,
    _In_ UINT32                     index,
    _In_ HWNODE_TYPE                type,
    _In_ UINT32                     parent,
    _In_ UINT32                     previous,
    _In_ UINT16                     group,
    _In_ UINT64                     mask
    )
{
    HwTreeInitializeNode(pTree, index, type, parent, previous);
    pTree->Nodes[index].Affinity.GroupId = group;
    pTree->Nodes[index].Affinity.Mask    = mask;
}


//
// Summary:
//  Utility function to initialize the specified machine node
//
// Parameters:
//  pTree           - pointer to the tree to manipulate
//  index           - index of the node within the tree
//  type            - the type to set on the node
//  parent          - the parent ID of the node
//  previous        - the previous node
//  hostId          - the hostid of the machine node
//
static void
HwTreeInitializeMachineNode(
    _Inout_ HWTREE*                 pTree,
    _In_ UINT32                     index,
    _In_ HWNODE_TYPE                type,
    _In_ UINT32                     parent,
    _In_ UINT32                     previous,
    _In_ int                        hostId
    )
{
    HwTreeInitializeNode(pTree, index, type, parent, previous);
    pTree->Nodes[index].HostId = hostId;
}


//
// Summary:
//  Initialize all logical core elements in the tree.
//
// Parameter:
//  pTree                   - pointer to the tree to populate
//  pStrides                - pointer to the current strides of each level in the tree
//  pEnds                   - pointer to the current ends of each level in the tree
//  group                   - the processor group that this core belongs to.
//  pcoreMask               - the processor mask for the parent physical core
//
static HRESULT
HwTreeInitializeLCores(
    _Inout_ HWTREE*                            pTree,
    _Inout_updates_(HWNODE_MAX_DEPTH) UINT32    pStrides[],
    _In_reads_(HWNODE_MAX_DEPTH) const UINT32  pEnds[],
    _In_ UINT16                                group,
    _In_ UINT64                                pcoreMask
    )
{
    UINT32  last = HWNODEID_NONE;
    UINT64  fullMask = pcoreMask;

    //
    // Get the lowest set bit from pcoreMask (the first logical core)
    //
    UINT64  mask = ((~pcoreMask << 1 ) | 1) & pcoreMask;

    while( 0 != fullMask )
    {
        Assert(mask & pcoreMask);

        if( pStrides[HWNODE_TYPE_LCORE] >= pEnds[HWNODE_TYPE_LCORE] )
        {
            return E_UNEXPECTED;
        }

        HwTreeInitializeAffinityNode(
            pTree,
            pStrides[HWNODE_TYPE_LCORE],
            HWNODE_TYPE_LCORE,
            pStrides[HWNODE_TYPE_PCORE],
            last,
            group,
            mask
            );

        last = pStrides[HWNODE_TYPE_LCORE];
        pStrides[HWNODE_TYPE_LCORE]++;
        pTree->Counts[HWNODE_TYPE_LCORE]++;

        //
        // Remove mask from the remaining bits
        //
        fullMask &= (~mask);

        //
        // Move to the next bit
        //
        mask     <<= 1;
    }
    return S_OK;
}


//
// Summary:
//  Initialize all physical core elements in the tree.
//
// Parameter:
//  pTree                   - pointer to the tree to populate
//  pStrides                - pointer to the current strides of each level in the tree
//  pEnds                   - pointer to the current ends of each level in the tree
//  numaMask                - the processor mask for the parent numa node
//  pInfo                   - pointer to the HWINFO for the current processor group
//
static HRESULT
HwTreeInitializePcores(
    _Inout_ HWTREE*                            pTree,
    _Inout_updates_(HWNODE_MAX_DEPTH) UINT32   pStrides[],
    _In_reads_(HWNODE_MAX_DEPTH) const UINT32  pEnds[],
    UINT64                                     numaMask,
    const HWINFO*                              pInfo
    )
{
    HRESULT hr;
    UINT32  last = HWNODEID_NONE;
    UINT64  fullMask = numaMask;

    //
    // Get the lowest N bits from numaMask where N is PCoreWidth (the first physical core)
    //
    Assert(pInfo->PcoreWidth>0);
    UINT64  mask = ((~numaMask << pInfo->PcoreWidth ) | WidthMasks[pInfo->PcoreWidth-1]) & numaMask;

    while( 0 != fullMask )
    {
        Assert( mask & numaMask );

        if( pStrides[HWNODE_TYPE_PCORE] >= pEnds[HWNODE_TYPE_PCORE] )
        {
            return E_UNEXPECTED;
        }

        HwTreeInitializeAffinityNode(
            pTree,
            pStrides[HWNODE_TYPE_PCORE],
            HWNODE_TYPE_PCORE,
            pStrides[HWNODE_TYPE_NUMA],
            last,
            pInfo->Group,
            mask
            );

        hr = HwTreeInitializeLCores( pTree, pStrides, pEnds, pInfo->Group, mask );
        if( FAILED( hr ) )
        {
            return hr;
        }

        last = pStrides[HWNODE_TYPE_PCORE];
        pStrides[HWNODE_TYPE_PCORE]++;
        pTree->Counts[HWNODE_TYPE_PCORE]++;

        //
        // Remove mask from the remaining bits
        //
        fullMask &= (~mask);

        //
        // Move to the next physical core
        //
        mask <<= pInfo->PcoreWidth;
    }
    return S_OK;
}


//
// Summary:
//  Initialize all numa elements in the tree.
//
// Parameter:
//  pTree                   - pointer to the tree to populate
//  pStrides                - pointer to the current strides of each level in the tree
//  pEnds                   - pointer to the current ends of each level in the tree
//  pInfo                   - pointer to the HWINFO for the current processor group
//
static HRESULT
HwTreeInitializeNuma(
    _Inout_ HWTREE*                              pTree,
    _Inout_updates_(HWNODE_MAX_DEPTH) UINT32     pStrides[],
    _In_reads_(HWNODE_MAX_DEPTH) const UINT32    pEnds[],
    _In_ const HWINFO*                           pInfo
    )
{
    HRESULT hr;
    UINT32  last = HWNODEID_NONE;
   UINT64  fullMask = pInfo->Mask;

    //
    // Get the lowest N bits from group mask where N is NumaWidth (the numa node)
    //
    Assert(pInfo->NumaWidth>0);
    UINT64  mask = ((~pInfo->Mask << pInfo->NumaWidth ) | WidthMasks[pInfo->NumaWidth-1]) & pInfo->Mask;

    while( 0 != fullMask )
    {
        Assert( mask & pInfo->Mask );

        if( pStrides[HWNODE_TYPE_NUMA] >= pEnds[HWNODE_TYPE_NUMA] )
        {
            return E_UNEXPECTED;
        }

        HwTreeInitializeAffinityNode(
            pTree,
            pStrides[HWNODE_TYPE_NUMA],
            HWNODE_TYPE_NUMA,
            pStrides[HWNODE_TYPE_GROUP],
            last,
            pInfo->Group,
            mask
            );

        hr = HwTreeInitializePcores(pTree, pStrides, pEnds, mask, pInfo);
        if( FAILED( hr ) )
        {
            return hr;
        }

        last = pStrides[HWNODE_TYPE_NUMA];
        pStrides[HWNODE_TYPE_NUMA]++;
        pTree->Counts[HWNODE_TYPE_NUMA]++;

        //
        // Remove mask from the remaining bits
        //
        fullMask &= (~mask);

        //
        // Move to the next numa node
        //
        mask    <<= pInfo->NumaWidth;
    }
    return S_OK;
}


//
// Summary:
//  Initialize all Group and Machine element in the tree.
//
// Parameter:
//  pTree                   - pointer to the tree to populate
//  pStrides                - pointer to the current strides of each level in the tree
//  pEnds                   - pointer to the current ends of each level in the tree
//  pSummary                - Pointer to machine's summary
//
static HRESULT
HwTreeInitializeGroups(
    _Inout_ HWTREE*                              pTree,
    _Inout_updates_(HWNODE_MAX_DEPTH) UINT32     pStrides[],
    _In_reads_(HWNODE_MAX_DEPTH) const UINT32    pEnds[],
    _In_ const HWSUMMARY*                        pSummary

    )
{
    UINT32  lastMachine = HWNODEID_NONE;
    UINT32  lastGroup = HWNODEID_NONE;
    HRESULT hr;

    if(pSummary == nullptr)
    {
        return S_OK;
    }

    if( pStrides[HWNODE_TYPE_MACHINE] >= pEnds[HWNODE_TYPE_MACHINE] )
    {
        return E_UNEXPECTED;
    }

    HwTreeInitializeMachineNode(
        pTree,
        pStrides[HWNODE_TYPE_MACHINE],
        HWNODE_TYPE_MACHINE,
        HWNODEID_WORLD,
        lastMachine,
        HWMACHINEID_SELF
        );
    lastGroup = HWNODEID_NONE;

    for( UINT32 i = 0; i < pSummary->Count; i++ )
    {
        if( pStrides[HWNODE_TYPE_GROUP] >= pEnds[HWNODE_TYPE_GROUP] )
        {
            return E_UNEXPECTED;
        }

        HwTreeInitializeAffinityNode(
            pTree,
            pStrides[HWNODE_TYPE_GROUP],
            HWNODE_TYPE_GROUP,
            pStrides[HWNODE_TYPE_MACHINE],
            lastGroup,
            pSummary->Infos[i].Group,
            pSummary->Infos[i].Mask
            );

        hr = HwTreeInitializeNuma( pTree, pStrides, pEnds, &pSummary->Infos[i] );
        if( FAILED( hr ) )
        {
            return hr;
        }

        lastGroup = pStrides[HWNODE_TYPE_GROUP];
        pStrides[HWNODE_TYPE_GROUP]++;
        pTree->Counts[HWNODE_TYPE_GROUP]++;
    }

    lastMachine = pStrides[HWNODE_TYPE_MACHINE];
    pStrides[HWNODE_TYPE_MACHINE]++;
    pTree->Counts[HWNODE_TYPE_MACHINE]++;

    return S_OK;
}


//
// Summary:
//  Calculate the size of the tree required for the specified HWINFO array
//
// Parameters:
//  pSummary            - Pointer to a machine's summary information.
//  pCounts             - Pointer to array of UINT32 for holding the counts of each level in the tree.
//
// Returns:
//  Total node code for full tree.
//
_Success_(return > 0)
static UINT32
HwTreeCalculateNodeCounts(
    _In_opt_ const HWSUMMARY*             pSummary,
    _Out_writes_(HWNODE_MAX_DEPTH) UINT32 pCounts[]
    )
{
    if(pSummary == nullptr)
    {
        return 0;
    }

    pCounts[HWNODE_TYPE_MACHINE]  = 0;
    pCounts[HWNODE_TYPE_GROUP]    = 0;
    pCounts[HWNODE_TYPE_NUMA]     = 0;
    pCounts[HWNODE_TYPE_PCORE]    = 0;
    pCounts[HWNODE_TYPE_LCORE]    = 0;


    pCounts[HWNODE_TYPE_MACHINE]++;
    //one for every group
    pCounts[HWNODE_TYPE_GROUP] += pSummary->Count;

    for( UINT32 i = 0; i < pSummary->Count; i++ )
    {

        pCounts[HWNODE_TYPE_NUMA] += pSummary->Infos[i].GroupWidth / pSummary->Infos[i].NumaWidth;

        //
        // 1 for each pcore
        //
        pCounts[HWNODE_TYPE_PCORE] += pSummary->Infos[i].GroupWidth / pSummary->Infos[i].PcoreWidth;

        //
        // 1 for each lcore
        //
        pCounts[HWNODE_TYPE_LCORE] += pSummary->Infos[i].GroupWidth;
    }

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
//  Initialize hardware tree representing all machines.
//
// Parameters:
//  pcbTree     - On input, current size of pTree
//                On output, the size of the buffer used.
//                if return code is HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
//                the required size will be written here.
//  pTree       - pointer to HWTREE buffer
//  pSummary    - Pointer to a machine's summary information.
//
// NOTE:
//  The memory is sequentially allocated, and can be marshalled with memcpy.
//
HRESULT
HwTreeInitialize(
    _Inout_ UINT32*                                 pcbTree,
    _Inout_updates_to_(*pcbTree,*pcbTree) HWTREE*   pTree,
    _In_ const HWSUMMARY*                           pSummary
    )
{
    HRESULT hr;
    UINT32  cb;
    UINT32  nNodes;
    UINT32  strides[HWNODE_MAX_DEPTH];
    UINT32  ends[HWNODE_MAX_DEPTH];

    //
    // Count total nodes + 1 for world
    //
    nNodes = HwTreeCalculateNodeCounts( pSummary, ends );

    //
    // There must be at least 1 node for each level in the tree.
    //
    Assert( nNodes >= HWNODE_MAX_DEPTH );

    cb = sizeof(*pTree) - sizeof(pTree->Nodes) + (sizeof(pTree->Nodes[0]) * nNodes);

    if( *pcbTree < cb )
    {
        *pcbTree = cb;
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

    pTree->Strides[HWNODE_TYPE_MACHINE] = strides[HWNODE_TYPE_MACHINE];
    pTree->Strides[HWNODE_TYPE_GROUP]   = strides[HWNODE_TYPE_GROUP];
    pTree->Strides[HWNODE_TYPE_NUMA]    = strides[HWNODE_TYPE_NUMA];
    pTree->Strides[HWNODE_TYPE_PCORE]   = strides[HWNODE_TYPE_PCORE];
    pTree->Strides[HWNODE_TYPE_LCORE]   = strides[HWNODE_TYPE_LCORE];

    pTree->Counts[HWNODE_TYPE_MACHINE]  = 0;
    pTree->Counts[HWNODE_TYPE_GROUP]    = 0;
    pTree->Counts[HWNODE_TYPE_NUMA]     = 0;
    pTree->Counts[HWNODE_TYPE_PCORE]    = 0;
    pTree->Counts[HWNODE_TYPE_LCORE]    = 0;

    //
    // update the "ends" to be offset from the strides so we don't
    //  have to track the start of the chain through the iteration.
    //
    ends[HWNODE_TYPE_MACHINE]   += strides[HWNODE_TYPE_MACHINE];
    ends[HWNODE_TYPE_GROUP]     += strides[HWNODE_TYPE_GROUP];
    ends[HWNODE_TYPE_NUMA]      += strides[HWNODE_TYPE_NUMA];
    ends[HWNODE_TYPE_PCORE]     += strides[HWNODE_TYPE_PCORE];
    ends[HWNODE_TYPE_LCORE]     += strides[HWNODE_TYPE_LCORE];


    pTree->Nodes[HWNODEID_WORLD].Type           = HWNODE_TYPE_WORLD;
    pTree->Nodes[HWNODEID_WORLD].Parent         = HWNODEID_NONE;
    pTree->Nodes[HWNODEID_WORLD].FirstChild     = HWNODEID_NONE;
    pTree->Nodes[HWNODEID_WORLD].LastChild      = HWNODEID_NONE;
    pTree->Nodes[HWNODEID_WORLD].NextSibling    = HWNODEID_NONE;
    pTree->Nodes[HWNODEID_WORLD].PrevSibling    = HWNODEID_NONE;


    hr = HwTreeInitializeGroups(pTree, strides, ends, pSummary);
    if( FAILED( hr ) )
    {
        return hr;
    }

    *pcbTree = cb;
    return S_OK;
}

