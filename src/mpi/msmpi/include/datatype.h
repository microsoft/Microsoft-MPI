// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef MPID_DATATYPE_H
#define MPID_DATATYPE_H

#include "objpool.h"
#include "mpihandlemem.h"
#include "mpierror.h"

//
//  Datatypes handles of type HANDLE_TYPE_BUILTIN are laid out as follows:
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-------+-------------------+-------------------------------+
//  |Typ| Kind  |                   |      Size     |     Index     |
//  +---+-------+-------------------+-------------------------------+
//

//
// Define an internal type handle for MPI_DATATYPE_NULL that will give us a
// valid datatype object.
//
#define MPID_DATATYPE_NULL  HANDLE_SET_TYPE(MPI_DATATYPE_NULL, HANDLE_TYPE_BUILTIN)

//
// Macro to support compile-time assertions.
//
#define MPID_BUILTIN_TYPE_SIZE( _h )    ((_h & 0x0000ff00)>>8)

/* MPID_Datatype_get_basic_id() is useful for creating and indexing into arrays
   that store data on a per-basic type basis */
static inline int MPID_Datatype_get_basic_size( __in MPI_Datatype t )
{
    return MPID_BUILTIN_TYPE_SIZE(t);
}

static inline bool MPID_Datatype_is_predefined( __in MPI_Datatype t )
{
    return (HANDLE_GET_TYPE(t) == HANDLE_TYPE_BUILTIN) ||
          (t == MPI_FLOAT_INT)  ||
          (t == MPI_DOUBLE_INT) ||
          (t == MPI_LONG_INT)   ||
          (t == MPI_SHORT_INT)  ||
          (t == MPI_LONG_DOUBLE_INT);
}

static inline bool MPID_Datatype_is_integer(__in MPI_Datatype t)
{
    return (t == MPI_INT ||
        t == MPI_LONG ||
        t == MPI_SHORT ||
        t == MPI_UNSIGNED_SHORT ||
        t == MPI_UNSIGNED ||
        t == MPI_UNSIGNED_LONG ||
        t == MPI_LONG_LONG_INT ||
        t == MPI_LONG_LONG ||
        t == MPI_UNSIGNED_LONG_LONG ||
        t == MPI_SIGNED_CHAR ||
        t == MPI_UNSIGNED_CHAR ||
        t == MPI_INT8_T ||
        t == MPI_INT16_T ||
        t == MPI_INT32_T ||
        t == MPI_INT64_T ||
        t == MPI_UINT8_T ||
        t == MPI_UINT16_T ||
        t == MPI_UINT32_T ||
        t == MPI_UINT64_T ||
        t == MPI_INTEGER ||
        t == MPI_INTEGER1 ||
        t == MPI_INTEGER2 ||
        t == MPI_INTEGER4 ||
        t == MPI_INTEGER8 ||
        t == MPI_INTEGER16);
}

static inline bool MPID_Datatype_is_logical(__in MPI_Datatype t)
{
    return (t == MPI_LOGICAL || t == MPI_C_BOOL);
}

static inline bool MPID_Datatype_is_multilanguage(__in MPI_Datatype t)
{
    return (t == MPI_AINT || t == MPI_OFFSET || t == MPI_COUNT);
}

/*
 * The following macros allow us to access and set common dataloop fields
 */
#define MPID_GET_FIELD(ptr_, hetero_, value_, fieldname_)               \
{                                                                       \
  if ( hetero_ == DLOOP_DATALOOP_HOMOGENEOUS )                          \
      value_ = ptr_->dataloop##fieldname_;                              \
  else                                                                  \
      value_ = ptr_->hetero_dloop##fieldname_;                          \
}

#define MPID_SET_FIELD(ptr_, hetero_, value_, fieldname_)                \
{                                                                       \
  if ( hetero_ == DLOOP_DATALOOP_HOMOGENEOUS )                          \
      ptr_->dataloop##fieldname_ = value_;                              \
  else                                                                  \
      ptr_->hetero_dloop##fieldname_ = value_;                          \
}


/*S
  MPID_Datatype_contents - Holds envelope and contents data for a given
                           datatype

  Notes:
  Space is allocated beyond the structure itself in order to hold the
  arrays of types, ints, and aints, in that order.

  S*/

struct MPID_Datatype_contents
{
    int combiner;
    int nr_ints;
    int nr_aints;
    int nr_types;
    /* space allocated beyond structure used to store the aints[], types[],
     * and ints[] in that order.  This is because the aints are likely the
     * largest, followed by the types potentially being pointers, followed
     * by the ints.  This lets them pack nicely without having to pad.
     */
    MPI_Aint aints[ANYSIZE_ARRAY];

public:
    inline MPI_Aint* Aints(){ return aints; };
    inline MPI_Datatype* Types(){ return reinterpret_cast<MPI_Datatype*>(aints + nr_aints); }
    inline int* Ints(){ return reinterpret_cast<int*>(Types() + nr_types); }
};

//
// C_ASSERTS to support layout of aints[], types[], and ints[] above.
//
C_ASSERT( sizeof(MPI_Aint) >= sizeof(void*) );
C_ASSERT( (sizeof(MPI_Aint) % sizeof(void*)) == 0 );
C_ASSERT( sizeof(MPI_Datatype) >= sizeof(int) );
C_ASSERT( (sizeof(MPI_Datatype) % sizeof(int)) == 0 );

//
// The types created by MPI_TYPE_CREATE_F90_{INTEGER,REAL,COMPLEX} return the precision and range
// in the returned integers when queried by MPI_TYPE_GET_CONTENTS.
//
// The range is the number of decimal digits (that can have any value 1-9)
// that can be represented.
//
// e.g. 999,999,999 is the largest that can be represented in a 4-byte integer,
// despite this being less than INT_MAX.
//
// The values for precision and range for real/complex come from the external32
// representation of these datatypes in the MPI standard (MPI-3 p619, line 46).
//
// We need the precision and ranges in various places, so we define them here.  Note that
// the COMPLEX versions use the same values as the REALs they are made up of,
// e.g. MPI_F90_COMPLEX8 is made up of two MPI_REAL4's.
//
#define F90_INTEGER1_RANGE 2    // 99 (max is 127)
#define F90_INTEGER2_RANGE 4    // 9,999 (max is 32,767)
#define F90_INTEGER4_RANGE 9    // 999,999,999 (max is 2,147,483,647)
#define F90_INTEGER8_RANGE 18   // 999,999,999,999,999,999 (max is 9,223,372,036,854,775,807)
#define F90_REAL4_RANGE 6
#define F90_REAL8_RANGE 15
#define F90_REAL4_PRECISION 37
#define F90_REAL8_PRECISION 307

//
// Limit the number of parameterized F90 datatypes we can track.
//
#define MAX_F90_TYPES 16
extern struct MPID_Datatype* f90types[MAX_F90_TYPES];


//
// The MPI pair types (MPI_FLOAT_INT, etc.) are sadly not considered built-in types, but come
// from the direct pool.  There are 5 pair types.  We pre-allocate 8 extra direct types for the
// users.
//
// The handle values for builtin types encode the type size in the second byte - the lowest
// byte serves as the index into the builtin table.  We must mask off the index for builtin
// types, and do that with the BUILTINMASK.
//
// The number of builtin types is defined by the MPI standard, and constrained by our existing
// handle value definitions in order to preserve backward compatibility.  The number of builtin
// types must be one larger than the largest lower byte of the predefined type handles.
//
struct DatatypeTraits : public ObjectTraits<MPI_Datatype>
{
    static const MPID_Object_kind KIND = MPID_DATATYPE;
    static const int DIRECT = 13;
    static const int BUILTINMASK = 0xFF;
    static const int BUILTIN = 62;
};


class TypePool;
class TypeHandle;


/* Datatype Structure */
/*S
  MPID_Datatype - Description of the MPID Datatype structure

  Notes:
  The 'ref_count' is needed for nonblocking operations such as
.vb
   MPI_Type_create_struct( ... , &newtype );
   MPI_Irecv( buf, 1000, newtype, ..., &request );
   MPI_Type_free( &newtype );
   ...
   MPI_Wait( &request, &status );
.ve

  Module:
  Datatype-DS

  Notes:

  Alternatives:
  The following alternatives for the layout of this structure were considered.
  Most were not chosen because any benefit in performance or memory
  efficiency was outweighed by the added complexity of the implementation.

  A number of fields contain only boolean inforation ('is_contig',
  'has_sticky_ub', 'has_sticky_lb', 'is_permanent', 'is_committed').  These
  could be combined and stored in a single bit vector.

  'MPI_Type_dup' could be implemented with a shallow copy, where most of the
  data fields, would not be copied into the new object created by
  'MPI_Type_dup'; instead, the new object could point to the data fields in
  the old object.  However, this requires more code to make sure that fields
  are found in the correct objects and that deleting the old object doesn't
  invalidate the dup'ed datatype.

  Originally we attempted to keep contents/envelope data in a non-optimized
  dataloop.  The subarray and darray types were particularly problematic,
  and eventually we decided it would be simpler to just keep contents/
  envelope data in arrays separately.

  Earlier versions of the ADI used a single API to change the 'ref_count',
  with each MPI object type having a separate routine.  Since reference
  count changes are always up or down one, and since all MPI objects
  are defined to have the 'ref_count' field in the same place, the current
  ADI3 API uses two routines, 'MPIU_Object_add_ref' and
  'MPIU_Object_release_ref', to increment and decrement the reference count.

  S*/
struct MPID_Datatype
{
    friend class ObjectPool<TypePool, MPID_Datatype, DatatypeTraits>;
    friend class TypeHandle;
    friend void MPIR_Datatype_init(void);

public:
    int      handle; /* value of MPI_Datatype for structure */
    int      size;
    //
    // basic parameters for datatype, accessible via MPI calls
    //
    //  extent - the size including all padding.
    //  size - the size excluding all padding.
    //  lb - the offset to the padding preceding the contents.
    //  ub - the offset of the end of the padding following the contents.
    //  true_lb - the offset to the beginning of the contents.
    //  true_lb - the offset to the end of the contents.
    //
    union
    {
        MPI_Aint extent;
        MPID_Datatype* next;    /* Used by ObjectPool for free objects */
    };
    MPI_Aint ub;
    MPI_Aint lb;
    MPI_Aint true_ub;
    MPI_Aint true_lb;
private:
    volatile long ref_count;
public:

    /* chars affecting subsequent datatype processing and creation */
    unsigned int alignsize;

    /* element information; used for accumulate and get elements
     *
     * if type is composed of more than one element type, then
     * eltype == MPI_DATATYPE_NULL and element_size == -1
     */
    MPI_Datatype eltype;
    int      n_elements;
    MPI_Count element_size;

    /* pointer to contents and envelope data for the datatype */
    MPID_Datatype_contents *contents;

    /* dataloop members, including a pointer to the loop, the size in bytes,
     * and a depth used to verify that we can process it (limited stack depth
     */
    struct MPID_Dataloop *dataloop; /* might be optimized for homogenous */
    int                   dataloop_size;
    int                   dataloop_depth;

    //
    // The heterogeneous dataloop is poorly named - it is an unoptimized
    // dataloop, that preserves information about the stored types.  This
    // is needed for the ext32 data representation to allow pack/unpack
    // to byteswap the data properly.
    //
    struct MPID_Dataloop *hetero_dloop; /* heterogeneous dataloop */
    int                   hetero_dloop_size;
    int                   hetero_dloop_depth;

    /* MPI-2 attributes and name */
    struct MPID_Attribute *attributes;

    /* Upper bound on the number of contig blocks for one instance.
     * It is not trivial to calculate the *real* number of contig
     * blocks in the case where old datatype is non-contiguous
     */
    int max_contig_blocks;

    bool has_sticky_ub;
    bool has_sticky_lb;
    bool is_permanent;
    bool is_committed;
    /* information on contiguity of type, for processing shortcuts.
     *
     * is_contig is non-zero only if N instances of the type would be
     * contiguous.
     */
    bool is_contig;

    const char* name;

public:
    static void GlobalInit(void);

    //
    // This constructor is public so that the debugger extension
    // can instantiate a temporary object to store the data it reads
    // from the MPI process
    //

    //
    // The default constructor is minimal - it does not init all fields,
    // as datatype objects, when pulled from the pool, must be specialized.
    // It only initializes fields that would prevent successful lookup of
    // invalid object, and to support the Destroy function.
    //
    MPID_Datatype() :
        handle( 0 ),
        ref_count( 0 ),
        contents( nullptr ),
        dataloop( nullptr ),
        hetero_dloop( nullptr ),
        attributes( nullptr ),
        name( nullptr )
    {
    }

public:
    __forceinline void AddRef()
    {
        InterlockedIncrement(&ref_count);
    }

    __forceinline int Release()
    {
        MPIU_Assert( ref_count > 0 );
        long value = InterlockedDecrement(&ref_count);
        if( value == 0 )
        {
            return Destroy();
        }
        return MPI_SUCCESS;
    }

    __forceinline struct MPID_Dataloop* GetDataloop( bool hetero ) const
    {
        if( hetero )
        {
            return hetero_dloop;
        }
        else
        {
            return dataloop;
        }
    }

    __forceinline int GetDataloopSize( bool hetero ) const
    {
        if( hetero )
        {
            return hetero_dloop_size;
        }
        else
        {
            return dataloop_size;
        }
    }

    __forceinline int GetDataloopDepth( bool hetero ) const
    {
        if( hetero )
        {
            return hetero_dloop_depth;
        }
        else
        {
            return dataloop_depth;
        }
    }

    __forceinline void SetDataloop( bool hetero, struct MPID_Dataloop* pDataloop )
    {
        if( hetero )
        {
            hetero_dloop = pDataloop;
        }
        else
        {
            dataloop = pDataloop;
        }
    }

    __forceinline void SetDataloopSize( bool hetero, int size )
    {
        if( hetero )
        {
            hetero_dloop_size = size;
        }
        else
        {
            dataloop_size = size;
        }
    }

    __forceinline void SetDataloopDepth( bool hetero, int depth )
    {
        if( hetero )
        {
            hetero_dloop_depth = depth;
        }
        else
        {
            dataloop_depth = depth;
        }
    }

    const char* GetDefaultName() const;
private:
    int Destroy();
};


//
// We require 8-byte alignment for the TypeHandle to use 3 bits for encoding.
//
C_ASSERT( __alignof(MPID_Datatype) == 8 );


class TypePool : public ObjectPool<TypePool, MPID_Datatype, DatatypeTraits>
{
    typedef ObjectPool<TypePool, MPID_Datatype, DatatypeTraits> _Base;
    friend class _Base;

    //
    // Get is an unsafe lookup by handle.  It does not do any kind of validation
    // of the input handle at runtime (though it does assert on bounds checks).
    //
    _Ret_notnull_ static __forceinline MPID_Datatype* GetImpl( MPI_Datatype h )
    {
        if( h == MPI_DATATYPE_NULL )
        {
            h = MPID_DATATYPE_NULL;
        }
        return _Base::GetImpl( h );
    }
};

//
// TODO: REMOVE THESE!!!
//
static inline MPI_Datatype MPID_Datatype_get_basic_type( __in MPI_Datatype t )
{
    return TypePool::Get( t )->eltype;
}

static inline int MPID_Datatype_get_size( MPI_Datatype t )
{
    return TypePool::Get( t )->size;
}

static inline MPI_Aint MPID_Datatype_get_extent( __in MPI_Datatype t )
{
    return TypePool::Get( t )->extent;
}

#define MPID_DTYPE_BEGINNING  0
#define MPID_DTYPE_END       -1

/* LB/UB calculation helper macros */

/* MPID_DATATYPE_CONTIG_LB_UB()
 *
 * Determines the new LB and UB for a block of old types given the
 * old type's LB, UB, and extent, and a count of these types in the
 * block.
 *
 * Note: if the displacement is non-zero, the MPID_DATATYPE_BLOCK_LB_UB()
 * should be used instead (see below).
 */
#define MPID_DATATYPE_CONTIG_LB_UB(cnt_,                \
                                   old_lb_,             \
                                   old_ub_,             \
                                   old_extent_, \
                                   lb_,         \
                                   ub_)         \
{                                                       \
    if (cnt_ == 0) {                                    \
        lb_ = old_lb_;                          \
        ub_ = old_ub_;                          \
    }                                                   \
    else if (old_ub_ >= old_lb_) {                      \
        lb_ = old_lb_;                          \
        ub_ = old_ub_ + (old_extent_) * (cnt_ - 1);     \
    }                                                   \
    else /* negative extent */ {                        \
        lb_ = old_lb_ + (old_extent_) * (cnt_ - 1);     \
        ub_ = old_ub_;                          \
    }                                                   \
}

/* MPID_DATATYPE_VECTOR_LB_UB()
 *
 * Determines the new LB and UB for a vector of blocks of old types
 * given the old type's LB, UB, and extent, and a count, stride, and
 * blocklen describing the vectorization.
 */
#define MPID_DATATYPE_VECTOR_LB_UB(cnt_,                        \
                                   stride_,                     \
                                   blklen_,                     \
                                   old_lb_,                     \
                                   old_ub_,                     \
                                   old_extent_,         \
                                   lb_,                 \
                                   ub_)                 \
{                                                               \
    if (cnt_ == 0 || blklen_ == 0) {                            \
        lb_ = old_lb_;                                  \
        ub_ = old_ub_;                                  \
    }                                                           \
    else if (stride_ >= 0 && (old_extent_) >= 0) {              \
        lb_ = old_lb_;                                  \
        ub_ = old_ub_ + (old_extent_) * ((blklen_) - 1) +       \
            (stride_) * ((cnt_) - 1);                           \
    }                                                           \
    else if (stride_ < 0 && (old_extent_) >= 0) {               \
        lb_ = old_lb_ + (stride_) * ((cnt_) - 1);               \
        ub_ = old_ub_ + (old_extent_) * ((blklen_) - 1);        \
    }                                                           \
    else if (stride_ >= 0 && (old_extent_) < 0) {               \
        lb_ = old_lb_ + (old_extent_) * ((blklen_) - 1);        \
        ub_ = old_ub_ + (stride_) * ((cnt_) - 1);               \
    }                                                           \
    else {                                                      \
        lb_ = old_lb_ + (old_extent_) * ((blklen_) - 1) +       \
            (stride_) * ((cnt_) - 1);                           \
        ub_ = old_ub_;                                  \
    }                                                           \
}

/* MPID_DATATYPE_BLOCK_LB_UB()
 *
 * Determines the new LB and UB for a block of old types given the LB,
 * UB, and extent of the old type as well as a new displacement and count
 * of types.
 *
 * Note: we need the extent here in addition to the lb and ub because the
 * extent might have some padding in it that we need to take into account.
 */
#define MPID_DATATYPE_BLOCK_LB_UB(cnt_,                         \
                                  disp_,                                \
                                  old_lb_,                              \
                                  old_ub_,                              \
                                  old_extent_,                          \
                                  lb_,                                  \
                                  ub_)                                  \
{                                                                       \
    if (cnt_ == 0) {                                                    \
        lb_ = old_lb_ + (disp_);                                        \
        ub_ = old_ub_ + (disp_);                                        \
    }                                                                   \
    else if (old_ub_ >= old_lb_) {                                      \
        lb_ = old_lb_ + (disp_);                                        \
        ub_ = old_ub_ + (disp_) + (old_extent_) * ((cnt_) - 1); \
    }                                                                   \
    else /* negative extent */ {                                        \
        lb_ = old_lb_ + (disp_) + (old_extent_) * ((cnt_) - 1); \
        ub_ = old_ub_ + (disp_);                                        \
    }                                                                   \
}


//
// Define a type for a validated datatype handle.
//
class TypeHandle
{
private:
    //
    // Bit shifts are faster than bitwise AND, so we store the pointer
    // shifted right and use the upper 3 bits to encode the log2 size
    // of the type.  Since the log2 size is 0 for single-byte datatypes,
    // we use 7 as the sentinel to indicate that the size is not encoded
    // and the pointer must be dereferenced.
    //
    size_t m_Value;
#define TYPE_HANDLE_SIZE_SENTINEL   7
#define TYPE_HANDLE_SIZE_ZERO       6

private:
    __forceinline size_t EncodedSize() const
    {
        return m_Value >> (RTL_BITS_OF(m_Value) - 3);
    }

public:
    //
    // Some internal functions create datatypes (by calling the API) so we need
    // to provide a function to create a TypeHandle from a MPI_Datatype.  When
    // we fix calling our own APIs internally and return TypeHandle directly,
    // we will be able to eliminate this.
    //
    static TypeHandle Create( _In_ MPI_Datatype datatype )
    {
        TypeHandle hType;
        hType.Init( TypePool::Get( datatype ) );
        return hType;
    }

    //
    // We can't define a constructor if we want to be able to use TypeHandle in unions, so
    // we define an explicit initializer.
    //
    void Init(
        _In_ const MPID_Datatype* pType
        )
    {
        m_Value = reinterpret_cast<size_t>(pType) >> 3;

        unsigned long logSize;
        if( HANDLE_GET_TYPE(pType->handle) != HANDLE_TYPE_BUILTIN )
        {
            logSize = TYPE_HANDLE_SIZE_SENTINEL;
        }
        else if( 0 == _BitScanForward(
                        &logSize,
                        static_cast<unsigned long>(pType->size)
                        )
            )
        {
            MPIU_Assert(
                pType->handle == MPID_DATATYPE_NULL ||
                pType->handle == MPI_LB ||
                pType->handle == MPI_UB
                );
            logSize = TYPE_HANDLE_SIZE_ZERO;
        }
        else
        {
#if DBG
            unsigned long lsb;
            _BitScanReverse(
                &lsb,
                static_cast<unsigned long>( pType->size )
                );
            MPIU_Assert( lsb == logSize );
#endif
            MPIU_Assert( logSize < TYPE_HANDLE_SIZE_SENTINEL );
        }

        m_Value |= static_cast<size_t>(logSize) << (RTL_BITS_OF(m_Value) - 3);
    }

    bool operator != (const TypeHandle& rhs) const
    {
        return m_Value != rhs.m_Value;
    }

    bool operator == (const TypeHandle& rhs) const
    {
        return m_Value == rhs.m_Value;
    }

    operator size_t () const
    {
        return m_Value;
    }

    //
    // Temporary until the full stack handles TypeHandle natively.
    //
    inline MPI_Datatype GetMpiHandle() const
    {
        return Get()->handle;
    }

    //
    // Returns the pointer to the underlying datatype.
    // Note that calling Get() on an uninitialized instance will
    // indeed return NULL, but that is a programming error.
    _Notnull_
    __forceinline MPID_Datatype* Get() const
    {
        return reinterpret_cast<MPID_Datatype*>(
            m_Value << 3
            );
    }


    __forceinline bool IsBuiltin() const
    {
        return EncodedSize() != TYPE_HANDLE_SIZE_SENTINEL;
    }


    //
    // Returns the size.
    //
    __forceinline MPI_Count GetSize() const
    {
        //
        // To properly support MPI_Count in data type,
        // we first need to make sure we can send/receive more than 2GB of data,
        // channels and dataloop code need to be modified.
        // After that, we can fix up data type code to truly allow creation of
        // complex data type with size up to 2^63 bytes.
        // For now, it just retrieves the internally defined integer size and
        // return it as MPI_Count.
        //
        size_t encodedSize = EncodedSize();
        if (encodedSize == TYPE_HANDLE_SIZE_SENTINEL)
        {
            return Get()->size;
        }

        if( encodedSize == TYPE_HANDLE_SIZE_ZERO )
        {
            return 0;
        }

        return 1LL << encodedSize;
    }


    inline MPI_Count GetExtentAndLowerBound( _Out_ MPI_Count* pLowerBound ) const
    {
        //
        // To properly support MPI_Count in data type,
        // we first need to make sure we can send/receive more than 2GB of data,
        // channels and dataloop code need to be modified.
        // After that, we can fix up data type code to truly allow creation of
        // complex data type with size up to 2^63 bytes.
        // For now, it just retrieves the internally defined MPI_Aint lb and
        // extent, and return them as MPI_Count.
        //
        size_t encodedSize = EncodedSize();
        if( encodedSize == TYPE_HANDLE_SIZE_SENTINEL )
        {
            const MPID_Datatype* pdt = Get();
            *pLowerBound = pdt->lb;
            return pdt->extent;
        }

        *pLowerBound = 0;

        if( encodedSize == TYPE_HANDLE_SIZE_ZERO )
        {
            return 0;
        }

        return 1LL << encodedSize;
    }


    inline MPI_Count GetExtent() const
    {
        //
        // To properly support MPI_Count in data type,
        // we first need to make sure we can send/receive more than 2GB of data,
        // channels and dataloop code need to be modified.
        // After that, we can fix up data type code to truly allow creation of
        // complex data type with size up to 2^63 bytes.
        // For now, it just retrieves the internally defined MPI_Aint lb and
        // extent, and return them as MPI_Count.
        //
        size_t encodedSize = EncodedSize();
        if( encodedSize == TYPE_HANDLE_SIZE_SENTINEL )
        {
            const MPID_Datatype* pdt = Get();
            return pdt->extent;
        }

        if( encodedSize == TYPE_HANDLE_SIZE_ZERO )
        {
            return 0;
        }

        return 1LL << encodedSize;
    }


    inline MPI_Count GetTrueExtentAndLowerBound( _Out_ MPI_Count* pLowerBound ) const
    {
        //
        // To properly support MPI_Count in data type,
        // we first need to make sure we can send/receive more than 2GB of data,
        // channels and dataloop code need to be modified.
        // After that, we can fix up data type code to truly allow creation of
        // complex data type with size up to 2^63 bytes.
        // For now, it just retrieves the internally defined MPI_Aint true_lb
        // and calculated MPI_Aint true extent and return them as MPI_Count.
        //
        size_t encodedSize = EncodedSize();
        if( encodedSize == TYPE_HANDLE_SIZE_SENTINEL )
        {
            const MPID_Datatype* pdt = Get();
            *pLowerBound = pdt->true_lb;
            return pdt->true_ub - pdt->true_lb;
        }

        *pLowerBound = 0;

        if( encodedSize == TYPE_HANDLE_SIZE_ZERO )
        {
            return 0;
        }

        return 1LL << encodedSize;
    }


    //
    // Returns the size, lower bound, and whether the type is contiguous in memory.
    //
    __forceinline size_t
    GetSizeAndInfo(
        _In_ size_t count,
        _Out_ bool* pContig,
        _Out_ MPI_Aint* pTrueLowerBound
        ) const
    {
        size_t encodedSize = EncodedSize();
        if( encodedSize == TYPE_HANDLE_SIZE_SENTINEL )
        {
            MPID_Datatype* pType = Get();
            *pTrueLowerBound = pType->true_lb;
            *pContig = pType->is_contig;
            return pType->size * count;
        }

        *pTrueLowerBound = 0;
        *pContig = true;

        if( encodedSize == TYPE_HANDLE_SIZE_ZERO )
        {
            return 0;
        }

        return count << encodedSize;
    }


    __forceinline bool IsPredefined() const;
};


//
// Pre-defined handles for builtin types, to allow for easy comparisson.
//

union BuiltinTypeHandles
{
    friend void MPID_Datatype::GlobalInit( void );

private:
    TypeHandle m_hTypes[DatatypeTraits::BUILTIN];
public:
    //
    // Helper struct for direct access to named types.
    //
    struct
    {
        TypeHandle MPI_Datatype_null;
        TypeHandle MPI_Char;
        TypeHandle MPI_Unsigned_char;
        TypeHandle MPI_Short;
        TypeHandle MPI_Unsigned_short;
        TypeHandle MPI_Int;
        TypeHandle MPI_Unsigned;
        TypeHandle MPI_Long;
        TypeHandle MPI_Unsigned_long;
        TypeHandle MPI_Long_long;
        TypeHandle MPI_Float;
        TypeHandle MPI_Double;
        TypeHandle MPI_Long_double;
        TypeHandle MPI_Byte;
        TypeHandle MPI_Wchar;
        TypeHandle MPI_Packed;
        TypeHandle MPI_Lb;
        TypeHandle MPI_Ub;
        TypeHandle MPI_C_complex;
        TypeHandle MPI_C_float_complex;
        TypeHandle MPI_C_double_complex;
        TypeHandle MPI_C_long_double_complex;
        TypeHandle MPI_2int;
        TypeHandle MPI_C_bool;
        TypeHandle MPI_Signed_char;
        TypeHandle MPI_Unsigned_long_long;
        TypeHandle MPI_Character;
        TypeHandle MPI_Integer;
        TypeHandle MPI_Real;
        TypeHandle MPI_Logical;
        TypeHandle MPI_Complex;
        TypeHandle MPI_Double_precision;
        TypeHandle MPI_2integer;
        TypeHandle MPI_2real;
        TypeHandle MPI_Double_complex;
        TypeHandle MPI_2double_precision;
        TypeHandle MPI_2complex;
        TypeHandle MPI_2double_complex;
        TypeHandle MPI_Real2; // Maps to null type.
        TypeHandle MPI_Real4;
        TypeHandle MPI_Complex8;
        TypeHandle MPI_Real8;
        TypeHandle MPI_Complex16;
        TypeHandle MPI_Real16; // Maps to null type.
        TypeHandle MPI_Complex32; // Maps to null type.
        TypeHandle MPI_Integer1;
        TypeHandle MPI_Complex4;
        TypeHandle MPI_Integer2;
        TypeHandle MPI_Integer4;
        TypeHandle MPI_Integer8;
        TypeHandle MPI_Integer16; //Maps to null type.
        TypeHandle MPI_Int8_t;
        TypeHandle MPI_Int16_t;
        TypeHandle MPI_Int32_t;
        TypeHandle MPI_Int64_t;
        TypeHandle MPI_Uint8_t;
        TypeHandle MPI_Uint16_t;
        TypeHandle MPI_Uint32_t;
        TypeHandle MPI_Uint64_t;
        TypeHandle MPI_Aint;
        TypeHandle MPI_Offset;
        TypeHandle MPI_Count;
    };

protected:
    void Set( _In_ MPI_Datatype type, _In_ MPID_Datatype* pType )
    {
        m_hTypes[(HANDLE_DIRECT_INDEX( type ) & DatatypeTraits::BUILTINMASK)].Init( pType );
    }

public:
    __forceinline TypeHandle Get( _In_ MPI_Datatype type )
    {
        MPIU_Assert( MPID_Datatype_is_predefined( type ) );
        return m_hTypes[(HANDLE_DIRECT_INDEX( type ) & DatatypeTraits::BUILTINMASK)];
    }
};


extern BuiltinTypeHandles g_hBuiltinTypes;
extern TypeHandle MPID_FloatInt;
extern TypeHandle MPID_DoubleInt;
extern TypeHandle MPID_LongInt;
extern TypeHandle MPID_ShortInt;
extern TypeHandle MPID_LongDoubleInt;


__forceinline bool TypeHandle::IsPredefined() const
{
    return(
        IsBuiltin() ||
        *this == MPID_FloatInt ||
        *this == MPID_DoubleInt ||
        *this == MPID_LongInt ||
        *this == MPID_ShortInt ||
        *this == MPID_LongDoubleInt
        );
}


/* Datatype functions */
MPI_RESULT MPID_Type_commit(MPID_Datatype* datatype_ptr);

MPI_RESULT
MPID_Type_dup(
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    );


MPI_RESULT
MPID_Type_struct(
    _In_range_(>=, 0) int count,
    _In_reads_opt_(count) const int blocklength_array[],
    _In_reads_opt_(count) const MPI_Aint displacement_array[],
    _In_reads_opt_(count) const MPI_Datatype oldtype_array[],
    _Outptr_ MPID_Datatype** ppNewType
    );


MPI_RESULT
MPID_Type_indexed(
    _In_range_(>=, 0) int count,
    _In_reads_opt_(count) const int blocklength_array[],
    _In_opt_ const void *displacement_array,
    _In_ bool dispinbytes,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    );

MPI_RESULT
MPID_Type_blockindexed(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _In_ const void *displacement_array,
    _In_ bool dispinbytes,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    );

MPI_RESULT
MPID_Type_vector(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _In_ MPI_Aint stride,
    _In_ bool strideinbytes,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    );

MPI_RESULT
MPID_Type_contiguous(
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    );

MPI_RESULT
MPID_Type_zerolen(
    _Outptr_ MPID_Datatype** ppNewType
    );

MPI_RESULT MPID_Type_create_resized(MPI_Datatype oldtype,
                             MPI_Aint lb,
                             MPI_Aint extent,
                             MPI_Datatype *newtype);

void MPID_Type_get_envelope(MPI_Datatype datatype,
                           int *num_integers,
                           int *num_addresses,
                           int *num_datatypes,
                           int *combiner);

MPI_RESULT
MPID_Type_get_contents(
    _In_ MPI_Datatype datatype,
    _In_range_(>=, 0) int max_integers,
    _In_range_(>=, 0) int max_addresses,
    _In_range_(>=, 0) int max_datatypes,
    _Out_writes_opt_(max_integers) int array_of_integers[],
    _Out_writes_opt_(max_addresses) MPI_Aint array_of_addresses[],
    _Out_writes_(max_datatypes) MPI_Datatype array_of_datatypes[]
    );


MPI_RESULT MPID_Type_create_pairtype(MPI_Datatype datatype,
                              _Inout_ MPID_Datatype *new_dtp);

/* internal debugging functions */
void MPIDI_Datatype_printf(MPI_Datatype type,
                           int depth,
                           MPI_Aint displacement,
                           int blocklength,
                           int header);

static
inline
void
MPID_Segment_pack_msg(
    struct DLOOP_Segment *segp,
    DLOOP_Offset first,
    MPIDI_msg_sz_t *lastp,
    void * pack_buffer
    )
{
    DLOOP_Offset last = *lastp;
    MPID_Segment_pack(segp, first, &last, pack_buffer);
    MPIU_Assert(last <= MPIDI_MSG_SZ_MAX);
    *lastp = (MPIDI_msg_sz_t)last;
}

static
inline
void
MPID_Segment_unpack_msg(
    struct DLOOP_Segment *segp,
    DLOOP_Offset first,
    MPIDI_msg_sz_t *lastp,
    const void * unpack_buffer
    )
{
    DLOOP_Offset last = *lastp;
    MPID_Segment_unpack(segp, first, &last, unpack_buffer);
    MPIU_Assert(last <= MPIDI_MSG_SZ_MAX);
    *lastp = (MPIDI_msg_sz_t)last;
}


void MPID_Segment_pack_vector(struct DLOOP_Segment *segp,
                              DLOOP_Offset first,
                              DLOOP_Offset *lastp,
                              MPID_IOV *vector,
                              int *lengthp);

static
inline
void
MPID_Segment_pack_vector_msg(
    struct DLOOP_Segment *segp,
    DLOOP_Offset first,
    MPIDI_msg_sz_t *lastp,
    MPID_IOV *vector,
    int *lengthp
    )
{
    DLOOP_Offset last = *lastp;
    MPID_Segment_pack_vector(segp, first, &last, vector, lengthp);
    MPIU_Assert(last <= MPIDI_MSG_SZ_MAX);
    *lastp = (MPIDI_msg_sz_t)last;
}


void MPID_Segment_unpack_vector(struct DLOOP_Segment *segp,
                                DLOOP_Offset first,
                                DLOOP_Offset *lastp,
                                MPID_IOV *vector,
                                int *lengthp);

static
inline
void
MPID_Segment_unpack_vector_msg(
    struct DLOOP_Segment *segp,
    DLOOP_Offset first,
    MPIDI_msg_sz_t *lastp,
    MPID_IOV *vector,
    int *lengthp
    )
{
    DLOOP_Offset last = *lastp;
    MPID_Segment_unpack_vector(segp, first, &last, vector, lengthp);
    MPIU_Assert(last <= MPIDI_MSG_SZ_MAX);
    *lastp = (MPIDI_msg_sz_t)last;
}


void MPID_Segment_count_contig_blocks(struct DLOOP_Segment *segp,
                                      DLOOP_Offset first,
                                      DLOOP_Offset *lastp,
                                      int *countp);

void MPID_Segment_flatten(struct DLOOP_Segment *segp,
                          DLOOP_Offset first,
                          DLOOP_Offset *lastp,
                          DLOOP_Offset *offp,
                          int *sizep,
                          DLOOP_Offset *lengthp);

/* misc */
MPI_RESULT
MPID_Datatype_set_contents(struct MPID_Datatype *ptr,
                           int combiner,
                           int nr_ints,
                           int nr_aints,
                           int nr_types,
                           const int array_of_ints[],
                           const MPI_Aint array_of_aints[],
                           const MPI_Datatype array_of_types[] );

void MPID_Datatype_free_contents(struct MPID_Datatype *ptr);

void MPID_Datatype_free(struct MPID_Datatype *ptr);

int MPIR_Type_get_contig_blocks(MPI_Datatype type,
                                int *nr_blocks_p);

MPI_RESULT
MPIR_Type_flatten(
    _In_ MPI_Datatype type,
    _Out_ MPI_Aint *off_array,
    _Out_ int *size_array,
    _Inout_ MPI_Aint *array_len_p
    );

void MPID_Segment_pack_external32(struct DLOOP_Segment *segp,
                                  DLOOP_Offset first,
                                  DLOOP_Offset *lastp,
                                  void *pack_buffer);

void MPID_Segment_unpack_external32(struct DLOOP_Segment *segp,
                                    DLOOP_Offset first,
                                    DLOOP_Offset *lastp,
                                    DLOOP_Buffer unpack_buffer);

MPI_Aint MPID_Datatype_size_external32(_In_ const MPID_Datatype* type_ptr, MPI_Datatype type);
MPI_Aint MPIDI_Datatype_get_basic_size_external32(MPI_Datatype el_type);

/* debugging helper functions */
const char* MPIDU_Datatype_builtin_to_string(MPI_Datatype type);
const char* MPIDU_Datatype_combiner_to_string(int combiner);
void MPIDU_Datatype_debug(MPI_Datatype type, int array_ct);

/* contents accessor functions */
void MPID_Type_access_contents(MPI_Datatype type,
                               int **ints_p,
                               MPI_Aint **aints_p,
                               MPI_Datatype **types_p);


void MPIR_Datatype_iscontig(MPI_Datatype datatype, int* flag);
MPI_RESULT MPIR_Datatype_cleanup();
MPI_RESULT MPIR_Create_f90_predefined(MPI_Datatype basetype,
                               int combiner,
                               int precision,
                               int range,
                               MPI_Datatype *newtype );

MPI_RESULT MPIR_Type_block(const int *array_of_gsizes,
                               int dim,
                               int ndims,
                               int nprocs,
                               int rank,
                               int darg,
                               int order,
                               MPI_Aint orig_extent,
                               MPI_Datatype type_old,
                               _Outptr_ MPID_Datatype** ppNewType,
                               MPI_Aint *st_offset);

MPI_RESULT MPIR_Type_cyclic(const int *array_of_gsizes,
                                int dim,
                                int ndims,
                                int nprocs,
                                int rank,
                                int darg,
                                int order,
                                MPI_Aint orig_extent,
                                MPI_Datatype type_old,
                                _Outptr_ MPID_Datatype** ppNewType,
                                MPI_Aint *st_offset);

MPI_Count
MPIR_Type_get_predef_type_elements(
    _Inout_ MPI_Count *bytes_p,
    _In_ int count,
    _In_ MPI_Datatype datatype
    );

MPI_Count
MPIR_Type_get_elements(
    _Inout_ MPI_Count *bytes_p,
    _In_ int count,
    _In_ MPI_Datatype datatype
    );


//
// Check if MPI_Count value will overflow the standard 32 bit int type
//
inline
int
CheckIntRange(
    _In_ MPI_Count value
    )
{
    int mpi_errno;
    if (value > INT_MAX)
    {
        mpi_errno =  MPIU_ERR_CREATE(MPI_ERR_COUNT, "**intoverflow %l", value);
    }
    else
    {
        mpi_errno = MPI_SUCCESS;
    }
    return mpi_errno;
}


#endif
