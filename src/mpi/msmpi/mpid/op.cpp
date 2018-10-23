// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "mpi_fortlogical.h"


template<typename T>
class Op
{
public:
    static inline void Max(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] = max(inoutVec[len], inVec[len]);
        }
    }

    static inline void Min(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] = min(inoutVec[len], inVec[len]);
        }
    }

    static inline void Sum(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] += inVec[len];
        }
    }

    static inline void Prod(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] *= inVec[len];
        }
    }

    static inline void LogicalAnd(
        __in_ecount(len) const T*const  __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] = inoutVec[len] && inVec[len];
        }
    }

    static inline void BitwiseAnd(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] &= inVec[len];
        }
    }

    static inline void LogicalOr(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] = inoutVec[len] || inVec[len];
        }
    }

    static inline void BitwiseOr(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] |= inVec[len];
        }
    }

    static inline void LogicalXor(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] = (inoutVec[len] && !inVec[len]) || (!inoutVec[len] && inVec[len]);
        }
    }

    static inline void BitwiseXor(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len] ^= inVec[len];
        }
    }

    static inline void MaxLoc(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len].MaxLoc( inVec[len] );
        }
    }

    static inline void MinLoc(
        __in_ecount(len) const T* const __restrict inVec,
        __inout_ecount(len) T* __restrict inoutVec,
        __in SSIZE_T len
        )
    {
        while( --len >= 0 )
        {
            inoutVec[len].MinLoc( inVec[len] );
        }
    }
};

template<typename T>
class OpCast
{
public:
    static inline void Max(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::Max( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void Min(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::Min( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void Sum(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::Sum( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void Prod(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::Prod( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void LogicalAnd(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::LogicalAnd( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void BitwiseAnd(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::BitwiseAnd( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void LogicalOr(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::LogicalOr( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void BitwiseOr(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::BitwiseOr( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void LogicalXor(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::LogicalXor( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void BitwiseXor(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::BitwiseXor( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void MaxLoc(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::MaxLoc( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }

    static inline void MinLoc(
        _In_count_(len) const void* inVec,
        _Inout_count_(len) void* inoutVec,
        __in SSIZE_T len
        )
    {
        Op<T>::MinLoc( static_cast<const T* const>(inVec), static_cast<T*>(inoutVec), len );
    }
};


//
// Template class to support SUM and PROD operations on complex types.
//
template<typename T>
class complex
{
    T re;
    T im;

public:
    complex<T>& operator+=( const complex<T>& rhs )
    {
        re += rhs.re;
        im += rhs.im;
        return (*this);
    }

    complex<T>& operator*=( const complex<T>& rhs )
    {
        T r;
        T i;
        r = (re * rhs.re) - (im * rhs.im);
        i = (re * rhs.im) + (rhs.re * im);
        re = r;
        im = i;
        return (*this);
    }
};


//
// Template class to support MAXLOC and MINLOC operations.
//
template<typename V, typename L>
class loctype
{
    V value;
    L location;

public:
    inline void MaxLoc( const loctype<V,L>& rhs )
    {
        if( value == rhs.value )
        {
            location = min(location, rhs.location);
        }
        else if( value < rhs.value )
        {
            *this = rhs;
        }
    }

    inline void MinLoc( const loctype<V,L>& rhs )
    {
        if( value == rhs.value )
        {
            location = min(location, rhs.location);
        }
        else if( value > rhs.value )
        {
            *this = rhs;
        }
    }
};


#define CASE_MPI_C_INTS( Op_ )                                      \
    case MPI_INT:                                                   \
        OpCast<int>::Op_;                                           \
        return;                                                     \
    case MPI_LONG:                                                  \
        OpCast<long>::Op_;                                          \
        return;                                                     \
    case MPI_SHORT:                                                 \
        OpCast<short>::Op_;                                         \
        return;                                                     \
    case MPI_UNSIGNED_SHORT:                                        \
        OpCast<unsigned short>::Op_;                                \
        return;                                                     \
    case MPI_UNSIGNED:                                              \
        OpCast<unsigned int>::Op_;                                  \
        return;                                                     \
    case MPI_UNSIGNED_LONG:                                         \
        OpCast<unsigned long>::Op_;                                 \
        return;                                                     \
    case MPI_LONG_LONG:                                             \
        OpCast<long long>::Op_;                                     \
        return;                                                     \
    case MPI_UNSIGNED_LONG_LONG:                                    \
        OpCast<unsigned long long>::Op_;                            \
        return;                                                     \
    case MPI_SIGNED_CHAR:                                           \
        OpCast<char>::Op_;                                          \
        return;                                                     \
    case MPI_UNSIGNED_CHAR:                                         \
        OpCast<unsigned char>::Op_;                                 \
        return;                                                     \
    case MPI_INT8_T:                                                \
        OpCast<__int8>::Op_;                                        \
        return;                                                     \
    case MPI_INT16_T:                                               \
        OpCast<__int16>::Op_;                                       \
        return;                                                     \
    case MPI_INT32_T:                                               \
        OpCast<__int32>::Op_;                                       \
        return;                                                     \
    case MPI_INT64_T:                                               \
        OpCast<__int64>::Op_;                                       \
        return;                                                     \
    case MPI_UINT8_T:                                               \
        OpCast<unsigned __int8>::Op_;                               \
        return;                                                     \
    case MPI_UINT16_T:                                              \
        OpCast<unsigned __int16>::Op_;                              \
        return;                                                     \
    case MPI_UINT32_T:                                              \
        OpCast<unsigned __int32>::Op_;                              \
       return;                                                      \
    case MPI_UINT64_T:                                              \
        OpCast<unsigned __int64>::Op_;                              \
        return;


#ifdef MPIR_INTEGER16_CTYPE
#define CASE_MPI_INTEGER16( Op_ )                                   \
    case MPI_INTEGER16:                                             \
        OpCast<MPIR_INTEGER16_CTYPE>::Op_;                          \
        return;
#else
#define CASE_MPI_INTEGER16( Op_ )
#endif

#define CASE_MPI_F_INTS( Op_ )                                      \
    case MPI_INTEGER:                                               \
        OpCast<MPI_Fint>::Op_;                                      \
        return;                                                     \
    case MPI_AINT:                                                  \
        OpCast<MPI_Aint>::Op_;                                      \
        return;                                                     \
    case MPI_OFFSET:                                                \
        OpCast<MPI_Offset>::Op_;                                    \
        return;                                                     \
    case MPI_INTEGER1:                                              \
        OpCast<__int8>::Op_;                                        \
        return;                                                     \
    case MPI_INTEGER2:                                              \
        OpCast<__int16>::Op_;                                       \
        return;                                                     \
    case MPI_INTEGER4:                                              \
        OpCast<__int32>::Op_;                                       \
        return;                                                     \
    case MPI_INTEGER8:                                              \
        OpCast<__int64>::Op_;                                       \
        return;                                                     \
    CASE_MPI_INTEGER16( Op_ )



#ifdef MPIR_REAL2_CTYPE
#define CASE_MPI_REAL2( Op_ )                                       \
        OpCast<MPIR_REAL2_CTYPE>::Op_;                              \
        return;
#else
#define CASE_MPI_REAL2( Op_ )
#endif

#ifdef MPIR_REAL16_CTYPE
#define CASE_MPI_REAL16( Op_ )                                      \
    case MPI_REAL16:                                                \
        OpCast<MPIR_REAL16_CTYPE>::Op_;                             \
        return;
#else
#define CASE_MPI_REAL16( Op_ )
#endif

#define CASE_MPI_FLOATS( Op_ )                                      \
    case MPI_FLOAT:                                                 \
    case MPI_REAL: /* FIXME: This assumes C float = Fortran real */ \
    case MPI_REAL4:                                                 \
        OpCast<float>::Op_;                                         \
        return;                                                     \
    case MPI_DOUBLE:                                                \
    case MPI_DOUBLE_PRECISION: /* FIXME: This assumes C double = Fortran double precision */ \
    case MPI_REAL8:                                                 \
        OpCast<double>::Op_;                                        \
        return;                                                     \
    case MPI_LONG_DOUBLE:                                           \
        OpCast<long double>::Op_;                                   \
        return;                                                     \
    CASE_MPI_REAL2( Op_ )                                           \
    CASE_MPI_REAL16( Op_ )


#ifdef MPIR_REAL16_CTYPE
#define CASE_MPI_COMPLEX32( Op_ )                                   \
    case MPI_COMPLEX32:                                             \
        OpCast<complex<MPIR_REAL16_CTYPE>>::Prod( invec, inoutvec, *Len ); \
        return;
#else
#define CASE_MPI_COMPLEX32( Op_ )
#endif

#define CASE_MPI_COMPLEXES( Op_ )                                   \
    case MPI_COMPLEX8:                                              \
    case MPI_COMPLEX:                                               \
    case MPI_C_COMPLEX:                                             \
    case MPI_C_FLOAT_COMPLEX:                                       \
        OpCast<complex<float>>::Op_;                                \
        return;                                                     \
    case MPI_COMPLEX16:                                             \
    case MPI_DOUBLE_COMPLEX:                                        \
    case MPI_C_DOUBLE_COMPLEX:                                      \
    case MPI_C_LONG_DOUBLE_COMPLEX:                                 \
        OpCast<complex<double>>::Op_;                               \
        return;                                                     \
    CASE_MPI_COMPLEX32( Op_ )


#define CASE_MPI_LOGICALS( Op_ )                                    \
    case MPI_LOGICAL:                                               \
        OpCast<MPI_Fint>::Op_;                                      \
        return;                                                     \
    case MPI_C_BOOL:                                                \
        OpCast<bool>::Op_;                                          \
        return;                                                     \


#define CASE_MPI_PRINTABLE_CHARS( Op_ )                             \
    case MPI_CHAR:                                                  \
    case MPI_CHARACTER:                                             \
        OpCast<char>::Op_;                                          \
        return;


#define CASE_MPI_LOCTYPES( Op_ )                                    \
    case MPI_2INT:                                                  \
    case MPI_2INTEGER:                                              \
        OpCast<loctype<int,int>>::Op_;                              \
        return;                                                     \
    case MPI_FLOAT_INT:                                             \
        OpCast<loctype<float,int>>::Op_;                            \
        return;                                                     \
    case MPI_LONG_INT:                                              \
        OpCast<loctype<long,int>>::Op_;                             \
        return;                                                     \
    case MPI_SHORT_INT:                                             \
        OpCast<loctype<short,int>>::Op_;                            \
        return;                                                     \
    case MPI_DOUBLE_INT:                                            \
        OpCast<loctype<double,int>>::Op_;                           \
        return;                                                     \
    case MPI_LONG_DOUBLE_INT:                                       \
        OpCast<loctype<long double,int>>::Op_;                      \
        return;                                                     \
    case MPI_2REAL:                                                 \
        OpCast<loctype<float,float>>::Op_;                          \
        return;                                                     \
    case MPI_2DOUBLE_PRECISION:                                      \
        OpCast<loctype<double,double>>::Op_;                        \
        return;                                                     \


//
// The default Op proxy for C, just forward the call (and cast away the constness)
//
void MPIAPI MPIR_Op_c_proxy(MPI_User_function* uop, const void* a, void* b, const int* len, const MPI_Datatype* datatype)
{
    uop( const_cast<void*>(a), b, const_cast<int*>(len), const_cast<MPI_Datatype*>(datatype) );
}



#ifndef MPID_OP_PREALLOC
#define MPID_OP_PREALLOC 16
#endif

C_ASSERT( HANDLE_GET_TYPE(MPI_OP_NULL) == HANDLE_TYPE_INVALID );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_OP_NULL) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_MAX) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_MAX) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_MIN) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_MIN) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_SUM) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_SUM) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_PROD) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_PROD) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_LAND) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_LAND) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_BAND) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_BAND) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_LOR) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_LOR) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_BOR) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_BOR) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_LXOR) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_LXOR) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_BXOR) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_BXOR) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_MINLOC) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_MINLOC) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_MAXLOC) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_MAXLOC) == MPID_OP );

C_ASSERT( HANDLE_GET_TYPE(MPI_REPLACE) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_REPLACE) == MPID_OP );

C_ASSERT(HANDLE_GET_TYPE(MPI_NO_OP) == HANDLE_TYPE_BUILTIN);
C_ASSERT(HANDLE_GET_MPI_KIND(MPI_NO_OP) == MPID_OP);

OpPool OpPool::s_instance;


//OACR_WARNING_DISABLE(25120, "OACR does not properly analyze function pointers.");
MPI_User_function MPIR_Op_max;
MPI_User_function MPIR_Op_min;
MPI_User_function MPIR_Op_sum;
MPI_User_function MPIR_Op_prod;
MPI_User_function MPIR_Op_land;
MPI_User_function MPIR_Op_band;
MPI_User_function MPIR_Op_lor;
MPI_User_function MPIR_Op_bor;
MPI_User_function MPIR_Op_lxor;
MPI_User_function MPIR_Op_bxor;
MPI_User_function MPIR_Op_maxloc;
MPI_User_function MPIR_Op_minloc;
MPI_User_function MPIR_Op_replace;
MPI_User_function MPIR_Op_noop;
//OACR_WARNING_ENABLE(25120, "Turning on for remainder of code.");

MPI_User_function *MPIR_Op_table[] = { MPIR_Op_max, MPIR_Op_min, MPIR_Op_sum,
                                       MPIR_Op_prod, MPIR_Op_land,
                                       MPIR_Op_band, MPIR_Op_lor, MPIR_Op_bor,
                                       MPIR_Op_lxor, MPIR_Op_bxor,
                                       MPIR_Op_minloc, MPIR_Op_maxloc, };

struct OpInfo
{
    MPI_User_function* func;
    MPI_Op handle;
};


static const OpInfo opInfo[]
{
    { nullptr, HANDLE_SET_TYPE(MPI_OP_NULL, HANDLE_TYPE_BUILTIN) },
    { MPIR_Op_max, MPI_MAX },
    { MPIR_Op_min, MPI_MIN },
    { MPIR_Op_sum, MPI_SUM },
    { MPIR_Op_prod, MPI_PROD },
    { MPIR_Op_land, MPI_LAND },
    { MPIR_Op_band, MPI_BAND },
    { MPIR_Op_lor, MPI_LOR },
    { MPIR_Op_bor, MPI_BOR },
    { MPIR_Op_lxor, MPI_LXOR },
    { MPIR_Op_bxor, MPI_BXOR },
    { MPIR_Op_minloc, MPI_MINLOC },
    { MPIR_Op_maxloc, MPI_MAXLOC },
    { MPIR_Op_replace, MPI_REPLACE },
    { MPIR_Op_noop, MPI_NO_OP }
};

C_ASSERT( _countof( opInfo ) == OpTraits::BUILTIN );


MPIR_Op_check_dtype_fn MPIR_Op_max_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_min_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_sum_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_prod_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_land_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_band_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_lor_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_bor_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_lxor_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_bxor_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_maxloc_check_dtype;
MPIR_Op_check_dtype_fn MPIR_Op_minloc_check_dtype;

MPIR_Op_check_dtype_fn *MPIR_Op_check_dtype_table[] = {
    MPIR_Op_max_check_dtype, MPIR_Op_min_check_dtype,
    MPIR_Op_sum_check_dtype,
    MPIR_Op_prod_check_dtype, MPIR_Op_land_check_dtype,
    MPIR_Op_band_check_dtype, MPIR_Op_lor_check_dtype, MPIR_Op_bor_check_dtype,
    MPIR_Op_lxor_check_dtype, MPIR_Op_bxor_check_dtype,
    MPIR_Op_minloc_check_dtype, MPIR_Op_maxloc_check_dtype, };


void MPID_Op::GlobalInit( void )
{
    //
    // Special initialization for MPI_OP_NULL.
    //
    for( unsigned i = 0; i < _countof(opInfo); i++ )
    {
        MPID_Op* pOp = OpPool::Get( opInfo[i].handle );

        pOp->handle = opInfo[i].handle;
        pOp->ref_count = 1;
        pOp->kind = static_cast<MPID_Op_kind>(i);
        pOp->user_function = opInfo[i].func;
        pOp->proxy = MPIR_Op_c_proxy;
    }
}


void MPID_Op::Destroy( void )
{
    OpPool::Free( this );
}

/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran integer,
 * and byte data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_band (
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );

    switch (*type)
    {
    CASE_MPI_C_INTS( BitwiseAnd( invec, inoutvec, *Len ) );
    // Fortran integer not in standard, but makes sense.
    CASE_MPI_F_INTS( BitwiseAnd( invec, inoutvec, *Len ) );

    case MPI_BYTE:
        OpCast<BYTE>::BitwiseAnd( invec, inoutvec, *Len );
        return;

#ifndef USE_STRICT_MPI
    CASE_MPI_PRINTABLE_CHARS( BitwiseAnd( invec, inoutvec, *Len ) );
    case MPI_LOGICAL:
        OpCast<MPI_Fint>::BitwiseAnd( invec, inoutvec, *Len );
        return;
#endif  // USE_STRICT_MPI

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_BAND" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_band_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif  // MPIR_INTEGER16_CTYPE
    //
    // Byte:
    //
    case MPI_BYTE:
#ifndef USE_STRICT_MPI
    //
    // Non-standard:
    //
    case MPI_LOGICAL:
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif  // USE_STRICT_MPI
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_BAND" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran integer,
 * and byte data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_bor (
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );
    switch (*type)
    {
    CASE_MPI_C_INTS( BitwiseOr( invec, inoutvec, *Len ) );
    // Fortran integer not in standard, but makes sense.
    CASE_MPI_F_INTS( BitwiseOr( invec, inoutvec, *Len ) );

    case MPI_BYTE:
        OpCast<BYTE>::BitwiseOr( invec, inoutvec, *Len );
        return;

#ifndef USE_STRICT_MPI
    CASE_MPI_PRINTABLE_CHARS( BitwiseOr( invec, inoutvec, *Len ) );
    case MPI_LOGICAL:
        OpCast<MPI_Fint>::BitwiseOr( invec, inoutvec, *Len );
        return;
#endif  // USE_STRICT_MPI

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_BOR" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_bor_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif  // MPIR_INTEGER16_CTYPE
    //
    // Byte:
    //
    case MPI_BYTE:
#ifndef USE_STRICT_MPI
    //
    // Non-standard:
    //
    case MPI_LOGICAL:
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif  // USE_STRICT_MPI
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_BOR" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran integer,
 * and byte data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_bxor (
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );

    switch (*type)
    {
    CASE_MPI_C_INTS( BitwiseXor( invec, inoutvec, *Len ) );
    // Fortran integer not in standard, but makes sense.
    CASE_MPI_F_INTS( BitwiseXor( invec, inoutvec, *Len ) );

    case MPI_BYTE:
        OpCast<BYTE>::BitwiseXor( invec, inoutvec, *Len );
        return;

#ifndef USE_STRICT_MPI
    CASE_MPI_PRINTABLE_CHARS( BitwiseXor( invec, inoutvec, *Len ) );
    case MPI_LOGICAL:
        OpCast<MPI_Fint>::BitwiseXor( invec, inoutvec, *Len );
        return;
#endif  // USE_STRICT_MPI

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_BXOR" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_bxor_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif  // MPIR_INTEGER16_CTYPE
    //
    // Byte:
    //
    case MPI_BYTE:
#ifndef USE_STRICT_MPI
    //
    // Non-standard:
    //
    case MPI_LOGICAL:
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif  // USE_STRICT_MPI
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_BXOR" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran logical
 * data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_land(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );

    switch (*type)
    {
    CASE_MPI_C_INTS( LogicalAnd( invec, inoutvec, *Len ) );
    CASE_MPI_LOGICALS( LogicalAnd( invec, inoutvec, *Len ) );

#ifndef USE_STRICT_MPI
    CASE_MPI_F_INTS( LogicalAnd( invec, inoutvec, *Len ) );
    CASE_MPI_FLOATS( LogicalAnd( invec, inoutvec, *Len ) );
    CASE_MPI_PRINTABLE_CHARS( LogicalAnd( invec, inoutvec, *Len ) );
#endif  // USE_STRICT_MPI

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_LAND" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_land_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
#ifndef USE_STRICT_MPI
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif
#endif
    //
    // Logical:
    //
    case MPI_LOGICAL:
    case MPI_C_BOOL:
#ifndef USE_STRICT_MPI
    //
    // Floating point:
    //
    case MPI_FLOAT:
    case MPI_DOUBLE:
    case MPI_REAL:
    case MPI_DOUBLE_PRECISION:
    case MPI_LONG_DOUBLE:
#ifdef MPIR_REAL2_CTYPE
    case MPI_REAL2:
#endif
    case MPI_REAL4:
    case MPI_REAL8:
#ifdef MPIR_REAL16_CTYPE
    case MPI_REAL16:
#endif
    //
    // Non-standard:
    //
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_LAND" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran logical
 * data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_lor(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );

    switch (*type)
    {
    CASE_MPI_C_INTS( LogicalOr( invec, inoutvec, *Len ) );
    CASE_MPI_LOGICALS( LogicalOr( invec, inoutvec, *Len ) );

#ifndef USE_STRICT_MPI
    CASE_MPI_F_INTS( LogicalOr( invec, inoutvec, *Len ) );
    CASE_MPI_FLOATS( LogicalOr( invec, inoutvec, *Len ) );
    CASE_MPI_PRINTABLE_CHARS( LogicalOr(invec, inoutvec, *Len ) );
#endif  // USE_STRICT_MPI

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_LOR" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_lor_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
#ifndef USE_STRICT_MPI
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif
#endif
    //
    // Logical:
    //
    case MPI_LOGICAL:
    case MPI_C_BOOL:
#ifndef USE_STRICT_MPI
    //
    // Floating point:
    //
    case MPI_FLOAT:
    case MPI_DOUBLE:
    case MPI_REAL:
    case MPI_DOUBLE_PRECISION:
    case MPI_LONG_DOUBLE:
#ifdef MPIR_REAL2_CTYPE
    case MPI_REAL2:
#endif
    case MPI_REAL4:
    case MPI_REAL8:
#ifdef MPIR_REAL16_CTYPE
    case MPI_REAL16:
#endif
    //
    // Non-standard:
    //
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_LOR" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran logical
 * data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_lxor(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );

    switch (*type)
    {
    CASE_MPI_C_INTS( LogicalXor( invec, inoutvec, *Len ) );
    CASE_MPI_LOGICALS( LogicalXor( invec, inoutvec, *Len ) );

#ifndef USE_STRICT_MPI
    CASE_MPI_F_INTS( LogicalXor( invec, inoutvec, *Len ) );
    CASE_MPI_FLOATS( LogicalXor( invec, inoutvec, *Len ) );
    CASE_MPI_PRINTABLE_CHARS( LogicalXor( invec, inoutvec, *Len ) );
#endif  // USE_STRICT_MPI

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_LXOR" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_lxor_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
#ifndef USE_STRICT_MPI
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif
#endif
    //
    // Logical:
    //
    case MPI_LOGICAL:
    case MPI_C_BOOL:
#ifndef USE_STRICT_MPI
    //
    // Floating point:
    //
    case MPI_FLOAT:
    case MPI_DOUBLE:
    case MPI_REAL:
    case MPI_DOUBLE_PRECISION:
    case MPI_LONG_DOUBLE:
#ifdef MPIR_REAL2_CTYPE
    case MPI_REAL2:
#endif
    case MPI_REAL4:
    case MPI_REAL8:
#ifdef MPIR_REAL16_CTYPE
    case MPI_REAL16:
#endif
    //
    // Non-standard:
    //
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_LXOR" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran integer,
 * and floating point data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_max(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );

    switch (*type)
    {
    CASE_MPI_C_INTS( Max(invec, inoutvec, *Len) );
    CASE_MPI_F_INTS( Max(invec, inoutvec, *Len) );
    CASE_MPI_FLOATS( Max(invec, inoutvec, *Len) );
#ifndef USE_STRICT_MPI
    CASE_MPI_PRINTABLE_CHARS( Max(invec, inoutvec, *Len) );
#endif

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MAX" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_max_check_dtype( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif
    //
    // Floating point:
    //
    case MPI_FLOAT:
    case MPI_DOUBLE:
    case MPI_REAL:
    case MPI_DOUBLE_PRECISION:
    case MPI_LONG_DOUBLE:
#ifdef MPIR_REAL2_CTYPE
    case MPI_REAL2:
#endif
    case MPI_REAL4:
    case MPI_REAL8:
#ifdef MPIR_REAL16_CTYPE
    case MPI_REAL16:
#endif
#ifndef USE_STRICT_MPI
    //
    // Non-standard:
    //
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MAX" );
    }
}


void MPIAPI MPIR_Op_maxloc(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );

    switch (*type)
    {
    CASE_MPI_LOCTYPES( MaxLoc( invec, inoutvec, *Len ) );

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MAXLOC" );
        break;
    }
    }

}


MPI_RESULT MPIR_Op_maxloc_check_dtype( MPI_Datatype type )
{
    switch (type)
    {
    /* first the C types */
    case MPI_2INT:
    case MPI_FLOAT_INT:
    case MPI_LONG_INT:
    case MPI_SHORT_INT:
    case MPI_DOUBLE_INT:
    case MPI_LONG_DOUBLE_INT:
    /* now the Fortran types */
    case MPI_2INTEGER:
    case MPI_2REAL:
    case MPI_2DOUBLE_PRECISION:
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MAXLOC" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran integer,
 * and floating point data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_min(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );
    switch (*type)
    {
    CASE_MPI_C_INTS( Min(invec, inoutvec, *Len) );
    CASE_MPI_F_INTS( Min(invec, inoutvec, *Len) );
    CASE_MPI_FLOATS( Min(invec, inoutvec, *Len) );
#ifndef USE_STRICT_MPI
    CASE_MPI_PRINTABLE_CHARS( Min(invec, inoutvec, *Len) );
#endif

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MIN" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_min_check_dtype( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif
    //
    // Floating point:
    //
    case MPI_FLOAT:
    case MPI_DOUBLE:
    case MPI_REAL:
    case MPI_DOUBLE_PRECISION:
    case MPI_LONG_DOUBLE:
#ifdef MPIR_REAL2_CTYPE
    case MPI_REAL2:
#endif
    case MPI_REAL4:
    case MPI_REAL8:
#ifdef MPIR_REAL16_CTYPE
    case MPI_REAL16:
#endif
#ifndef USE_STRICT_MPI
    //
    // Non-standard:
    //
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MIN" );
    }
}


void MPIAPI MPIR_Op_minloc(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );
    switch (*type)
    {
    CASE_MPI_LOCTYPES( MinLoc( invec, inoutvec, *Len ) );

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MINLOC" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_minloc_check_dtype( MPI_Datatype type )
{
    switch (type)
    {
    /* first the C types */
    case MPI_2INT:
    case MPI_FLOAT_INT:
    case MPI_LONG_INT:
    case MPI_SHORT_INT:
    case MPI_DOUBLE_INT:
    case MPI_LONG_DOUBLE_INT:
   /* now the Fortran types */
    case MPI_2INTEGER:
    case MPI_2REAL:
    case MPI_2DOUBLE_PRECISION:
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_MINLOC" );
    }
}


/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran integer,
 * floating point, and complex data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_prod(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );
    switch (*type)
    {
    CASE_MPI_C_INTS( Prod( invec, inoutvec, *Len ) );
    CASE_MPI_F_INTS( Prod( invec, inoutvec, *Len ) );
    CASE_MPI_FLOATS( Prod( invec, inoutvec, *Len ) );
    CASE_MPI_COMPLEXES( Prod( invec, inoutvec, *Len ) );
#ifndef USE_STRICT_MPI
    CASE_MPI_PRINTABLE_CHARS( Prod( invec, inoutvec, *Len ) );
#endif

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_PROD" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_prod_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif
    //
    // Floating point:
    //
    case MPI_FLOAT:
    case MPI_DOUBLE:
    case MPI_REAL:
    case MPI_DOUBLE_PRECISION:
    case MPI_LONG_DOUBLE:
#ifdef MPIR_REAL2_CTYPE
    case MPI_REAL2:
#endif
    case MPI_REAL4:
    case MPI_REAL8:
#ifdef MPIR_REAL16_CTYPE
    case MPI_REAL16:
#endif
    //
    // Complex:
    //
    case MPI_COMPLEX:
    case MPI_C_COMPLEX:
    case MPI_C_FLOAT_COMPLEX:
    case MPI_C_DOUBLE_COMPLEX:
    case MPI_C_LONG_DOUBLE_COMPLEX:
    case MPI_DOUBLE_COMPLEX:
#ifdef MPIR_REAL2_CTYPE
    case MPI_COMPLEX4:
#endif
    case MPI_COMPLEX8:
    case MPI_COMPLEX16:
#ifdef MPIR_REAL16_CTYPE
    case MPI_COMPLEX32:
#endif
#ifndef USE_STRICT_MPI
    //
    // Non-standard extension
    //
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_PROD" );
    }
}

/*
 * In MPI-2.2, this operation is valid only for C integer, Fortran integer,
 * floating point, and complex data items (5.9.2 Predefined Reduction Operations)
 */

void MPIAPI MPIR_Op_sum(
    void *invec,
    void *inoutvec,
    int *Len,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR( invec );
    OACR_USE_PTR( Len );
    OACR_USE_PTR( type );
    switch (*type)
    {
    CASE_MPI_C_INTS( Sum( invec, inoutvec, *Len ) );
    CASE_MPI_F_INTS( Sum( invec, inoutvec, *Len ) );
    CASE_MPI_FLOATS( Sum( invec, inoutvec, *Len ) );
    CASE_MPI_COMPLEXES( Sum( invec, inoutvec, *Len ) );
#ifndef USE_STRICT_MPI
    CASE_MPI_PRINTABLE_CHARS( Sum( invec, inoutvec, *Len ) );
#endif

    default:
    {
        Mpi.CallState->op_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_SUM" );
        break;
    }
    }
}


MPI_RESULT MPIR_Op_sum_check_dtype ( MPI_Datatype type )
{
    switch (type)
    {
    //
    // In MPI Standard order (see 5.9.2, Predefined Reduction Operations)
    //
    // C integer:
    //
    case MPI_INT:
    case MPI_LONG:
    case MPI_SHORT:
    case MPI_UNSIGNED_SHORT:
    case MPI_UNSIGNED:
    case MPI_UNSIGNED_LONG:
    case MPI_LONG_LONG:
    case MPI_UNSIGNED_LONG_LONG:
    case MPI_SIGNED_CHAR:
    case MPI_UNSIGNED_CHAR:
    case MPI_INT8_T:
    case MPI_INT16_T:
    case MPI_INT32_T:
    case MPI_INT64_T:
    case MPI_UINT8_T:
    case MPI_UINT16_T:
    case MPI_UINT32_T:
    case MPI_UINT64_T:
    //
    // Fortran integer:
    //
    case MPI_INTEGER:
    case MPI_AINT:
    case MPI_OFFSET:
/* The length type can be provided without Fortran, so we do so */
    case MPI_INTEGER1:
    case MPI_INTEGER2:
    case MPI_INTEGER4:
    case MPI_INTEGER8:
#ifdef MPIR_INTEGER16_CTYPE
    case MPI_INTEGER16:
#endif
    //
    // Floating point:
    //
    case MPI_FLOAT:
    case MPI_DOUBLE:
    case MPI_REAL:
    case MPI_DOUBLE_PRECISION:
    case MPI_LONG_DOUBLE:
#ifdef MPIR_REAL2_CTYPE
    case MPI_REAL2:
#endif
    case MPI_REAL4:
    case MPI_REAL8:
#ifdef MPIR_REAL16_CTYPE
    case MPI_REAL16:
#endif
    //
    // Complex:
    //
    case MPI_COMPLEX:
    case MPI_C_COMPLEX:
    case MPI_C_FLOAT_COMPLEX:
    case MPI_C_DOUBLE_COMPLEX:
    case MPI_C_LONG_DOUBLE_COMPLEX:
    case MPI_DOUBLE_COMPLEX:
#ifdef MPIR_REAL2_CTYPE
    case MPI_COMPLEX4:
#endif
    case MPI_COMPLEX8:
    case MPI_COMPLEX16:
#ifdef MPIR_REAL16_CTYPE
    case MPI_COMPLEX32:
#endif
#ifndef USE_STRICT_MPI
    //
    // Non-standard extension
    //
    case MPI_CHAR:
    case MPI_CHARACTER:
#endif
        return MPI_SUCCESS;
    default:
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opundefined %s", "MPI_SUM" );
    }
}


void MPIAPI MPIR_Op_replace(
    void *invec,
    void *inoutvec,
    int *count,
    MPI_Datatype *type
    )
{
    //
    // The only errors that can occur are length or type mismatch, but these are the same
    // for both buffers.  We still need to check the return value, though, or PreFast will
    // complain.
    //
    TypeHandle hType = TypeHandle::Create( *type );
    MPI_RESULT mpi_errno = MPIR_Localcopy(
        invec,
        *count,
        hType,
        inoutvec,
        *count,
        hType
        );
    UNREFERENCED_PARAMETER( mpi_errno );
    MPIU_Assert( mpi_errno == MPI_SUCCESS );
}


void MPIAPI MPIR_Op_noop(
    void *invec,
    void *inoutvec,
    int *count,
    MPI_Datatype *type
    )
{
    OACR_USE_PTR(invec);
    OACR_USE_PTR(inoutvec);
    OACR_USE_PTR(count);
    OACR_USE_PTR(type);
}

