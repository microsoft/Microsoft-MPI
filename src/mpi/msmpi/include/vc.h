// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef VC_H
#define VC_H

struct MPIDI_VC_t;

/*S
 * MPIDI_VCRT - virtual connection reference table
 *
 * handle - this element is not used, but exists so that we may use the
 * MPIU_Object routines for reference counting
 *
 * ref_count - number of references to this table
 *
 * vcr_table - array of virtual connection references
 S*/
struct MPIDI_VCRT_t
{
    int handle;
    volatile long ref_count;
    int size;
    MPIDI_VC_t* vcr_table[1];
};

typedef struct MPIDI_VCRT_t MPID_VCRT;
typedef struct MPIDI_VC_t* MPID_VCR;

/*@
  MPID_VCRT_Create - Create a virtual connection reference table
  @*/
MPID_VCRT* MPID_VCRT_Create(int size);

/*@
  MPID_VCRT_Add_ref - Add a reference to a VCRT
  @*/
void MPID_VCRT_Add_ref(MPID_VCRT* vcrt);

/*@
  MPID_VCRT_Release - Release a reference to a VCRT

  Notes:
  The 'isDisconnect' argument allows this routine to handle the special
  case of 'MPI_Comm_disconnect', which needs to take special action
  if all references to a VC are removed.
  @*/
int MPID_VCRT_Release(MPID_VCRT* vcrt, int isDisconnect);

/*@
  MPID_VCRT_Get_ptr -
  @*/
MPID_VCR* MPID_VCRT_Get_ptr(MPID_VCRT* vcrt);

/*@
  MPID_VCR_Dup - Create a duplicate reference to a virtual connection
  @*/
_Ret_notnull_
MPID_VCR MPID_VCR_Dup( _Inout_ MPID_VCR orig_vcr );

/*@
   MPID_VCR_Get_lpid - Get the local process id that corresponds to a
   virtual connection reference.

   Notes:
   The local process ids are described elsewhere.  Basically, they are
   a nonnegative number by which this process can refer to other processes
   to which it is connected.  These are local process ids because different
   processes may use different ids to identify the same target process
  @*/
int MPID_VCR_Get_lpid(const MPIDI_VC_t *vcr);



#endif // VC_H
