// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* This is the utility file for datatypes that contains the basic datatype
   items and storage management.  It also contains a temporary routine
   that is used by ROMIO to test to see if datatypes are contiguous */


typedef struct mpi_names_t
{
    MPI_Datatype dtype;
    const char *name;

} mpi_names_t;


/* The MPI standard specifies that the names must be the MPI names,
   not the related language names (e.g., MPI_CHAR, not char) */
#define _N(x) { x, #x }

static const mpi_names_t mpi_names[] =
{
    {MPID_DATATYPE_NULL, ""},
    _N(MPI_CHAR),
    _N(MPI_UNSIGNED_CHAR),
    _N(MPI_SHORT),
    _N(MPI_UNSIGNED_SHORT),
    _N(MPI_INT),
    _N(MPI_UNSIGNED),
    _N(MPI_LONG),
    _N(MPI_UNSIGNED_LONG),
/*  _N(MPI_LONG_LONG_INT), allowed as an alias; we don't make it a separate type */
    _N(MPI_LONG_LONG),
    _N(MPI_FLOAT),
    _N(MPI_DOUBLE),
    _N(MPI_LONG_DOUBLE),
    _N(MPI_BYTE),
    _N(MPI_WCHAR),

    _N(MPI_PACKED),
    _N(MPI_LB),
    _N(MPI_UB),

    _N(MPI_C_COMPLEX),
    _N(MPI_C_FLOAT_COMPLEX),
    _N(MPI_C_DOUBLE_COMPLEX),
    _N(MPI_C_LONG_DOUBLE_COMPLEX),

    /* The maxloc/minloc types are not builtin because their size and
       extent may not be the same. (except for MPI_2INT) */
    _N(MPI_2INT),
    _N(MPI_C_BOOL),
    _N(MPI_SIGNED_CHAR),
    _N(MPI_UNSIGNED_LONG_LONG),

    /* Fortran */
    _N(MPI_CHARACTER),
    _N(MPI_INTEGER),
    _N(MPI_REAL),
    _N(MPI_LOGICAL),
    _N(MPI_COMPLEX),
    _N(MPI_DOUBLE_PRECISION),
    _N(MPI_2INTEGER),
    _N(MPI_2REAL),
    _N(MPI_DOUBLE_COMPLEX),
    _N(MPI_2DOUBLE_PRECISION),
    _N(MPI_2COMPLEX),
    _N(MPI_2DOUBLE_COMPLEX),

#ifdef MPIR_REAL2_CTYPE
    _N(MPI_REAL2),      // 0x4c000226
#else
    {0, ""},
#endif

    /* Size-specific types (C, Fortran, and C++) */
    _N(MPI_REAL4),
    _N(MPI_COMPLEX8),
    _N(MPI_REAL8),
    _N(MPI_COMPLEX16),

#ifdef MPIR_REAL16_CTYPE
    _N(MPI_REAL16),     // 0x4c00102b
    _N(MPI_COMPLEX32),  // 0x4c00202c
#else
    {0, ""},
    {0, ""},
#endif
    _N(MPI_INTEGER1),

#ifdef MPIR_REAL2_CTYPE
    _N(MPI_COMPLEX4),   // 0x4c00042e
#else
    {0, ""},
#endif

    _N(MPI_INTEGER2),
    _N(MPI_INTEGER4),
    _N(MPI_INTEGER8),
#ifdef MPIR_INTEGER16_CTYPE
    _N(MPI_INTEGER16),  // 0x4c001032
#else
    {0, ""},
#endif

    _N(MPI_INT8_T),
    _N(MPI_INT16_T),
    _N(MPI_INT32_T),
    _N(MPI_INT64_T),
    _N(MPI_UINT8_T),
    _N(MPI_UINT16_T),
    _N(MPI_UINT32_T),
    _N(MPI_UINT64_T),
    _N(MPI_AINT),
    _N(MPI_OFFSET),
    _N(MPI_COUNT)
};

C_ASSERT( HANDLE_BUILTIN_INDEX(MPI_COUNT) == (DatatypeTraits::BUILTIN - 1));
C_ASSERT( HANDLE_BUILTIN_INDEX(MPI_COUNT) == (_countof(mpi_names) - 1) );
C_ASSERT( _countof(mpi_names) == DatatypeTraits::BUILTIN );


#ifndef MPID_DATATYPE_PREALLOC
#define MPID_DATATYPE_PREALLOC 8
#endif

static mpi_names_t mpi_pair_names[] =
{
    _N(MPI_FLOAT_INT),
    _N(MPI_DOUBLE_INT),
    _N(MPI_LONG_INT),
    _N(MPI_SHORT_INT),
    _N(MPI_LONG_DOUBLE_INT)
};

C_ASSERT( (DatatypeTraits::DIRECT) == _countof(mpi_pair_names) + MPID_DATATYPE_PREALLOC );

MPID_Datatype* f90types[MAX_F90_TYPES];


TypePool TypePool::s_instance;


BuiltinTypeHandles g_hBuiltinTypes;
TypeHandle MPID_FloatInt;
TypeHandle MPID_DoubleInt;
TypeHandle MPID_LongInt;
TypeHandle MPID_ShortInt;
TypeHandle MPID_LongDoubleInt;


/*
  MPID_Datatype::GlobalInit()

  Main purpose of this function is to set up the following pair types:
  - MPI_FLOAT_INT
  - MPI_DOUBLE_INT
  - MPI_LONG_INT
  - MPI_SHORT_INT
  - MPI_LONG_DOUBLE_INT

  The assertions in this code ensure that:
  - this function is called before other types are allocated
  - there are enough spaces in the direct block to hold our types
  - we actually get the values we expect (otherwise errors regarding
    these types could be terribly difficult to track down!)

 */

//TODO: TypePool destructor?
static int MPIR_Datatype_finalize(void* /*dummy*/)
{
    int i;

    for (i = 0; i < _countof(mpi_names); i++)
    {
        if( mpi_names[i].dtype == 0 )
        {
            //
            // Skip over dummy entries.
            //
            continue;
        }

        MPID_Datatype *dptr = TypePool::Get(mpi_names[i].dtype);

        CleanupName<MPID_Datatype>( dptr );
    }

    for (i = 0; i < _countof(mpi_pair_names); i++)
    {
        MPID_Datatype *dptr;
        dptr = TypePool::Get(mpi_pair_names[i].dtype);
        dptr->Release();
    }

    for( i = 0; i < MAX_F90_TYPES; i++ )
    {
        if( f90types[i] == nullptr )
        {
            break;
        }

        f90types[i]->Release();
        f90types[i] = nullptr;
    }

    return 0;
}


//TODO: TypePool constructor?
void MPID_Datatype::GlobalInit(void)
{
    int i;

    //
    // MPI_DATATYPE_NULL, the first entry in the builtin datatype array, is not
    // a valid builtin type (TypePool::Lookup fails), but TypePool::Get succeeds.
    // We must initialize it.
    //
    for (i = 0; i < _countof(mpi_names); i++)
    {
        if( mpi_names[i].dtype == 0 )
        {
            //
            // Skip over dummy entries.
            //
            g_hBuiltinTypes.Set( MPID_DATATYPE_NULL, TypePool::Get( MPID_DATATYPE_NULL ) );
            continue;
        }

        /* Compute the index from the value of the handle */
        MPI_Datatype d = mpi_names[i].dtype;

        MPID_Datatype* dptr = TypePool::Get(d);

        /* dptr points into MPID_Datatype_builtin */
        dptr->handle        = mpi_names[i].dtype;
        dptr->is_permanent  = true;
        dptr->is_committed  = true;
        dptr->is_contig     = true;
        dptr->max_contig_blocks = 1;
        dptr->size          = MPID_Datatype_get_basic_size(d);
        dptr->extent        = dptr->size;
        dptr->ub            = dptr->size;
        dptr->lb            = 0;
        dptr->true_ub       = dptr->size;
        dptr->true_lb       = 0;
        switch( d )
        {
        case MPI_2INTEGER:
        case MPI_2INT:
            dptr->alignsize = __alignof(int);
            break;

        case MPI_2REAL:
        case MPI_COMPLEX:
        case MPI_2COMPLEX:
        case MPI_COMPLEX8:
        case MPI_C_COMPLEX:
        case MPI_C_FLOAT_COMPLEX:
            dptr->alignsize = __alignof(float);
            break;

        case MPI_DOUBLE_COMPLEX:
        case MPI_2DOUBLE_PRECISION:
        case MPI_2DOUBLE_COMPLEX:
        case MPI_COMPLEX16:
        case MPI_C_DOUBLE_COMPLEX:
        case MPI_C_LONG_DOUBLE_COMPLEX:
            dptr->alignsize = __alignof(double);
            break;

        default:
            dptr->alignsize     = dptr->size;
        }
        dptr->has_sticky_ub = false;
        dptr->has_sticky_lb = false;
        dptr->eltype        = d;
        dptr->n_elements    = 1;
        dptr->element_size  = dptr->size;
        dptr->contents      = NULL; /* should never get referenced? */

        dptr->dataloop       = NULL;
        dptr->dataloop_size  = -1;
        dptr->dataloop_depth = -1;
        dptr->hetero_dloop   = NULL;
        dptr->hetero_dloop_size  = -1;
        dptr->hetero_dloop_depth = -1;

        dptr->attributes    = NULL;
        dptr->name          = mpi_names[i].name;
        dptr->ref_count     = 1;

        g_hBuiltinTypes.Set( d, dptr );
    }

    for (i = 0; i < _countof(mpi_pair_names); i++)
    {
        MPID_Datatype* dptr = TypePool::Alloc();
        MPIU_Assert( dptr == TypePool::Get(mpi_pair_names[i].dtype) );

        // TODO: can we do in-place construction?
        MPID_Type_create_pairtype(mpi_pair_names[i].dtype, dptr);
    }

    for( i = 0; i < MAX_F90_TYPES; i++ )
    {
        //
        // The parameterized F90 types are demand-created.
        //
        f90types[i] = nullptr;
    }

    MPIR_Add_finalize(MPIR_Datatype_finalize, 0, MPIR_FINALIZE_CALLBACK_PRIO-1);
}


MPI_RESULT MPIR_Datatype_cleanup()
{
    int i;
    MPI_RESULT mpi_errno;

    for (i = 0; i < _countof(mpi_names); i++)
    {
        if( mpi_names[i].dtype == 0 )
        {
            //
            // Skip over dummy entries.
            //
            continue;
        }

        MPID_Datatype *dptr = TypePool::Get(mpi_names[i].dtype);

        mpi_errno = MPIR_Attr_delete_list( dptr->handle,
                                           &dptr->attributes );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    for (i = 0; i < _countof(mpi_pair_names); i++)
    {
        MPID_Datatype *dptr = TypePool::Get(mpi_pair_names[i].dtype);

        mpi_errno = MPIR_Attr_delete_list( dptr->handle,
                                           &dptr->attributes );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    for( i = 0; i < MAX_F90_TYPES; i++ )
    {
        if( f90types[i] == nullptr )
        {
            break;
        }

        mpi_errno = MPIR_Attr_delete_list( f90types[i]->handle,
            &f90types[i]->attributes );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    return MPI_SUCCESS;
}


/* This will eventually be removed once ROMIO knows more about MPICH2 */
// TODO: Make this return bool.
void MPIR_Datatype_iscontig(MPI_Datatype datatype, int *flag)
{
    *flag = TypePool::Get(datatype)->is_contig;
}


const char* MPID_Datatype::GetDefaultName() const
{
    if( MPID_Datatype_is_predefined( handle ) == false )
    {
        return mpi_names[0].name;
    }

    if( HANDLE_GET_TYPE(handle) != HANDLE_TYPE_BUILTIN )
    {
        return mpi_pair_names[HANDLE_DIRECT_INDEX(handle)].name;
    }

    return mpi_names[HANDLE_BUILTIN_INDEX(handle)].name;
}


MPI_RESULT
MPIR_Type_block(
    const int *array_of_gsizes,
    int dim,
    int ndims,
    int nprocs,
    int rank,
    int darg,
    int order,
    MPI_Aint orig_extent,
    MPI_Datatype type_old,
    _Outptr_ MPID_Datatype** ppNewType,
    MPI_Aint *st_offset )
{
/* nprocs = no. of processes in dimension dim of grid
   rank = coordinate of this process in dimension dim */
    MPI_RESULT mpi_errno;
    int blksize, global_size, mysize, i, j;
    MPI_Aint stride;
    MPID_Datatype* pNewType;

    global_size = array_of_gsizes[dim];

    if (darg == MPI_DISTRIBUTE_DFLT_DARG)
    {
        blksize = (global_size + nprocs - 1)/nprocs;
    }
    else
    {
        blksize = darg;

        if (blksize <= 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**darrayblock %d", blksize);
            return mpi_errno;
        }
        if (blksize * nprocs < global_size)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**darrayblock2 %d %d", blksize*nprocs, global_size);
            return mpi_errno;
        }
    }

    j = global_size - blksize*rank;
    mysize = min(blksize, j);
    if (mysize < 0) mysize = 0;

    stride = orig_extent;
    if (order == MPI_ORDER_FORTRAN)
    {
        if (dim == 0)
        {
            mpi_errno = MPID_Type_contiguous(mysize,
                                             type_old,
                                             &pNewType);
            ON_ERROR_FAIL(mpi_errno);
        }
        else
        {
            for (i=0; i<dim; i++) stride *= array_of_gsizes[i];
            mpi_errno = MPID_Type_vector(mysize,
                                         1,
                                         stride,
                                         true, /* stride in bytes */
                                         type_old,
                                         &pNewType);
            ON_ERROR_FAIL(mpi_errno);
        }
    }
    else
    {
        if (dim == ndims-1)
        {
            mpi_errno = MPID_Type_contiguous(mysize,
                                             type_old,
                                             &pNewType);
            ON_ERROR_FAIL(mpi_errno);
        }
        else
        {
            for (i=ndims-1; i>dim; i--) stride *= array_of_gsizes[i];
            mpi_errno = MPID_Type_vector(mysize,
                                         1,
                                         stride,
                                         true, /* stride in bytes */
                                         type_old,
                                         &pNewType);
            ON_ERROR_FAIL(mpi_errno);
        }
    }

    *st_offset = blksize * rank;
     /* in terms of no. of elements of type oldtype in this dimension */
    if (mysize == 0) *st_offset = 0;

    *ppNewType = pNewType;
    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}


MPI_RESULT MPIR_Type_cyclic(
    const int *array_of_gsizes,
    int dim,
    int ndims,
    int nprocs,
    int rank,
    int darg,
    int order,
    MPI_Aint orig_extent,
    MPI_Datatype type_old,
    _Outptr_ MPID_Datatype** ppNewType,
    MPI_Aint *st_offset )
{
/* nprocs = no. of processes in dimension dim of grid
   rank = coordinate of this process in dimension dim */
    MPI_RESULT mpi_errno;
    int blksize, i, blklens[3], st_index, end_index,
        local_size, rem, count;
    MPI_Aint stride, disps[3];
    MPI_Datatype types[3];

    if (darg == MPI_DISTRIBUTE_DFLT_DARG) blksize = 1;
    else blksize = darg;

    if (blksize <= 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**darraycyclic %d", blksize);
        return mpi_errno;
    }

    st_index = rank*blksize;
    end_index = array_of_gsizes[dim] - 1;

    if (end_index < st_index) local_size = 0;
    else
    {
        local_size = ((end_index - st_index + 1)/(nprocs*blksize))*blksize;
        rem = (end_index - st_index + 1) % (nprocs*blksize);
        local_size += min(rem, blksize);
    }

    count = local_size/blksize;
    rem = local_size % blksize;

    stride = nprocs*blksize*orig_extent;
    if (order == MPI_ORDER_FORTRAN)
        for (i=0; i<dim; i++) stride *= array_of_gsizes[i];
    else for (i=ndims-1; i>dim; i--) stride *= array_of_gsizes[i];

    MPID_Datatype* pTypeNew;
    mpi_errno = MPID_Type_vector(count,
                                 blksize,
                                 stride,
                                 true, /* stride in bytes */
                                 type_old,
                                 &pTypeNew);
    ON_ERROR_FAIL(mpi_errno);

    if (rem)
    {
        /* if the last block is of size less than blksize, include
           it separately using MPI_Type_create_struct */

        types[0] = pTypeNew->handle;
        types[1] = type_old;
        disps[0] = 0;
        disps[1] = count*stride;
        blklens[0] = 1;
        blklens[1] = rem;

        MPID_Datatype* pTypeTmp;
        mpi_errno = MPID_Type_struct(2,
                                     blklens,
                                     disps,
                                     types,
                                     &pTypeTmp);

        pTypeNew->Release();
        pTypeNew = pTypeTmp;

        ON_ERROR_FAIL(mpi_errno);
    }

    /* In the first iteration, we need to set the displacement in that
       dimension correctly. */
    if ( ((order == MPI_ORDER_FORTRAN) && (dim == 0)) ||
         ((order == MPI_ORDER_C) && (dim == ndims-1)) )
    {
        types[0] = MPI_LB;
        disps[0] = 0;
        types[1] = pTypeNew->handle;
        disps[1] = rank * blksize * orig_extent;
        types[2] = MPI_UB;
        disps[2] = orig_extent * array_of_gsizes[dim];
        blklens[0] = blklens[1] = blklens[2] = 1;

        MPID_Datatype* pTypeTmp;
        mpi_errno = MPID_Type_struct(3,
                                     blklens,
                                     disps,
                                     types,
                                     &pTypeTmp);

        pTypeNew->Release();
        pTypeNew = pTypeTmp;

        ON_ERROR_FAIL(mpi_errno);

        *st_offset = 0;  /* set it to 0 because it is taken care of in
                            the struct above */
    }
    else
    {
        *st_offset = rank * blksize;
        /* st_offset is in terms of no. of elements of type oldtype in
         * this dimension */
    }

    if (local_size == 0) *st_offset = 0;

    *ppNewType = pTypeNew;
    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}


/* MPIR_Type_get_predef_type_elements()
 *
 * Arguments:
 * - bytes_p - input/output byte count
 * - count - maximum number of this type to subtract from the bytes; a count
 *           of -1 indicates use as many as we like
 * - datatype - input datatype
 *
 * Returns number of elements available given the two constraints of number of
 * bytes and count of types.  Also reduces the byte count by the amount taken
 * up by the types.
 *
 * Assumptions:
 * - the type passed to this function must be predefined (i.e. basic or pairtype)
 * - the count is not zero (otherwise we can't tell between a "no more
 *   complete types" case and a "zero count" case)
 *
 * As per section 4.9.3 of the MPI 1.1 specification, the two-part reduction
 * types are to be treated as structs of the constituent types.  So we have to
 * do something special to handle them correctly in here.
 *
 * As per section 3.12.5 get_count and get_elements report the same value for
 * basic datatypes; I'm currently interpreting this to *not* include these
 * reduction types, as they are considered structs.
 */
MPI_Count
MPIR_Type_get_predef_type_elements(
    _Inout_ MPI_Count *bytes_p,
    _In_ int count,
    _In_ MPI_Datatype datatype
    )
{
    MPI_Count elements, usable_bytes, used_bytes;
    int type1_sz, type2_sz;

    if (count == 0) return 0;

    MPIU_Assert(MPID_Datatype_is_predefined(datatype));

    /* determine the maximum number of bytes we should take from the
     * byte count.  Use MPIR_Datatype_get_size_macro(), since
     * MPID_Datatype_get_basic_size() does not work for pairtypes.
     */
    if (count < 0)
    {
        usable_bytes = *bytes_p;
    }
    else
    {
        usable_bytes = min( *bytes_p, static_cast<MPI_Count>( count ) * MPID_Datatype_get_size( datatype ) );
    }

    switch (datatype)
    {
        /* we don't get valid fortran datatype handles in all cases... */
#ifdef HAVE_FORTRAN_BINDING
        case MPI_2REAL:
            type1_sz = type2_sz = MPID_Datatype_get_basic_size(MPI_REAL);
            break;
        case MPI_2DOUBLE_PRECISION:
            type1_sz = type2_sz =
                MPID_Datatype_get_basic_size(MPI_DOUBLE_PRECISION);
            break;
        case MPI_2INTEGER:
            type1_sz = type2_sz = MPID_Datatype_get_basic_size(MPI_INTEGER);
            break;
#endif
        case MPI_2INT:
            type1_sz = type2_sz = MPID_Datatype_get_basic_size(MPI_INT);
            break;
        case MPI_FLOAT_INT:
            type1_sz = MPID_Datatype_get_basic_size(MPI_FLOAT);
            type2_sz = MPID_Datatype_get_basic_size(MPI_INT);
            break;
        case MPI_DOUBLE_INT:
            type1_sz = MPID_Datatype_get_basic_size(MPI_DOUBLE);
            type2_sz = MPID_Datatype_get_basic_size(MPI_INT);
            break;
        case MPI_LONG_INT:
            type1_sz = MPID_Datatype_get_basic_size(MPI_LONG);
            type2_sz = MPID_Datatype_get_basic_size(MPI_INT);
            break;
        case MPI_SHORT_INT:
            type1_sz = MPID_Datatype_get_basic_size(MPI_SHORT);
            type2_sz = MPID_Datatype_get_basic_size(MPI_INT);
            break;
        case MPI_LONG_DOUBLE_INT:
            type1_sz = MPID_Datatype_get_basic_size(MPI_LONG_DOUBLE);
            type2_sz = MPID_Datatype_get_basic_size(MPI_INT);
            break;
        default:
            /* all other types.  this is more complicated than
             * necessary for handling these types, but it puts us in the
             * same code path for all the basics, so we stick with it.
             */
            type1_sz = type2_sz = MPID_Datatype_get_basic_size( datatype );
            break;
    }

    /* determine the number of elements in the region */
    elements = 2 * (usable_bytes / (type1_sz + type2_sz));
    if (usable_bytes % (type1_sz + type2_sz) >= type1_sz) elements++;

    /* determine how many bytes we used up with those elements */
    used_bytes = ((elements / 2) * (type1_sz + type2_sz));
    if (elements % 2 == 1) used_bytes += type1_sz;

    *bytes_p -= used_bytes;
    return elements;
}


/* MPIR_Type_get_elements
 *
 * Arguments:
 * - bytes_p - input/output byte count
 * - count - maximum number of this type to subtract from the bytes; a count
 *           of -1 indicates use as many as we like
 * - datatype - input datatype
 *
 * Returns number of elements available given the two constraints of number of
 * bytes and count of types.  Also reduces the byte count by the amount taken
 * up by the types.
 *
 * This is called from MPI_Get_elements().  This function may recurse.
 *
 * This function handles the following cases:
 * - nice, simple, single element type (predefined or derived)
 * - derived type with a zero size
 * - type with multiple element types (nastiest)
 */
MPI_Count
MPIR_Type_get_elements(
    _Inout_ MPI_Count *bytes_p,
    _In_ int count,
    _In_ MPI_Datatype datatype
    )
{
    if (MPID_Datatype_is_predefined(datatype))
    {
        return MPIR_Type_get_predef_type_elements(bytes_p, count, datatype);
    }

    const MPID_Datatype *datatype_ptr = TypePool::Get(datatype);

    if (datatype_ptr->size == 0)
    {
        /* This is ambiguous.  However, discussions on MPI Forum
         * reached a consensus that this is the correct return
         * value
         */
        return 0;
    }

    if( datatype_ptr->eltype != MPI_DATATYPE_NULL )
    {
        if (count >= 0)
        {
            count *= datatype_ptr->n_elements;
        }
        return MPIR_Type_get_predef_type_elements(bytes_p,
                                                   count,
                                                   datatype_ptr->eltype);
    }

    /* we have bytes left and still don't have a single element size; must
     * recurse.
     */
    int i, typecount = 0, *ints;
    MPI_Count nr_elements = 0, last_nr_elements;
    MPI_Aint *aints;
    MPI_Datatype *types;

    if (datatype_ptr->contents == NULL)
    {
        return MPI_ERR_TYPE;
    }

    MPID_Type_access_contents( datatype, &ints, &aints, &types );

    switch (datatype_ptr->contents->combiner)
    {
        case MPI_COMBINER_NAMED:
        case MPI_COMBINER_DUP:
        case MPI_COMBINER_RESIZED:
            return MPIR_Type_get_elements(bytes_p, count, types[0]);

        case MPI_COMBINER_CONTIGUOUS:
        case MPI_COMBINER_VECTOR:
        case MPI_COMBINER_HVECTOR_INTEGER:
        case MPI_COMBINER_HVECTOR:
            /* count is first in ints array */
            return MPIR_Type_get_elements( bytes_p, count * ints[0], types[0] );

        case MPI_COMBINER_INDEXED_BLOCK:
        case MPI_COMBINER_HINDEXED_BLOCK:
            /* count is first in ints array, blocklength is second */
            return MPIR_Type_get_elements(bytes_p,
                                          count * ints[0] * ints[1],
                                          types[0]);

        case MPI_COMBINER_INDEXED:
        case MPI_COMBINER_HINDEXED_INTEGER:
        case MPI_COMBINER_HINDEXED:
            for( i = 1; i <= ints[0]; i++ )
            {
                /* add up the blocklengths to get a max. # of the next type */
                typecount += ints[i];
            }
            return MPIR_Type_get_elements(bytes_p, count * typecount, types[0]);

        case MPI_COMBINER_STRUCT_INTEGER:
        case MPI_COMBINER_STRUCT:
            /* In this case we can't simply multiply the count of the next
             * type by the count of the current type, because we need to
             * cycle through the types just as the struct would.  thus the
             * nested loops.
             *
             * We need to keep going until we see a "0" elements returned.
             */

            for( unsigned int j = 0; (j < (unsigned int)count); j++ )
            {
                /* recurse on each type; bytes are reduced in calls */
                for (i=1; i <= ints[0]; i++)
                {
                    /* skip zero-count elements of the struct */
                    if (ints[i] == 0) continue;

                    last_nr_elements = MPIR_Type_get_elements(bytes_p,
                                                              ints[i],
                                                              types[i-1]);
                    if( last_nr_elements == 0 )
                    {
                        return nr_elements;
                    }

                    nr_elements += last_nr_elements;
                }
            }
            return nr_elements;

        case MPI_COMBINER_SUBARRAY:
        case MPI_COMBINER_DARRAY:
        case MPI_COMBINER_F90_REAL:
        case MPI_COMBINER_F90_COMPLEX:
        case MPI_COMBINER_F90_INTEGER:
        default:
            MPIU_Assert(0);
            return MPI_UNDEFINED;
    }
}


static int MPIDI_Type_blockindexed_count_contig(int count,
                                                int blklen,
                                                const void *disp_array,
                                                int dispinbytes,
                                                MPI_Aint old_extent)
{
    int i, contig_count = 1;

    if (!dispinbytes)
    {
        int cur_tdisp = ((int *) disp_array)[0];

        for (i=1; i < count; i++)
        {
            if (cur_tdisp + blklen != ((int *) disp_array)[i])
            {
                contig_count++;
            }
            cur_tdisp = ((int *) disp_array)[i];
        }
    }
    else
    {
        int cur_bdisp = (int)((MPI_Aint *) disp_array)[0];

        for (i=1; i < count; i++)
        {
            if (cur_bdisp + blklen * old_extent !=
                ((MPI_Aint *) disp_array)[i])
            {
                contig_count++;
            }
            cur_bdisp = (int)((MPI_Aint *) disp_array)[i];
        }
    }
    return contig_count;
}

/*@
  MPID_Type_blockindexed - create a block indexed datatype

  Input Parameters:
+ count - number of blocks in type
. blocklength - number of elements in each block
. displacement_array - offsets of blocks from start of type (see next
  parameter for units)
. dispinbytes - if nonzero, then displacements are in bytes, otherwise
  they in terms of extent of oldtype
- oldtype - type (using handle) of datatype on which new type is based

  Output Parameters:
. newtype - handle of new block indexed datatype

  Return Value:
  MPI_SUCCESS on success, MPI error on failure.
@*/

MPI_RESULT
MPID_Type_blockindexed(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _In_ const void *displacement_array,
    _In_ bool dispinbytes,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    )
{
    int i;
    bool is_builtin;
    int contig_count;
    bool old_is_contig;
    MPI_Aint el_sz;
    MPI_Datatype el_type;
    MPI_Aint old_lb, old_ub, old_extent, old_true_lb, old_true_ub;
    MPI_Aint min_lb = 0, max_ub = 0, eff_disp;

    MPID_Datatype *new_dtp;

    if (count == 0)
    {
        return MPID_Type_zerolen(ppNewType);
    }

    /* allocate new datatype object and handle */
    new_dtp = TypePool::Alloc();
    if( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_dtp->is_permanent = false;
    new_dtp->is_committed = false;
    new_dtp->attributes   = NULL;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->contents     = NULL;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = -1;
    new_dtp->dataloop_depth = -1;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = -1;
    new_dtp->hetero_dloop_depth = -1;

    is_builtin = (HANDLE_GET_TYPE(oldtype) == HANDLE_TYPE_BUILTIN);

    if (is_builtin)
    {
        el_sz   = MPID_Datatype_get_basic_size(oldtype);
        el_type = oldtype;

        old_lb        = 0;
        old_true_lb   = 0;
        old_ub        = el_sz;
        old_true_ub   = el_sz;
        old_extent    = el_sz;
        old_is_contig = true;

        new_dtp->size          = (int)(count * blocklength * el_sz);
        new_dtp->has_sticky_lb = false;
        new_dtp->has_sticky_ub = false;

        new_dtp->alignsize    = (int)el_sz; /* ??? */
        new_dtp->n_elements   = count * blocklength;
        new_dtp->element_size = el_sz;
        new_dtp->eltype       = el_type;

        new_dtp->max_contig_blocks = count;
    }
    else
    {
        /* user-defined base type (oldtype) */
        const MPID_Datatype *old_dtp;

        old_dtp = TypePool::Get( oldtype );
        el_type = old_dtp->eltype;

        old_lb        = old_dtp->lb;
        old_true_lb   = old_dtp->true_lb;
        old_ub        = old_dtp->ub;
        old_true_ub   = old_dtp->true_ub;
        old_extent    = old_dtp->extent;
        old_is_contig = old_dtp->is_contig;

        new_dtp->size           = count * blocklength * old_dtp->size;
        new_dtp->has_sticky_lb  = old_dtp->has_sticky_lb;
        new_dtp->has_sticky_ub  = old_dtp->has_sticky_ub;

        new_dtp->alignsize    = old_dtp->alignsize;
        new_dtp->n_elements   = count * blocklength * old_dtp->n_elements;
        new_dtp->element_size = old_dtp->element_size;
        new_dtp->eltype       = el_type;

        new_dtp->max_contig_blocks = old_dtp->max_contig_blocks * count * blocklength;
    }

    /* priming for loop */
    eff_disp = (dispinbytes) ? ((MPI_Aint *) displacement_array)[0] :
        (((MPI_Aint) ((int *) displacement_array)[0]) * old_extent);
    MPID_DATATYPE_BLOCK_LB_UB((MPI_Aint) blocklength,
                              eff_disp,
                              old_lb,
                              old_ub,
                              old_extent,
                              min_lb,
                              max_ub);

    /* determine new min lb and max ub */
    for (i=1; i < count; i++)
    {
        MPI_Aint tmp_lb, tmp_ub;

        eff_disp = (dispinbytes) ? ((MPI_Aint *) displacement_array)[i] :
            (((MPI_Aint) ((int *) displacement_array)[i]) * old_extent);
        MPID_DATATYPE_BLOCK_LB_UB((MPI_Aint) blocklength,
                                  eff_disp,
                                  old_lb,
                                  old_ub,
                                  old_extent,
                                  tmp_lb,
                                  tmp_ub);

        if (tmp_lb < min_lb) min_lb = tmp_lb;
        if (tmp_ub > max_ub) max_ub = tmp_ub;
    }

    new_dtp->lb      = min_lb;
    new_dtp->ub      = max_ub;
    new_dtp->true_lb = min_lb + (old_true_lb - old_lb);
    new_dtp->true_ub = max_ub + (old_true_ub - old_ub);
    new_dtp->extent  = max_ub - min_lb;

    /* new type is contig for N types if it is all one big block,
     * its size and extent are the same, and the old type was also
     * contiguous.
     */
    new_dtp->is_contig = false;
    if (old_is_contig)
    {
        contig_count = MPIDI_Type_blockindexed_count_contig(count,
                                                            blocklength,
                                                            displacement_array,
                                                            dispinbytes,
                                                            old_extent);
        new_dtp->max_contig_blocks = contig_count;
        if( (contig_count == 1) &&
            (new_dtp->size == new_dtp->extent) )
        {
            new_dtp->is_contig = true;
        }
    }

    *ppNewType = new_dtp;

    return MPI_SUCCESS;
}


/*@
  MPID_Type_commit

  Input Parameters:
. datatype_ptr - pointer to MPID_datatype

  Output Parameters:

  Return Value:
    MPI error value.
@*/
MPI_RESULT MPID_Type_commit(MPID_Datatype *datatype_ptr)
{
    MPIU_Assert(HANDLE_GET_TYPE(datatype_ptr->handle) != HANDLE_TYPE_BUILTIN);

    if (datatype_ptr->is_committed == 1)
    {
        return MPI_SUCCESS;
    }

    MPI_RESULT mpi_errno = MPID_Dataloop_create(datatype_ptr->handle,
                                                &datatype_ptr->dataloop,
                                                &datatype_ptr->dataloop_size,
                                                &datatype_ptr->dataloop_depth,
                                                DLOOP_DATALOOP_HOMOGENEOUS);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    /* create heterogeneous dataloop */
    mpi_errno = MPID_Dataloop_create(datatype_ptr->handle,
                            &datatype_ptr->hetero_dloop,
                            &datatype_ptr->hetero_dloop_size,
                            &datatype_ptr->hetero_dloop_depth,
                            DLOOP_DATALOOP_HETEROGENEOUS);
    if( mpi_errno == MPI_SUCCESS )
    {
        datatype_ptr->is_committed = 1;
    }

    return mpi_errno;
}


/*@
  MPID_Type_contiguous - create a contiguous datatype

  Input Parameters:
+ count - number of elements in the contiguous block
- oldtype - type (using handle) of datatype on which vector is based

  Output Parameters:
. newtype - handle of new contiguous datatype

  Return Value:
  MPI_SUCCESS on success, MPI error code on failure.
@*/
MPI_RESULT
MPID_Type_contiguous(
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    )
{
    bool is_builtin;
    int el_sz;
    MPI_Datatype el_type;
    MPID_Datatype *new_dtp;

    if( count == 0 )
    {
        return MPID_Type_zerolen( ppNewType );
    }

    /* allocate new datatype object and handle */
    new_dtp = TypePool::Alloc();
    if( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_dtp->is_permanent = false;
    new_dtp->is_committed = false;
    new_dtp->attributes   = NULL;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->contents     = NULL;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = -1;
    new_dtp->dataloop_depth = -1;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = -1;
    new_dtp->hetero_dloop_depth = -1;

    is_builtin = (HANDLE_GET_TYPE(oldtype) == HANDLE_TYPE_BUILTIN);

    if (is_builtin)
    {
        el_sz   = MPID_Datatype_get_basic_size(oldtype);
        el_type = oldtype;

        new_dtp->size          = count * el_sz;
        new_dtp->has_sticky_ub = 0;
        new_dtp->has_sticky_lb = 0;
        new_dtp->true_lb       = 0;
        new_dtp->lb            = 0;
        new_dtp->true_ub       = count * el_sz;
        new_dtp->ub            = new_dtp->true_ub;
        new_dtp->extent        = new_dtp->ub - new_dtp->lb;

        new_dtp->alignsize     = el_sz;
        new_dtp->n_elements    = count;
        new_dtp->element_size  = el_sz;
        new_dtp->eltype        = el_type;
        new_dtp->is_contig     = true;
        new_dtp->max_contig_blocks = 1;

    }
    else
    {
        /* user-defined base type (oldtype) */
        const MPID_Datatype *old_dtp;

        old_dtp = TypePool::Get( oldtype );
        el_sz   = (int)old_dtp->element_size;
        el_type = old_dtp->eltype;

        new_dtp->size           = count * old_dtp->size;
        new_dtp->has_sticky_ub  = old_dtp->has_sticky_ub;
        new_dtp->has_sticky_lb  = old_dtp->has_sticky_lb;

        MPID_DATATYPE_CONTIG_LB_UB(count,
                                   old_dtp->lb,
                                   old_dtp->ub,
                                   old_dtp->extent,
                                   new_dtp->lb,
                                   new_dtp->ub);

        /* easiest to calc true lb/ub relative to lb/ub; doesn't matter
         * if there are sticky lb/ubs or not when doing this.
         */
        new_dtp->true_lb = new_dtp->lb + (old_dtp->true_lb - old_dtp->lb);
        new_dtp->true_ub = new_dtp->ub + (old_dtp->true_ub - old_dtp->ub);
        new_dtp->extent  = new_dtp->ub - new_dtp->lb;

        new_dtp->alignsize    = old_dtp->alignsize;
        new_dtp->n_elements   = count * old_dtp->n_elements;
        new_dtp->element_size = old_dtp->element_size;
        new_dtp->eltype       = el_type;

        new_dtp->is_contig    = old_dtp->is_contig;
        if(old_dtp->is_contig)
        {
            new_dtp->max_contig_blocks = 1;
        }
        else
        {
            new_dtp->max_contig_blocks = count * old_dtp->max_contig_blocks;
        }
    }

    *ppNewType = new_dtp;

    return MPI_SUCCESS;
}


/* PAIRTYPE_SIZE_EXTENT - calculates size, extent, etc. for pairtype by
 * defining the appropriate C type.
 */
template<typename T1, typename T2> void SetTypeCharacteristics(
    _Inout_ MPID_Datatype* pType
    )
{
    struct _foo{ T1 a; T2 b; };

    pType->size = sizeof(T1) + sizeof(T2);
    pType->extent = sizeof(_foo);
    pType->element_size = (sizeof(T1) == sizeof(T2)) ? sizeof(T1) : static_cast<MPI_Count>(-1);
    pType->true_ub = FIELD_OFFSET(_foo, b) + sizeof(T2);
    pType->alignsize = max(__alignof(T1), __alignof(T2));
}

/*@
  MPID_Type_create_pairtype - create necessary data structures for certain
  pair types (all but MPI_2INT etc., which never have the size != extent
  issue).

  This function is different from the other MPID_Type_create functions in that
  it fills in an already- allocated MPID_Datatype.  This is important for
  allowing configure-time determination of the MPI type values (these types
  are stored in the "direct" space, for those familiar with how MPICH2 deals
  with type allocation).

  Input Parameters:
+ type - name of pair type (e.g. MPI_FLOAT_INT)
- new_dtp - pointer to previously allocated datatype structure, which is
            filled in by this function

  Return Value:
  MPI_SUCCESS on success, MPI errno on failure.

  Note:
  Section 4.9.3 (MINLOC and MAXLOC) of the MPI-1 standard specifies that
  these types should be built as if by the following (e.g. MPI_FLOAT_INT):

    type[0] = MPI_FLOAT
    type[1] = MPI_INT
    disp[0] = 0
    disp[1] = sizeof(float) <---- questionable displacement!
    block[0] = 1
    block[1] = 1
    MPI_TYPE_STRUCT(2, block, disp, type, MPI_FLOAT_INT)

  However, this is a relatively naive approach that does not take struct
  padding into account when setting the displacement of the second element.
  Thus in our implementation we have chosen to instead use the actual
  difference in starting locations of the two types in an actual struct.
@*/
MPI_RESULT
MPID_Type_create_pairtype(
    MPI_Datatype type,
    _Inout_ MPID_Datatype *new_dtp)
{
    MPIU_Assert( new_dtp->handle == type );

    new_dtp->is_permanent = true;
    new_dtp->is_committed = true; /* predefined types are pre-committed */
    new_dtp->attributes   = NULL;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->contents     = NULL;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = -1;
    new_dtp->dataloop_depth = -1;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = -1;
    new_dtp->hetero_dloop_depth = -1;

    switch(type)
    {
        case MPI_FLOAT_INT:
            SetTypeCharacteristics<float,int>( new_dtp );
            MPID_FloatInt.Init( new_dtp );
            break;
        case MPI_DOUBLE_INT:
            SetTypeCharacteristics<double,int>( new_dtp );
            MPID_DoubleInt.Init( new_dtp );
            break;
        case MPI_LONG_INT:
            SetTypeCharacteristics<long,int>( new_dtp );
            MPID_LongInt.Init( new_dtp );
            break;
        case MPI_SHORT_INT:
            SetTypeCharacteristics<short,int>( new_dtp );
            MPID_ShortInt.Init( new_dtp );
            break;
        case MPI_LONG_DOUBLE_INT:
            SetTypeCharacteristics<long double,int>( new_dtp );
            MPID_LongDoubleInt.Init( new_dtp );
            break;
        default:
            return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**dtype");
    }

    new_dtp->n_elements      = 2;
    new_dtp->eltype          = MPI_DATATYPE_NULL;

    new_dtp->has_sticky_lb   = false;
    new_dtp->true_lb         = 0;
    new_dtp->lb              = 0;

    new_dtp->has_sticky_ub   = false;

    new_dtp->ub              = new_dtp->extent; /* possible padding */

    new_dtp->is_contig       = (new_dtp->size == new_dtp->extent);
    new_dtp->max_contig_blocks = (new_dtp->size == new_dtp->extent) ? 1 : 2;

    /* fill in dataloops -- only case where we precreate dataloops
     *
     * this is necessary because these types aren't committed by the
     * user, which is the other place where we create dataloops. so
     * if the user uses one of these w/out building some more complex
     * type and then committing it, then the dataloop will be missing.
     */
    MPI_RESULT mpi_errno = MPID_Dataloop_create_pairtype(type,
                                        &(new_dtp->dataloop),
                                        &(new_dtp->dataloop_size),
                                        &(new_dtp->dataloop_depth),
                                        DLOOP_DATALOOP_HOMOGENEOUS);
    if (mpi_errno == MPI_SUCCESS)
    {
        return MPID_Dataloop_create_pairtype(type,
                                             &(new_dtp->hetero_dloop),
                                             &(new_dtp->hetero_dloop_size),
                                             &(new_dtp->hetero_dloop_depth),
                                             DLOOP_DATALOOP_HETEROGENEOUS);
    }

    return mpi_errno;
}


MPI_RESULT
MPID_Type_create_resized(
    MPI_Datatype oldtype,
    MPI_Aint lb,
    MPI_Aint extent,
    MPI_Datatype *newtype_p)
{
    MPID_Datatype *new_dtp;

    new_dtp = TypePool::Alloc();
    if ( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_dtp->is_permanent = 0;
    new_dtp->is_committed = 0;
    new_dtp->attributes   = 0;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->contents     = 0;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = -1;
    new_dtp->dataloop_depth = -1;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = -1;
    new_dtp->hetero_dloop_depth = -1;

    /* if oldtype is a basic, we build a contiguous dataloop of count = 1 */
    // TODO: Can the if/else parts be merged?
    if (HANDLE_GET_TYPE(oldtype) == HANDLE_TYPE_BUILTIN)
    {
        int oldsize = MPID_Datatype_get_basic_size(oldtype);

        new_dtp->size           = oldsize;
        new_dtp->has_sticky_ub  = true;
        new_dtp->has_sticky_lb  = true;
        new_dtp->dataloop_depth = 1;
        new_dtp->true_lb        = 0;
        new_dtp->lb             = lb;
        new_dtp->true_ub        = oldsize;
        new_dtp->ub             = lb + extent;
        new_dtp->extent         = extent;
        new_dtp->alignsize      = oldsize; /* FIXME ??? */
        new_dtp->n_elements     = 1;
        new_dtp->element_size   = oldsize;
        new_dtp->is_contig      = (extent == oldsize);
        new_dtp->eltype         = oldtype;
    }
    else
    {
        /* user-defined base type */
        const MPID_Datatype *old_dtp;

        old_dtp = TypePool::Get( oldtype );

        new_dtp->size           = old_dtp->size;
        new_dtp->has_sticky_ub  = true;
        new_dtp->has_sticky_lb  = true;
        new_dtp->dataloop_depth = old_dtp->dataloop_depth;
        new_dtp->true_lb        = old_dtp->true_lb;
        new_dtp->lb             = lb;
        new_dtp->true_ub        = old_dtp->true_ub;
        new_dtp->ub             = lb + extent;
        new_dtp->extent         = extent;
        new_dtp->alignsize      = old_dtp->alignsize;
        new_dtp->n_elements     = old_dtp->n_elements;
        new_dtp->element_size   = old_dtp->element_size;
        new_dtp->eltype         = old_dtp->eltype;

        new_dtp->is_contig      =
            (extent == old_dtp->size) ? old_dtp->is_contig : false;
    }

    *newtype_p = new_dtp->handle;

    return MPI_SUCCESS;
}


/* MPI datatype debugging helper routines.
 *
 * The one you want to call is:
 *   MPIDU_Datatype_debug(MPI_Datatype type, int array_ct)
 *
 * The "array_ct" value tells the call how many array values to print
 * for struct, indexed, and blockindexed types.
 *
 */

#define ON_DT_RET(t_, x_) \
    if(t_ == x_) return #x_

/* longest string is 21 characters */
const char* MPIDU_Datatype_builtin_to_string(MPI_Datatype type)
{
    ON_DT_RET(type, MPI_CHAR);
    ON_DT_RET(type, MPI_UNSIGNED_CHAR);
    ON_DT_RET(type, MPI_SIGNED_CHAR);
    ON_DT_RET(type, MPI_BYTE);
    ON_DT_RET(type, MPI_WCHAR);
    ON_DT_RET(type, MPI_SHORT);
    ON_DT_RET(type, MPI_UNSIGNED_SHORT);
    ON_DT_RET(type, MPI_INT);
    ON_DT_RET(type, MPI_UNSIGNED);
    ON_DT_RET(type, MPI_LONG);
    ON_DT_RET(type, MPI_UNSIGNED_LONG);
    ON_DT_RET(type, MPI_FLOAT);
    ON_DT_RET(type, MPI_DOUBLE);
    ON_DT_RET(type, MPI_LONG_DOUBLE);
    ON_DT_RET(type, MPI_LONG_LONG_INT);
    ON_DT_RET(type, MPI_LONG_LONG);
    ON_DT_RET(type, MPI_UNSIGNED_LONG_LONG);

    ON_DT_RET(type, MPI_PACKED);
    ON_DT_RET(type, MPI_LB);
    ON_DT_RET(type, MPI_UB);

    ON_DT_RET(type, MPI_FLOAT_INT);
    ON_DT_RET(type, MPI_DOUBLE_INT);
    ON_DT_RET(type, MPI_LONG_INT);
    ON_DT_RET(type, MPI_SHORT_INT);
    ON_DT_RET(type, MPI_2INT);
    ON_DT_RET(type, MPI_LONG_DOUBLE_INT);

    ON_DT_RET(type, MPI_COMPLEX);
    ON_DT_RET(type, MPI_DOUBLE_COMPLEX);
    ON_DT_RET(type, MPI_LOGICAL);
    ON_DT_RET(type, MPI_REAL);
    ON_DT_RET(type, MPI_DOUBLE_PRECISION);
    ON_DT_RET(type, MPI_INTEGER);
    ON_DT_RET(type, MPI_2INTEGER);
    ON_DT_RET(type, MPI_2COMPLEX);
    ON_DT_RET(type, MPI_2DOUBLE_COMPLEX);
    ON_DT_RET(type, MPI_2REAL);
    ON_DT_RET(type, MPI_2DOUBLE_PRECISION);
    ON_DT_RET(type, MPI_CHARACTER);

    return NULL;
}


/* MPIDU_Datatype_combiner_to_string(combiner)
 *
 * Converts a numeric combiner into a pointer to a string used for printing.
 *
 * longest string is 16 characters.
 */
const char* MPIDU_Datatype_combiner_to_string(int combiner)
{
    if (combiner == MPI_COMBINER_NAMED)            return "named";
    if (combiner == MPI_COMBINER_CONTIGUOUS)       return "contig";
    if (combiner == MPI_COMBINER_VECTOR)           return "vector";
    if (combiner == MPI_COMBINER_HVECTOR)          return "hvector";
    if (combiner == MPI_COMBINER_INDEXED)          return "indexed";
    if (combiner == MPI_COMBINER_HINDEXED)         return "hindexed";
    if (combiner == MPI_COMBINER_HINDEXED_BLOCK)   return "hindexed_block";
    if (combiner == MPI_COMBINER_STRUCT)           return "struct";
    if (combiner == MPI_COMBINER_DUP)              return "dup";
    if (combiner == MPI_COMBINER_HVECTOR_INTEGER)  return "hvector_integer";
    if (combiner == MPI_COMBINER_HINDEXED_INTEGER) return "hindexed_integer";
    if (combiner == MPI_COMBINER_INDEXED_BLOCK)    return "indexed_block";
    if (combiner == MPI_COMBINER_STRUCT_INTEGER)   return "struct_integer";
    if (combiner == MPI_COMBINER_SUBARRAY)         return "subarray";
    if (combiner == MPI_COMBINER_DARRAY)           return "darray";
    if (combiner == MPI_COMBINER_F90_REAL)         return "f90_real";
    if (combiner == MPI_COMBINER_F90_COMPLEX)      return "f90_complex";
    if (combiner == MPI_COMBINER_F90_INTEGER)      return "f90_integer";
    if (combiner == MPI_COMBINER_RESIZED)          return "resized";

    return NULL;
}


/*@
  MPID_Type_dup - create a copy of a datatype

  Input Parameters:
- oldtype - handle of original datatype

  Output Parameters:
. newtype - handle of newly created copy of datatype

  Return Value:
  0 on success, -1 on failure.
@*/
MPI_RESULT
MPID_Type_dup(
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    )
{
    if (HANDLE_GET_TYPE(oldtype) == HANDLE_TYPE_BUILTIN)
    {
        /* create a new type and commit it. */
        return MPID_Type_contiguous(1, oldtype, ppNewType);
    }

    /* allocate new datatype object and handle */
    MPID_Datatype* new_dtp = TypePool::Alloc();
    if( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    MPID_Datatype* old_dtp = TypePool::Get( oldtype );

    /* fill in datatype */
    new_dtp->is_contig     = old_dtp->is_contig;
    new_dtp->size          = old_dtp->size;
    new_dtp->extent        = old_dtp->extent;
    new_dtp->ub            = old_dtp->ub;
    new_dtp->lb            = old_dtp->lb;
    new_dtp->true_ub       = old_dtp->true_ub;
    new_dtp->true_lb       = old_dtp->true_lb;
    new_dtp->alignsize     = old_dtp->alignsize;
    new_dtp->has_sticky_ub = old_dtp->has_sticky_ub;
    new_dtp->has_sticky_lb = old_dtp->has_sticky_lb;
    new_dtp->is_permanent  = false;
    new_dtp->is_committed  = old_dtp->is_committed;
    new_dtp->attributes    = NULL; /* ??? */
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->n_elements    = old_dtp->n_elements;
    new_dtp->element_size  = old_dtp->element_size;
    new_dtp->eltype        = old_dtp->eltype;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = old_dtp->dataloop_size;
    new_dtp->dataloop_depth = old_dtp->dataloop_depth;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = old_dtp->hetero_dloop_size;
    new_dtp->hetero_dloop_depth = old_dtp->hetero_dloop_depth;

    if (old_dtp->is_committed)
    {
        MPIU_Assert(old_dtp->dataloop != NULL);
        MPID_Dataloop_dup(old_dtp->dataloop,
                            old_dtp->dataloop_size,
                            &new_dtp->dataloop);
        if (old_dtp->hetero_dloop != NULL)
        {
            /* at this time MPI_COMPLEX doesn't have this loop...
                * -- RBR, 02/01/2007
                */
            MPID_Dataloop_dup(old_dtp->hetero_dloop,
                                old_dtp->hetero_dloop_size,
                                &new_dtp->hetero_dloop);
        }
    }

    *ppNewType = new_dtp;
    return MPI_SUCCESS;
}


/*@
  MPID_Type_get_contents - get content information from datatype

  Input Parameters:
+ datatype - MPI datatype
. max_integers - size of array_of_integers
. max_addresses - size of array_of_addresses
- max_datatypes - size of array_of_datatypes

  Output Parameters:
+ array_of_integers - integers used in creating type
. array_of_addresses - MPI_Aints used in creating type
- array_of_datatypes - MPI_Datatypes used in creating type

@*/
MPI_RESULT
MPID_Type_get_contents(
    _In_ MPI_Datatype datatype,
    _In_range_(>=, 0) int max_integers,
    _In_range_(>=, 0) int max_addresses,
    _In_range_(>=, 0) int max_datatypes,
    _Out_writes_opt_(max_integers) int array_of_integers[],
    _Out_writes_opt_(max_addresses) MPI_Aint array_of_addresses[],
    _Out_writes_(max_datatypes) MPI_Datatype array_of_datatypes[]
    )
{
    int i;
    MPI_RESULT mpi_errno;
    MPID_Datatype *dtp;
    MPID_Datatype_contents *cp;

    /* these are checked at the MPI layer, so I feel that asserts
     * are appropriate.
     */
    MPIU_Assert(MPID_Datatype_is_predefined(datatype) == false);

    dtp = TypePool::Get(datatype);
    cp = dtp->contents;
    MPIU_Assert(cp != NULL);

    if (max_integers < cp->nr_ints ||
        max_addresses < cp->nr_aints ||
        max_datatypes < cp->nr_types)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**dtype");
        return mpi_errno;
    }

    if (cp->nr_ints > 0)
    {
        memcpy( array_of_integers, cp->Ints(), sizeof(int) * cp->nr_ints );
    }

    if (cp->nr_aints > 0)
    {
        memcpy( array_of_addresses, cp->Aints(), sizeof(MPI_Aint) * cp->nr_aints );
    }

    for (i=0; i < cp->nr_types; i++)
    {
        __analysis_assume( i < max_datatypes );
        array_of_datatypes[i] = cp->Types()[i];
        if( MPID_Datatype_is_predefined(array_of_datatypes[i]) == false )
        {
            TypePool::Get(array_of_datatypes[i])->AddRef();
        }
    }

    return MPI_SUCCESS;
}

/*@
  MPID_Type_get_envelope - get envelope information from datatype

  Input Parameters:
. datatype - MPI datatype

  Output Parameters:
+ num_integers - number of integers used to create datatype
. num_addresses - number of MPI_Aints used to create datatype
. num_datatypes - number of MPI_Datatypes used to create datatype
- combiner - function type used to create datatype
@*/

void MPID_Type_get_envelope(MPI_Datatype datatype,
                           int *num_integers,
                           int *num_addresses,
                           int *num_datatypes,
                           int *combiner)
{
    if (MPID_Datatype_is_predefined(datatype) == true)
    {
        *combiner      = MPI_COMBINER_NAMED;
        *num_integers  = 0;
        *num_addresses = 0;
        *num_datatypes = 0;
    }
    else
    {
        const MPID_Datatype *dtp;

        dtp = TypePool::Get( datatype );
        MPIU_Assert( dtp != NULL );

        *combiner      = dtp->contents->combiner;
        *num_integers  = dtp->contents->nr_ints;
        *num_addresses = dtp->contents->nr_aints;
        *num_datatypes = dtp->contents->nr_types;
    }
}


/* MPIDI_Type_indexed_count_contig()
 *
 * Determines the actual number of contiguous blocks represented by the
 * blocklength/displacement arrays.  This might be less than count (as
 * few as 1).
 *
 * Extent passed in is for the original type.
 */
int MPIDI_Type_indexed_count_contig(int count,
                                    const int *blocklength_array,
                                    const void *displacement_array,
                                    int dispinbytes,
                                    MPI_Aint old_extent)
{
    int i, contig_count = 1;
    int cur_blklen = blocklength_array[0];

    if (!dispinbytes)
    {
        int cur_tdisp = ((int *) displacement_array)[0];

        for (i = 1; i < count; i++)
        {
            if (blocklength_array[i] == 0)
            {
                continue;
            }
            else if (cur_tdisp + cur_blklen == ((int *) displacement_array)[i])
            {
                /* adjacent to current block; add to block */
                cur_blklen += blocklength_array[i];
            }
            else
            {
                cur_tdisp  = ((int *) displacement_array)[i];
                cur_blklen = blocklength_array[i];
                contig_count++;
            }
        }
    }
    else
    {
        MPI_Aint cur_bdisp = ((MPI_Aint *) displacement_array)[0];

        for (i = 1; i < count; i++)
        {
            if (blocklength_array[i] == 0)
            {
                continue;
            }
            else if (cur_bdisp + cur_blklen * old_extent ==
                     ((MPI_Aint *) displacement_array)[i])
            {
                /* adjacent to current block; add to block */
                cur_blklen += blocklength_array[i];
            }
            else
            {
                cur_bdisp  = ((MPI_Aint *) displacement_array)[i];
                cur_blklen = blocklength_array[i];
                contig_count++;
            }
        }
    }
    return contig_count;
}


/*@
  MPID_Type_indexed - create an indexed datatype

  Input Parameters:
+ count - number of blocks in type
. blocklength_array - number of elements in each block
. displacement_array - offsets of blocks from start of type (see next
  parameter for units)
. dispinbytes - if true, then displacements are in bytes, otherwise
  they in terms of extent of oldtype
- oldtype - type (using handle) of datatype on which new type is based

  Output Parameters:
. newtype - handle of new indexed datatype

  Return Value:
  0 on success, -1 on failure.
@*/
MPI_RESULT
MPID_Type_indexed(
    _In_range_(>=, 0) int count,
    _In_reads_opt_(count) const int blocklength_array[],
    _In_opt_ const void *displacement_array,
    _In_ bool dispinbytes,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    )
{
    bool is_builtin, old_is_contig;
    int i, contig_count;
    int el_sz, el_ct, old_ct, old_sz;
    MPI_Aint old_lb, old_ub, old_extent, old_true_lb, old_true_ub;
    MPI_Aint min_lb = 0, max_ub = 0, eff_disp;
    MPI_Datatype el_type;

    MPID_Datatype* new_dtp;

    //
    // Find the first nonzero blocklength element
    //
    int firstNZ = 0;
    while (firstNZ < count && blocklength_array[firstNZ] == 0)
    {
        ++firstNZ;
    }

    if (firstNZ == count)
    {
        int mpi_errno = MPID_Type_zerolen( &new_dtp );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    else
    {

        /* allocate new datatype object and handle */
        new_dtp = TypePool::Alloc();
        if( new_dtp == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        new_dtp->is_permanent = 0;
        new_dtp->is_committed = 0;
        new_dtp->attributes   = NULL;
        InitName<MPID_Datatype>( new_dtp );
        new_dtp->contents     = NULL;

        new_dtp->dataloop       = NULL;
        new_dtp->dataloop_size  = -1;
        new_dtp->dataloop_depth = -1;
        new_dtp->hetero_dloop       = NULL;
        new_dtp->hetero_dloop_size  = -1;
        new_dtp->hetero_dloop_depth = -1;

        is_builtin = (HANDLE_GET_TYPE(oldtype) == HANDLE_TYPE_BUILTIN);

        if (is_builtin)
        {
            /* builtins are handled differently than user-defined types because
             * they have no associated dataloop or datatype structure.
             */
            el_sz      = MPID_Datatype_get_basic_size(oldtype);
            old_sz     = el_sz;
            el_ct      = 1;
            el_type    = oldtype;

            old_lb        = 0;
            old_true_lb   = 0;
            old_ub        = el_sz;
            old_true_ub   = el_sz;
            old_extent    = el_sz;
            old_is_contig = 1;

            new_dtp->has_sticky_ub = false;
            new_dtp->has_sticky_lb = false;

            new_dtp->alignsize    = el_sz; /* ??? */
            new_dtp->element_size = el_sz;
            new_dtp->eltype       = el_type;

            new_dtp->max_contig_blocks = count;
        }
        else
        {
            /* user-defined base type (oldtype) */
            const MPID_Datatype *old_dtp;

            old_dtp = TypePool::Get( oldtype );
            el_sz   = (int)old_dtp->element_size;
            old_sz  = old_dtp->size;
            el_ct   = old_dtp->n_elements;
            el_type = old_dtp->eltype;

            old_lb        = old_dtp->lb;
            old_true_lb   = old_dtp->true_lb;
            old_ub        = old_dtp->ub;
            old_true_ub   = old_dtp->true_ub;
            old_extent    = old_dtp->extent;
            old_is_contig = old_dtp->is_contig;

            new_dtp->has_sticky_lb = old_dtp->has_sticky_lb;
            new_dtp->has_sticky_ub = old_dtp->has_sticky_ub;
            new_dtp->element_size  = el_sz;
            new_dtp->eltype        = el_type;

            new_dtp->max_contig_blocks = 0;
            for(i=0; i<count; i++)
            {
                new_dtp->max_contig_blocks +=
                    old_dtp->max_contig_blocks * blocklength_array[i];
            }
        }

        i = firstNZ;

        /* priming for loop */
        old_ct = blocklength_array[i];
        eff_disp = (dispinbytes) ? ((MPI_Aint *) displacement_array)[i] :
            (((MPI_Aint) ((int *) displacement_array)[i]) * old_extent);

        MPID_DATATYPE_BLOCK_LB_UB((MPI_Aint) blocklength_array[i],
                                  eff_disp,
                                  old_lb,
                                  old_ub,
                                  old_extent,
                                  min_lb,
                                  max_ub);

        /* determine min lb, max ub, and count of old types in remaining
         * nonzero size blocks
         */
        for (i++; i < count; i++)
        {
            MPI_Aint tmp_lb, tmp_ub;

            if (blocklength_array[i] > 0)
            {
                old_ct += blocklength_array[i]; /* add more oldtypes */

                eff_disp = (dispinbytes) ? ((MPI_Aint *) displacement_array)[i] :
                    (((MPI_Aint) ((int *) displacement_array)[i]) * old_extent);

                /* calculate ub and lb for this block */
                MPID_DATATYPE_BLOCK_LB_UB((MPI_Aint) blocklength_array[i],
                                          eff_disp,
                                          old_lb,
                                          old_ub,
                                          old_extent,
                                          tmp_lb,
                                          tmp_ub);

                if (tmp_lb < min_lb) min_lb = tmp_lb;
                if (tmp_ub > max_ub) max_ub = tmp_ub;
            }
        }

        new_dtp->size = old_ct * old_sz;

        new_dtp->lb      = min_lb;
        new_dtp->ub      = max_ub;
        new_dtp->true_lb = min_lb + (old_true_lb - old_lb);
        new_dtp->true_ub = max_ub + (old_true_ub - old_ub);
        new_dtp->extent  = max_ub - min_lb;

        new_dtp->n_elements = old_ct * el_ct;

        /* new type is only contig for N types if it's all one big
         * block, its size and extent are the same, and the old type
         * was also contiguous.
         */
        new_dtp->is_contig = false;
        if(old_is_contig)
        {
            contig_count = MPIDI_Type_indexed_count_contig(count,
                                                           blocklength_array,
                                                           displacement_array,
                                                           dispinbytes,
                                                           old_extent);
            new_dtp->max_contig_blocks = contig_count;
            if( (contig_count == 1) &&
                (new_dtp->size == new_dtp->extent))
            {
                new_dtp->is_contig = true;
            }
        }
    }

    StackGuardArray<int> ints;
    MPI_RESULT mpi_errno;
    if( !dispinbytes )
    {
        //
        // MPI_Type_indexed uses multiples of types as displacements
        //

        //
        // Copy all integer values into a temporary buffer; this
        // includes the count, the blocklengths, and the displacements.
        //
        ints = new int[2 * count + 1];
        if( ints == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        ints[0] = count;
        int* pBlockLens = &ints[1];
        int* pIndices = pBlockLens + count;
        for (i=0; i < count; i++)
        {
            *pBlockLens = blocklength_array[i];
            pBlockLens++;

            *pIndices = reinterpret_cast<const int*>(displacement_array)[i];
            pIndices++;
        }

        mpi_errno = MPID_Datatype_set_contents(
            new_dtp,
            MPI_COMBINER_INDEXED,
            2*count + 1, /* ints */
            0, /* aints  */
            1, /* types */
            ints,
            nullptr,
            &oldtype);
    }
    else
    {
        //
        // MPI_Type_create_hindexed uses bytes as displacements
        //

        ints = new int[count + 1];
        if( ints == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        ints[0] = count;

        for (i=0; i < count; i++)
        {
            ints[i+1] = blocklength_array[i];
        }

        mpi_errno = MPID_Datatype_set_contents(
            new_dtp,
            MPI_COMBINER_HINDEXED,
            count+1, /* ints (count, blocklengths) */
            count, /* aints (displacements) */
            1, /* types */
            ints,
            reinterpret_cast<const MPI_Aint*>(displacement_array),
            &oldtype);
    }
    
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }


    *ppNewType = new_dtp;
    return MPI_SUCCESS;
}


/* MPID_Type_struct_alignsize
 *
 * This function guesses at how the C compiler would align a structure
 * with the given components.
 */
static
unsigned int
MPID_Type_struct_alignsize(
    _In_range_(0, INT_MAX) int count,
    _In_count_(count) const MPI_Datatype oldtype_array[],
    _In_count_(count) const MPI_Aint displacement_array[]
    )
{
    unsigned long max_alignsize = 0;
    unsigned long tmp_alignsize;
    unsigned long unalignment;

    for (int i=0; i < count; i++)
    {
        /* shouldn't be called with an LB or UB, but we'll handle it nicely */
        if (oldtype_array[i] == MPI_LB || oldtype_array[i] == MPI_UB)
        {
            continue;
        }

        const MPID_Datatype *dtp;
        dtp = TypePool::Get( oldtype_array[i] );

        tmp_alignsize = dtp->alignsize;

        if( tmp_alignsize == 0 )
        {
            continue;
        }

        unalignment = displacement_array[i] % tmp_alignsize;

        if( unalignment != 0 )
        {
            //
            // Type is explicitly unaligned, use lowest bit of displacement as alignment.
            //
            _BitScanForward( &tmp_alignsize, unalignment );
            tmp_alignsize = 1 << tmp_alignsize;
        }

        if (max_alignsize < tmp_alignsize)
        {
            max_alignsize = tmp_alignsize;
        }
    }

    return static_cast<int>(max_alignsize);
}


/*@
  MPID_Type_struct - create a struct datatype

  Input Parameters:
+ count - number of blocks in vector
. blocklength_array - number of elements in each block
. displacement_array - offsets of blocks from start of type in bytes
- oldtype_array - types (using handle) of datatypes on which vector is based

  Output Parameters:
. newtype - handle of new struct datatype

  Return Value:
  MPI_SUCCESS on success, MPI errno on failure.
@*/
MPI_RESULT
MPID_Type_struct(
    _In_range_(>=, 0) int count,
    _In_reads_opt_(count) const int blocklength_array[],
    _In_reads_opt_(count) const MPI_Aint displacement_array[],
    _In_reads_opt_(count) const MPI_Datatype oldtype_array[],
    _Outptr_ MPID_Datatype** ppNewType
    )
{
    int i, old_are_contig = 1, definitely_not_contig = 0;
    bool found_sticky_lb = false, found_sticky_ub = false, found_true_lb = false,
        found_true_ub = false, found_el_type = false;
    MPI_Count el_sz = 0;
    int size = 0;
    MPI_Datatype el_type = MPI_DATATYPE_NULL;
    MPI_Aint true_lb_disp = 0, true_ub_disp = 0, sticky_lb_disp = 0,
        sticky_ub_disp = 0;

    MPIU_Assert( count >= 0 );

    if( count == 0 )
    {
        return MPID_Type_zerolen( ppNewType );
    }

    /* allocate new datatype object and handle */
    MPID_Datatype* new_dtp = TypePool::Alloc();
    if( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_dtp->is_permanent = 0;
    new_dtp->is_committed = 0;
    new_dtp->attributes   = NULL;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->contents     = NULL;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = -1;
    new_dtp->dataloop_depth = -1;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = -1;
    new_dtp->hetero_dloop_depth = -1;

    /* check for junk struct with all zero blocks */
    for (i=0; i < count; i++)
    {
        if (blocklength_array[i] != 0)
        {
            break;
        }
    }

    if (i == count)
    {
        TypePool::Free( new_dtp );
        return MPID_Type_zerolen(ppNewType);
    }

    new_dtp->max_contig_blocks = 0;
    for (i=0; i < count; i++)
    {
        int is_builtin =
            (HANDLE_GET_TYPE(oldtype_array[i]) == HANDLE_TYPE_BUILTIN);
        MPI_Aint tmp_lb, tmp_ub, tmp_true_lb, tmp_true_ub;
        MPI_Count tmp_el_sz;
        MPI_Datatype tmp_el_type;
        const MPID_Datatype *old_dtp = NULL;

        /* Interpreting typemap to not include 0 blklen things, including
         * MPI_LB and MPI_UB. -- Rob Ross, 10/31/2005
         */
        if (blocklength_array[i] == 0)
        {
            continue;
        }

        if (is_builtin)
        {
            tmp_el_sz   = MPID_Datatype_get_basic_size(oldtype_array[i]);
            tmp_el_type = oldtype_array[i];

            MPID_DATATYPE_BLOCK_LB_UB(blocklength_array[i],
                                      displacement_array[i],
                                      0,
                                      static_cast<MPI_Aint>(tmp_el_sz),
                                      static_cast<MPI_Aint>(tmp_el_sz),
                                      tmp_lb,
                                      tmp_ub);
            tmp_true_lb = tmp_lb;
            tmp_true_ub = tmp_ub;

            size += static_cast<int>(tmp_el_sz * blocklength_array[i]);

            new_dtp->max_contig_blocks++;
        }
        else
        {
            old_dtp = TypePool::Get( oldtype_array[i] );

            tmp_el_sz   = old_dtp->element_size;
            tmp_el_type = old_dtp->eltype;

            MPID_DATATYPE_BLOCK_LB_UB(blocklength_array[i],
                                      displacement_array[i],
                                      old_dtp->lb,
                                      old_dtp->ub,
                                      old_dtp->extent,
                                      tmp_lb,
                                      tmp_ub);
            tmp_true_lb = tmp_lb + (old_dtp->true_lb - old_dtp->lb);
            tmp_true_ub = tmp_ub + (old_dtp->true_ub - old_dtp->ub);

            size += old_dtp->size * blocklength_array[i];

            new_dtp->max_contig_blocks += old_dtp->max_contig_blocks;
        }

        /* element size and type */
        if (oldtype_array[i] != MPI_LB && oldtype_array[i] != MPI_UB)
        {
            if (found_el_type == false)
            {
                el_sz         = tmp_el_sz;
                el_type       = tmp_el_type;
                found_el_type = true;
            }
            else if (el_sz != tmp_el_sz)
            {
                el_sz = -1;
                el_type = MPI_DATATYPE_NULL;
            }
            else if (el_type != tmp_el_type)
            {
                /* Q: should we set el_sz = -1 even though the same? */
                el_type = MPI_DATATYPE_NULL;
            }
        }

        /* keep lowest sticky lb */
        if ((oldtype_array[i] == MPI_LB) ||
            (!is_builtin && old_dtp->has_sticky_lb))
        {
            if (found_sticky_lb == false)
            {
                found_sticky_lb = true;
                sticky_lb_disp  = tmp_lb;
            }
            else if (sticky_lb_disp > tmp_lb)
            {
                sticky_lb_disp = tmp_lb;
            }
        }

        /* keep highest sticky ub */
        if ((oldtype_array[i] == MPI_UB) ||
            (!is_builtin && old_dtp->has_sticky_ub))
        {
            if (found_sticky_ub == false)
            {
                found_sticky_ub = true;
                sticky_ub_disp  = tmp_ub;
            }
            else if (sticky_ub_disp < tmp_ub)
            {
                sticky_ub_disp = tmp_ub;
            }
        }

        /* keep lowest true lb and highest true ub
         *
         * note: checking for contiguity at the same time, to avoid
         *       yet another pass over the arrays
         */
        if (oldtype_array[i] != MPI_UB && oldtype_array[i] != MPI_LB)
        {
            if (found_true_lb == false)
            {
                found_true_lb = true;
                true_lb_disp  = tmp_true_lb;
            }
            else if (true_lb_disp > tmp_true_lb)
            {
                /* element starts before previous */
                true_lb_disp = tmp_true_lb;
                definitely_not_contig = 1;
            }

            if (found_true_ub == false)
            {
                found_true_ub = true;
                true_ub_disp  = tmp_true_ub;
            }
            else if (true_ub_disp < tmp_true_ub)
            {
                true_ub_disp = tmp_true_ub;
            }
            else
            {
                /* element ends before previous ended */
                definitely_not_contig = 1;
            }
        }

        if (!is_builtin && !old_dtp->is_contig)
        {
            old_are_contig = 0;
        }
    }

    new_dtp->n_elements = -1; /* TODO */
    new_dtp->element_size = el_sz;
    new_dtp->eltype = el_type;

    new_dtp->has_sticky_lb = found_sticky_lb;
    new_dtp->true_lb       = true_lb_disp;
    new_dtp->lb = (found_sticky_lb) ? sticky_lb_disp : true_lb_disp;

    new_dtp->has_sticky_ub = found_sticky_ub;
    new_dtp->true_ub       = true_ub_disp;
    new_dtp->ub = (found_sticky_ub) ? sticky_ub_disp : true_ub_disp;

    new_dtp->alignsize = MPID_Type_struct_alignsize(count,
                                                    oldtype_array,
                                                    displacement_array);

    new_dtp->extent = new_dtp->ub - new_dtp->lb;
    if ((!found_sticky_lb) && (!found_sticky_ub))
    {
        /* account for padding */
        MPI_Aint epsilon = (new_dtp->alignsize > 0) ?
            new_dtp->extent % new_dtp->alignsize : 0;

        if (epsilon)
        {
            new_dtp->ub    += (new_dtp->alignsize - epsilon);
            new_dtp->extent = new_dtp->ub - new_dtp->lb;
        }
    }

    new_dtp->size = size;

    /* new type is contig for N types if its size and extent are the
     * same, and the old type was also contiguous, and we didn't see
     * something noncontiguous based on true ub/ub.
     */
    if ((new_dtp->size == new_dtp->extent) && old_are_contig &&
        (! definitely_not_contig))
    {
        new_dtp->is_contig = 1;
    }
    else
    {
        new_dtp->is_contig = 0;
    }

    *ppNewType = new_dtp;

    return MPI_SUCCESS;
}


/*@
  MPID_Type_vector - create a vector datatype

  Input Parameters:
+ count - number of blocks in vector
. blocklength - number of elements in each block
. stride - distance from beginning of one block to the next (see next
  parameter for units)
. strideinbytes - if nonzero, then stride is in bytes, otherwise stride
  is in terms of extent of oldtype
- oldtype - type (using handle) of datatype on which vector is based

  Output Parameters:
. newtype - handle of new vector datatype

  Return Value:
  0 on success, MPI error code on failure.
@*/
MPI_RESULT
MPID_Type_vector(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _In_ MPI_Aint stride,
    _In_ bool strideinbytes,
    _In_ MPI_Datatype oldtype,
    _Outptr_ MPID_Datatype** ppNewType
    )
{
    int is_builtin, old_is_contig;
    MPI_Aint el_sz, old_sz;
    MPI_Datatype el_type;
    MPI_Aint old_lb, old_ub, old_extent, old_true_lb, old_true_ub, eff_stride;

    if( count == 0 )
    {
        return MPID_Type_zerolen( ppNewType );
    }

    /* allocate new datatype object and handle */
    MPID_Datatype* new_dtp = TypePool::Alloc();
    if( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_dtp->is_permanent = 0;
    new_dtp->is_committed = 0;
    new_dtp->attributes   = NULL;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->contents     = NULL;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = -1;
    new_dtp->dataloop_depth = -1;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = -1;
    new_dtp->hetero_dloop_depth = -1;

    is_builtin = (HANDLE_GET_TYPE(oldtype) == HANDLE_TYPE_BUILTIN);

    if (is_builtin)
    {
        el_sz   = MPID_Datatype_get_basic_size(oldtype);
        el_type = oldtype;

        old_lb        = 0;
        old_true_lb   = 0;
        old_ub        = el_sz;
        old_true_ub   = el_sz;
        old_sz        = el_sz;
        old_extent    = el_sz;
        old_is_contig = 1;

        new_dtp->size           = (int)(count * blocklength * el_sz);
        new_dtp->has_sticky_lb  = false;
        new_dtp->has_sticky_ub  = false;

        new_dtp->alignsize    = (int)el_sz; /* ??? */
        new_dtp->n_elements   = count * blocklength;
        new_dtp->element_size = el_sz;
        new_dtp->eltype       = el_type;

        new_dtp->max_contig_blocks = count;

        eff_stride = (strideinbytes) ? stride : (stride * el_sz);
    }
    else /* user-defined base type (oldtype) */
    {
        const MPID_Datatype *old_dtp;

        old_dtp = TypePool::Get( oldtype );
        el_type = old_dtp->eltype;

        old_lb        = old_dtp->lb;
        old_true_lb   = old_dtp->true_lb;
        old_ub        = old_dtp->ub;
        old_true_ub   = old_dtp->true_ub;
        old_sz        = old_dtp->size;
        old_extent    = old_dtp->extent;
        old_is_contig = old_dtp->is_contig;

        new_dtp->size           = count * blocklength * old_dtp->size;
        new_dtp->has_sticky_lb  = old_dtp->has_sticky_lb;
        new_dtp->has_sticky_ub  = old_dtp->has_sticky_ub;

        new_dtp->alignsize    = old_dtp->alignsize;
        new_dtp->n_elements   = count * blocklength * old_dtp->n_elements;
        new_dtp->element_size = old_dtp->element_size;
        new_dtp->eltype       = el_type;

        new_dtp->max_contig_blocks = old_dtp->max_contig_blocks * count * blocklength;

        eff_stride = (strideinbytes) ? stride : (stride * old_dtp->extent);
    }

    MPID_DATATYPE_VECTOR_LB_UB(count,
                               eff_stride,
                               blocklength,
                               old_lb,
                               old_ub,
                               old_extent,
                               new_dtp->lb,
                               new_dtp->ub);
    new_dtp->true_lb = new_dtp->lb + (old_true_lb - old_lb);
    new_dtp->true_ub = new_dtp->ub + (old_true_ub - old_ub);
    new_dtp->extent  = new_dtp->ub - new_dtp->lb;

    /* new type is only contig for N types if old one was, and
     * size and extent of new type are equivalent, and stride is
     * equal to blocklength * size of old type.
     */
    if (new_dtp->size == new_dtp->extent &&
        eff_stride == blocklength * old_sz &&
        old_is_contig)
    {
        new_dtp->is_contig = true;
        new_dtp->max_contig_blocks = 1;
    }
    else
    {
        new_dtp->is_contig = false;
    }

    *ppNewType = new_dtp;

    return MPI_SUCCESS;
}


/*@
  MPID_Type_zerolen - create an empty datatype

  Input Parameters:
. none

  Output Parameters:
. newtype - handle of new contiguous datatype

  Return Value:
  MPI_SUCCESS on success, MPI error code on failure.
@*/

MPI_RESULT
MPID_Type_zerolen(
    _Outptr_ MPID_Datatype** ppNewType
    )
{
    MPID_Datatype *new_dtp;

    /* allocate new datatype object and handle */
    new_dtp = TypePool::Alloc();
    if( new_dtp == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_dtp->is_permanent = 0;
    new_dtp->is_committed = 0;
    new_dtp->attributes   = NULL;
    InitName<MPID_Datatype>( new_dtp );
    new_dtp->contents     = NULL;

    new_dtp->dataloop       = NULL;
    new_dtp->dataloop_size  = -1;
    new_dtp->dataloop_depth = -1;
    new_dtp->hetero_dloop       = NULL;
    new_dtp->hetero_dloop_size  = -1;
    new_dtp->hetero_dloop_depth = -1;

    new_dtp->size          = 0;
    new_dtp->has_sticky_ub = 0;
    new_dtp->has_sticky_lb = 0;
    new_dtp->lb            = 0;
    new_dtp->ub            = 0;
    new_dtp->true_lb       = 0;
    new_dtp->true_ub       = 0;
    new_dtp->extent        = 0;

    new_dtp->alignsize     = 0;
    new_dtp->element_size  = 0;
    new_dtp->eltype        = 0;
    new_dtp->n_elements    = 0;
    new_dtp->is_contig     = 1;

    *ppNewType = new_dtp;

    return MPI_SUCCESS;
}


/*@

  MPID_Datatype_free

  Input Parameters:
. MPID_Datatype ptr - pointer to MPID datatype structure that is no longer
  referenced

  Output Parameters:
  none

  Return Value:
  none

  This function handles freeing dynamically allocated memory associated with
  the datatype.  In the process MPID_Datatype_free_contents() is also called,
  which handles decrementing reference counts to constituent types (in
  addition to freeing the space used for contents information).
  MPID_Datatype_free_contents() will call MPID_Datatype_free() on constituent
  types that are no longer referenced as well.

  @*/
MPI_RESULT MPID_Datatype::Destroy()
{
    /* before freeing the contents, check whether the pointer is not
       null because it is null in the case of a datatype shipped to the target
       for RMA ops */
    if (contents != NULL)
    {
        MPID_Datatype_free_contents(this);
    }

    if (dataloop != NULL)
    {
        MPID_Dataloop_free(&dataloop);
    }

    if (hetero_dloop != NULL)
    {
        MPID_Dataloop_free(&hetero_dloop);
    }

    CleanupName<MPID_Datatype>( this );

    TypePool::Free( this );
    return MPI_SUCCESS;
}


/*@
  MPID_Datatype_set_contents - store contents information for use in
                               MPI_Type_get_contents.

  Returns MPI_SUCCESS on success, MPI error code on error.
@*/
MPI_RESULT
MPID_Datatype_set_contents(
    MPID_Datatype *new_dtp,
    int combiner,
    int nr_ints,
    int nr_aints,
    int nr_types,
    const int array_of_ints[],
    const MPI_Aint array_of_aints[],
    const MPI_Datatype array_of_types[])
{
    int contents_size;
    int struct_sz, ints_sz, aints_sz, types_sz;
    MPID_Datatype_contents *cp;

    struct_sz = sizeof(MPID_Datatype_contents) - sizeof(MPI_Aint);
    types_sz  = nr_types * sizeof(MPI_Datatype);
    ints_sz   = nr_ints * sizeof(int);
    aints_sz  = nr_aints * sizeof(MPI_Aint);

    contents_size = struct_sz + types_sz + ints_sz + aints_sz;

    cp = static_cast<MPID_Datatype_contents*>( MPIU_Malloc(contents_size) );
    if( cp == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    cp->combiner = combiner;
    cp->nr_ints  = nr_ints;
    cp->nr_aints = nr_aints;
    cp->nr_types = nr_types;

    if (nr_ints > 0)
    {
        memcpy(cp->Ints(), array_of_ints, ints_sz);
    }

    if (nr_aints > 0)
    {
        memcpy(cp->Aints(), array_of_aints, aints_sz);
    }

    /* increment reference counts on all the derived types used here */
    for (int i=0; i < nr_types; i++)
    {
        cp->Types()[i] = array_of_types[i];
        if (MPID_Datatype_is_predefined(array_of_types[i]) == false)
        {
            TypePool::Get(array_of_types[i])->AddRef();
        }
    }

    new_dtp->contents = cp;
    return MPI_SUCCESS;
}


void MPID_Datatype_free_contents(MPID_Datatype *dtp)
{
    for( int i=0; i < dtp->contents->nr_types; i++ )
    {
        if( MPID_Datatype_is_predefined( dtp->contents->Types()[i] ) == false )
        {
            TypePool::Get( dtp->contents->Types()[i] )->Release();
        }
    }

    MPIU_Free(dtp->contents);
    dtp->contents = NULL;
}


void MPID_Type_access_contents(MPI_Datatype type,
                                int **ints_p,
                                MPI_Aint **aints_p,
                                MPI_Datatype **types_p)
{
    MPID_Datatype *dtp;
    MPID_Datatype_contents *cp;

    /* hardcoded handling of MPICH2 contents format... */
    dtp = TypePool::Get(type);
    MPIU_Assert(dtp != NULL);

    cp = dtp->contents;
    MPIU_Assert(cp != NULL);

    *types_p = cp->Types();
    *ints_p  = cp->Ints();
    *aints_p = cp->Aints();
    /* end of hardcoded handling of MPICH2 contents format */
}


/*@
  MPIR_Type_flatten

  Input Parameters:
. type - MPI Datatype (must have been committed)

  Output Parameters:
. off_array - pointer to array to fill in with offsets
. size_array - pointer to array to fill in with sizes

  Input/Output Parameters:
. array_len_p - pointer to value holding size of arrays; # used is returned

  Return Value:
  MPI_SUCCESS on success, and error code on failure.
@*/

MPI_RESULT
MPIR_Type_flatten(
    _In_ MPI_Datatype type,
    _Out_ MPI_Aint *off_array,
    _Out_ int *size_array,
    _Inout_ MPI_Aint *array_len_p)
{
    MPI_RESULT mpi_errno;
    MPI_Aint first, last;
    const MPID_Datatype *datatype_ptr;
    MPID_Segment *segp;

    if (HANDLE_GET_TYPE(type) == HANDLE_TYPE_BUILTIN)
    {
        off_array[0] = 0;
        size_array[0] = MPID_Datatype_get_size(type);
        *array_len_p = 1;
        return MPI_SUCCESS;
    }

    datatype_ptr = TypePool::Get(type);
    MPIU_Assert(datatype_ptr->is_committed);
    MPIU_Assert(*array_len_p >= datatype_ptr->max_contig_blocks);

    segp = MPID_Segment_alloc();
    if(segp == NULL)
    {
        return MPIU_ERR_NOMEM();
    }

    mpi_errno = MPID_Segment_init(0, 1, type, segp, 0);

    if (mpi_errno == MPI_SUCCESS)
    {
        first = 0;
        last  = SEGMENT_IGNORE_LAST;

        MPID_Segment_flatten(segp,
                             first,
                             &last,
                             off_array,
                             size_array,
                             array_len_p);
    }

    MPID_Segment_free(segp);

    return mpi_errno;
}


/*@
  MPIR_Type_get_contig_blocks

  Input Parameters:
. type - MPI Datatype (must have been committed)

  Output Parameters:
. nr_blocks_p - pointer to int in which to store the number of contiguous blocks in the type


  Return Value:
  0 on success, -1 on failure.
@*/

int MPIR_Type_get_contig_blocks(MPI_Datatype type,
                                int *nr_blocks_p)
{
    const MPID_Datatype *datatype_ptr;

    if (HANDLE_GET_TYPE(type) == HANDLE_TYPE_BUILTIN)
    {
        *nr_blocks_p = 1;
        return 0;
    }

    datatype_ptr = TypePool::Get(type);
    MPIU_Assert(datatype_ptr->is_committed);

    *nr_blocks_p = datatype_ptr->max_contig_blocks;
    return 0;
}


/* Returns MPI_SUCCESS on success, an MPI error code on failure.  Code above
 * needs to call MPIO_Err_return_xxx.
 */
static MPI_RESULT
MPIOI_Type_block(const int *array_of_gsizes, int dim, int ndims, int nprocs,
                     int rank, int darg, int order, MPI_Aint orig_extent,
                     MPI_Datatype type_old, MPI_Datatype *type_new,
                     MPI_Aint *st_offset)
{
/* nprocs = no. of processes in dimension dim of grid
   rank = coordinate of this process in dimension dim */
    int blksize, global_size, mysize, i, j;
    MPI_Aint stride;

    global_size = array_of_gsizes[dim];

    if (darg == MPI_DISTRIBUTE_DFLT_DARG)
    {
        blksize = (global_size + nprocs - 1)/nprocs;
    }
    else
    {
        blksize = darg;

        if (blksize <= 0)
        {
            return MPI_ERR_ARG;
        }

        if (blksize * nprocs < global_size)
        {
            return MPI_ERR_ARG;
        }
    }

    j = global_size - blksize*rank;
    mysize = (blksize < j) ? blksize : j;
    if (mysize < 0) mysize = 0;

    stride = orig_extent;
    if (order == MPI_ORDER_FORTRAN)
    {
        if (dim == 0)
        {
            NMPI_Type_contiguous(mysize, type_old, type_new);
        }
        else
        {
            for (i=0; i<dim; i++) stride *= array_of_gsizes[i];
            NMPI_Type_create_hvector(mysize, 1, stride, type_old, type_new);
        }
    }
    else
    {
        if (dim == ndims-1)
        {
            NMPI_Type_contiguous(mysize, type_old, type_new);
        }
        else
        {
            for (i=ndims-1; i>dim; i--) stride *= array_of_gsizes[i];
            NMPI_Type_create_hvector(mysize, 1, stride, type_old, type_new);
        }

    }

    *st_offset = blksize * rank;
     /* in terms of no. of elements of type oldtype in this dimension */
    if (mysize == 0) *st_offset = 0;

    return MPI_SUCCESS;
}


/* Returns MPI_SUCCESS on success, an MPI error code on failure.  Code above
 * needs to call MPIO_Err_return_xxx.
 */
static MPI_RESULT
MPIOI_Type_cyclic(const int *array_of_gsizes, int dim, int ndims, int nprocs,
                      int rank, int darg, int order, MPI_Aint orig_extent,
                      MPI_Datatype type_old, MPI_Datatype *type_new,
                      MPI_Aint *st_offset)
{
/* nprocs = no. of processes in dimension dim of grid
   rank = coordinate of this process in dimension dim */
    int blksize, i, blklens[3], st_index, end_index, local_size, rem, count;
    MPI_Aint stride, disps[3];
    MPI_Datatype type_tmp, types[3];

    if (darg == MPI_DISTRIBUTE_DFLT_DARG)
    {
        blksize = 1;
    }
    else
    {
        blksize = darg;
    }

    if (blksize <= 0)
    {
        return MPI_ERR_ARG;
    }

    st_index = rank*blksize;
    end_index = array_of_gsizes[dim] - 1;

    if (end_index < st_index)
    {
        local_size = 0;
    }
    else
    {
        local_size = ((end_index - st_index + 1)/(nprocs*blksize))*blksize;
        rem = (end_index - st_index + 1) % (nprocs*blksize);
        local_size += (rem < blksize) ? rem : blksize;
    }

    count = local_size/blksize;
    rem = local_size % blksize;

    stride = nprocs*blksize*orig_extent;
    if (order == MPI_ORDER_FORTRAN)
    {
        for (i=0; i<dim; i++) stride *= array_of_gsizes[i];
    }
    else
    {
        for (i=ndims-1; i>dim; i--) stride *= array_of_gsizes[i];
    }

    NMPI_Type_create_hvector(count, blksize, stride, type_old, type_new);

    if (rem)
    {
        /* if the last block is of size less than blksize, include
           it separately using MPI_Type_create_struct */

        types[0] = *type_new;
        types[1] = type_old;
        disps[0] = 0;
        disps[1] = count*stride;
        blklens[0] = 1;
        blklens[1] = rem;

        NMPI_Type_create_struct(2, blklens, disps, types, &type_tmp);

        NMPI_Type_free(type_new);
        *type_new = type_tmp;
    }

    /* In the first iteration, we need to set the displacement in that
       dimension correctly. */
    if ( ((order == MPI_ORDER_FORTRAN) && (dim == 0)) ||
         ((order == MPI_ORDER_C) && (dim == ndims-1)) )
    {
        types[0] = MPI_LB;
        disps[0] = 0;
        types[1] = *type_new;
        disps[1] = rank * blksize * orig_extent;
        types[2] = MPI_UB;
        disps[2] = orig_extent * array_of_gsizes[dim];
        blklens[0] = blklens[1] = blklens[2] = 1;
        NMPI_Type_create_struct(3, blklens, disps, types, &type_tmp);
        NMPI_Type_free(type_new);
        *type_new = type_tmp;

        *st_offset = 0;  /* set it to 0 because it is taken care of in
                            the struct above */
    }
    else
    {
        *st_offset = rank * blksize;
        /* st_offset is in terms of no. of elements of type oldtype in
         * this dimension */
    }

    if (local_size == 0) *st_offset = 0;

    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Type_convert_darray(
    int size,
    int rank,
    int ndims,
    const int *array_of_gsizes,
    const int *array_of_distribs,
    const int *array_of_dargs,
    const int *array_of_psizes,
    int order,
    MPI_Datatype oldtype,
    MPI_Datatype *newtype)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPI_Datatype type_old, type_new=MPI_DATATYPE_NULL, types[3];
    int procs, tmp_rank, i, tmp_size, blklens[3], *coords;
    MPI_Aint *st_offsets, orig_extent, orig_lb, disps[3];

    NMPI_Type_get_extent(oldtype, &orig_lb, &orig_extent);

/* calculate position in Cartesian grid as MPI would (row-major
   ordering) */
    coords = MPIU_Malloc_objn(ndims, int);
    if( coords == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    procs = size;
    tmp_rank = rank;
    for (i=0; i<ndims; i++)
    {
        procs = procs/array_of_psizes[i];
        coords[i] = tmp_rank/procs;
        tmp_rank = tmp_rank % procs;
    }

    st_offsets = MPIU_Malloc_objn(ndims, MPI_Aint);
    if( st_offsets == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    type_old = oldtype;

    if (order == MPI_ORDER_FORTRAN)
    {
      /* dimension 0 changes fastest */
        for (i=0; i<ndims; i++)
        {
            switch(array_of_distribs[i])
            {
            case MPI_DISTRIBUTE_BLOCK:
                MPIOI_Type_block(array_of_gsizes, i, ndims,
                                 array_of_psizes[i],
                                 coords[i], array_of_dargs[i],
                                 order, orig_extent,
                                 type_old, &type_new,
                                 st_offsets+i);
                break;
            case MPI_DISTRIBUTE_CYCLIC:
                MPIOI_Type_cyclic(array_of_gsizes, i, ndims,
                                  array_of_psizes[i], coords[i],
                                  array_of_dargs[i], order,
                                  orig_extent, type_old,
                                  &type_new, st_offsets+i);
                break;
            case MPI_DISTRIBUTE_NONE:
                /* treat it as a block distribution on 1 process */
                MPIOI_Type_block(array_of_gsizes, i, ndims, 1, 0,
                                 MPI_DISTRIBUTE_DFLT_DARG, order,
                                 orig_extent,
                                 type_old, &type_new,
                                 st_offsets+i);
                break;
            }
            if (i) NMPI_Type_free(&type_old);
            type_old = type_new;
        }

        /* add displacement and UB */
        disps[1] = st_offsets[0];
        tmp_size = 1;
        for (i=1; i<ndims; i++)
        {
            tmp_size *= array_of_gsizes[i-1];
            disps[1] += tmp_size*st_offsets[i];
        }
        /* rest done below for both Fortran and C order */
    }

    else /* order == MPI_ORDER_C */
    {
        /* dimension ndims-1 changes fastest */
        for (i=ndims-1; i>=0; i--)
        {
            switch(array_of_distribs[i])
            {
            case MPI_DISTRIBUTE_BLOCK:
                MPIOI_Type_block(array_of_gsizes, i, ndims, array_of_psizes[i],
                                 coords[i], array_of_dargs[i], order,
                                 orig_extent, type_old, &type_new,
                                 st_offsets+i);
                break;
            case MPI_DISTRIBUTE_CYCLIC:
                MPIOI_Type_cyclic(array_of_gsizes, i, ndims,
                                  array_of_psizes[i], coords[i],
                                  array_of_dargs[i], order,
                                  orig_extent, type_old, &type_new,
                                  st_offsets+i);
                break;
            case MPI_DISTRIBUTE_NONE:
                /* treat it as a block distribution on 1 process */
                MPIOI_Type_block(array_of_gsizes, i, ndims, array_of_psizes[i],
                      coords[i], MPI_DISTRIBUTE_DFLT_DARG, order, orig_extent,
                           type_old, &type_new, st_offsets+i);
                break;
            }
            if (i != ndims-1) NMPI_Type_free(&type_old);
            type_old = type_new;
        }

        /* add displacement and UB */
        disps[1] = st_offsets[ndims-1];
        tmp_size = 1;
        for (i=ndims-2; i>=0; i--)
        {
            tmp_size *= array_of_gsizes[i+1];
            disps[1] += tmp_size*st_offsets[i];
        }
    }

    disps[1] *= orig_extent;

    disps[2] = orig_extent;
    for (i=0; i<ndims; i++) disps[2] *= array_of_gsizes[i];

    disps[0] = 0;
    blklens[0] = blklens[1] = blklens[2] = 1;
    types[0] = MPI_LB;
    types[1] = type_new;
    types[2] = MPI_UB;

    NMPI_Type_create_struct(3, blklens, disps, types, newtype);

    NMPI_Type_free(&type_new);
    MPIU_Free(st_offsets);
fn_fail:
    MPIU_Free(coords);
    return mpi_errno;
}


MPI_RESULT
MPID_Type_convert_subarray(
    int ndims,
    const int *array_of_sizes,
    const int *array_of_subsizes,
    const int *array_of_starts,
    int order,
    MPI_Datatype oldtype,
    MPI_Datatype *newtype)
{
    MPI_Aint extent, lb, disps[3], size;
    int i, blklens[3];
    MPI_Datatype tmp1, tmp2, types[3];

    MPI_RESULT mpi_errno = NMPI_Type_get_extent(oldtype, &lb, &extent);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if (order == MPI_ORDER_FORTRAN)
    {
        /* dimension 0 changes fastest */
        if (ndims == 1)
        {
            mpi_errno = NMPI_Type_contiguous(array_of_subsizes[0], oldtype, &tmp1);
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            mpi_errno = NMPI_Type_vector(array_of_subsizes[1],
                                         array_of_subsizes[0],
                                         array_of_sizes[0], oldtype, &tmp1);
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }

            size = array_of_sizes[0]*extent;
            for (i=2; i<ndims; i++)
            {
                size *= array_of_sizes[i-1];
                mpi_errno = NMPI_Type_create_hvector(array_of_subsizes[i], 1, size, tmp1, &tmp2);
                NMPI_Type_free(&tmp1);
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
                tmp1 = tmp2;
            }
        }

        /* add displacement and UB */
        disps[1] = array_of_starts[0];
        size = 1;
        for (i=1; i<ndims; i++)
        {
            size *= array_of_sizes[i-1];
            disps[1] += size*array_of_starts[i];
        }
        /* rest done below for both Fortran and C order */
    }
    else /* order == MPI_ORDER_C */
    {
        if (ndims == 1)
        {
            mpi_errno = NMPI_Type_contiguous(array_of_subsizes[0], oldtype, &tmp1);
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        else
        {
            /* dimension ndims-1 changes fastest */
            mpi_errno = NMPI_Type_vector(array_of_subsizes[ndims-2],
                                         array_of_subsizes[ndims-1],
                                         array_of_sizes[ndims-1], oldtype, &tmp1);
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }

            size = array_of_sizes[ndims-1]*extent;
            for (i=ndims-3; i>=0; i--)
            {
                size *= array_of_sizes[i+1];
                mpi_errno = NMPI_Type_create_hvector(array_of_subsizes[i], 1, size, tmp1, &tmp2);
                NMPI_Type_free(&tmp1);
                if( mpi_errno != MPI_SUCCESS )
                {
                    return mpi_errno;
                }
                tmp1 = tmp2;
            }
        }

        /* add displacement and UB */
        disps[1] = array_of_starts[ndims-1];
        size = 1;
        for (i=ndims-2; i>=0; i--)
        {
            size *= array_of_sizes[i+1];
            disps[1] += size*array_of_starts[i];
        }
    }

    disps[1] *= extent;

    disps[2] = extent;
    for (i=0; i<ndims; i++) disps[2] *= array_of_sizes[i];

    disps[0] = 0;
    blklens[0] = blklens[1] = blklens[2] = 1;
    types[0] = MPI_LB;
    types[1] = tmp1;
    types[2] = MPI_UB;

    mpi_errno = NMPI_Type_create_struct(3, blklens, disps, types, newtype);

    NMPI_Type_free(&tmp1);

    return mpi_errno;
}


//
// Array of ext32 sizes for basic types.
//
// Indexed using the same index encoded into the type handle.
//
// The majority of the types have the same size in native and ext32,
// except:
//  - MPI_LONG_DOUBLE
//  - MPI_C_LONG_DOUBLE_COMPLEX
//
static DLOOP_Offset ext32_size_array[] =
{
    MPID_BUILTIN_TYPE_SIZE(MPI_DATATYPE_NULL),
    MPID_BUILTIN_TYPE_SIZE(MPI_CHAR),
    MPID_BUILTIN_TYPE_SIZE(MPI_UNSIGNED_CHAR),
    MPID_BUILTIN_TYPE_SIZE(MPI_SHORT),
    MPID_BUILTIN_TYPE_SIZE(MPI_UNSIGNED_SHORT),
    MPID_BUILTIN_TYPE_SIZE(MPI_INT),
    MPID_BUILTIN_TYPE_SIZE(MPI_UNSIGNED),
    MPID_BUILTIN_TYPE_SIZE(MPI_LONG),
    MPID_BUILTIN_TYPE_SIZE(MPI_UNSIGNED_LONG),
    MPID_BUILTIN_TYPE_SIZE(MPI_LONG_LONG_INT), // also MPI_LONG_LONG
    MPID_BUILTIN_TYPE_SIZE(MPI_FLOAT),
    MPID_BUILTIN_TYPE_SIZE(MPI_DOUBLE),
    0x10,  // MPI_LONG_DOUBLE - != native
    MPID_BUILTIN_TYPE_SIZE(MPI_BYTE),
    MPID_BUILTIN_TYPE_SIZE(MPI_WCHAR),

    MPID_BUILTIN_TYPE_SIZE(MPI_PACKED),
    0x00,  // MPI_LB - invalid in data conversion.
    0x00,  // MPI_UB - invalid in data conversion.

    MPID_BUILTIN_TYPE_SIZE(MPI_C_COMPLEX),
    MPID_BUILTIN_TYPE_SIZE(MPI_C_FLOAT_COMPLEX),
    MPID_BUILTIN_TYPE_SIZE(MPI_C_DOUBLE_COMPLEX),
    0x20,  // MPI_C_LONG_DOUBLE_COMPLEX - != native.

    MPID_BUILTIN_TYPE_SIZE(MPI_2INT),
    MPID_BUILTIN_TYPE_SIZE(MPI_C_BOOL),
    MPID_BUILTIN_TYPE_SIZE(MPI_SIGNED_CHAR),
    MPID_BUILTIN_TYPE_SIZE(MPI_UNSIGNED_LONG_LONG),

/* Fortran types */
    MPID_BUILTIN_TYPE_SIZE(MPI_CHARACTER),
    MPID_BUILTIN_TYPE_SIZE(MPI_INTEGER),
    MPID_BUILTIN_TYPE_SIZE(MPI_REAL),
    MPID_BUILTIN_TYPE_SIZE(MPI_LOGICAL),
    MPID_BUILTIN_TYPE_SIZE(MPI_COMPLEX),
    MPID_BUILTIN_TYPE_SIZE(MPI_DOUBLE_PRECISION),
    MPID_BUILTIN_TYPE_SIZE(MPI_2INTEGER),
    MPID_BUILTIN_TYPE_SIZE(MPI_2REAL),
    MPID_BUILTIN_TYPE_SIZE(MPI_DOUBLE_COMPLEX),
    MPID_BUILTIN_TYPE_SIZE(MPI_2DOUBLE_PRECISION),
    MPID_BUILTIN_TYPE_SIZE(MPI_2COMPLEX),
    MPID_BUILTIN_TYPE_SIZE(MPI_2DOUBLE_COMPLEX),

/* Size-specific types (see MPI 2.2, 16.2.5) */
    0x02,  // MPI_REAL2 - not supported.
    MPID_BUILTIN_TYPE_SIZE(MPI_REAL4),
    MPID_BUILTIN_TYPE_SIZE(MPI_COMPLEX8),
    MPID_BUILTIN_TYPE_SIZE(MPI_REAL8),
    MPID_BUILTIN_TYPE_SIZE(MPI_COMPLEX16),
    0x10,  // MPI_REAL16 - not supported.
    0x20,  // MPI_COMPLEX32 - not supported.
    MPID_BUILTIN_TYPE_SIZE(MPI_INTEGER1),
    0x04,  // MPI_COMPLEX4 - not supported.
    MPID_BUILTIN_TYPE_SIZE(MPI_INTEGER2),
    MPID_BUILTIN_TYPE_SIZE(MPI_INTEGER4),
    MPID_BUILTIN_TYPE_SIZE(MPI_INTEGER8),
    0x10,  // MPI_INTEGER16 - not supported.
    MPID_BUILTIN_TYPE_SIZE(MPI_INT8_T),
    MPID_BUILTIN_TYPE_SIZE(MPI_INT16_T),
    MPID_BUILTIN_TYPE_SIZE(MPI_INT32_T),
    MPID_BUILTIN_TYPE_SIZE(MPI_INT64_T),
    MPID_BUILTIN_TYPE_SIZE(MPI_UINT8_T),
    MPID_BUILTIN_TYPE_SIZE(MPI_UINT16_T),
    MPID_BUILTIN_TYPE_SIZE(MPI_UINT32_T),
    MPID_BUILTIN_TYPE_SIZE(MPI_UINT64_T),

    0x8,  // MPI_AINT - != native for 32-bit.
    MPID_BUILTIN_TYPE_SIZE(MPI_OFFSET),
    MPID_BUILTIN_TYPE_SIZE(MPI_COUNT)
};

C_ASSERT( _countof(ext32_size_array) == DatatypeTraits::BUILTIN );

DLOOP_Offset MPIDI_Datatype_get_basic_size_external32(MPI_Datatype type)
{
    MPIU_Assert( HANDLE_GET_TYPE(type) == HANDLE_TYPE_BUILTIN );
    MPIU_Assert( HANDLE_BUILTIN_INDEX(type) < _countof(ext32_size_array) );
    return ext32_size_array[HANDLE_BUILTIN_INDEX(type)];
}

DLOOP_Offset MPID_Datatype_size_external32(_In_ const MPID_Datatype* type_ptr, MPI_Datatype type)
{
    if (HANDLE_GET_TYPE(type) == HANDLE_TYPE_BUILTIN)
    {
        return MPIDI_Datatype_get_basic_size_external32(type);
    }
    else
    {
        MPID_Dataloop *dlp = type_ptr->GetDataloop( true );
        return MPID_Dataloop_stream_size(dlp,
                                         MPIDI_Datatype_get_basic_size_external32);
    }
}


/* The MPI standard requires that the datatypes that are returned from
   the MPI_Create_f90_xxx be both predefined and return the r (and p
   if real/complex) with which it was created.  This file contains routines
   that keep track of the datatypes that have been created.  The interface
   that is used is a single routine that is passed as input the
   chosen base function and combiner, along with r,p, and returns the
   appropriate datatype, creating it if necessary */


MPI_RESULT MPIR_Create_f90_predefined(MPI_Datatype basetype,
                               int combiner,
                               int precision,
                               int range,
                               MPI_Datatype *newtype )
{
    int i;

    /* Has this type been defined already? */
    for (i=0; i < MAX_F90_TYPES; i++)
    {
        if( f90types[i] == nullptr )
        {
            break;
        }

        if( f90types[i]->contents->combiner != combiner )
        {
            continue;
        }

        if( combiner == MPI_COMBINER_F90_INTEGER )
        {
            if( f90types[i]->contents->Ints()[0] != range )
            {
                continue;
            }
        }
        else
        {
            MPIU_Assert( combiner == MPI_COMBINER_F90_REAL ||
                combiner == MPI_COMBINER_F90_COMPLEX );

            if( f90types[i]->contents->Ints()[0] != precision ||
                f90types[i]->contents->Ints()[1] != range )
            {
                continue;
            }
        }

        //
        // We don't take an extra reference, since users may not free
        // these types.
        //
        *newtype = f90types[i]->handle;
        return MPI_SUCCESS;
    }

    if( i == MAX_F90_TYPES )
    {
        return MPIU_ERR_CREATE(MPI_ERR_INTERN, "**f90typetoomany" );
    }

    int nvals;
    int vals[2];

    if( combiner == MPI_COMBINER_F90_INTEGER )
    {
        nvals = 1;
        vals[0] = range;
    }
    else
    {
        nvals = 2;
        vals[0] = precision;
        vals[1] = range;
    }

    MPID_Datatype* new_dtp;
    MPI_RESULT mpi_errno = MPID_Type_contiguous( 1, basetype, &new_dtp );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // Cache the precision and range parameters such that
    // MPI_Type_get_contents can return them.
    //
    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           combiner,
                                           nvals, /* ints (count) */
                                           0,
                                           0,
                                           vals,
                                           nullptr,
                                           nullptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        new_dtp->Release();
        return mpi_errno;
    }

    //
    // The MPI Standard requires that these types are pre-committed
    // (MPI-2.2, sec 16.2.5, pg 492)
    //
    mpi_errno = MPID_Type_commit( new_dtp );
    if( mpi_errno != MPI_SUCCESS )
    {
        new_dtp->Release();
        return mpi_errno;
    }

    //
    // Parameterized F90 types are permanent (it's erroneous to free them).
    //
    new_dtp->is_permanent   = true;
    f90types[i] = new_dtp;

    *newtype = new_dtp->handle;
    return MPI_SUCCESS;
}
