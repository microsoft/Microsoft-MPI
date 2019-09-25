// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"
/* These are pointers to the static variables in src/mpid/ch3/src/ch3u_recvq.c
   that contains the *addresses* of the posted and unexpected queue head
   pointers */
extern MPID_Request ** const MPID_Recvq_posted_head_ptr,
    ** const MPID_Recvq_unexpected_head_ptr;

#include "mpi_interface.h"

/* This is from dbginit.c; it is not exported to other files */
typedef struct MPIR_Sendq {
    MPID_Request *sreq;
    int tag, rank;
    GUID context_id;
    struct MPIR_Sendq *next;
} MPIR_Sendq;
extern MPIR_Sendq *MPIR_Sendq_head;

/*
   This file contains emulation routines for the methods and functions normally
   provided by the debugger.  This file is only used for testing the
   dll_mpich2.c debugger interface.
 */

/* These are mock-ups of the find type and find offset routines.  Since
   there are no portable routines to access the symbol table in an image,
   we hand-code the specific types that we use in this code.
   These routines (more precisely, the field_offset routine) need to
   known the layout of the internal data structures */
enum { TYPE_UNKNOWN = 0,
       TYPE_MPID_COMM = 1,
       TYPE_MPIR_COMM_LIST = 2,
       TYPE_MPIDI_REQUEST = 3,
       TYPE_MPIDI_MESSAGE_MATCH = 4,
       TYPE_MPID_REQUEST = 5,
       TYPE_MPIR_SENDQ = 6,
} KnownTypes;
static int curType = TYPE_UNKNOWN;
mqs_type * dbgrI_find_type(mqs_image *image, char *name,
                                  mqs_lang_code lang)
{
    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        name,
                        -1,
                        "MPID_Comm",
                        -1 ) == CSTR_EQUAL )
    {
        curType = TYPE_MPID_COMM;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        name,
                        -1,
                        "MPIR_Comm_list",
                        -1 ) == CSTR_EQUAL )
    {
        curType = TYPE_MPIR_COMM_LIST;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        name,
                        -1,
                        "MPIDI_Request",
                        -1 ) == CSTR_EQUAL )
    {
        curType = TYPE_MPIDI_REQUEST;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        name,
                        -1,
                        "MPIDI_Message_match",
                        -1 ) == CSTR_EQUAL )
    {
        curType = TYPE_MPIDI_MESSAGE_MATCH;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        name,
                        -1,
                        "MPID_Request",
                        -1 ) == CSTR_EQUAL )
    {
        curType = TYPE_MPID_REQUEST;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        name,
                        -1,
                        "MPIR_Sendq",
                        -1 ) == CSTR_EQUAL )
    {
        curType = TYPE_MPIR_SENDQ;
    }
    else
    {
        curType = TYPE_UNKNOWN;
    }
    return (mqs_type *)&curType;
}
int dbgrI_field_offset(mqs_type *type, char *name)
{
    int off = -1;
    switch (curType) {
    case TYPE_MPID_COMM:
        {
            MPID_Comm c;
            if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "name",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char*)&(c.name[0]) - (char*)&c.handle);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "comm_next",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char*)&c.comm_next - (char*)&c.handle);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "remote_size",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char*)&c.remote_size - (char*)&c.handle);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "rank",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char*)&c.rank - (char*)&c.handle);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "context_id",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char*)&c.context_id - (char*)&c.handle);
            }
        }
        break;
    case TYPE_MPIR_COMM_LIST:
        {
            MPIR_Comm_list c;
            if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "sequence_number",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char*)&c.sequence_number - (char*)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "head",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char*)&c.head - (char*)&c);
            }
        }
        break;
    case TYPE_MPIDI_REQUEST:
        {
            struct MPIDI_Request c;
            if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "next",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.next - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "match",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.match - (char *)&c);
            }
        }
    case TYPE_MPIDI_MESSAGE_MATCH:
        {
            MPIDI_Message_match c;
            if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "tag",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.tag - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "rank",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.rank - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "context_id",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.context_id - (char *)&c);
            }
        }
        break;
    case TYPE_MPID_REQUEST:
        {
            MPID_Request c;
            if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "dev",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.dev - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "status",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.status - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "cc",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.cc - (char *)&c);
            }
        }
        break;
    case TYPE_MPIR_SENDQ:
        {
            struct MPIR_Sendq c;
            if( CompareStringA( LOCALE_INVARIANT,
                                0,
                                name,
                                -1,
                                "next",
                                -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.next - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "tag",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.tag - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "rank",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.rank - (char *)&c);
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                                     0,
                                     name,
                                     -1,
                                     "context_id",
                                     -1 ) == CSTR_EQUAL )
            {
                off = ((char *)&c.context_id - (char *)&c);
            }
        }
        break;
    case TYPE_UNKNOWN:
        off = -1;
        break;
    default:
        off = -1;
        break;
    }
    return off;
}

/* Simulate converting name to the address of a variable/symbol */
int dbgrI_find_symbol( mqs_image *image, char *name, mqs_taddr_t * loc )
{
    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        name,
                        -1,
                        "MPIR_All_communicators",
                        -1 ) == CSTR_EQUAL )
    {
        *loc = (mqs_taddr_t)&MPIR_All_communicators;
        return mqs_ok;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                             0,
                             name,
                             -1,
                             "MPID_Recvq_posted_head_ptr",
                             -1 ) == CSTR_EQUAL )
    {
        *loc = (mqs_taddr_t)&MPID_Recvq_posted_head_ptr;
        return mqs_ok;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                             0,
                             name,
                             -1,
                             "MPID_Recvq_unexpected_head_ptr",
                             -1 ) == CSTR_EQUAL )
    {
        *loc = (mqs_taddr_t)&MPID_Recvq_unexpected_head_ptr;
        return mqs_ok;
    }
    else if( CompareStringA( LOCALE_INVARIANT,
                             0,
                             name,
                             -1,
                             "MPIR_Sendq_head",
                             -1 ) == CSTR_EQUAL )
    {
        *loc = (mqs_taddr_t)&MPIR_Sendq_head;
        return mqs_ok;
    }
    return 1;
}
