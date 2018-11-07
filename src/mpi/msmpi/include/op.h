// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */
#include <oacr.h>

#pragma once

#ifndef OP_H
#define OP_H

/* ------------------------------------------------------------------------- */
/* Reduction and accumulate operations */
/*E
  MPID_Op_kind - Enumerates types of MPI_Op types

  Notes:
  These are needed for implementing 'MPI_Accumulate', since only predefined
  operations are allowed for that operation.

  A gap in the enum values was made allow additional predefined operations
  to be inserted.  This might include future additions to MPI or experimental
  extensions (such as a Read-Modify-Write operation).

  Module:
  Collective-DS
  E*/
enum MPID_Op_kind
{
    MPID_OP_NULL = 0,
    MPID_OP_MAX = HANDLE_BUILTIN_INDEX( MPI_MAX ),
    MPID_OP_MIN = HANDLE_BUILTIN_INDEX( MPI_MIN ),
    MPID_OP_SUM = HANDLE_BUILTIN_INDEX( MPI_SUM ),
    MPID_OP_PROD = HANDLE_BUILTIN_INDEX( MPI_PROD ),
    MPID_OP_LAND = HANDLE_BUILTIN_INDEX( MPI_LAND ),
    MPID_OP_BAND = HANDLE_BUILTIN_INDEX( MPI_BAND ),
    MPID_OP_LOR = HANDLE_BUILTIN_INDEX( MPI_LOR ),
    MPID_OP_BOR = HANDLE_BUILTIN_INDEX( MPI_BOR ),
    MPID_OP_LXOR = HANDLE_BUILTIN_INDEX( MPI_LXOR ),
    MPID_OP_BXOR = HANDLE_BUILTIN_INDEX( MPI_BXOR ),
    MPID_OP_MINLOC = HANDLE_BUILTIN_INDEX( MPI_MINLOC ),
    MPID_OP_MAXLOC = HANDLE_BUILTIN_INDEX( MPI_MAXLOC ),
    MPID_OP_REPLACE = HANDLE_BUILTIN_INDEX( MPI_REPLACE ),
    MPID_OP_NOOP = HANDLE_BUILTIN_INDEX( MPI_NO_OP ),
    MPID_OP_USER_NONCOMMUTE=32,
    MPID_OP_USER=33
};


struct OpTraits : public ObjectTraits<MPI_Op>
{
    static const MPID_Object_kind KIND = MPID_OP;
    static const int DIRECT = 16;
    static const int BUILTINMASK = 0xFFFFFFFF;
    static const int BUILTIN = 15;

};


class OpPool;


/*S
  MPID_Op - MPI_Op structure

  Notes:
  All of the predefined functions are commutative.  Only user functions may
  be noncommutative, so there are two separate op types for commutative and
  non-commutative user-defined operations.

  Operations do not require reference counts because there are no nonblocking
  operations that accept user-defined operations.  Thus, there is no way that
  a valid program can free an 'MPI_Op' while it is in use.

  Module:
  Collective-DS
  S*/
struct MPID_Op
{
    friend class ObjectPool<OpPool, MPID_Op, OpTraits>;

    int                handle;      /* value of MPI_Op for this structure */
private:
    mutable volatile long   ref_count;
public:
    union
    {
        MPI_User_function* user_function;
        MPID_Op* next;
    };
    MPID_User_function_proxy* proxy;
    MPID_Op_kind       kind;

public:
    static void GlobalInit();

public:
    __forceinline void AddRef() const
    {
        MPIU_Assert( ref_count != 0 );
        ::InterlockedIncrement(&ref_count);
    }

    __forceinline void Release()
    {
        MPIU_Assert( ref_count > 0 );
        long value = ::InterlockedDecrement(&ref_count);
        if( value == 0 )
        {
            Destroy();
        }
    }

    inline bool IsCommutative() const
    {
        return (
            kind != MPID_OP_USER_NONCOMMUTE && 
            kind != MPID_OP_NOOP &&
            kind != MPID_OP_REPLACE
            );
    }

    inline bool IsBuiltin() const
    {
        return kind <= MPID_OP_NOOP;
    }

private:
    void Destroy();
};


class OpPool : public ObjectPool<OpPool, MPID_Op, OpTraits>
{
};


void MPID_Op_init( void );


#define MPIR_PREDEF_OP_COUNT 12
extern MPI_User_function *MPIR_Op_table[];

typedef MPI_RESULT (MPIR_Op_check_dtype_fn) ( MPI_Datatype );
extern MPIR_Op_check_dtype_fn *MPIR_Op_check_dtype_table[];


/*D
  MPID_Uop - Support structure for calling user operation in different langs
  support provided at run time.

  Module:
  Collective-DS
  D*/

void
MPIAPI
MPIR_Op_c_proxy(
    MPI_User_function* uop,
    const void* a,
    void* b,
    const int* len,
    const MPI_Datatype* datatype
    );


static inline void MPID_Uop_call(const MPID_Op* op, const void* invec, void* inoutvec, const int* len, const MPI_Datatype* datatype)
{
    op->proxy(op->user_function, invec, inoutvec, len, datatype);
}


#endif // OP_H
