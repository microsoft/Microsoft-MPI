// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "hwtree.h"
#include "stdio.h"


//
// Summary:
//  Utility function to get a simple string for error message formatting.
//
inline const char* GetNodeTypeString(HWNODE_TYPE targetType)
{
    switch( targetType )
    {
    case HWNODE_TYPE_MACHINE:
        return "M";
    case HWNODE_TYPE_GROUP:
        return "G";
    case HWNODE_TYPE_NUMA:
        return "N";
    case HWNODE_TYPE_PCORE:
        return "P";
    case HWNODE_TYPE_LCORE:
        return "L";
    default:
        Assert(0);
        return "Invalid";
    }
}


//
// FW delclare some local functions
//
static HRESULT
HwLayoutProcessEnum(
    _In_     const HWLAYOUT*     pLayout,
    _In_     const HWVIEW*       pView,
    _In_     PVOID               pData,
    _In_     FN_HwLayoutProcessCallback* pfn,
    _In_     HWLAYOUT_STATE*     pState,
    _In_     UINT32              enumIndex
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
    _In_    const HWLAYOUT*     pLayout,
    _In_    const HWVIEW*       pView,
    _In_    UINT32              minProc,
    _In_    UINT32              maxProc,
    _In_    PVOID               pData,
    _In_    FN_HwLayoutProcessCallback* pfn
    )
{
    HRESULT hr;
    HWLAYOUT_STATE  state;

    state.MaxProc = maxProc;
    state.MinProc = minProc;
    state.ProcCount = 0;

    for( UINT32 i = 0; i < pLayout->EnumCount; i++ )
    {
        state.Enums[i].Start    = 0;
        state.Enums[i].Size     = 0;
        state.Enums[i].Current  = 0;
    }

    do
    {
        hr = HwLayoutProcessEnum( pLayout, pView, pData, pfn, &state, 0 );
        if( FAILED( hr ) )
        {
            return hr;
        }
        if( S_FALSE == hr )
        {
            break;
        }

    } while( state.ProcCount < state.MinProc );

    return S_OK;
}


//
// Summary:
//  Process the target of each iteration.
//
// Parameters:
//  pLayout             - Pointer to the current layout
//  pView               - Pointer to the view where the location is set
//  pData               - Pointer to the opaque data to pass to the callback
//  pfn                 - Callback function invoked for each iteration target
//  pState              - Pointer to the current state of the iterations.
//
inline HRESULT
HwLayoutProcessTarget(
    _In_        const HWLAYOUT*     pLayout,
    _In_        const HWVIEW*       pView,
    _In_        PVOID               pData,
    _In_        FN_HwLayoutProcessCallback* pfn,
    _In_        HWLAYOUT_STATE*     pState
    )
{
    HRESULT hr;
    UINT32 location = pState->Enums[pLayout->EnumCount-1].Current;
    UINT32 count = pLayout->TargetCount;

    if( 0 != pState->MaxProc )
    {
        count = min(count,pState->MaxProc - pState->ProcCount);
    }

    if( pView == nullptr)
    {
        return S_FALSE;
    }

    while( pView->Nodes[location].Type > pLayout->TargetType )
    {
        location = pView->Nodes[location].Parent;
    }

    Assert( 0 != location );

    hr = pfn( pLayout, pState, pView, location, pData, &count );
    if( FAILED( hr ) )
    {
        return hr;
    }

    pState->ProcCount += count;

    if( S_FALSE == hr )
    {
        return hr;
    }

    if( 0 != pState->MaxProc && pState->ProcCount >= pState->MaxProc )
    {
        return S_FALSE;
    }
    return S_OK;
}


//
// Summary:
//  Lookup the count of elements for a give type within the scope of the current layout
//
// Parameters:
//  pLayout         - The layout to use to scope the returned size
//  pState          - The state of the layout enumeration
//  enumIndex       - The current layout enumeration being processed
//  pView           - The view to use to resolve the size
//  type            - The type of node we need a count for
//
static UINT32
HwLayoutResolveWellknownExpression(
    _In_    const HWLAYOUT*         pLayout,
    _In_    const HWLAYOUT_STATE*   pState,
    _In_    UINT32                  enumIndex,
    _In_    const HWVIEW*           pView,
    _In_    HWNODE_TYPE             type
    )
{
    Assert( type >= HWNODE_TYPE_MACHINE );
    Assert( pLayout->Enums[enumIndex].Type >= type );

    //
    // First find the Enumerator in the layout that provides the
    //  scope we need resolve the wellknown value.
    //
    while( enumIndex > 0 && pLayout->Enums[enumIndex].Type > type )
    {
        enumIndex--;
    }

    //
    // If expression matches an enumerator value, just return the size
    //  of that exact match.
    //
    if( pLayout->Enums[enumIndex].Type == type )
    {
        return pState->Enums[enumIndex].Size;
    }


    //
    // Else, we must be closer to the root of the tree with the
    //  currently selected enumerator, so continue to walk the span
    //  of the tree until we reach the correct depth, and return the width
    //  of that range.
    //
    UINT32  start = pState->Enums[enumIndex].Current;
    UINT32  end = start;

    do
    {
        start = pView->Nodes[start].FirstChild;
        end = pView->Nodes[end].LastChild;

    }while( pView->Nodes[start].Type < type );

    return end - start + 1;
}


//
// Summary:
//  Resolve the HWENUM_EXPR value to the resulting UINT32
//
// Parameters:
//  pLayout         - The layout to use to scope the returned size
//  pState          - The state of the layout enumeration
//  enumIndex       - The current layout enumeration being processed
//  pView           - The view to use to resolve the size
//  pExpression     - The expression to resolve
//
inline UINT32
HwLayoutResolveExpression(
    _In_     const HWLAYOUT*         pLayout,
    _In_     const HWLAYOUT_STATE*   pState,
    _In_     UINT32                  enumIndex,
    _In_     const HWVIEW*           pView,
    _In_     const HWENUM_EXPR*      pExpression
    )
{
    if( 0 == pExpression->Flags )
    {
        return pExpression->Left;
    }
    else
    {
        UINT32 left = pExpression->Left;
        UINT32 right = pExpression->Right;

        if( 0 != ( pExpression->Flags & HWENUM_EXPR_WELLKNOWN_LEFT ) )
        {
            left = HwLayoutResolveWellknownExpression(
                pLayout,
                pState,
                enumIndex,
                pView,
                static_cast<HWNODE_TYPE>(left)
                );
        }
        if( 0 == ( pExpression->Flags & HWENUM_EXPR_DIVIDE_BY_RIGHT ) )
        {
            return left;
        }

        if( 0 != ( pExpression->Flags & HWENUM_EXPR_WELLKNOWN_RIGHT ) )
        {
            right = HwLayoutResolveWellknownExpression(
                pLayout,
                pState,
                enumIndex,
                pView,
                static_cast<HWNODE_TYPE>(right)
                );
        }

        Assert(0 != right);

        return left / right;
    }
}


inline UINT32 HwLayoutResolveExpressionAndRoundUp(
    _In_     const HWLAYOUT*         pLayout,
    _In_     const HWLAYOUT_STATE*   pState,
    _In_     UINT32                  enumIndex,
    _In_     const HWVIEW*           pView,
    _In_     const HWENUM_EXPR*      pExpression
    )
{
    UINT32 i = HwLayoutResolveExpression(pLayout,pState,enumIndex,pView,pExpression);
    if( i == 0 )
    {
        i++;
    }
    return i;
}


//
// Summary:
//  Run the chain of enumerations recursively
//
// Parameters:
//  pLayout             - Pointer to the current layout
//  pView               - Pointer to the view where the location is set
//  pData               - Pointer to the opaque data to pass to the callback
//  pfn                 - Callback function invoked for each iteration target
//  pState              - Pointer to the current state of the iterations.
//  enumIndex           - The index of the current enumation level.
//
static HRESULT
HwLayoutProcessEnum(
    _In_    const HWLAYOUT*     pLayout,
    _In_    const HWVIEW*       pView,
    _In_    PVOID               pData,
    _In_    FN_HwLayoutProcessCallback* pfn,
    _In_    HWLAYOUT_STATE*     pState,
    _In_    UINT32              enumIndex
    )
{
    HRESULT hr;
    if( enumIndex == pLayout->EnumCount )
    {
        return HwLayoutProcessTarget( pLayout, pView, pData, pfn, pState );
    }

    HWENUM_STATE* pEnumState = &pState->Enums[ enumIndex ];
    const HWENUM* pEnum = &pLayout->Enums[ enumIndex ];

    //
    // Work out the start and count of elements in the list
    //
    if( 0 != enumIndex )
    {
        UINT32 end = pState->Enums[enumIndex-1].Current;
        pEnumState->Start   = end;

        Assert(pView != nullptr);

        while( pView->Nodes[end].Type != pEnum->Type )
        {
            Assert( pView->Nodes[pEnumState->Start].Type == pView->Nodes[end].Type );

            pEnumState->Start = pView->Nodes[pEnumState->Start].FirstChild;
            end   = pView->Nodes[end].LastChild;
        }
        pEnumState->Size = (end - pEnumState->Start) + 1;
    }
    else
    {
        pEnumState->Start  = pView->Strides[pEnum->Type];
        pEnumState->Size   = pView->Counts[pEnum->Type];
    }

    //
    // There are cases where you can scope the values of an expression and get
    //  a resulting 0 from the divide operation.  This can happen when the Left side of the expression
    //  results in a value < the right.  For example, the pattern "MNL{1,0,N/2}" on boxes where there is only 1 numa node.
    //  We round these value up to 1 to ensure that we place at least a single item..
    //
    UINT32 offset       = HwLayoutResolveExpression( pLayout, pState, enumIndex, pView, &pEnum->Offset );
    UINT32 count        = HwLayoutResolveExpressionAndRoundUp( pLayout, pState, enumIndex, pView, &pEnum->Count );
    UINT32 stride       = HwLayoutResolveExpressionAndRoundUp( pLayout, pState, enumIndex, pView, &pEnum->Stride );
    UINT32 repeatCount  = HwLayoutResolveExpressionAndRoundUp( pLayout, pState, enumIndex, pView, &pEnum->RepeatCount );
    UINT32 repeatOffset = HwLayoutResolveExpression( pLayout, pState, enumIndex, pView, &pEnum->RepeatOffset );

    Assert(repeatCount>0);
    Assert(count>0);
    Assert(stride>0);

    pEnumState->Current = pEnumState->Start + ( offset % pEnumState->Size );

    do
    {
        for( UINT32 i = 0; i < count; i++ )
        {
            hr = HwLayoutProcessEnum( pLayout, pView, pData, pfn, pState, enumIndex + 1 );
            if( FAILED( hr ) || S_FALSE == hr )
            {
                return hr;
            }

            pEnumState->Current = pEnumState->Start + ((pEnumState->Current - pEnumState->Start + stride) % pEnumState->Size);
        }

        pEnumState->Current = pEnumState->Start + ((pEnumState->Current - pEnumState->Start + repeatOffset) % pEnumState->Size);

        repeatCount--;

    } while( repeatCount > 0 );


    return S_OK;
}

