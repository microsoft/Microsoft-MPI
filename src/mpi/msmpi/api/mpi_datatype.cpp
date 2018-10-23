// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
    MPI_Type_commit - Commits the datatype

Input Parameter:
. datatype - datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Type_commit(
    _In_ MPI_Datatype* datatype
    )
{
    OACR_USE_PTR( datatype );
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_commit(*datatype);

    int mpi_errno;
    TypeHandle hType;

    if( datatype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "datatype" );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( *datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( MpiaDatatypeValidateNotPermanent( hType ) != MPI_SUCCESS )
    {
        goto fn_exit;
    }

    mpi_errno = MPID_Type_commit( hType.Get() );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

  fn_exit:
    TraceLeave_MPI_Type_commit();
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_commit %p",
            datatype
            )
        );
    TraceError(MPI_Type_commit, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Type_contiguous - Creates a contiguous datatype

Input Parameters:
+ count - replication count (nonnegative integer)
- oldtype - old datatype (handle)

Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_COUNT
.N MPI_ERR_EXHAUSTED
@*/
EXTERN_C
MPI_METHOD
MPI_Type_contiguous(
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_contiguous(count, oldtype);

    int mpi_errno = MPI_SUCCESS;
    TypeHandle hOldType;

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }

    if( newtype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newtype" );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Datatype* new_dtp;
    mpi_errno = MPID_Type_contiguous(count,
                                     hOldType.GetMpiHandle(),
                                     &new_dtp);

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           MPI_COMBINER_CONTIGUOUS,
                                           1, /* ints (count) */
                                           0,
                                           1,
                                           &count,
                                           NULL,
                                           &oldtype);

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;

    TraceLeave_MPI_Type_contiguous(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_contiguous %d %D %p",
            count,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_contiguous, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_darray - Create a datatype representing a distributed array

   Input Parameters:
+ size - size of process group (positive integer)
. rank - rank in process group (nonnegative integer)
. ndims - number of array dimensions as well as process grid dimensions (positive integer)
. array_of_gsizes - number of elements of type oldtype in each dimension of global array (array of positive integers)
. array_of_distribs - distribution of array in each dimension (array of state)
. array_of_dargs - distribution argument in each dimension (array of positive integers)
. array_of_psizes - size of process grid in each dimension (array of positive integers)
. order - array storage order flag (state)
- oldtype - old datatype (handle)

    Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
_Pre_satisfies_(
    order == MPI_DISTRIBUTE_DFLT_DARG ||
    (order >= MPI_DISTRIBUTE_BLOCK && order <= MPI_DISTRIBUTE_NONE)
    )
MPI_METHOD
MPI_Type_create_darray(
    _In_range_(>=, 0) int size,
    _In_range_(>=, 0) int rank,
    _In_range_(>=, 0) int ndims,
    _mpi_reads_(ndims) const int array_of_gsizes[],
    _mpi_reads_(ndims) const int array_of_distribs[],
    _mpi_reads_(ndims) const int array_of_dargs[],
    _mpi_reads_(ndims) const int array_of_psizes[],
    _In_ int order,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_darray(size, rank, ndims, TraceArrayLength(1), array_of_gsizes, TraceArrayLength(1), array_of_distribs, TraceArrayLength(1), array_of_dargs,TraceArrayLength(1),array_of_psizes,order,oldtype);

    int i;

    int procs, tmp_rank, tmp_size;
    MPI_Aint orig_extent, disps[3];
    StackGuardArray<MPI_Aint> st_offsets;
    StackGuardArray<int> coords;
    int *ints;
    TypeHandle hOldType;

    int mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( rank < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "rank", rank );
        goto fn_fail;
    }
    if( size < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argnonpos %s %d", "size", size );
        goto fn_fail;
    }
    if( ndims < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argnonpos %s %d", "ndims", ndims );
        goto fn_fail;
    }

    if( array_of_gsizes == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_gsizes" );
        goto fn_fail;
    }
    if( array_of_distribs == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_distribs" );
        goto fn_fail;
    }
    if( array_of_dargs == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_dargs" );
        goto fn_fail;
    }
    if( array_of_psizes == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_psizes" );
        goto fn_fail;
    }
    if (order != MPI_ORDER_C && order != MPI_ORDER_FORTRAN)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "order");
        goto fn_fail;
    }

    for (i=0; i < ndims; i++)
    {
        if( array_of_gsizes[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argnonpos %s %d", "gsize", array_of_gsizes[i] );
            goto fn_fail;
        }
        if( array_of_psizes[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argnonpos %s %d", "psize", array_of_psizes[i] );
            goto fn_fail;
        }

        if ((array_of_distribs[i] != MPI_DISTRIBUTE_NONE) &&
            (array_of_distribs[i] != MPI_DISTRIBUTE_BLOCK) &&
            (array_of_distribs[i] != MPI_DISTRIBUTE_CYCLIC))
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**darrayunknown");
            goto fn_fail;
        }

        if ((array_of_dargs[i] != MPI_DISTRIBUTE_DFLT_DARG) &&
            (array_of_dargs[i] <= 0))
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "array_of_dargs");
            goto fn_fail;
        }

        if ((array_of_distribs[i] == MPI_DISTRIBUTE_NONE) &&
            (array_of_psizes[i] != 1))
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**darraydist %d %d", i, array_of_psizes[i]);
            goto fn_fail;
        }
    }

    orig_extent = MPID_Datatype_get_extent(oldtype);

/* calculate position in Cartesian grid as MPI would (row-major
   ordering) */
    //
    // We allocate a single temporary buffer for the coordinates as well
    // as the ints array used later.  Note that the buffer is never used
    // for both purposes at the same time, so we only need to allocate
    // the largest size, not the sum, which is the ints array size.
    //
    coords = new int[4 * ndims + 4];
    if( coords == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }
    ints = coords;

    procs = size;
    tmp_rank = rank;
    for (i=0; i<ndims; i++)
    {
        procs = procs/array_of_psizes[i];
        coords[i] = tmp_rank/procs;
        tmp_rank = tmp_rank % procs;
    }

    st_offsets = new MPI_Aint[ndims];
    if( st_offsets == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    MPID_Datatype* pOldType = hOldType.Get();
    //
    // Take a reference so we don't have to special case the first iteration in the loops below.
    // We need to initialize pNewType to pOldType for the case where ndmis == 0.
    //
    MPID_Datatype* pNewType = pOldType;
    pOldType->AddRef();

    if (order == MPI_ORDER_FORTRAN)
    {
      /* dimension 0 changes fastest */
        for (i=0; i<ndims; i++)
        {
            switch(array_of_distribs[i])
            {
            case MPI_DISTRIBUTE_BLOCK:
                mpi_errno = MPIR_Type_block(array_of_gsizes,
                                            i,
                                            ndims,
                                            array_of_psizes[i],
                                            coords[i],
                                            array_of_dargs[i],
                                            order,
                                            orig_extent,
                                            pOldType->handle,
                                            &pNewType,
                                            st_offsets+i);
                break;
            case MPI_DISTRIBUTE_CYCLIC:
                mpi_errno = MPIR_Type_cyclic(array_of_gsizes,
                                             i,
                                             ndims,
                                             array_of_psizes[i],
                                             coords[i],
                                             array_of_dargs[i],
                                             order,
                                             orig_extent,
                                             pOldType->handle,
                                             &pNewType,
                                             st_offsets+i);
                break;
            case MPI_DISTRIBUTE_NONE:
            default:
                /* treat it as a block distribution on 1 process */
                mpi_errno = MPIR_Type_block(array_of_gsizes,
                                            i,
                                            ndims,
                                            1,
                                            0,
                                            MPI_DISTRIBUTE_DFLT_DARG,
                                            order,
                                            orig_extent,
                                            pOldType->handle,
                                            &pNewType,
                                            st_offsets+i);
                break;
            }

            pOldType->Release();
            pOldType = pNewType;

            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
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
                mpi_errno = MPIR_Type_block(array_of_gsizes,
                                            i,
                                            ndims,
                                            array_of_psizes[i],
                                            coords[i],
                                            array_of_dargs[i],
                                            order,
                                            orig_extent,
                                            pOldType->handle,
                                            &pNewType,
                                            st_offsets+i);
                break;
            case MPI_DISTRIBUTE_CYCLIC:
                mpi_errno = MPIR_Type_cyclic(array_of_gsizes,
                                             i,
                                             ndims,
                                             array_of_psizes[i],
                                             coords[i],
                                             array_of_dargs[i],
                                             order,
                                             orig_extent,
                                             pOldType->handle,
                                             &pNewType,
                                             st_offsets+i);
                break;
            case MPI_DISTRIBUTE_NONE:
            default:
                /* treat it as a block distribution on 1 process */
                mpi_errno = MPIR_Type_block(array_of_gsizes,
                                            i,
                                            ndims,
                                            array_of_psizes[i],
                                            coords[i],
                                            MPI_DISTRIBUTE_DFLT_DARG,
                                            order,
                                            orig_extent,
                                            pOldType->handle,
                                            &pNewType,
                                            st_offsets+i);
                break;
            }

            pOldType->Release();
            pOldType = pNewType;

            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
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
    int blklens[3];
    blklens[0] = blklens[1] = blklens[2] = 1;
    MPI_Datatype types[3];
    types[0] = MPI_LB;
    types[1] = pNewType->handle;
    types[2] = MPI_UB;

    MPID_Datatype* datatype_ptr;
    mpi_errno = MPID_Type_struct(3,
                                 blklens,
                                 disps,
                                 types,
                                 &datatype_ptr
                                 );

    pNewType->Release();

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* at this point we have the new type, and we've cleaned up any
     * intermediate types created in the process.  we just need to save
     * all our contents/envelope information.
     */

    /* Save contents */
    ints[0] = size;
    ints[1] = rank;
    ints[2] = ndims;

    int* gsizes = &ints[3];
    int* distribs = gsizes + ndims;
    int* dargs = distribs + ndims;
    int* psizes = dargs + ndims;
    for (i=0; i < ndims; i++)
    {
        *gsizes = array_of_gsizes[i];
        gsizes++;

        *distribs = array_of_distribs[i];
        distribs++;

        *dargs = array_of_dargs[i];
        dargs++;

        *psizes = array_of_psizes[i];
        psizes++;
    }
    ints[4*ndims + 3] = order;
    mpi_errno = MPID_Datatype_set_contents(datatype_ptr,
                                           MPI_COMBINER_DARRAY,
                                           4*ndims + 4,
                                           0,
                                           1,
                                           ints,
                                           NULL,
                                           &oldtype);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = datatype_ptr->handle;
    TraceLeave_MPI_Type_create_darray(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_darray %d %d %d %p %p %p %p %d %D %p",
            size,
            rank,
            ndims,
            array_of_gsizes,
            array_of_distribs,
            array_of_dargs,
            array_of_psizes,
            order,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_create_darray, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_hindexed - Create a datatype for an indexed datatype with
   displacements in bytes

   Input Parameters:
+ count - number of blocks --- also number of entries in
  displacements and blocklengths (integer)
. blocklengths - number of elements in each block (array of nonnegative integers)
. displacements - byte displacement of each block (array of integer)
- oldtype - old datatype (handle)

   Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_hindexed(
    _In_range_(>=, 0) int count,
    _mpi_reads_(count) const int array_of_blocklengths[],
    _mpi_reads_(count) const MPI_Aint array_of_displacements[],
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_hindexed(count, TraceArrayLength(1), array_of_blocklengths, TraceArrayLength(1), (const void**)array_of_displacements, oldtype);

    int mpi_errno = MPI_SUCCESS;
    int i;
    TypeHandle hOldType;

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }
    if (count > 0)
    {
        if( array_of_blocklengths == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_blocklengths" );
            goto fn_fail;
        }
        if( array_of_displacements == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_displacements" );
            goto fn_fail;
        }
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    for (i=0; i < count; i++)
    {
        if( array_of_blocklengths[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "blocklen", array_of_blocklengths[i] );
            goto fn_fail;
        }
    }
    if( newtype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newtype" );
        goto fn_fail;
    }

    MPID_Datatype* new_dtp;
    mpi_errno = MPID_Type_indexed(count,
                                  array_of_blocklengths,
                                  array_of_displacements,
                                  true, /* displacements in bytes */
                                  oldtype,
                                  &new_dtp
                                  );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;
    TraceLeave_MPI_Type_create_hindexed(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_hindexed %d %p %p %D %p",
            count,
            array_of_blocklengths,
            array_of_displacements,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_create_hindexed, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_hindexed_block - Create an indexed datatype with
     constant-sized blocks and displacements in bytes

Input Parameters:
+ count - number of blocks --- also number of entries in
  displacements (integer)
. blocklength - number of elements in every block (integer)
. array_of_displacements - byte displacement of each block (array of integer)
- oldtype - old datatype (handle)

Output Parameters:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_hindexed_block(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _mpi_reads_(count) const MPI_Aint array_of_displacements[],
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_hindexed_block( count, blocklength, TraceArrayLength(1), (const void**)array_of_displacements, oldtype );

    int mpi_errno;
    MPID_Datatype *new_dtp;
    int ints[2];
    TypeHandle hOldType;

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );
        goto fn_fail;
    }
    if( blocklength < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "blocklength", blocklength );
        goto fn_fail;
    }
    if( count > 0 && array_of_displacements == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_displacements" );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Type_blockindexed(
        count,
        blocklength,
        array_of_displacements,
        true, // displacement in bytes
        oldtype,
        &new_dtp
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    ints[0] = count;
    ints[1] = blocklength;

    mpi_errno = MPID_Datatype_set_contents(
        new_dtp,
        MPI_COMBINER_HINDEXED_BLOCK,
        2,     // number of ints
        count, // number of aints
        1,     // number of types
        ints,
        array_of_displacements,
        &oldtype
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;
    TraceLeave_MPI_Type_create_hindexed_block( *newtype );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_hindexed_block %d %d %p %D %p",
            count,
            blocklength,
            array_of_displacements,
            oldtype,
            newtype
            )
        );
    TraceError( MPI_Type_create_hindexed_block, mpi_errno );
    goto fn_exit;
}


/*@
   MPI_Type_create_hvector - Create a datatype with a constant stride given
     in bytes

   Input Parameters:
+ count - number of blocks (nonnegative integer)
. blocklength - number of elements in each block (nonnegative integer)
. stride - number of bytes between start of each block (integer)
- oldtype - old datatype (handle)

   Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_hvector(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _In_ MPI_Aint stride,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_hvector(count, blocklength, stride, oldtype);

    int mpi_errno = MPI_SUCCESS;
    MPID_Datatype *new_dtp;
    int ints[2];
    TypeHandle hOldType;

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }
    if( blocklength < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "blocklen", blocklength );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newtype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newtype" );
        goto fn_fail;
    }

    mpi_errno = MPID_Type_vector(count,
                                 blocklength,
                                 stride,
                                 true, /* stride in bytes */
                                 oldtype,
                                 &new_dtp
                                 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    ints[0] = count;
    ints[1] = blocklength;
    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           MPI_COMBINER_HVECTOR,
                                           2, /* ints (count, blocklength) */
                                           1, /* aints */
                                           1, /* types */
                                           ints,
                                           &stride,
                                           &oldtype);

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;
    TraceLeave_MPI_Type_create_hvector(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_hvector %d %d %d %D %p",
            count,
            blocklength,
            stride,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_create_hvector, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_indexed_block - Create an indexed
     datatype with constant-sized blocks

   Input Parameters:
+ count - length of array of displacements (integer)
. blocklength - size of block (integer)
. array_of_displacements - array of displacements (array of integer)
- oldtype - old datatype (handle)

    Output Parameter:
. newtype - new datatype (handle)

Notes:
The indices are displacements, and are based on a zero origin.  A common error
is to do something like the following
.vb
    integer a(100)
    integer blens(10), indices(10)
    do i=1,10
10       indices(i) = 1 + (i-1)*10
    call MPI_TYPE_CREATE_INDEXED_BLOCK(10,1,indices,MPI_INTEGER,newtype,ierr)
    call MPI_TYPE_COMMIT(newtype,ierr)
    call MPI_SEND(a,1,newtype,...)
.ve
expecting this to send 'a(1),a(11),...' because the indices have values
'1,11,...'.   Because these are `displacements` from the beginning of 'a',
it actually sends 'a(1+1),a(1+11),...'.

If you wish to consider the displacements as indices into a Fortran array,
consider declaring the Fortran array with a zero origin
.vb
    integer a(0:99)
.ve

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_indexed_block(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _mpi_reads_(count) const int array_of_displacements[],
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_indexed_block(count, blocklength, TraceArrayLength(1), array_of_displacements, oldtype);

    int mpi_errno;
    MPID_Datatype *new_dtp;
    int i;
    StackGuardArray<int> ints;
    TypeHandle hOldType;

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }
    if( blocklength < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "blocklen", blocklength );
        goto fn_fail;
    }
    if (count > 0)
    {
        if( array_of_displacements == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_displacements" );
            goto fn_fail;
        }
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Type_blockindexed(count,
                                       blocklength,
                                       array_of_displacements,
                                       false, /* dispinbytes */
                                       oldtype,
                                       &new_dtp);

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    ints = new int[count + 2];
    if( ints == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    ints[0] = count;
    ints[1] = blocklength;

    for (i=0; i < count; i++)
    {
        ints[i+2] = array_of_displacements[i];
    }

    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           MPI_COMBINER_INDEXED_BLOCK,
                                           count + 2, /* ints */
                                           0, /* aints */
                                           1, /* types */
                                           ints,
                                           NULL,
                                           &oldtype);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;

    TraceLeave_MPI_Type_create_indexed_block(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_indexed_block %d %d %p %D %p",
            count,
            blocklength,
            array_of_displacements,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_create_indexed_block, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_resized - Create a datatype with a new lower bound and
     extent from an existing datatype

   Input Parameters:
+ oldtype - input datatype (handle)
. lb - new lower bound of datatype (integer)
- extent - new extent of datatype (integer)

   Output Parameter:
. newtype - output datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_resized(
    _In_ MPI_Datatype oldtype,
    _In_ MPI_Aint lb,
    _In_range_(>=, 0) MPI_Aint extent,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_resized(oldtype, lb, extent);

    TypeHandle hOldType;

    int mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Type_create_resized(oldtype, lb, extent, newtype);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MPI_Aint aints[2];
    aints[0] = lb;
    aints[1] = extent;

    MPID_Datatype* new_dtp = TypePool::Get(*newtype);
    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           MPI_COMBINER_RESIZED,
                                           0,
                                           2, /* Aints */
                                           1,
                                           NULL,
                                           aints,
                                           &oldtype);

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Type_create_resized(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_resized %D %d %d %p",
            oldtype,
            lb,
            extent,
            newtype
            )
        );
    TraceError(MPI_Type_create_resized, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_struct - Create an MPI datatype from a general set of
   datatypes, displacements, and block sizes

   Input Parameters:
+ count - number of blocks (integer) --- also number of entries in arrays
  array_of_types, array_of_displacements and array_of_blocklengths
. array_of_blocklength - number of elements in each block (array of integer)
. array_of_displacements - byte displacement of each block (array of integer)
- array_of_types - type of elements in each block (array of handles to
  datatype objects)

   Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_struct(
    _In_range_(>=, 0) int count,
    _mpi_reads_(count) const int array_of_blocklengths[],
    _mpi_reads_(count) const MPI_Aint array_of_displacements[],
    _mpi_reads_(count) const MPI_Datatype array_of_types[],
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_struct(count,TraceArrayLength(1),array_of_blocklengths,TraceArrayLength(1),(const void**)array_of_displacements,TraceArrayLength(1),array_of_types);

    int mpi_errno = MPI_SUCCESS;
    int i;
    StackGuardArray<int> ints;
    MPID_Datatype *new_dtp;

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", count );
        goto fn_fail;
    }

    if (count > 0)
    {
        if( array_of_blocklengths == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_blocklengths" );
            goto fn_fail;
        }
        if( array_of_displacements == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_displacements" );
            goto fn_fail;
        }
        if( array_of_types == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_types" );
            goto fn_fail;
        }
    }

    for (i=0; i < count; i++)
    {
        TypeHandle hOldType;

        if( array_of_blocklengths[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "blocklen", array_of_blocklengths[i] );
            goto fn_fail;
        }

        mpi_errno = MpiaDatatypeValidateHandle( array_of_types[i], "array_of_types", &hOldType );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    mpi_errno = MPID_Type_struct(count,
                                 array_of_blocklengths,
                                 array_of_displacements,
                                 array_of_types,
                                 &new_dtp
                                 );

    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ints = new int[count + 1];
    if( ints == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    ints[0] = count;
    for (i=0; i < count; i++)
    {
        ints[i+1] = array_of_blocklengths[i];
    }

    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           MPI_COMBINER_STRUCT,
                                           count+1, /* ints (cnt,blklen) */
                                           count, /* aints (disps) */
                                           count, /* types */
                                           ints,
                                           array_of_displacements,
                                           array_of_types);

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;
    TraceLeave_MPI_Type_create_struct(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_struct %d %p %p %p %p",
            count,
            array_of_blocklengths,
            array_of_displacements,
            array_of_types,
            newtype
            )
        );
    TraceError(MPI_Type_create_struct, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_subarray - Create a datatype for a subarray of a regular,
    multidimensional array

   Input Parameters:
+ ndims - number of array dimensions (positive integer)
. array_of_sizes - number of elements of type oldtype in each dimension of the
  full array (array of positive integers)
. array_of_subsizes - number of elements of type oldtype in each dimension of
  the subarray (array of positive integers)
. array_of_starts - starting coordinates of the subarray in each dimension
  (array of nonnegative integers)
. order - array storage order flag (state)
- oldtype - array element datatype (handle)

   Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_subarray(
    _In_range_(>=, 0) int ndims,
    _mpi_reads_(ndims) const int array_of_sizes[],
    _mpi_reads_(ndims) const int array_of_subsizes[],
    _mpi_reads_(ndims) const int array_of_starts[],
    _In_range_(MPI_ORDER_C, MPI_ORDER_FORTRAN) int order,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_subarray(ndims,TraceArrayLength(1),array_of_sizes,TraceArrayLength(1),array_of_subsizes,TraceArrayLength(1),array_of_starts,order,oldtype);

    int mpi_errno, i;

    /* these variables are from the original version in ROMIO */
    MPI_Aint size, extent, lb, disps[3];
    int blklens[3];
    MPI_Datatype types[3];

    /* for saving contents */
    StackGuardArray<int> ints;
    TypeHandle hOldType;

    /* Check parameters */
    if( ndims < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argnonpos %s %d", "ndims", ndims );
        goto fn_fail;
    }
    if( array_of_sizes == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_sizes" );
        goto fn_fail;
    }
    if( array_of_subsizes == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_subsizes" );
        goto fn_fail;
    }
    if( array_of_starts == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_starts" );
        goto fn_fail;
    }
    for (i=0; i < ndims; i++)
    {
        if( array_of_sizes[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**argnonpos %s %d",
                "size",
                array_of_sizes[i]
            );
            goto fn_fail;
        }
        if( array_of_subsizes[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**argnonpos %s %d",
                "subsize",
                array_of_subsizes[i]
            );
            goto fn_fail;
        }
        if( array_of_starts[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**argneg %s %d",
                "start",
                array_of_starts[i]
            );
            goto fn_fail;
        }
        if (array_of_subsizes[i] > array_of_sizes[i])
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**argrange %s %d %d",
                "array_of_subsizes",
                array_of_subsizes[i],
                array_of_sizes[i]
            );
            goto fn_fail;
        }
        if (array_of_starts[i] > (array_of_sizes[i] - array_of_subsizes[i]))
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**argrange %s %d %d",
                "array_of_starts",
                array_of_starts[i],
                array_of_sizes[i] - array_of_subsizes[i]
            );
            goto fn_fail;
        }
    }
    if (order != MPI_ORDER_FORTRAN && order != MPI_ORDER_C)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "order");
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* TODO: GRAB EXTENT WITH A MACRO OR SOMETHING FASTER */
    NMPI_Type_get_extent(oldtype, &lb, &extent);

    MPID_Datatype* pTmp1;
    MPID_Datatype* pTmp2;
    if (order == MPI_ORDER_FORTRAN)
    {
        if (ndims == 1)
        {
            mpi_errno = MPID_Type_contiguous(array_of_subsizes[0],
                                             oldtype,
                                             &pTmp1);
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            mpi_errno = MPID_Type_vector(array_of_subsizes[1],
                                         array_of_subsizes[0],
                                         array_of_sizes[0],
                                         false, /* stride in types */
                                         oldtype,
                                         &pTmp1
                                         );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            size = array_of_sizes[0]*extent;
            for (i=2; i<ndims; i++)
            {
                size *= array_of_sizes[i-1];
                mpi_errno = MPID_Type_vector(array_of_subsizes[i],
                                             1,
                                             size,
                                             true, /* stride in bytes */
                                             pTmp1->handle,
                                             &pTmp2
                                             );
                pTmp1->Release();

                if( mpi_errno != MPI_SUCCESS )
                {
                    goto fn_fail;
                }

                pTmp1 = pTmp2;
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
    else /* MPI_ORDER_C */
    {
        /* dimension ndims-1 changes fastest */
        if (ndims == 1)
        {
            mpi_errno = MPID_Type_contiguous(array_of_subsizes[0],
                                             oldtype,
                                             &pTmp1);

            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            mpi_errno = MPID_Type_vector(array_of_subsizes[ndims-2],
                                         array_of_subsizes[ndims-1],
                                         array_of_sizes[ndims-1],
                                         false, /* stride in types */
                                         oldtype,
                                         &pTmp1
                                         );

            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            size = array_of_sizes[ndims-1]*extent;
            for (i=ndims-3; i>=0; i--)
            {
                size *= array_of_sizes[i+1];
                mpi_errno = MPID_Type_vector(array_of_subsizes[i],
                                             1,    /* blocklen */
                                             size, /* stride */
                                             true,    /* stride in bytes */
                                             pTmp1->handle, /* old type */
                                             &pTmp2
                                             );

                pTmp1->Release();

                if( mpi_errno != MPI_SUCCESS )
                {
                    goto fn_fail;
                }

                pTmp1 = pTmp2;
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
    types[1] = pTmp1->handle;
    types[2] = MPI_UB;

    /* TODO:
     * if we were to do all this as an mpid function, we could just
     * directly adjust the LB and UB in the MPID_Datatype structure
     * instead of jumping through this hoop.
     *
     * i suppose we could do the same thing here...
     *
     * another alternative would be to use MPID_Type_create_resized()
     * instead of building the struct.  that would also be cleaner.
     */
    MPID_Datatype* new_dtp;
    mpi_errno = MPID_Type_struct(3,
                                 blklens,
                                 disps,
                                 types,
                                 &new_dtp
                                 );

    pTmp1->Release();

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* at this point we have the new type, and we've cleaned up any
     * intermediate types created in the process.  we just need to save
     * all our contents/envelope information.
     */

    /* Save contents */
    ints = new int[3 * ndims + 2];
    if( ints == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    ints[0] = ndims;
    int* sizes = &ints[1];
    int* subsizes = sizes + ndims;
    int* starts = subsizes + ndims;

    for (i=0; i < ndims; i++)
    {
        *sizes = array_of_sizes[i];
        sizes++;

        *subsizes = array_of_subsizes[i];
        subsizes++;

        *starts = array_of_starts[i];
        starts++;
    }
    ints[3*ndims + 1] = order;

    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           MPI_COMBINER_SUBARRAY,
                                           3 * ndims + 2, /* ints */
                                           0, /* aints */
                                           1, /* types */
                                           ints,
                                           NULL,
                                           &oldtype);

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;
    TraceLeave_MPI_Type_create_subarray(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_subarray %d %p %p %p %d %D %p",
            ndims,
            array_of_sizes,
            array_of_subsizes,
            array_of_starts,
            order,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_create_subarray, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_dup - Duplicate a datatype

   Input Parameter:
. type - datatype (handle)

   Output Parameter:
. newtype - copy of type (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Type_dup(
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_dup(oldtype);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newtype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newtype" );
        goto fn_fail;
    }

    MPID_Datatype* new_dtp;
    mpi_errno = MPID_Type_dup(hType.GetMpiHandle(), &new_dtp);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                           MPI_COMBINER_DUP,
                                           0, /* ints */
                                           0, /* aints */
                                           1, /* types */
                                           NULL,
                                           NULL,
                                           &oldtype);

    mpi_errno = MPID_Type_commit(new_dtp);
    ON_ERROR_FAIL(mpi_errno);

    /* Copy attributes, executing the attribute copy functions */
    /* This accesses the attribute dup function through the perprocess
       structure to prevent type_dup from forcing the linking of the
       attribute functions.  The actual function is (by default)
       MPIR_Attr_dup_list
    */
    if (mpi_errno == MPI_SUCCESS)
    {
        new_dtp->attributes = 0;
        mpi_errno = MPIR_Attr_dup_list( oldtype,
            hType.Get()->attributes,
            &new_dtp->attributes );
        if( mpi_errno != MPI_SUCCESS )
        {
            new_dtp->Release();
            *newtype = MPI_DATATYPE_NULL;
            goto fn_fail;
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;

    TraceLeave_MPI_Type_dup(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_dup %D %p",
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_dup, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Type_extent - Returns the extent of a datatype

Input Parameters:
. datatype - datatype (handle)

Output Parameter:
. extent - datatype extent (integer)

.N SignalSafe

.N Deprecated
The replacement for this routine is 'MPI_Type_get_extent'.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Type_extent(
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Aint* extent
    )
{
    MPI_Aint lb;
    return NMPI_Type_get_extent( datatype, &lb, extent );
}


/*@
    MPI_Type_free - Frees the datatype

Input Parameter:
. datatype - datatype that is freed (handle)

Predefined types:

The MPI standard states that (in Opaque Objects)
.Bqs
MPI provides certain predefined opaque objects and predefined, static handles
to these objects. Such objects may not be destroyed.
.Bqe

Thus, it is an error to free a predefined datatype.  The same section makes
it clear that it is an error to free a null datatype.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_free(
    _Deref_out_range_(==, MPI_DATATYPE_NULL) _Inout_ MPI_Datatype* datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_free(*datatype);

    int mpi_errno;
    TypeHandle hType;

    if( datatype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "datatype" );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( *datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateNotPermanent( hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (hType.Get()->attributes != NULL)
    {
        mpi_errno = MPIR_Attr_delete_list( hType.GetMpiHandle(), &hType.Get()->attributes );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    hType.Get()->Release();
    *datatype = MPI_DATATYPE_NULL;

    TraceLeave_MPI_Type_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_free %p",
            datatype
            )
        );
    TraceError(MPI_Type_free, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_get_contents - get type contents

   Arguments:
+  MPI_Datatype datatype - datatype
.  int max_integers - max integers
.  int max_addresses - max addresses
.  int max_datatypes - max datatypes
.  int array_of_integers[] - integers
.  MPI_Aint array_of_addresses[] - addresses
-  MPI_Datatype array_of_datatypes[] - datatypes

   Notes:

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_contents(
    _In_ MPI_Datatype datatype,
    _In_range_(>=, 0) int max_integers,
    _In_range_(>=, 0) int max_addresses,
    _In_range_(>=, 0) int max_datatypes,
    _mpi_writes_(max_integers) int array_of_integers[],
    _mpi_writes_(max_addresses) MPI_Aint array_of_addresses[],
    _mpi_writes_(max_datatypes) MPI_Datatype array_of_datatypes[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_contents(datatype, max_integers, max_addresses, max_datatypes);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateNotPermanent( hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( max_integers > 0 && array_of_integers == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_integers" );
        goto fn_fail;
    }

    if( max_addresses > 0 && array_of_addresses == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_addresses" );
        goto fn_fail;
    }

    if( max_datatypes > 0 && array_of_datatypes == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_datatypes" );
        goto fn_fail;
    }

    mpi_errno = MPID_Type_get_contents(hType.GetMpiHandle(),
                                       max_integers,
                                       max_addresses,
                                       max_datatypes,
                                       array_of_integers,
                                       array_of_addresses,
                                       array_of_datatypes);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Type_get_contents(TraceArrayLength(1),array_of_integers,TraceArrayLength(1),(const void**)array_of_addresses,TraceArrayLength(1),array_of_datatypes);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_contents %D %d %d %d %p %p %p",
            datatype,
            max_integers,
            max_addresses,
            max_datatypes,
            array_of_integers,
            array_of_addresses,
            array_of_datatypes
            )
        );
    TraceError(MPI_Type_get_contents, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_get_envelope - get type envelope

   Arguments:
+  MPI_Datatype datatype - datatype
.  int *num_integers - num integers
.  int *num_addresses - num addresses
.  int *num_datatypes - num datatypes
-  int *combiner - combiner

   Notes:

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_envelope(
    _In_ MPI_Datatype datatype,
    _Out_ _Deref_out_range_(>=, 0) int* num_integers,
    _Out_ _Deref_out_range_(>=, 0) int* num_addresses,
    _Out_ _Deref_out_range_(>=, 0) int* num_datatypes,
    _Out_ _Deref_out_range_(MPI_COMBINER_NAMED, MPI_COMBINER_RESIZED) int* combiner
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_envelope(datatype);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( num_integers == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "num_integers" );
        goto fn_fail;
    }

    if( num_addresses == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "num_addresses" );
        goto fn_fail;
    }

    if( num_datatypes == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "num_datatypes" );
        goto fn_fail;
    }

    if( combiner == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "combiner" );
        goto fn_fail;
    }

    MPID_Type_get_envelope(hType.GetMpiHandle(),
                           num_integers,
                           num_addresses,
                           num_datatypes,
                           combiner);

    TraceLeave_MPI_Type_get_envelope(*num_integers, *num_addresses, *num_datatypes, *combiner);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_envelope %D %p %p %p %p",
            datatype,
            num_integers,
            num_addresses,
            num_datatypes,
            combiner
            )
        );
    TraceError(MPI_Type_get_envelope, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Type_get_extent - Get the lower bound and extent for a Datatype

   Input Parameter:
. datatype - datatype to get information on (handle)

   Output Parameters:
+ lb - lower bound of datatype (integer)
- extent - extent of datatype (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_extent(
    _In_ MPI_Datatype datatype,
    _mpi_out_(lb, MPI_UNDEFINED) MPI_Aint* lb,
    _mpi_out_(extent, MPI_UNDEFINED) MPI_Aint* extent
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_extent(datatype);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( lb == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "lb" );
        goto fn_fail;
    }

    if( extent == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "extent" );
        goto fn_fail;
    }

    MPI_Count lb_x;
    MPI_Count extent_x = hType.GetExtentAndLowerBound( &lb_x );

    *lb = static_cast<MPI_Aint>( lb_x );
    *extent = static_cast<MPI_Aint>( extent_x );

#ifndef _WIN64
    if( lb_x > INT_MAX )
    {
        *lb = MPI_UNDEFINED;
    }
    if( extent_x > INT_MAX )
    {
        *extent = MPI_UNDEFINED;
    }
#endif

    TraceLeave_MPI_Type_get_extent(*lb, *extent);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_extent %D %p %p",
            datatype,
            lb,
            extent
            )
        );
    TraceError(MPI_Type_get_extent, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_get_extent_x - Get the lower bound and extent for a Datatype

   Input Parameter:
. datatype - datatype to get information on (handle)

   Output Parameters:
+ lb - lower bound of datatype (MPI_Count)
- extent - extent of datatype (MPI_Count)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_extent_x(
    _In_ MPI_Datatype datatype,
    _mpi_out_(lb, MPI_UNDEFINED) MPI_Count *lb,
    _mpi_out_(extent, MPI_UNDEFINED) MPI_Count *extent
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_extent_x( datatype );

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( lb == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "lb" );
        goto fn_fail;
    }

    if( extent == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "extent" );
        goto fn_fail;
    }

    *extent = hType.GetExtentAndLowerBound( lb );

    TraceLeave_MPI_Type_get_extent_x( *lb, *extent );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_extent_x %D %p %p",
            datatype,
            lb,
            extent
            )
        );
    TraceError( MPI_Type_get_extent_x, mpi_errno );
    goto fn_exit;
}


/*@
   MPI_Type_get_name - Get the print name for a datatype

   Input Parameter:
. type - datatype whose name is to be returned (handle)

   Output Parameters:
+ type_name - the name previously stored on the datatype, or a empty string
  if no such name exists (string)
- resultlen - length of returned name (integer)

.N ThreadSafeNoUpdate

.N NULL

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_name(
    _In_ MPI_Datatype datatype,
    _Out_writes_z_(MPI_MAX_OBJECT_NAME) char* type_name,
    _Out_ int* resultlen
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_name(datatype);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( type_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "type_name" );
        goto fn_fail;
    }

    if( resultlen == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "resultlen" );
        goto fn_fail;
    }

    /* Include the null in MPI_MAX_OBJECT_NAME */
    MPIU_Strncpy( type_name, hType.Get()->name, MPI_MAX_OBJECT_NAME );

    //
    // Cast is ok here because type_name is at most MPI_MAX_OBJECT_NAME which is
    // currently defined to be 128.
    //
    C_ASSERT(MPI_MAX_OBJECT_NAME <= INT_MAX);
    *resultlen = static_cast<int>( MPIU_Strlen( type_name, MPI_MAX_OBJECT_NAME ) );

    TraceLeave_MPI_Type_get_name( *resultlen, type_name );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_name %D %p %p",
            datatype,
            type_name,
            resultlen
            )
        );
    TraceError(MPI_Type_get_name, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_get_true_extent - Get the true lower bound and extent for a
     datatype

   Input Parameter:
. datatype - datatype to get information on (handle)

   Output Parameters:
+ true_lb - true lower bound of datatype (integer)
- true_extent - true size of datatype (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_true_extent(
    _In_ MPI_Datatype datatype,
    _mpi_out_(true_lb, MPI_UNDEFINED) MPI_Aint* true_lb,
    _mpi_out_(true_extent, MPI_UNDEFINED) MPI_Aint* true_extent
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_true_extent(datatype);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( true_lb == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "true_lb" );
        goto fn_fail;
    }

    if( true_extent == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "true_extent" );
        goto fn_fail;
    }

    MPI_Count true_lb_x;
    MPI_Count true_extent_x = hType.GetTrueExtentAndLowerBound( &true_lb_x );

    *true_lb = static_cast<MPI_Aint>( true_lb_x );
    *true_extent = static_cast<MPI_Aint>( true_extent_x );

#ifndef _WIN64
    if( true_lb_x > INT_MAX )
    {
        *true_lb = MPI_UNDEFINED;
    }
    if( true_extent_x > INT_MAX )
    {
        *true_extent = MPI_UNDEFINED;
    }
#endif

    TraceLeave_MPI_Type_get_true_extent(*true_lb, *true_extent);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_true_extent %D %p %p",
            datatype,
            true_lb,
            true_extent
            )
        );
    TraceError(MPI_Type_get_true_extent, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_get_true_extent_x - Get the true lower bound and extent for a
     datatype

   Input Parameter:
. datatype - datatype to get information on (handle)

   Output Parameters:
+ true_lb - true lower bound of datatype (MPI_Count)
- true_extent - true size of datatype (MPI_Count)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_true_extent_x(
    _In_ MPI_Datatype datatype,
    _mpi_out_(true_lb, MPI_UNDEFINED) MPI_Count *true_lb,
    _mpi_out_(true_extent, MPI_UNDEFINED) MPI_Count *true_extent
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_true_extent_x( datatype );

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( true_lb == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "true_lb" );
        goto fn_fail;
    }

    if( true_extent == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "true_extent" );
        goto fn_fail;
    }

    *true_extent = hType.GetTrueExtentAndLowerBound( true_lb );

    TraceLeave_MPI_Type_get_true_extent_x( *true_lb, *true_extent );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_true_extent_x %D %p %p",
            datatype,
            true_lb,
            true_extent
            )
        );
    TraceError( MPI_Type_get_true_extent_x, mpi_errno );
    goto fn_exit;
}


/*@
    MPI_Type_indexed - Creates an indexed datatype

Input Parameters:
+ count - number of blocks -- also number of entries in indices and blocklens
. blocklens - number of elements in each block (array of nonnegative integers)
. indices - displacement of each block in multiples of oldtype (array of
  integers)
- oldtype - old datatype (handle)

Output Parameter:
. newtype - new datatype (handle)

.N ThreadSafe

.N Fortran

The indices are displacements, and are based on a zero origin.  A common error
is to do something like to following
.vb
    integer a(100)
    integer blens(10), indices(10)
    do i=1,10
         blens(i)   = 1
10       indices(i) = 1 + (i-1)*10
    call MPI_TYPE_INDEXED(10,blens,indices,MPI_INTEGER,newtype,ierr)
    call MPI_TYPE_COMMIT(newtype,ierr)
    call MPI_SEND(a,1,newtype,...)
.ve
expecting this to send 'a(1),a(11),...' because the indices have values
'1,11,...'.   Because these are `displacements` from the beginning of 'a',
it actually sends 'a(1+1),a(1+11),...'.

If you wish to consider the displacements as indices into a Fortran array,
consider declaring the Fortran array with a zero origin
.vb
    integer a(0:99)
.ve

.N Errors
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
.N MPI_ERR_EXHAUSTED
@*/
EXTERN_C
MPI_METHOD
MPI_Type_indexed(
    _In_range_(>=, 0) int count,
    _mpi_reads_(count) const int array_of_blocklengths[],
    _mpi_reads_(count) const int array_of_displacements[],
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_indexed(
        count,
        TraceArrayLength(1),
        array_of_blocklengths,
        TraceArrayLength(1),
        array_of_displacements,
        oldtype
        );

    int mpi_errno;
    MPID_Datatype *new_dtp;
    int i;
    TypeHandle hOldType;

    /* Validate parameters and objects (post conversion) */
    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", count );
        goto fn_fail;
    }

    if (count > 0)
    {
        if( array_of_blocklengths == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "blocklens" );
            goto fn_fail;
        }
        if( array_of_displacements == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "indices" );
            goto fn_fail;
        }
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    for (i=0; i < count; i++)
    {
        if( array_of_blocklengths[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "blocklen", array_of_blocklengths[i] );
            goto fn_fail;
        }
    }
    if( newtype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newtype" );
        goto fn_fail;
    }

    mpi_errno = MPID_Type_indexed(count,
                                  array_of_blocklengths,
                                  array_of_displacements,
                                  false, /* displacements not in bytes */
                                  hOldType.GetMpiHandle(),
                                  &new_dtp
                                  );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;
    TraceLeave_MPI_Type_indexed(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_indexed %d %p %p %D %p",
            count,
            array_of_blocklengths,
            array_of_displacements,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_indexed, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Type_lb - Returns the lower-bound of a datatype

Input Parameters:
. datatype - datatype (handle)

Output Parameter:
. displacement - displacement of lower bound from origin,
                             in bytes (integer)

.N Deprecated
The replacement for this routine is 'MPI_Type_get_extent'.

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_lb(
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Aint* displacement
    )
{
    MPI_Aint extent;
    return NMPI_Type_get_extent( datatype, displacement, &extent );
}


/*@
   MPI_Type_match_size - Find an MPI datatype matching a specified size

   Input Parameters:
+ typeclass - generic type specifier (integer)
- size - size, in bytes, of representation (integer)

   Output Parameter:
. type - datatype with correct type, size (handle)

Notes:
'typeclass' is one of 'MPI_TYPECLASS_REAL', 'MPI_TYPECLASS_INTEGER' and
'MPI_TYPECLASS_COMPLEX', corresponding to the desired typeclass.
The function returns an MPI datatype matching a local variable of type
'( typeclass, size )'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_match_size(
    _In_ int typeclass,
    _In_ int size,
    _Out_ MPI_Datatype* datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_match_size(typeclass, size);

    int mpi_errno = MPI_SUCCESS;
    static const char *tname = 0;
    static MPI_Datatype real_types[] = { MPI_FLOAT, MPI_DOUBLE
                                         ,MPI_LONG_DOUBLE
    };
    static MPI_Datatype int_types[] = { MPI_CHAR, MPI_SHORT, MPI_INT,
                                        MPI_LONG,
                                        MPI_LONG_LONG
    };
    static MPI_Datatype complex_types[] = { MPI_COMPLEX, MPI_DOUBLE_COMPLEX };
    MPI_Datatype matched_datatype = MPI_DATATYPE_NULL;
    int i, tsize;

    /* Validate parameters and objects (post conversion) */
    if( datatype == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "datatype" );
        goto fn_fail;
    }

    /* FIXME: Should make use of Fortran optional types (e.g., MPI_INTEGER2) */

    /* The following implementation follows the suggestion in the
       MPI-2 standard.
       The version in the MPI-2 spec makes use of the Fortran optional types;
       currently, we don't support these from C (see mpi.h.in).
       Thus, we look at the candidate types and make use of the first fit.
    */
    switch (typeclass)
    {
    case MPI_TYPECLASS_REAL:
        tname = "MPI_TYPECLASS_REAL";
        for (i=0; i<sizeof(real_types)/sizeof(MPI_Datatype); i++)
        {
            if (real_types[i] == MPI_DATATYPE_NULL) { continue; }
            NMPI_Type_size( real_types[i], &tsize );
            if (tsize == size)
            {
                matched_datatype = real_types[i];
                break;
            }
        }
        break;
    case MPI_TYPECLASS_INTEGER:
        tname = "MPI_TYPECLASS_INTEGER";
        for (i=0; i<sizeof(int_types)/sizeof(MPI_Datatype); i++)
        {
            if (int_types[i] == MPI_DATATYPE_NULL) { continue; }
            NMPI_Type_size( int_types[i], &tsize );
            if (tsize == size)
            {
                matched_datatype = int_types[i];
                break;
            }
        }
        break;
    case MPI_TYPECLASS_COMPLEX:
        tname = "MPI_TYPECLASS_COMPLEX";
        for (i=0; i<sizeof(complex_types)/sizeof(MPI_Datatype); i++)
        {
            if (complex_types[i] == MPI_DATATYPE_NULL) { continue; }
            NMPI_Type_size( complex_types[i], &tsize );
            if (tsize == size)
            {
                matched_datatype = complex_types[i];
                break;
            }
        }
        break;
    default:
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**typematchnoclass");
        goto fn_fail;
    }

    if (matched_datatype == MPI_DATATYPE_NULL)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**typematchsize %s %d", tname, size);
        goto fn_fail;
    }

    *datatype = matched_datatype;

    TraceLeave_MPI_Type_match_size(*datatype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_match_size %d %d %p",
            typeclass,
            size,
            datatype
            )
        );
    TraceError(MPI_Type_match_size, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_set_name - set datatype name

   Input Parameters:
+ type - datatype whose identifier is to be set (handle)
- type_name - the character string which is remembered as the name (string)

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_TYPE
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Type_set_name(
    _In_ MPI_Datatype datatype,
    _In_z_ const char* type_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_set_name(datatype, type_name);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( type_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "type_name" );
        goto fn_fail;
    }

    if( SetName<MPID_Datatype>( hType.Get(), type_name ) == false )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    TraceLeave_MPI_Type_set_name();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_set_name %D %s",
            datatype,
            type_name
            )
        );
    TraceError(MPI_Type_set_name, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Type_size - Return the number of bytes occupied by entries
                    in the datatype

Input Parameters:
. datatype - datatype (handle)

Output Parameter:
. size - datatype size (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_size(
    _In_ MPI_Datatype datatype,
    _mpi_out_(size, MPI_UNDEFINED) int* size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_size(datatype);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "size" );
        goto fn_fail;
    }

    MPI_Count size_x = hType.GetSize();

    if( size_x > INT_MAX )
    {
        *size = MPI_UNDEFINED;
    }
    else
    {
        *size = static_cast<int>( size_x );
    }


    TraceLeave_MPI_Type_size(*size);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_size %D %p",
            datatype,
            size
            )
        );
    TraceError(MPI_Type_size, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Type_size_x - Return the number of bytes occupied by entries
                    in the datatype

Input Parameters:
. datatype - datatype (handle)

Output Parameter:
. size - datatype size (MPI_Count)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_size_x(
    _In_ MPI_Datatype datatype,
    _mpi_out_(size, MPI_UNDEFINED) MPI_Count *size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_size_x( datatype );

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "size" );
        goto fn_fail;
    }

    *size = hType.GetSize();

    TraceLeave_MPI_Type_size_x( *size );
  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_size_x %D %p",
            datatype,
            size
            )
        );
    TraceError( MPI_Type_size_x, mpi_errno );
    goto fn_exit;
}


/*@
    MPI_Type_ub - Returns the upper bound of a datatype

Input Parameters:
. datatype - datatype (handle)

Output Parameter:
. displacement - displacement of upper bound from origin,
                             in bytes (integer)

.N Deprecated
The replacement for this routine is 'MPI_Type_get_extent'

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_ub(
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Aint* displacement
    )
{
    MPI_Aint lb;
    MPI_Aint extent;
    int mpi_errno = NMPI_Type_get_extent( datatype, &lb, &extent );
    if( mpi_errno != MPI_SUCCESS )
        return mpi_errno;

    *displacement = lb + extent;
    return MPI_SUCCESS;
}


/*@
    MPI_Type_vector - Creates a vector (strided) datatype

Input Parameters:
+ count - number of blocks (nonnegative integer)
. blocklength - number of elements in each block
  (nonnegative integer)
. stride - number of elements between start of each block (integer)
- oldtype - old datatype (handle)

Output Parameter:
. newtype_p - new datatype (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Type_vector(
    _In_range_(>=, 0) int count,
    _In_range_(>=, 0) int blocklength,
    _In_ int stride,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_vector(count, blocklength, stride, oldtype);

    TypeHandle hOldType;
    int mpi_errno;

    /* Validate parameters, especially handles needing to be converted */
    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }
    if( blocklength < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "blocklen", blocklength );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( oldtype, "oldtype", &hOldType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Datatype *new_dtp;
    mpi_errno = MPID_Type_vector(count,
                                 blocklength,
                                 (MPI_Aint) stride,
                                 false, /* stride not in bytes, in extents */
                                 hOldType.GetMpiHandle(),
                                 &new_dtp
                                 );

    if (mpi_errno == MPI_SUCCESS)
    {
        int ints[3];

        ints[0] = count;
        ints[1] = blocklength;
        ints[2] = stride;
        mpi_errno = MPID_Datatype_set_contents(new_dtp,
                                               MPI_COMBINER_VECTOR,
                                               3, /* ints (cnt, blklen, str) */
                                               0, /* aints */
                                               1, /* types */
                                               ints,
                                               NULL,
                                               &oldtype);
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newtype = new_dtp->handle;
    TraceLeave_MPI_Type_vector(*newtype);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_vector %d %d %d %D %p",
            count,
            blocklength,
            stride,
            oldtype,
            newtype
            )
        );
    TraceError(MPI_Type_vector, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_f90_integer - Return a predefined type that matches
   the specified range

   Input Arguments:
.  range - Decimal range (number of digits) desired

   Output Arguments:
.  newtype - A predefine MPI Datatype that matches the range.

   Notes:
If there is no corresponding type for the specified range, the call is
erroneous.  This implementation sets 'newtype' to 'MPI_DATATYPE_NULL' and
returns an error of class 'MPI_ERR_ARG'.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_f90_integer(
    _In_ int r,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();

    int mpi_errno;

    MPI_Datatype basetype;

    if( r <= F90_INTEGER1_RANGE )
    {
        basetype = MPI_INTEGER1;
    }
    else if( r <= F90_INTEGER2_RANGE )
    {
        basetype = MPI_INTEGER2;
    }
    else if( r <= F90_INTEGER4_RANGE )
    {
        basetype = MPI_INTEGER4;
    }
    else if( r <= F90_INTEGER8_RANGE )
    {
        basetype = MPI_INTEGER8;
    }
    else
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**f90typeintnone %d", r );
        goto fn_exit;
    }

    mpi_errno = MPIR_Create_f90_predefined(
        basetype,
        MPI_COMBINER_F90_INTEGER,
        -1,
        r,
        newtype
        );

fn_exit:
    MpiaExit();
    return mpi_errno;
}


/*@
   MPI_Type_create_f90_real - Return a predefined type that matches
   the specified range

   Input Arguments:
+  precision - Number of decimal digits in mantissa
-  range - Decimal exponent range desired

   Output Arguments:
.  newtype - A predefine MPI Datatype that matches the range.

   Notes:
If there is no corresponding type for the specified range, the call is
erroneous.  This implementation sets 'newtype' to 'MPI_DATATYPE_NULL' and
returns an error of class 'MPI_ERR_ARG'.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_f90_real(
    _In_ int p,
    _In_ int r,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();

    int mpi_errno;

    MPI_Datatype basetype;

    if( r <= F90_REAL4_RANGE && p <= F90_REAL4_PRECISION )
    {
        basetype = MPI_REAL4;
    }
    else if( r <= F90_REAL8_RANGE && p <= F90_REAL8_PRECISION )
    {
        basetype = MPI_REAL8;
    }
    else
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**f90typerealnone %d %d", p, r );
        goto fn_exit;
    }

    mpi_errno = MPIR_Create_f90_predefined(
        basetype,
        MPI_COMBINER_F90_REAL,
        p,
        r,
        newtype
        );

fn_exit:
    MpiaExit();
    return mpi_errno;
}


/*@
   MPI_Type_create_f90_complex - Return a predefined type that matches
   the specified range

   Input Arguments:
.  range - Decimal range desired

   Output Arguments:
.  newtype - A predefine MPI Datatype that matches the range.

   Notes:
If there is no corresponding type for the specified range, the call is
erroneous.  This implementation sets 'newtype' to 'MPI_DATATYPE_NULL' and
returns an error of class 'MPI_ERR_ARG'.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_f90_complex(
    _In_ int p,
    _In_ int r,
    _Out_ MPI_Datatype* newtype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();

    int mpi_errno;

    MPI_Datatype basetype;

    if( r <= F90_REAL4_RANGE && p <= F90_REAL4_PRECISION )
    {
        basetype = MPI_COMPLEX8;
    }
    else if( r <= F90_REAL8_RANGE && p <= F90_REAL8_PRECISION )
    {
        basetype = MPI_COMPLEX16;
    }
    else
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**f90typecomplexnone %d %d", p, r );
        goto fn_exit;
    }

    mpi_errno = MPIR_Create_f90_predefined(
        basetype,
        MPI_COMBINER_F90_COMPLEX,
        p,
        r,
        newtype
        );

fn_exit:
    MpiaExit();
    return mpi_errno;
}
