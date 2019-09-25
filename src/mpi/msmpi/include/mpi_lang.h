// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef MPI_LANG_H
#define MPI_LANG_H

#if defined(__cplusplus)
extern "C" {
#endif

/*E
  Language bindings for MPI

  A few operations in MPI need to know how to marshal the callback into the calling
  lanuage calling convention. The marshaling code is provided by a thunk layer which
  implements the correct behavior.  Examples of these callback functions are the
  keyval attribute copy and delete functions.

  Module:
  Attribute-DS
  E*/

/*
 * Support bindings for Op callbacks
 */
typedef
void
(MPIAPI MPID_User_function_proxy)(
    MPI_User_function* user_function,
    const void* a,
    void* b,
    const int* len,
    const MPI_Datatype* datatype
    );

void
MPIAPI
MPIR_Op_set_proxy(
    MPI_Op op,
    MPID_User_function_proxy* proxy
    );


/*
 * Support bindings for Comm errhandler callbacks
 */
typedef
void
(MPIAPI MPID_Comm_errhandler_proxy)(
    MPI_Comm_errhandler_fn* user_function,
    MPI_Comm* comm,
    int* errcode,
    ...
    );

void
MPIAPI
MPIR_Comm_errhandler_set_proxy(
    MPI_Errhandler handler,
    MPID_Comm_errhandler_proxy* proxy
    );


/*
 * Support bindings for File errhandler callbacks
 */
typedef
void
(MPIAPI MPID_File_errhandler_proxy)(
    MPI_File_errhandler_fn* user_function,
    MPI_File* file,
    int* errcode,
    ...
    );

void
MPIAPI
MPIR_File_errhandler_set_proxy(
    MPI_Errhandler handler,
    MPID_File_errhandler_proxy* proxy
    );


/*
 * Support bindings for Win errhandler callbacks
 */
typedef
void
(MPIAPI MPID_Win_errhandler_proxy)(
    MPI_Win_errhandler_fn* user_function,
    MPI_Win* win,
    int* errcode,
    ...
    );

void
MPIAPI
MPIR_Win_errhandler_set_proxy(
    MPI_Errhandler handler,
    MPID_Win_errhandler_proxy* proxy
    );



/*
 * Support bindings for Attribute copy/del callbacks
 * Consolidate Comm/Type/Win attribute functions together as the handle type is the same
 * use MPI_Comm for the prototypes
 */
typedef
int
(MPIAPI MPID_Attr_copy_proxy)(
    MPI_Comm_copy_attr_function* user_function,
    MPI_Comm comm,
    int keyval,
    void* extra_state,
    void* attrib,
    void** attrib_copy,
    int* flag
    );

typedef
int
(MPIAPI MPID_Attr_delete_proxy)(
    MPI_Comm_delete_attr_function* user_function,
    MPI_Comm comm,
    int keyval,
    void* attrib,
    void* extra_state
    );

void
MPIAPI
MPIR_Keyval_set_proxy(
    int keyval,
    MPID_Attr_copy_proxy copy_proxy,
    MPID_Attr_delete_proxy delete_proxy
    );


/*
 * Support bindings for Grequest callbacks
 */
typedef
int
(MPIAPI MPIR_Grequest_query_proxy)(
    MPI_Grequest_query_function* user_function,
    void* extra_state,
    MPI_Status* status
    );

typedef
int
(MPIAPI MPIR_Grequest_free_proxy)(
    MPI_Grequest_free_function* user_function,
    void* extra_state
    );

typedef
int
(MPIAPI MPIR_Grequest_cancel_proxy)(
    MPI_Grequest_cancel_function* user_function,
    void* extra_state,
    int complete
    );


void
MPIAPI
MPIR_Grequest_set_proxy(
    MPI_Request request,
    MPIR_Grequest_query_proxy query_proxy,
    MPIR_Grequest_free_proxy free_proxy,
    MPIR_Grequest_cancel_proxy cancel_proxy
    );


#if defined(__cplusplus)
}
#endif

#endif // MPI_LANG_H
