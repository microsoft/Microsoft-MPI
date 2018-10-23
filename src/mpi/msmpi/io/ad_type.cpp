// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"
#include "adio_extern.h"


void ADIOI_Datatype_iscontig(MPI_Datatype datatype, int *flag)
{
    MPIR_Datatype_iscontig(datatype, flag);

    /* if it is MPICH2 and the datatype is reported as contigous,
       check if the true_lb is non-zero, and if so, mark the
       datatype as noncontiguous */
    if (*flag)
    {
        MPI_Aint true_extent, true_lb;

        NMPI_Type_get_true_extent(datatype, &true_lb, &true_extent);

        if (true_lb > 0)
            *flag = 0;
    }
}


/* Returns MPI_SUCCESS on success, an MPI error code on failure.  Code above
 * needs to call MPIO_Err_return_xxx.
 */
static int MPIOI_Type_block(const int *array_of_gsizes, int dim, int ndims, int nprocs,
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
        blksize = global_size / nprocs;
        if (global_size % nprocs != 0)
        {
            ++blksize;
        }
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
    if (j <= 0)
    {
        *st_offset = mysize = 0;
    }
    else
    {
        mysize = min(blksize, j);
        /* in terms of no. of elements of type oldtype in this dimension */
        *st_offset = blksize * rank;
    }

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

    return MPI_SUCCESS;
}


/* Returns MPI_SUCCESS on success, an MPI error code on failure.  Code above
 * needs to call MPIO_Err_return_xxx.
 */
static int MPIOI_Type_cyclic(const int *array_of_gsizes, int dim, int ndims, int nprocs,
                      int rank, int darg, int order, MPI_Aint orig_extent,
                      MPI_Datatype type_old, MPI_Datatype *type_new,
                      MPI_Aint *st_offset)
{
/* nprocs = no. of processes in dimension dim of grid
   rank = coordinate of this process in dimension dim */
    int blksize, i, blklens[3], st_index, end_index, local_size, rem, count;
    MPI_Aint stride, disps[3];
    MPI_Datatype type_tmp, types[3];

    if (darg == MPI_DISTRIBUTE_DFLT_DARG) blksize = 1;
    else blksize = darg;

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
        local_size += min(rem, blksize);
    }

    count = local_size/blksize;
    rem = local_size % blksize;

    stride = nprocs*blksize*orig_extent;
    if (order == MPI_ORDER_FORTRAN)
        for (i=0; i<dim; i++) stride *= array_of_gsizes[i];
    else for (i=ndims-1; i>dim; i--) stride *= array_of_gsizes[i];

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
        *st_offset = (local_size != 0) ? rank * blksize : 0;
        /* st_offset is in terms of no. of elements of type oldtype in
         * this dimension */
    }

    return MPI_SUCCESS;
}


int ADIO_Type_create_darray(
    _In_ int size,
    _In_ int rank,
    _In_ int ndims,
    _In_reads_(ndims) const int* array_of_gsizes,
    _In_reads_(ndims) const int* array_of_distribs,
    _In_reads_(ndims) int* array_of_dargs,
    _In_reads_(ndims) int* array_of_psizes,
    _In_ int order,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MPI_Datatype type_old, type_new=MPI_DATATYPE_NULL, types[3];
    int procs, tmp_rank, i, tmp_size, blklens[3], *coords;
    MPI_Aint *st_offsets, orig_extent, orig_lb, disps[3];

    NMPI_Type_get_extent(oldtype, &orig_lb, &orig_extent);

/* calculate position in Cartesian grid as MPI would (row-major
   ordering) */
    coords = static_cast<int*>(ADIOI_Malloc(ndims*sizeof(int)));
    procs = size;
    tmp_rank = rank;
    for (i=0; i<ndims; i++)
    {
        procs = procs/array_of_psizes[i];
        coords[i] = tmp_rank/procs;
        tmp_rank = tmp_rank % procs;
    }

    st_offsets = static_cast<MPI_Aint*>(ADIOI_Malloc(ndims*sizeof(MPI_Aint)));
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
            if (i > 0)
            {
                NMPI_Type_free(&type_old);
            }
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
            if (i != ndims-1)
            {
                NMPI_Type_free(&type_old);
            }
            type_old = type_new;
        }

        /* add displacement and UB */
        disps[1] = st_offsets[ndims-1];
        tmp_size = 1;
        i = ndims - 1;
        while (i > 0)
        {
            tmp_size *= array_of_gsizes[i];
            //
            // step down, but never below 0 index
            //
            disps[1] += tmp_size*st_offsets[--i];
        }
    }

    disps[1] *= orig_extent;

    disps[2] = orig_extent;
    for (i=0; i<ndims; i++)
    {
        disps[2] *= array_of_gsizes[i];
    }
    disps[0] = 0;
    blklens[0] = blklens[1] = blklens[2] = 1;
    types[0] = MPI_LB;
    types[1] = type_new;
    types[2] = MPI_UB;

    NMPI_Type_create_struct(3, blklens, disps, types, newtype);

    NMPI_Type_free(&type_new);
    ADIOI_Free(st_offsets);
    ADIOI_Free(coords);
    return MPI_SUCCESS;
}


int ADIO_Type_create_subarray(
    _In_ int ndims,
    _In_reads_(ndims) int* array_of_sizes,
    _In_reads_(ndims) int* array_of_subsizes,
    _In_reads_(ndims) const int* array_of_starts,
    _In_ int order,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    )
{
    MPI_Aint extent, lb, disps[3], size;
    int i, blklens[3];
    MPI_Datatype tmp1, tmp2, types[3];

    NMPI_Type_get_extent(oldtype, &lb, &extent);

    if (order == MPI_ORDER_FORTRAN)
    {
        /* dimension 0 changes fastest */
        if (ndims == 1)
        {
            NMPI_Type_contiguous(array_of_subsizes[0], oldtype, &tmp1);
        }
        else
        {
            NMPI_Type_vector(array_of_subsizes[1],
                            array_of_subsizes[0],
                            array_of_sizes[0], oldtype, &tmp1);

            size = array_of_sizes[0]*extent;
            for (i=2; i<ndims; i++)
            {
                size *= array_of_sizes[i-1];
                NMPI_Type_create_hvector(array_of_subsizes[i], 1, size, tmp1, &tmp2);
                NMPI_Type_free(&tmp1);
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
        /* dimension ndims-1 changes fastest */
        if (ndims == 1)
        {
            NMPI_Type_contiguous(array_of_subsizes[0], oldtype, &tmp1);
        }
        else
        {
            NMPI_Type_vector(array_of_subsizes[ndims-2],
                            array_of_subsizes[ndims-1],
                            array_of_sizes[ndims-1], oldtype, &tmp1);

            size = array_of_sizes[ndims-1]*extent;
            i = ndims - 2;
            while (i > 0)
            {
                size *= array_of_sizes[i];
                //
                // step down, but never below 0
                //
                NMPI_Type_create_hvector(array_of_subsizes[--i], 1, size, tmp1, &tmp2);
                NMPI_Type_free(&tmp1);
                tmp1 = tmp2;
            }
        }

        /* add displacement and UB */
        disps[1] = array_of_starts[ndims-1];
        size = 1;
        i = ndims - 1;
        while (i > 0)
        {
            size *= array_of_sizes[i];
            //
            // step down, but never below 0 index
            //
            disps[1] += size*array_of_starts[--i];
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

    NMPI_Type_create_struct(3, blklens, disps, types, newtype);

    NMPI_Type_free(&tmp1);

    return MPI_SUCCESS;
}
