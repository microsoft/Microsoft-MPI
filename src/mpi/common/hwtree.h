// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <oacr.h>
#include "hwtree_common.h"

#pragma once

//
// The maximum depth of the tree
//
#define HWNODE_MAX_DEPTH ((UINT32)HWNODE_TYPE_MAX)


//
// Fixed width field for holding affinity information.
//
// Fields:
//  Mask      - 64bit mask of processors with in the specified processor group.
//  GroupId   - The processor group id that the affinity mask applies to.
//  Padding   - padding to show wasted space.
//
typedef struct _HWAFFINITY
{
    UINT64                  Mask;
    UINT16                  GroupId;
    UINT16                  Padding[3];

} HWAFFINITY;


#define HWNODEID_WORLD      ((UINT32)0)
#define HWNODEID_NONE       ((UINT32)MAXDWORD)
#define HWMACHINEID_SELF    ((UINT32)1)

//
// Definition of basic tree node
//
// Fields:
//  Type          - The type of data within the node.
//  Parent        - Index of the parent element
//  FirstChild    - Index of this nodes first child
//  LastChild     - Index of this nodes last child
//  NextSibling   - Index of this nodes next sibling
//  PrevSibling   - Index of this nodes previous sibling
//
typedef struct HWNODEHEADER
{
    HWNODE_TYPE         Type;
    UINT32              Parent;
    UINT32              FirstChild;
    UINT32              LastChild;
    UINT32              NextSibling;
    UINT32              PrevSibling;

} HWNODEHEADER;


//
// Definition of a node in the HWTREE.
//
// Fields:
//  Affinity      - Specifies the affinity information represented by the node
//                  Valid only when Type > HWNODE_TYPE_MACHINE
//  HostId        - Specifies the identifier for the machine in the tree.
//                  Valid only when Type == HWNODE_TYPE_MACHINE
//
typedef struct _HWNODE
    : public HWNODEHEADER
{
    union
    {
        HWAFFINITY      Affinity;
        int             HostId;
    };

} HWNODE;


//
// Definition of a node in the HWVIEW.
//
// Fields:
//  NodeId        - Specifies node id in the original tree.
//
typedef struct _HWVIEWNODE
: public HWNODEHEADER
{
    UINT32              NodeId;

} HWVIEWNODE;


//
// Summary:
//  Data structure representing the a single machine hardware tree.
//
// Fields:
//  Counts      - The total counts of each depth in the tree
//  Strides     - The stride offset to the start of each level in the tree
//  Nodes       - This list of HWNODE structures that make up the tree.
//
typedef struct _HWTREE
{
    UINT32              Counts[HWNODE_MAX_DEPTH];
    UINT32              Strides[HWNODE_MAX_DEPTH];
    HWNODE              Nodes[ANYSIZE_ARRAY];

} HWTREE;


//
// Summary:
//  Structure that represents a portion of the tree within a HWTREE
//
// Fields:
//  Counts      - The total counts of each depth in the tree
//  Strides     - The stride offset to the start of each level in the tree
//  Nodes       - This list of HWVIEWNODE structures that make up the tree.
//
typedef struct _HWVIEW
{
    const HWTREE*       Tree;
    UINT32              Counts[HWNODE_MAX_DEPTH];
    UINT32              Strides[HWNODE_MAX_DEPTH];
    HWVIEWNODE          Nodes[ANYSIZE_ARRAY];

} HWVIEW;


//
// Summary:
//  Summary of the local information on an individual processor group
//
// Fields:
//  Mask                - The entire bitmask for the group
//  ActiveMask          - The active bitmask for the group after filter
//  Group               - the processor group id
//                        We only use UINT8 worth of groups.  There is no
//                        real world scenario where this will not work, and
//                        using this size makes the structure pack cleanly.
//  GroupWidth          - The number of logical cores for this group
//  NumaWidth           - the number of logical cores per numa node
//  PcoreWidth          - the number of logical cores per physical core
//  Padding             - Pad.
//
typedef struct _HWINFO
{
    UINT64      Mask;
    UINT64      ActiveMask;
    UINT8       Group;
    UINT8       GroupWidth;
    UINT8       NumaWidth;
    UINT8       PcoreWidth;
    UINT32      Padding;

} HWINFO;


//
// Summary:
//  Hosts the logical processor information for the processor groups on a machine.
//
// Fields:
//  Size        - The total size of the buffer
//  Count       - The count of elements in Infos
//  Infos       - The array of HWINFO, one for each Processor Group
//
typedef struct _HWSUMMARY
{
    UINT32              Size;
    UINT32              Count;
    HWINFO              Infos[ANYSIZE_ARRAY];

} HWSUMMARY;


//
// Summary:
//  Hosts the summary information for a local machine.
//
// Fields:
//  Next            - The pointer to the next machine in the chain
//  Summary         - Pointer to the summary information for the machine
//  HostId          - The unique id assigned to the machine.
//
typedef struct _HWMACHINEINFO
{
    struct _HWMACHINEINFO*  Next;
    HWSUMMARY*              Summary;
    int                     HostId;

} HWMACHINEINFO;


//
// Summary:
//  Represents an exprssion parsed from the LAYOUT format string
//
// Fields:
//  Left            - The left side of the expression
//  Right           - The right side of the expression
//  Flags           - The flags to indicate how to interpret Left and Right fields.
//                    HWENUM_EXPR_WELLKNOWN_LEFT - means that left must be resolved
//                    HWENUM_EXPR_DIVIDE_BY_RIGHT - means that right is present
//                    HWENUM_EXPR_WELLKNOWN_RIGHT - means that right must be resolved
//                    If no flags are set, Left is a constant value.
//
typedef struct _HWENUM_EXPR
{
    UINT32              Left;
    UINT32              Right;
    UINT32              Flags;

} HWENUM_EXPR;


#define HWENUM_EXPR_WELLKNOWN_LEFT  0x1
#define HWENUM_EXPR_DIVIDE_BY_RIGHT 0x2
#define HWENUM_EXPR_WELLKNOWN_RIGHT 0x4

//
// Summary:
//  Represents the settings for an individual enumeration in an HWTREE
//
// Fields:
//  Type                - The depth in the HWTREE to target
//  Offset              - The offset from the start to use
//  Count               - The count of steps.
//  Stride              - The stride to use for each step
//  RepeatCount         - The number of times repeat all steps.
//  RepeatOffset        - The size to step the offset each repeat.
//
typedef struct _HWENUM
{
    HWNODE_TYPE     Type;
    HWENUM_EXPR     Offset;
    HWENUM_EXPR     Count;
    HWENUM_EXPR     Stride;
    HWENUM_EXPR     RepeatCount;
    HWENUM_EXPR     RepeatOffset;

} HWENUM;


#define HWLAYOUT_ENUM_MAX   (HWNODE_MAX_DEPTH)

//
// Summary:
//  Represents the layout information of the processes.
//
// Fields:
//  TargetType          - The affinity target type
//  TargetCount         - The count of processes to create on each target
//  EnumCount           - The count of enumeration items specified
//                          Must be <= HWLAYOUT_ENUM_MAX.
//  Enums               - The array of enumeration items.
//
typedef struct _HWLAYOUT
{
    HWNODE_TYPE     TargetType;
    UINT32          TargetCount;
    UINT32          EnumCount;
    HWENUM          Enums[HWLAYOUT_ENUM_MAX];

} HWLAYOUT;


//
// Summary:
//  Holds the iteration state during the processing of an HWENUM
//
// Fields:
//  Start           - The start value for the full list
//  Size            - The count of elements for the full list
//  Current         - The current offset in the full list
//                    This is used for deriving the size of child HWENUM's
//
typedef struct _HWENUM_STATE
{
    UINT32      Start;
    UINT32      Size;
    UINT32      Current;

} HWENUM_STATE;


//
// Summary:
//  Holds the processing state of an HWLAYOUT
//
// Fields:
//  MinProc             - The number of process that must be created.
//  MaxProc             - The maximum number of processes to create.
//  ProcCount           - The total number of processes created.
//  Enums               - Array of states corresponding to the HWENUM in the layout
//                        For example, item 0 represents the state for HWENUM 0 in the layout.
//
typedef struct _HWLAYOUT_STATE
{
    UINT32          MinProc;
    UINT32          MaxProc;
    UINT32          ProcCount;
    HWENUM_STATE    Enums[HWLAYOUT_ENUM_MAX];

} HWLAYOUT_STATE;


//
// Summary:
//  Callback function invoked for each set of processes created during layout.
//
// Paramters:
//  pLayout             - Pointer to the requested layout information.
//  pState              - Pointer to the layout iteration state information.
//  pView               - The view to use to get the location information.
//  location            - The location within the view to assign the item.
//  pData               - Opaque data passed from entry to callback functions.
//  pCount              - On input, specifies the count of the processes to created.
//                        On output, specifies the number of processes created.
//
// NOTE: Return S_FALSE to terminate process creation immediately.
//
typedef HRESULT ( __stdcall FN_HwLayoutProcessCallback) (
    __in const HWLAYOUT*        pLayout,
    __in const HWLAYOUT_STATE*  pState,
    __in const HWVIEW*          pView,
    __in UINT32                 location,
    __in PVOID                  pData,
    __inout UINT32*             pCount
    );


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
//  pSummary    - pointer to a machine's summary information.
//
// NOTE:
//  The memory is sequentially allocated, and can be marshalled with memcpy.
//
HRESULT
HwTreeInitialize(
    _Inout_ UINT32*                                         pcbTree,
    _Inout_updates_to_(*pcbTree,*pcbTree) HWTREE*           pTree,
    _In_ const HWSUMMARY*                                   pSummary
    );


//
// Summary:
//  Find the node id for the specified machine
//
// Parameters:
//  pTree               - The tree to search for the machine node.
//  pInfo               - The HostId field is used as the search key.
//
// Returns:
//  HWNODEID_NONE - if not found
//
inline UINT32
HwTreeFindMachineNodeId(
    _In_ const HWTREE*                              pTree,
    _In_ const HWMACHINEINFO*                       pInfo
    )
{
    UINT32 stride;
    for( UINT32 i = 0; i < pTree->Counts[HWNODE_TYPE_MACHINE]; i++ )
    {
        stride = i + pTree->Strides[HWNODE_TYPE_MACHINE];
        if( pTree->Nodes[ stride ].HostId == pInfo->HostId )
        {
            return stride;
        }
    }
    return HWNODEID_NONE;
}


//
// Summary:
//  Initialize the local HWSUMMARY information
//
// Parameters:
//  pcbSummary          - On input, size of pSummary buffer
//                        on output, size used.
//                          if return code is HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
//                          the required count will be written here.
//  pSummary            - The summary information to initialize.
//
HRESULT
HwSummaryInitialize(
    _Inout_ UINT32*                                               pcbSummary,
    _Inout_updates_bytes_to_(*pcbSummary, *pcbSummary) HWSUMMARY* pSummary
    );


//
// Summary:
//  Filter out the processor information in each processor group.
//
// Parameters:
//  pSummary            - The summary information to filter.
//  pFilters            - The core filters to apply
//
inline void
HwSummaryFilter(
    _Inout_ HWSUMMARY*                              pSummary,
    _In_reads_(pSummary->Count) const UINT64        pFilters[]
    )
{
    for( UINT32 i = 0; i < pSummary->Count; i++ )
    {
        pSummary->Infos[i].ActiveMask &= pFilters[i];
    }
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
//  pSummary        - Pointer to a summary of machine.
//
HRESULT
HwViewInitialize(
    _Inout_ UINT32*                         pcbView,
    _Out_writes_to_(*pcbView,*pcbView) HWVIEW* pView,
    _In_ const HWTREE*                      pTree,
    _In_ const HWSUMMARY*                   pSummary
    );


//
// Summary:
//  Process the specified layout using the specified tree
//
// Parameters:
//  pLayout                 - Pointer to layout description
//  pView                   - Pointer to the view
//  minProc                 - Minimum number of processes to create
//  maxProc                 - Maximum number of processes to create
//  pData                   - Opaque data pointer to hand to callback
//  pfn                     - Callback function to invoke for each iteration
//
HRESULT
HwLayoutProcess(
    _In_     const HWLAYOUT*             pLayout,
    _In_     const HWVIEW*               pView,
    _In_     UINT32                      minProc,
    _In_     UINT32                      maxProc,
    _In_     PVOID                       pData,
    _In_     FN_HwLayoutProcessCallback* pfn
    );


