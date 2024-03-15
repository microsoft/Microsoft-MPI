// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef INFO_H
#define INFO_H

/* ------------------------------------------------------------------------- */
/* Info */
/*TInfoOverview.tex

  'MPI_Info' provides a way to create a list of '(key,value)' pairs
  where the 'key' and 'value' are both strings.  Because many routines, both
  in the MPI implementation and in related APIs such as the PMI process
  management interface, require 'MPI_Info' arguments, we define a simple
  structure for each 'MPI_Info' element.  Elements are allocated by the
  generic object allocator; the head element is always empty (no 'key'
  or 'value' is defined on the head element).

  For simplicity, we have not abstracted the info data structures;
  routines that want to work with the linked list may do so directly.
  Because the 'MPI_Info' type is a handle and not a pointer, an MPIU
  (utility) routine is provided to handle the
  deallocation of 'MPID_Info' elements.  See the implementation of
  'MPI_Info_create' for how an Info type is allocated.

  Thread Safety:

  The info interface itself is not thread-robust.  In particular, the routines
  'MPI_INFO_GET_NKEYS' and 'MPI_INFO_GET_NTHKEY' assume that no other
  thread modifies the info key.  (If the info routines had the concept
  of a next value, they would not be thread safe.  As it stands, a user
  must be careful if several threads have access to the same info object.)
  Further, 'MPI_INFO_DUP', while not
  explicitly advising implementers to be careful of one thread modifying the
  'MPI_Info' structure while 'MPI_INFO_DUP' is copying it, requires that the
  operation take place in a thread-safe manner.
  There isn'' much that we can do about these cases.  There are other cases
  that must be handled.  In particular, multiple threads are allowed to
  update the same info value.  Thus, all of the update routines must be thread
  safe; the simple implementation used in the MPICH implementation uses locks.
  Note that the 'MPI_Info_delete' call does not need a lock; the defintion of
  thread-safety means that any order of the calls functions correctly; since
  it invalid either to delete the same 'MPI_Info' twice or to modify an
  'MPI_Info' that has been deleted, only one thread at a time can call
  'MPI_Info_free' on any particular 'MPI_Info' value.

  T*/
/*S
  MPID_Info - Structure of an MPID info

  Notes:
  There is no reference count because 'MPI_Info' values, unlike other MPI
  objects, may be changed after they are passed to a routine without
  changing the routine''s behavior.  In other words, any routine that uses
  an 'MPI_Info' object must make a copy or otherwise act on any info value
  that it needs.

  A linked list is used because the typical 'MPI_Info' list will be short
  and a simple linked list is easy to implement and to maintain.  Similarly,
  a single structure rather than separate header and element structures are
  defined for simplicity.  No separate thread lock is provided because
  info routines are not performance critical; they may use the single
  critical section lock in the 'MPIR_Process' structure when they need a
  thread lock.

  This particular form of linked list (in particular, with this particular
  choice of the first two members) is used because it allows us to use
  the same routines to manage this list as are used to manage the
  list of free objects (in the file 'src/util/mem/handlemem.c').  In
  particular, if lock-free routines for updating a linked list are
  provided, they can be used for managing the 'MPID_Info' structure as well.

  The MPI standard requires that keys can be no less that 32 characters and
  no more than 255 characters.  There is no mandated limit on the size
  of values.

  Module:
  Info-DS
  S*/
struct MPID_Info
{
    int                handle;
    volatile int       ref_count;
    struct MPID_Info   *next;
    char               *key;
    char               *value;
};

extern MPIU_Object_alloc_t MPID_Info_mem;

/* Preallocated info objects */
extern MPID_Info MPID_Info_direct[];



/* Info iteration routines */
static inline MPID_Info* MPIU_Info_head(MPID_Info* list)
{
    /* skip the dummy first node in the info list */
    return list->next;
}


static inline MPID_Info* MPIU_Info_next(MPID_Info* info)
{
    return info->next;
}


static inline const char* MPIU_Info_get_key(const MPID_Info* info)
{
    return info->key;
}


static inline const char* MPIU_Info_get_value(const MPID_Info* info)
{
    return info->value;
}

MPID_Info* MPIU_Info_alloc_node(const char* key, const char* value);

void MPIU_Info_free_node(MPID_Info* p);

void MPIU_Info_free_list(MPID_Info* p);

#endif // INFO_H
