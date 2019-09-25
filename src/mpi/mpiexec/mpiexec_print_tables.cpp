// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "mpiexec.h"

const wchar_t g_HwNodeChars[] = L"MGNPL";


//
// Summary:
//  Print the specified element from the HWTREE
//
// Paramters:
//  pTree           - Pointer to the tree being printed
//  index           - index of the current node in the tree
//
static void
mpiexec_print_hwnode(
    __in const HWTREE* pTree,
    __in UINT32 index
    )
{
    wchar_t format[ 128 ] = {};
    UINT32 machineId = index;
    const smpd_host_t* host;
    HWNODE_TYPE type = pTree->Nodes[index].Type;

    while( pTree->Nodes[machineId].Type > HWNODE_TYPE_MACHINE )
    {
        machineId = pTree->Nodes[machineId].Parent;
    }

    for( host = smpd_process.host_list; host != NULL; host = static_cast<smpd_host_t*>( host->Next ) )
    {
        if( pTree->Nodes[machineId].HostId == host->HostId )
        {
            break;
        }
    }
    ASSERT(NULL != host);

    if( HWNODE_TYPE_MACHINE == type )
    {
        MPIU_Snprintf(format,_countof(format),L"%%%ds %%c %%%ds %%.6d %%.6d                                       %%s\n",type, HWNODE_TYPE_MAX-type);
        OACR_REVIEWED_CALL(mpicr,
            wprintf( format,
                     L"",
                     g_HwNodeChars[type] ,
                     L"",
                     index,
                     pTree->Nodes[index].Parent,
                     host->name )
            );
    }
    else
    {
        MPIU_Snprintf(format,_countof(format),L"%%%ds %%c %%%ds %%.6d %%.6d %%.3d %%.16I64x %%.16I64x\n",type, HWNODE_TYPE_MAX-type);
        OACR_REVIEWED_CALL(mpicr,
            wprintf( format,
                     L"",
                     g_HwNodeChars[type] ,
                     L"",
                     index,
                     pTree->Nodes[index].Parent,
                     pTree->Nodes[index].Affinity.GroupId,
                     pTree->Nodes[index].Affinity.Mask,
                     (pTree->Nodes[index].Affinity.Mask & host->Summary->Infos[pTree->Nodes[index].Affinity.GroupId].ActiveMask) )
            );
    }


    if( HWNODEID_NONE != pTree->Nodes[index].FirstChild )
    {
        mpiexec_print_hwnode(pTree,pTree->Nodes[index].FirstChild);
    }
    if( HWNODEID_NONE != pTree->Nodes[index].NextSibling )
    {
        mpiexec_print_hwnode(pTree,pTree->Nodes[index].NextSibling);
    }
}

//
// Summary:
//  print hwtree table 1. Table is sorted the parent-child relationships.
//
static void
mpiexec_print_hwtree1(
    __in const smpd_host_t* pHosts
    )
{
    wprintf(L"_______________________________________________________________________________\n" );
    wprintf(L"                 - MSMPI HwTree Table 1 (Parent/Child Tree) - \n" );
    wprintf(L"Type     NodeId Parent Grp Mask             ActiveMask       HostName\n" );
    wprintf(L"_______________________________________________________________________________\n" );

    for( const smpd_host_t* pHost = pHosts; NULL != pHost; pHost = static_cast<smpd_host_t*>( pHost->Next ) )
    {
        mpiexec_print_hwnode(pHost->hwTree, pHost->hwTree->Nodes[0].FirstChild);
    }

    wprintf(L"_______________________________________________________________________________\n" );
}


//
// Summary:
//  print hwtree table 2. Table is sorted by tree node id. This happens to also be the sorted by node type.
//
static void
mpiexec_print_hwtree2(
    __in const smpd_host_t* pHosts
    )
{
    static const wchar_t dashBuf[] = L"------";
    wchar_t fchildBuf[7];
    wchar_t siblingBuf[7];
    wchar_t valBuf[32];

    wprintf(L"_______________________________________________________________________________\n" );
    wprintf(L"                     - MSMPI HwTree Table 2 (Flat View) - \n" );
    wprintf(L"Type Stride  Count \n" );
    wprintf(L"_______________________________________________________________________________\n" );

    for( const smpd_host_t* pHost = pHosts; NULL != pHost; pHost = static_cast<smpd_host_t*>( pHost->Next ) )
    {
        HWTREE* pTree = pHost->hwTree;
        UINT32 totalCount = 1;//1 to start with (world)
        for( UINT32 i = (UINT32)HWNODE_TYPE_MACHINE; i < HWNODE_TYPE_MAX; i++ )
        {
            totalCount += pTree->Counts[i];
            wprintf(L" %c   %.6u  %.6u\n", g_HwNodeChars[i], pTree->Strides[i], pTree->Counts[i] );
        }
        wprintf(L"             ------\n" );
        wprintf(L"      Total: %.6u\n", totalCount );
        wprintf(L"\n\n" );
        wprintf(L"Type  NodeId  Parent  FChild  Siblng  Value\n" );
        wprintf(L"_______________________________________________________________________________\n" );
        wprintf(L" W    000000  ------  000001  ------\n");
        for( UINT32 i = 1; i < totalCount; i++ )
        {
            if( HWNODE_TYPE_MACHINE == pTree->Nodes[i].Type )
            {
                const smpd_host_t* host;
                for( host = smpd_process.host_list; host != NULL; host = static_cast<const smpd_host_t*>( host->Next ) )
                {
                    if( pTree->Nodes[i].HostId == host->HostId )
                    {
                        break;
                    }
                }
                ASSERT(NULL != host);
                MPIU_Snprintf(valBuf,_countof(valBuf),L"%.6d:%s",pTree->Nodes[i].HostId, host->name);
            }
            else
            {
                MPIU_Snprintf(valBuf,_countof(valBuf),L"%.16I64x:%.4d",pTree->Nodes[i].Affinity.Mask,pTree->Nodes[i].Affinity.GroupId);
            }

            if( HWNODEID_NONE == pTree->Nodes[i].FirstChild )
            {
                MPIU_Strcpy(fchildBuf,_countof(fchildBuf),dashBuf);
            }
            else
            {
                MPIU_Snprintf(fchildBuf,_countof(fchildBuf),L"%.6d",pTree->Nodes[i].FirstChild);
            }

            if( HWNODEID_NONE == pTree->Nodes[i].NextSibling )
            {
                MPIU_Strcpy(siblingBuf,_countof(siblingBuf),dashBuf);
            }
            else
            {
                MPIU_Snprintf(siblingBuf,_countof(siblingBuf),L"%.6d",pTree->Nodes[i].NextSibling);
            }

            wprintf( L" %c    %.6u  %.6u  %s  %s  %s\n",
                     g_HwNodeChars[pTree->Nodes[i].Type],
                     i,
                     pTree->Nodes[i].Parent,
                     fchildBuf,
                     siblingBuf,
                     valBuf
                );
        }
    }
    wprintf(L"_______________________________________________________________________________\n" );
}



//
// Summary:
//  Print the specified HWTREE if the MPIEXEC_HWTREE_TABLE environment variable is defined
//      2 == print table 2
//      1,on,yes == print table 1
//
// Parameters:
//  pHosts - pointer to the hosts whose hwtrees will be printed.
//
void
mpiexec_print_hwtree(
    __in const smpd_host_t* pHosts
    )
{
    if(!smpd_process.affinityOptions.isSet)
    {
        return;
    }

    switch(smpd_process.affinityOptions.hwTableStyle)
    {
    case 1:
        {
            mpiexec_print_hwtree1(pHosts);
            break;
        }
    case 2:
        {
            mpiexec_print_hwtree2(pHosts);
            break;
        }
    default:
        break;
    }
}



//
// Summary:
//  Test a core bit for existance, enabled, and usage and return string token
//
//  if coreBit not set in full, the core does not exist
//  if coreBit not set in active, the core is disabled
//  if coreBit not set in affinity, the core is unused
//
static inline const wchar_t*
get_core_bit_string(
    __in UINT64 coreBit,
    __in UINT64 full,
    __in UINT64 active,
    __in UINT64 affinity
    )
{
    if( 0 == (full & coreBit) )
    {
        return L"##";
    }
    if( coreBit == ( affinity & coreBit ) )
    {
        return L"++";
    }
    if( 0 == (active & coreBit ) )
    {
        return L"@@";
    }
    return L"..";
}


//
// Summary:
//  Test a core bit for existance, enabled, and usage and return char token
//
//  if coreBit not set in full, the core does not exist
//  if coreBit not set in active, the core is disabled
//  if coreBit not set in affinity, the cure is unused
//
static inline wchar_t
get_core_bit_char(
    __in UINT64 coreBit,
    __in UINT64 full,
    __in UINT64 active,
    __in UINT64 affinity
    )
{
    if( 0 == (full & coreBit) )
    {
        return L'#';
    }
    if( coreBit == ( affinity & coreBit ) )
    {
        return L'+';
    }
    if( 0 == (active & coreBit ) )
    {
        return L'@';
    }
    return L'.';
}


//
// Summary:
//  print affinity table 1. Table is sorted by machine then rank
//    This can display only the first 16 cores per machine.
//
static void
mpiexec_print_affinity_table1(
    __in const smpd_launch_node_t*const*    pList,
    __in const smpd_host_t*                 pHosts
    )
{
    wprintf(L"_______________________________________________________________________________\n" );
    wprintf(L"               - MSMPI Rank Affinity Table 1 (By Machine/Rank) - \n" );
    wprintf(L" Rank  -  0  1  2  3 |  4  5  6  7 |  8  9 10 11 | 12 13 14 15 - Mask\n" );

    for( const smpd_host_t* pHost = pHosts; NULL != pHost; pHost = static_cast<smpd_host_t*>( pHost->Next ) )
    {
        wprintf(L"_______________________________________________________________________________\n" );
        wprintf(L"%s\n", pHost->name );
        UINT64 fullMask = 0;
        for( const smpd_launch_node_t*const* ppNode = pList; NULL != *ppNode; ppNode++ )
        {
            if( (*ppNode)->host_id != pHost->HostId )
            {
                continue;
            }
            fullMask |= (*ppNode)->affinity->Mask;
            wprintf(
                L" %.5d - %s %s %s %s | %s %s %s %s | %s %s %s %s | %s %s %s %s - %.8I64x\n",
                (*ppNode)->iproc,
                get_core_bit_string(0x1, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x2, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x4, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x8, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x10, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x20, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x40, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x80, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x100, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x200, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x400, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x800, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x1000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x2000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x4000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                get_core_bit_string(0x8000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*ppNode)->affinity->Mask),
                (*ppNode)->affinity->Mask
                );
        }
        wprintf(
            L"                                                               - %.8I64x\n",
            fullMask
            );

    }
    wprintf(L"_______________________________________________________________________________\n" );
}


//
// Summary:
//  print affinity table 2. Table is sorted by rank
//    This can display only the first 32 cores per machine.
//
static void
mpiexec_print_affinity_table2(
    __in const smpd_launch_node_t*const* pList,
    __in const smpd_host_t*         pHosts
    )
{
    wprintf(L"_______________________________________________________________________________\n" );
    wprintf(L"                    - MSMPI Rank Affinity Table 2 (By Rank) -\n" );
    wprintf(L"Rank  HostName        Mask     0-3   4-7   8-11  12-15 16-19 20-23 24-27 28-31\n" );
    wprintf(L"_______________________________________________________________________________\n" );
    while( NULL != *pList )
    {
        const smpd_host_t* pHost = pHosts;
        while( pHost->HostId != (*pList)->host_id )
        {
            pHost = static_cast<const smpd_host_t*>( pHost->Next );
        }
        ASSERT(NULL != pHost);

        wprintf( L"%.5d %15s %.8I64x %c%c%c%c  %c%c%c%c  %c%c%c%c  %c%c%c%c  %c%c%c%c  %c%c%c%c  %c%c%c%c  %c%c%c%c\n",
                 (*pList)->iproc,
                 (*pList)->hostname,
                 (*pList)->affinity->Mask,
                 get_core_bit_char(0x1, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x2, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x4, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x8, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x10, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x20, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x40, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x80, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x100, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x200, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x400, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x800, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x1000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x2000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x4000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x8000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x10000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x20000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x40000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x80000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x100000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x200000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x400000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x800000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x1000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x2000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x4000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x8000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x10000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x20000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x40000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask),
                 get_core_bit_char(0x80000000, pHost->Summary->Infos[0].Mask, pHost->Summary->Infos[0].ActiveMask, (*pList)->affinity->Mask)
            );
        pList++;
    }
    wprintf( L"_______________________________________________________________________________\n" );
}


//
// Summary:
//  print affinity table 3. Table is sorted by Machine, Processor Group, then rank
//    This can display all cores on the machine.  It is not restricted to the first group.
//
static void
mpiexec_print_affinity_table3(
    __in const smpd_launch_node_t*const* pList,
    __in const smpd_host_t*         pHosts
    )
{
    wprintf( L"_______________________________________________________________________________\n" );
    wprintf( L"                    - MSMPI Rank Affinity Table 3 (By Processor Group) - \n" );
    wprintf( L"Rank        Cores\n" );
    wprintf( L"_______________________________________________________________________________\n" );
    for( const smpd_host_t* pHost = pHosts; NULL != pHost; pHost = static_cast<const smpd_host_t*>( pHost->Next ) )
    {
        for( UINT32 i = 0; i < pHost->Summary->Count; i++ )
        {
            wprintf(L"\n%s:%u\n", pHost->name, i );
            for( const smpd_launch_node_t*const* ppNode = pList; NULL != *ppNode; ppNode++ )
            {
                if( (*ppNode)->host_id != pHost->HostId ||
                    (*ppNode)->affinity->GroupId != static_cast<UINT16>( i )
                    )
                {
                    continue;
                }
                wprintf(
                    L"  %.8d %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
                    (*ppNode)->iproc,
                    get_core_bit_char(0x1, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x2, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x4, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x8, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x10, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x20, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x40, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x80, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x100, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x200, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x400, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x800, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x1000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x2000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x4000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x8000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x10000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x20000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x40000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x80000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x100000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x200000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x400000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x800000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x1000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x2000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x4000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x8000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x10000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x20000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x40000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x80000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x100000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x200000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x400000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x800000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x1000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x2000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x4000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x8000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x10000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x20000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x40000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x80000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x100000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x200000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x400000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x800000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x1000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x2000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x4000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x8000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x10000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x20000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x40000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x80000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x100000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x200000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x400000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x800000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x1000000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x2000000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x4000000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask),
                    get_core_bit_char(0x8000000000000000, pHost->Summary->Infos[i].Mask, pHost->Summary->Infos[i].ActiveMask, (*ppNode)->affinity->Mask)
                    );
            }
        }
    }
    wprintf( L"_______________________________________________________________________________\n" );
}

//
// Summary:
//  print affinity table. Enabled with MPIEXEC_AFFINITY_TABLE environment varaible.
//      3           = table 3
//      2           = table 2
//      1,on,yes    = table 1
//
void
mpiexec_print_affinity_table(
    __in const smpd_launch_node_t*        pLaunchList,
    __in const smpd_host_t*               pHosts
    )
{
    if(!smpd_process.affinityOptions.isSet ||
        smpd_process.affinityOptions.affinityTableStyle == 0)
    {
        return;
    }

    const smpd_launch_node_t** pFullList = const_cast<const smpd_launch_node_t**>(
        new smpd_launch_node_t*[smpd_process.nproc + 1] );
    if( pFullList == nullptr )
    {
        return; //print error!!
    }

    //
    // the pLaunchList is a singly linked list stored in reverse rank order
    //  we will copy it the pointers to an array in the correct order and use that
    //  for our affinity layout algorithm
    //
    int iProc = smpd_process.nproc;
    pFullList[iProc] = nullptr;
    const smpd_launch_node_t* node = pLaunchList;
    do
    {
        --iProc;
        ASSERT(iProc >= 0);
        pFullList[iProc] = node;
        node = node->next;
    } while( NULL != node );


    switch(smpd_process.affinityOptions.affinityTableStyle)
    {
    case 1:
        {
            mpiexec_print_affinity_table1(pFullList, pHosts);
            break;
        }
    case 2:
        {
            mpiexec_print_affinity_table2(pFullList, pHosts);
            break;
        }
    case 3:
        {
            mpiexec_print_affinity_table3(pFullList, pHosts);
            break;
        }
    default:
        break;
    }

    delete [] pFullList;
}
