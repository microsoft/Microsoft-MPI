// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef DBGSUPPORT_H
#define DBGSUPPORT_H

/* These macros allow us to implement a sendq when debugger support is
   selected.  As there is extra overhead for this, we only do this
   when specifically requested
*/
#ifdef HAVE_DEBUGGER_SUPPORT

void MPIR_Sendq_remember(MPID_Request *, int, int, const MPI_CONTEXT_ID& );
void MPIR_Sendq_forget(MPID_Request *);
void MPIR_CommL_remember( MPID_Comm * );
void MPIR_CommL_forget( MPID_Comm * );

#define MPIR_SENDQ_REMEMBER(_a,_b,_c,_d) MPIR_Sendq_remember(_a,_b,_c,_d)
#define MPIR_SENDQ_FORGET(_a) MPIR_Sendq_forget(_a)
#define MPIR_COMML_REMEMBER(_a) MPIR_CommL_remember( _a )
#define MPIR_COMML_FORGET(_a) MPIR_CommL_forget( _a )
#else
#define MPIR_SENDQ_REMEMBER(a,b,c,d)
#define MPIR_SENDQ_FORGET(a)
#define MPIR_COMML_REMEMBER(_a)
#define MPIR_COMML_FORGET(_a)
#endif


#endif // DBGSUPPORT_H
