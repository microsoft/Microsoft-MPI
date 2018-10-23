// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_connect_nd.c - Network Direct MPI CH3 Channel connection
                   related functionality.

--*/
#include "precomp.h"
#include "precomp.h"
#include "ch3u_nd1.h"
#include "ch3u_nd2.h"

using namespace CH3_ND;

//
// Getting the business card first listens for incoming connections on all
// network direct providers, and then fills in the address:portnum pairs
// in the business card.
//
int
MPIDI_CH3I_Nd_get_business_card(
    _Deref_pre_cap_c_( *pcbBusinessCard ) _Deref_out_z_cap_c_( *pcbBusinessCard ) char** pszBusinessCard,
    _Inout_ int* pcbBusinessCard
    )
{
    return g_NdEnv.GetBusinessCard( pszBusinessCard, pcbBusinessCard );
}


int
MPIDI_CH3I_Nd_connect(
    __inout MPIDI_VC_t* pVc,
    __in_z const char* szBusinessCard,
    __in int fForceUse,
    __out int* pbHandled
    )
{
    return g_NdEnv.Connect( pVc, szBusinessCard, fForceUse, pbHandled );
}


void
MPIDI_CH3I_Nd_disconnect(
    __inout MPIDI_VC_t* pVc
    )
{
    MPIU_Assert( pVc->ch.nd.pEp != NULL );
    pVc->ch.nd.pEp->Disconnect();
}
