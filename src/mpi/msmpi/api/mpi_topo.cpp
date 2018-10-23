// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2015 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


#define MAX_CART_DIM 16

/*@

MPI_Topo_test - Determines the type of topology (if any) associated with a
                communicator

Input Parameter:
. comm - communicator (handle)

Output Parameter:
. top_type - topology type of communicator 'comm' (integer).  If the
  communicator has no associated topology, returns 'MPI_UNDEFINED'.

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG

.seealso: MPI_Graph_create, MPI_Cart_create
@*/
EXTERN_C
MPI_METHOD
MPI_Topo_test(
    _In_ MPI_Comm comm,
    _Out_ int* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Topo_test(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    const MPIR_Topology* topo_ptr = MPIR_Topology_get( comm_ptr );
    if( topo_ptr != nullptr )
    {
        *status = topo_ptr->kind;
    }
    else
    {
        *status = MPI_UNDEFINED;
    }

    TraceLeave_MPI_Topo_test(*status);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_topo_test %C %p",
            comm,
            status
            )
        );
    TraceError(MPI_Topo_test, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Dims_create - Creates a division of processors in a cartesian grid

 Input Parameters:
+ nnodes - number of nodes in a grid (integer)
- ndims - number of cartesian dimensions (integer)

 In/Out Parameter:
. dims - integer array of size  'ndims' specifying the number of nodes in each
 dimension.  A value of 0 indicates that 'MPI_Dims_create' should fill in a
 suitable value.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Dims_create(
    _In_range_(>, 0) int nnodes,
    _In_range_(>=, 0) int ndims,
    _Inout_updates_opt_(ndims) int dims[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Dims_create(nnodes, ndims);

    int mpi_errno = MPI_SUCCESS;

    if (ndims == 0)
    {
        goto fn_exit;
    }

    if( nnodes <= 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argnonpos %s %d", "nnodes", nnodes );
        goto fn_fail;
    }
    if( ndims < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "ndims", ndims );
        goto fn_fail;
    }
    if( dims == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "dims" );
        goto fn_fail;
    }

    mpi_errno = MPIR_Dims_create( nnodes, ndims, dims );

    TraceLeave_MPI_Dims_create();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_dims_create %d %d %p",
            nnodes,
            ndims,
            dims
            )
        );
    TraceError(MPI_Dims_create, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Cart_coords - Determines process coords in cartesian topology given
                  rank in group

Input Parameters:
+ comm - communicator with cartesian structure (handle)
. rank - rank of a process within group of 'comm' (integer)
- maxdims - length of vector 'coords' in the calling program (integer)

Output Parameter:
. coords - integer array (of size 'ndims') containing the Cartesian
  coordinates of specified process (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_RANK
.N MPI_ERR_DIMS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cart_coords(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int rank,
    _In_range_(>=, 0) int maxdims,
    _Out_writes_opt_(maxdims) int coords[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cart_coords(comm, rank, maxdims);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPIR_Topology *cart_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_CART,
        const_cast<MPIR_Topology**>(&cart_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( cart_ptr->topo.cart.ndims > maxdims )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**dimsmany %d %d", cart_ptr->topo.cart.ndims, maxdims);
        goto fn_fail;
    }

    if( cart_ptr->topo.cart.ndims != 0 )
    {
        if( coords == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "coords" );
            goto fn_fail;
        }
    }

    mpi_errno = MpiaCommValidateRank( comm_ptr, rank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Calculate coords */
    int nnodes = cart_ptr->topo.cart.nnodes;
    for( int i = 0; i < cart_ptr->topo.cart.ndims; i++ )
    {
        __analysis_assume( i < maxdims );
        nnodes    = nnodes / cart_ptr->topo.cart.dims[i];
        coords[i] = rank / nnodes;
        rank      = rank % nnodes;
    }

    TraceLeave_MPI_Cart_coords(cart_ptr->topo.cart.ndims, TraceArrayLength(cart_ptr->topo.cart.ndims), coords);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cart_coords %C %d %d %p",
            comm,
            rank,
            maxdims,
            coords
            )
        );
    TraceError(MPI_Cart_coords, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Cart_create - Makes a new communicator to which topology information
                  has been attached

Input Parameters:
+ comm_old - input communicator (handle)
. ndims - number of dimensions of cartesian grid (integer)
. dims - integer array of size ndims specifying the number of processes in
  each dimension
. periods - logical array of size ndims specifying whether the grid is
  periodic (true) or not (false) in each dimension
- reorder - ranking may be reordered (true) or not (false) (logical)

Output Parameter:
. comm_cart - communicator with new cartesian topology (handle)

Algorithm:
We ignore 'reorder' info currently.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_DIMS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cart_create(
    _In_ MPI_Comm comm_old,
    _In_range_(>=, 0) int ndims,
    _In_reads_opt_(ndims) const int dims[],
    _In_reads_opt_(ndims) const int periods[],
    _In_ int reorder,
    _Out_ MPI_Comm* comm_cart
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cart_create(comm_old, ndims, TraceArrayLength(ndims), dims, TraceArrayLength(ndims), periods, reorder);

    MPID_Comm *comm_ptr = nullptr;
    int mpi_errno = MpiaCommValidateIntracomm( comm_old, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( comm_cart == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm_cart" );
        goto fn_fail;
    }
    if( ndims < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "ndims", ndims );
        goto fn_fail;
    }

    if (ndims > 0)
    {
        if( dims == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "dims" );
            goto fn_fail;
        }
        if( periods == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "periods" );
            goto fn_fail;
        }
    }

    mpi_errno = MPIR_Cart_create( comm_ptr, ndims, dims, periods, reorder, comm_cart );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Cart_create(*comm_cart);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cart_create %C %d %p %p %d %p",
            comm_old,
            ndims,
            dims,
            periods,
            reorder,
            comm_cart
            )
        );
    TraceError(MPI_Cart_create, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Cart_get - Retrieves Cartesian topology information associated with a
               communicator

Input Parameters:
+ comm - communicator with cartesian structure (handle)
- maxdims - length of vectors  'dims', 'periods', and 'coords'
in the calling program (integer)

Output Parameters:
+ dims - number of processes for each cartesian dimension (array of integer)
. periods - periodicity (true/false) for each cartesian dimension
(array of logical)
- coords - coordinates of calling process in cartesian structure
(array of integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cart_get(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int maxdims,
    _Out_writes_opt_(maxdims) int dims[],
    _Out_writes_opt_(maxdims) int periods[],
    _Out_writes_opt_(maxdims) int coords[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cart_get(comm, maxdims);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPIR_Topology *cart_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_CART,
        const_cast<MPIR_Topology**>(&cart_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( cart_ptr->topo.cart.ndims > maxdims )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**dimsmany %d %d", cart_ptr->topo.cart.ndims, maxdims);
        goto fn_fail;
    }

    if( cart_ptr->topo.cart.ndims != 0 )
    {
        if( dims == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "dims" );
            goto fn_fail;
        }
        if( periods == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "periods" );
            goto fn_fail;
        }
        if( coords == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "coords" );
            goto fn_fail;
        }
    }

    size_t cbCopy = sizeof(int) * cart_ptr->topo.cart.ndims;
    CopyMemory( dims, cart_ptr->topo.cart.dims, cbCopy );
    CopyMemory( periods, cart_ptr->topo.cart.periodic, cbCopy );
    CopyMemory( coords, cart_ptr->topo.cart.position, cbCopy );

    TraceLeave_MPI_Cart_get(cart_ptr->topo.cart.ndims, TraceArrayLength(cart_ptr->topo.cart.ndims),dims,TraceArrayLength(cart_ptr->topo.cart.ndims),periods, TraceArrayLength(cart_ptr->topo.cart.ndims),coords);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cart_get %C %d %p %p %p",
            comm,
            maxdims,
            dims,
            periods,
            coords
            )
        );
    TraceError(MPI_Cart_get, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Cart_map - Maps process to Cartesian topology information

Input Parameters:
+ comm - input communicator (handle)
. ndims - number of dimensions of Cartesian structure (integer)
. dims - integer array of size 'ndims' specifying the number of processes in
  each coordinate direction
- periods - logical array of size 'ndims' specifying the periodicity
  specification in each coordinate direction

Output Parameter:
. newrank - reordered rank of the calling process; 'MPI_UNDEFINED' if
  calling process does not belong to grid (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_DIMS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cart_map(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int ndims,
    _In_reads_opt_(ndims) const int dims[],
    _In_reads_opt_(ndims) const int periods[],
    _Out_ _Deref_out_range_(>=, MPI_UNDEFINED) int* newrank
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cart_map(comm, ndims, TraceArrayLength(ndims), dims, TraceArrayLength(ndims), periods);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newrank == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newrank" );
        goto fn_fail;
    }
    if( dims == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "dims" );
        goto fn_fail;
    }
    //
    // As of MPI 2.1, zero dimension Cartesian topologies are valid.
    //
    if (ndims < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DIMS, "**dims %d", ndims );
        goto fn_fail;
    }

    mpi_errno = MPIR_Cart_map( comm_ptr, ndims, dims, periods, newrank );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Cart_map(*newrank);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cart_map %C %d %p %p %p",
            comm,
            ndims,
            dims,
            periods,
            newrank
            )
        );
    TraceError(MPI_Cart_map, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Cart_rank - Determines process rank in communicator given Cartesian
                location

Input Parameters:
+ comm - communicator with cartesian structure (handle)
- coords - integer array (of size 'ndims', the number of dimensions of
    the Cartesian topology associated with 'comm') specifying the cartesian
  coordinates of a process

Output Parameter:
. rank - rank of specified process (integer)

Notes:
 Out-of-range coordinates are erroneous for non-periodic dimensions.
 Versions of MPICH before 1.2.2 returned 'MPI_PROC_NULL' for the rank in this
 case.

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_RANK
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cart_rank(
    _In_ MPI_Comm comm,
    _In_ const int coords[],
    _Out_ _Deref_out_range_(>=, 0) int* rank
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cart_rank(comm, TraceArrayLength(1), coords);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( rank == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "rank" );
        goto fn_fail;
    }

    const MPIR_Topology* cart_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_CART,
        const_cast<MPIR_Topology**>(&cart_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    int i, coord, multiplier;

    /* Validate coordinates */
    int ndims = cart_ptr->topo.cart.ndims;
    if( ndims != 0 && coords == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "coords" );
        goto fn_fail;
    }

    for( i = 0; i < ndims; i++ )
    {
        if( cart_ptr->topo.cart.periodic[i] == 0 )
        {
            coord = coords[i];
            if( coord < 0 || coord >= cart_ptr->topo.cart.dims[i] )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_ARG,
                    "**cartcoordinvalid %d %d %d",
                    i,
                    coords[i],
                    cart_ptr->topo.cart.dims[i] - 1
                    );
                goto fn_fail;
            }
        }
    }

    ndims = cart_ptr->topo.cart.ndims;
    *rank = 0;
    multiplier = 1;
    for ( i = ndims - 1; i >= 0; i-- )
    {
        coord = coords[i];
        if ( cart_ptr->topo.cart.periodic[i] )
        {
            if (coord >= cart_ptr->topo.cart.dims[i])
            {
                coord = coord % cart_ptr->topo.cart.dims[i];
            }
            else if (coord <  0)
            {
                coord = coord % cart_ptr->topo.cart.dims[i];
                if (coord != 0 )
                {
                    coord = cart_ptr->topo.cart.dims[i] + coord;
                }
            }
        }
        *rank += multiplier * coord;
        multiplier *= cart_ptr->topo.cart.dims[i];
    }

    TraceLeave_MPI_Cart_rank(*rank);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cart_rank %C %p %p",
            comm,
            coords,
            rank
            )
        );
    TraceError(MPI_Cart_rank, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Cart_shift - Returns the shifted source and destination ranks, given a
                 shift direction and amount

Input Parameters:
+ comm - communicator with cartesian structure (handle)
. direction - coordinate dimension of shift (integer)
- displ - displacement (> 0: upwards shift, < 0: downwards shift) (integer)

Output Parameters:
+ source - rank of source process (integer)
- dest - rank of destination process (integer)

Notes:
The 'direction' argument is in the range '[0,n-1]' for an n-dimensional
Cartesian mesh.

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cart_shift(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int direction,
    _In_ int disp,
    _Out_ _Deref_out_range_(>=, MPI_PROC_NULL)  int* rank_source,
    _Out_ _Deref_out_range_(>=, MPI_PROC_NULL)  int* rank_dest
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cart_shift(comm, direction, disp);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( rank_source == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "rank_source" );
        goto fn_fail;
    }
    if( rank_dest == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "rank_dest" );
        goto fn_fail;
    }
    if( direction < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "direction", direction );
        goto fn_fail;
    }
    /* Nothing in the standard indicates that a zero displacement
        is not valid, so we don't check for a zero shift */

    const MPIR_Topology* cart_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_CART,
        const_cast<MPIR_Topology**>(&cart_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( cart_ptr->topo.cart.ndims == 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_TOPOLOGY, "**dimszero");
        goto fn_fail;
    }

    if(direction >= cart_ptr->topo.cart.ndims)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**dimsmany %d %d", cart_ptr->topo.cart.ndims, direction);
        goto fn_fail;
    }

    int i;
    /* Check for the case of a 0 displacement */
    int rank = comm_ptr->rank;
    if (disp == 0)
    {
        *rank_source = *rank_dest = rank;
    }
    else
    {
        int pos[MAX_CART_DIM];
        /* To support advanced implementations that support MPI_Cart_create,
           we compute the new position and call MPI_Cart_rank to get the
           source and destination.  We could bypass that step if we know that
           the mapping is trivial.  Copy the current position. */
        __analysis_assume(cart_ptr->topo.cart.ndims > 0);
        __analysis_assume(direction < cart_ptr->topo.cart.ndims);
        for (i=0; i<cart_ptr->topo.cart.ndims; i++)
        {
            pos[i] = cart_ptr->topo.cart.position[i];
        }
        /* We must return MPI_PROC_NULL if shifted over the edge of a
           non-periodic mesh */
        pos[direction] += disp;
        if (!cart_ptr->topo.cart.periodic[direction] &&
            (pos[direction] >= cart_ptr->topo.cart.dims[direction] ||
             pos[direction] < 0))
        {
            *rank_dest = MPI_PROC_NULL;
        }
        else
        {
            (void) NMPI_Cart_rank( comm, pos, rank_dest );
        }

        pos[direction] = cart_ptr->topo.cart.position[direction] - disp;
        if (!cart_ptr->topo.cart.periodic[direction] &&
            (pos[direction] >= cart_ptr->topo.cart.dims[direction] ||
             pos[direction] < 0))
        {
            *rank_source = MPI_PROC_NULL;
        }
        else
        {
            (void) NMPI_Cart_rank( comm, pos, rank_source );
        }
    }

    TraceLeave_MPI_Cart_shift(*rank_source, *rank_dest);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cart_shift %C %d %d %p %p",
            comm,
            direction,
            disp,
            rank_source,
            rank_dest
            )
        );
    TraceError(MPI_Cart_shift, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Cart_sub - Partitions a communicator into subgroups which
               form lower-dimensional cartesian subgrids

Input Parameters:
+ comm - communicator with cartesian structure (handle)
- remain_dims - the  'i'th entry of remain_dims specifies whether the 'i'th
dimension is kept in the subgrid (true) or is dropped (false) (logical
vector)

Output Parameter:
. newcomm - communicator containing the subgrid that includes the calling
process (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cart_sub(
    _In_ MPI_Comm comm,
    _In_ const int remain_dims[],
    _Out_ MPI_Comm* newcomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cart_sub(comm, TraceArrayLength(1), remain_dims );

    int key, color, ndims_in_subcomm, nnodes_in_subcomm, i, j, rank;
    MPID_Comm *newcomm_ptr = NULL;
    StackGuardPointer<MPIR_Topology, CppDeallocator<void>> toponew_ptr;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newcomm" );
        goto fn_fail;
    }

    /* Check that the communicator already has a Cartesian topology */
    const MPIR_Topology* topo_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_CART,
        const_cast<MPIR_Topology**>(&topo_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    int ndims = topo_ptr->topo.cart.ndims;

    if( ndims > 0 && remain_dims == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "remain_dims" );
        goto fn_fail;
    }


    bool all_false = true;  /* all entries in remain_dims are false */
    for (i=0; i<ndims; i++)
    {
        if (remain_dims[i] != 0)
        {
            all_false = false;
            break;
        }
    }

    if (all_false)
    {
        /* ndims=0, or all entries in remain_dims are false.
           MPI 2.1 says return a 0D Cartesian topology. */
        mpi_errno = MPIR_Cart_create(comm_ptr, 0, NULL, NULL, 0, newcomm);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
    }
    else
    {
        /* Determine the number of remaining dimensions */
        ndims_in_subcomm = 0;
        nnodes_in_subcomm = 1;
        for (i=0; i<ndims; i++)
        {
            if (remain_dims[i])
            {
                ndims_in_subcomm ++;
                nnodes_in_subcomm *= topo_ptr->topo.cart.dims[i];
            }
        }

        /* Split this communicator.  Do this even if there are no remaining
        dimensions so that the topology information is attached */
        key   = 0;
        color = 0;
        for (i=0; i<ndims; i++)
        {
            if (remain_dims[i])
            {
                key = (key * topo_ptr->topo.cart.dims[i]) +
                    topo_ptr->topo.cart.position[i];
            }
            else
            {
                color = (color * topo_ptr->topo.cart.dims[i]) +
                    topo_ptr->topo.cart.position[i];
            }
        }

        //TODO: should use local comm var, not output param for newcomm.
        mpi_errno = NMPI_Comm_split( comm, color, key, newcomm );

        if( mpi_errno != MPI_SUCCESS || *newcomm == MPI_COMM_NULL )
        {
            goto fn_fail;
        }

        newcomm_ptr = CommPool::Get( *newcomm );

        /* Save the topology of this new communicator */
        toponew_ptr = static_cast<MPIR_Topology*>(
            MPIU_Malloc( sizeof(MPIR_Topology) + (sizeof(int) * 3 * ndims_in_subcomm) )
            );
        if( toponew_ptr == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        toponew_ptr->kind             = MPI_CART;
        toponew_ptr->topo.cart.ndims  = ndims_in_subcomm;
        toponew_ptr->topo.cart.nnodes = nnodes_in_subcomm;
        toponew_ptr->topo.cart.dims = reinterpret_cast<int*>( toponew_ptr + 1 );
        toponew_ptr->topo.cart.periodic = toponew_ptr->topo.cart.dims + ndims_in_subcomm;
        toponew_ptr->topo.cart.position = toponew_ptr->topo.cart.periodic + ndims_in_subcomm;

        j = 0;
        for (i=0; i<ndims; i++)
        {
            if (remain_dims[i])
            {
                toponew_ptr->topo.cart.dims[j] = topo_ptr->topo.cart.dims[i];
                toponew_ptr->topo.cart.periodic[j] = topo_ptr->topo.cart.periodic[i];
                j++;
            }
        }

        /* Compute the position of this process in the new communicator */
        rank = newcomm_ptr->rank;
        for (i=0; i<ndims_in_subcomm; i++)
        {
            nnodes_in_subcomm /= toponew_ptr->topo.cart.dims[i];
            toponew_ptr->topo.cart.position[i] = rank / nnodes_in_subcomm;
            rank = rank % nnodes_in_subcomm;
        }

        mpi_errno = MPIR_Topology_put( newcomm_ptr, toponew_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
        toponew_ptr.detach();
    }

    TraceLeave_MPI_Cart_sub(*newcomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    if( newcomm_ptr != NULL )
    {
        MPIR_Comm_release(newcomm_ptr, 0);
        *newcomm = MPI_COMM_NULL;
    }
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cart_sub %C %p %p",
            comm,
            remain_dims,
            newcomm
            )
        );
    TraceError(MPI_Cart_sub, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Cartdim_get - Retrieves Cartesian topology information associated with a
                  communicator

Input Parameter:
. comm - communicator with cartesian structure (handle)

Output Parameter:
. ndims - number of dimensions of the cartesian structure (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Cartdim_get(
    _In_ MPI_Comm comm,
    _Out_ int* ndims
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cartdim_get(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( ndims == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "ndims" );
        goto fn_fail;
    }

    const MPIR_Topology* cart_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_CART,
        const_cast<MPIR_Topology**>(&cart_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *ndims = cart_ptr->topo.cart.ndims;

    TraceLeave_MPI_Cartdim_get(*ndims);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cartdim_get %C %p",
            comm,
            ndims
            )
        );
    TraceError(MPI_Cartdim_get, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Graph_get - Retrieves graph topology information associated with a
                communicator

Input Parameters:
+ comm - communicator with graph structure (handle)
. maxindex - length of vector 'index' in the calling program  (integer)
- maxedges - length of vector 'edges' in the calling program  (integer)

Output Parameters:
+ index - array of integers containing the graph structure (for details see the definition of 'MPI_GRAPH_CREATE')
- edges - array of integers containing the graph structure

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Graph_get(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int maxindex,
    _In_range_(>=, 0) int maxedges,
    _Out_writes_opt_(maxindex) int index[],
    _Out_writes_opt_(maxedges) int edges[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Graph_get(comm, maxindex, maxedges);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( edges == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "edges" );
        goto fn_fail;
    }

    if( index == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "index" );
        goto fn_fail;
    }

    const MPIR_Topology* topo_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_GRAPH,
        const_cast<MPIR_Topology**>(&topo_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if(topo_ptr->topo.graph.nnodes > maxindex)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**argrange %s %d %d", "maxindex", maxindex, topo_ptr->topo.graph.nnodes);
        goto fn_fail;
    }

    if(topo_ptr->topo.graph.nedges > maxedges)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**argrange %s %d %d", "maxedges", maxedges, topo_ptr->topo.graph.nedges);
        goto fn_fail;
    }

    /* Get index */
    CopyMemory( index, topo_ptr->topo.graph.index, sizeof(int) * topo_ptr->topo.graph.nnodes );
    /* Get edges */
    CopyMemory( edges, topo_ptr->topo.graph.edges, sizeof(int) * topo_ptr->topo.graph.nedges );

    TraceLeave_MPI_Graph_get(topo_ptr->topo.graph.nnodes, index, topo_ptr->topo.graph.nedges, edges);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_graph_get %C %d %d %p %p",
            comm,
            maxindex,
            maxedges,
            index,
            edges
            )
        );
    TraceError(MPI_Graph_get, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Graph_map - Maps process to graph topology information

Input Parameters:
+ comm - input communicator (handle)
. nnodes - number of graph nodes (integer)
. index - integer array specifying the graph structure, see 'MPI_GRAPH_CREATE'
- edges - integer array specifying the graph structure

Output Parameter:
. newrank - reordered rank of the calling process; 'MPI_UNDEFINED' if the
calling process does not belong to graph (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Graph_map(
    _In_ MPI_Comm comm,
    _In_range_(>, 0) int nnodes,
    _In_reads_opt_(nnodes) const int index[],
    _In_opt_ const int edges[],
    _Out_ _Deref_out_range_(>=, MPI_UNDEFINED) int* newrank
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Graph_map(comm, nnodes);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newrank == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newrank" );
        goto fn_fail;
    }

    if( index == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "index" );
        goto fn_fail;
    }

    if( edges == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "edges" );
        goto fn_fail;
    }

    if( nnodes < 1 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argnonpos %s %d", "nnodes", nnodes );
        goto fn_fail;
    }

    if(comm_ptr->remote_size < nnodes)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**graphnnodes");
        goto fn_fail;
    }

    mpi_errno = MPIR_Graph_map( comm_ptr, nnodes, index, edges, newrank );

    TraceLeave_MPI_Graph_map(*newrank);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_graph_map %C %d %p %p %p",
            comm,
            nnodes,
            index,
            edges,
            newrank
            )
        );
    TraceError(MPI_Graph_map, mpi_errno);
    goto fn_exit;
}


/*@
 MPI_Graph_neighbors - Returns the neighbors of a node associated
                       with a graph topology

Input Parameters:
+ comm - communicator with graph topology (handle)
. rank - rank of process in group of comm (integer)
- maxneighbors - size of array neighbors (integer)

Output Parameters:
. neighbors - ranks of processes that are neighbors to specified process
 (array of integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
.N MPI_ERR_RANK
@*/
EXTERN_C
MPI_METHOD
MPI_Graph_neighbors(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int rank,
    _In_range_(>=, 0) int maxneighbors,
    _Out_writes_opt_(maxneighbors) int neighbors[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Graph_neighbors(comm, rank, maxneighbors);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( neighbors == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "neighbors" );
        goto fn_fail;
    }

    const MPIR_Topology* graph_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_GRAPH,
        const_cast<MPIR_Topology**>(&graph_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if(rank < 0 || rank >= graph_ptr->topo.graph.nnodes)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_RANK, "**rank %d %d", rank, graph_ptr->topo.graph.nnodes );
        goto fn_fail;
    }

    /* Get location in edges array of the neighbors of the specified rank */
    int iStart, iEnd;

    if (rank == 0)
    {
        iStart = 0;
    }
    else
    {
        iStart = graph_ptr->topo.graph.index[rank-1];
    }
    iEnd = graph_ptr->topo.graph.index[rank];

    if( (iEnd - iStart) > maxneighbors )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**neighborsmany %d %d",
            iEnd - iStart,
            maxneighbors
            );
        goto fn_fail;
    }

    /* Get neighbors */
    for( int i = iStart; i < iEnd; i++ )
    {
        *neighbors++ = graph_ptr->topo.graph.edges[i];
    }

    TraceLeave_MPI_Graph_neighbors(iEnd, neighbors);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_graph_neighbors %C %d %d %p",
            comm,
            rank,
            maxneighbors,
            neighbors
            )
        );
    TraceError(MPI_Graph_neighbors, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Graph_create - Makes a new communicator to which topology information
                 has been attached

Input Parameters:
+ comm_old - input communicator without topology (handle)
. nnodes - number of nodes in graph (integer)
. index - array of integers describing node degrees (see below)
. edges - array of integers describing graph edges (see below)
- reorder - ranking may be reordered (true) or not (false) (logical)

Output Parameter:
. comm_graph - communicator with graph topology added (handle)

Notes:
Each process must provide a description of the entire graph, not just the
neigbors of the calling process.

Algorithm:
We ignore the 'reorder' info currently.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG

@*/
EXTERN_C
MPI_METHOD
MPI_Graph_create(
    _In_ MPI_Comm comm_old,
    _In_range_(>=, 0) int nnodes,
    _In_reads_opt_(nnodes) const int index[],
    _In_reads_opt_(nnodes) const int edges[],
    _In_ int reorder,
    _Out_ MPI_Comm* comm_graph
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Graph_create(comm_old, nnodes, reorder);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm( comm_old, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( nnodes < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "nnodes", nnodes );
        goto fn_fail;
    }
    if (nnodes > 0)
    {
        if( index == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "index" );
            goto fn_fail;
        }

        if( edges == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "edges" );
            goto fn_fail;
        }
    }
    if( comm_graph == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm_graph" );
        goto fn_fail;
    }

    int i;
    int comm_size = comm_ptr->remote_size;

    /* Check that the communicator is large enough */
    if (nnodes > comm_size)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**topotoolarge %d %d", nnodes, comm_size );
        goto fn_fail;
    }

    /* Check that index is monotone nondecreasing */
    /* Use ERR_ARG instead of ERR_TOPOLOGY because there is no
        topology yet */
    for (i=0; i<nnodes; i++)
    {
        if (index[i] < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**indexneg %d %d", i, index[i] );
            goto fn_fail;
        }
        if (i+1<nnodes && index[i] > index[i+1])
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**indexnonmonotone %d %d %d", i, index[i], index[i+1] );
            goto fn_fail;
        }
    }

    /* Check that edge number is in range. Note that the
        edges refer to a rank in the communicator, and can
        be greater than nnodes */
    if (nnodes > 0)
    {
        for (i=0; i<index[nnodes-1]; i++)
        {
            if (edges[i] > comm_size || edges[i] < 0)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**edgeoutrange %d %d %d",  i, edges[i], comm_size );
                goto fn_fail;
            }
        }
    }

    /* Test for empty communicator */
    if (nnodes == 0)
    {
        *comm_graph = MPI_COMM_NULL;
        goto fn_exit;
    }

    mpi_errno = MPIR_Graph_create( comm_ptr, nnodes,
                                   (const int *)index,
                                   (const int *)edges,
                                   reorder, comm_graph );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

  fn_exit:
    TraceLeave_MPI_Graph_create(*comm_graph);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_graph_create %C %d %p %p %d %p",
            comm_old,
            nnodes,
            index,
            edges,
            reorder,
            comm_graph
            )
        );
    TraceError(MPI_Graph_create, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Graphdims_get - Retrieves graph topology information associated with a
                    communicator

Input Parameter:
. comm - communicator for group with graph structure (handle)

Output Parameters:
+ nnodes - number of nodes in graph (integer)
- nedges - number of edges in graph (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Graphdims_get(
   _In_  MPI_Comm comm,
    _Out_ int* nnodes,
    _Out_ int* nedges
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Graphdims_get(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( nnodes == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "nnodes" );
        goto fn_fail;
    }

    if( nedges == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "nedges" );
        goto fn_fail;
    }

    const MPIR_Topology* topo_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_GRAPH,
        const_cast<MPIR_Topology**>(&topo_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Set nnodes */
    *nnodes = topo_ptr->topo.graph.nnodes;

    /* Set nedges */
    *nedges = topo_ptr->topo.graph.nedges;

    TraceLeave_MPI_Graphdims_get(*nnodes, *nedges);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_graphdims_get %C %p %p",
            comm,
            nnodes,
            nedges
            )
        );
    TraceError(MPI_Graphdims_get, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Graph_neighbors_count - Returns the number of neighbors of a node
                            associated with a graph topology

Input Parameters:
+ comm - communicator with graph topology (handle)
- rank - rank of process in group of 'comm' (integer)

Output Parameter:
. nneighbors - number of neighbors of specified process (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
.N MPI_ERR_RANK
@*/
EXTERN_C
MPI_METHOD
MPI_Graph_neighbors_count(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int rank,
    _Out_ _Deref_out_range_(>=, 0) int* nneighbors
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Graph_neighbors_count(comm, rank);

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( nneighbors == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "nneighbors" );
        goto fn_fail;
    }

    const MPIR_Topology* graph_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_GRAPH,
        const_cast<MPIR_Topology**>(&graph_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if(rank < 0 || rank >= graph_ptr->topo.graph.nnodes)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_RANK, "**rank %d %d", rank, graph_ptr->topo.graph.nnodes );
        goto fn_fail;
    }

    if ( rank == 0 )
    {
        *nneighbors = graph_ptr->topo.graph.index[rank];
    }
    else
    {
        *nneighbors = graph_ptr->topo.graph.index[rank] -
            graph_ptr->topo.graph.index[rank-1];
    }

    TraceLeave_MPI_Graph_neighbors_count(*nneighbors);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_graph_neighbors_count %C %d %p",
            comm,
            rank,
            nneighbors
            )
        );
    TraceError(MPI_Graph_neighbors_count, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Dist_graph_neighbors_count - Returns the adjacency associated 
                                 with the distributed graph topology

Input Parameters:
+ comm - communicator with graph topology (handle)

Output Parameter:
. indegree  - no. of edges into this process (non-negative integer)
. outdegree - no. of edges out of this process (non-negative integer)
. weighted  - false if MPI_UNWEIGHTED was supplied during creation, true otherwise (logical)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Dist_graph_neighbors_count(
    _In_ MPI_Comm comm,
    _Out_ _Deref_out_range_(>=, 0) int *indegree,
    _Out_ _Deref_out_range_(>=, 0) int *outdegree,
    _Out_ _Deref_out_range_(>=, 0) int *weighted
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Dist_graph_neighbors_count(comm);

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( indegree == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "indegree" );
        goto fn_fail;
    }

    if( outdegree == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "outdegree" );
        goto fn_fail;
    }

    if( weighted == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "weighted" );
        goto fn_fail;
    }

    const MPIR_Topology* dist_graph_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_DIST_GRAPH,
        const_cast<MPIR_Topology**>(&dist_graph_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *indegree = dist_graph_ptr->topo.dist_graph.indegree;
    *outdegree = dist_graph_ptr->topo.dist_graph.outdegree;
    *weighted = ( dist_graph_ptr->topo.dist_graph.is_weighted ) ? 1 : 0;

    TraceLeave_MPI_Dist_graph_neighbors_count(*indegree, *outdegree, *weighted);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_dist_graph_neighbors_count %C",
            comm
            )
        );
    TraceError(MPI_Dist_graph_neighbors_count, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Dist_graph_neighbors - Returns the adjacency information 
                           associated with the distributed graph topology

Input Parameters:
+ comm         - communicator with graph topology (handle)
. maxindegree  - size of sources and sourceweights array (non-negative integer)
- maxoutdegree - size of destinations and destweights array (non-negative integer)

Output Parameter:
. sources       - processes for which the calling process is a destination ( array of non-negative integers )
. sourceweights - weights of the edges into the calling process ( array of non-negative integers )
. destinations  - processes for which the calling process is the source ( array of non-negative integers )
. destweights   - weights of the edges out of the calling process ( array of non-negative integers )

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TOPOLOGY
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Dist_graph_neighbors(
    _In_ MPI_Comm comm,
    _In_range_(>=, 0) int maxindegree,
    _Out_writes_opt_(maxindegree) int sources[],
    _Out_writes_opt_(maxindegree) int sourceweights[],
    _In_range_(>=, 0) int maxoutdegree,
    _Out_writes_opt_(maxoutdegree) int destinations[],
    _Out_writes_opt_(maxoutdegree) int destweights[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Dist_graph_neighbors(comm, maxindegree, maxoutdegree);

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( maxindegree < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
           MPI_ERR_ARG,
           "**argneg %s %d",
           "maxindegree",
           maxindegree
        );
        goto fn_fail;
    }

    if( maxoutdegree < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**argneg %s %d",
            "maxoutdegree",
            maxoutdegree );
        goto fn_fail;
    }

    const MPIR_Topology* dist_graph_ptr;
    mpi_errno = MpiaCommValidateTopology(
        comm_ptr,
        MPI_DIST_GRAPH,
        const_cast<MPIR_Topology**>(&dist_graph_ptr)
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // The MPI 3.0 standard does not specify what to do when maxindegree or maxoutdegree 
    // is greater than the topology indegree and outdegree.  In such a case, this 
    // implementation truncates the maxindegree and outdegree to the topology indegree 
    // and outdegree respectively.
    //
    int tindegree  = min( maxindegree, dist_graph_ptr->topo.dist_graph.indegree );
    int toutdegree = min( maxoutdegree, dist_graph_ptr->topo.dist_graph.outdegree );

    if( sources == nullptr && tindegree > 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**nullptr %s",
            "sources"
            );
        goto fn_fail;
    }

    if( destinations == nullptr && toutdegree > 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**nullptr %s",
            "destinations"
            );
        goto fn_fail;
    }

    if( dist_graph_ptr->topo.dist_graph.is_weighted )
    {
        if( sourceweights == nullptr && tindegree > 0 )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**nullptr %s",
                "sourceweights"
                );
            goto fn_fail;
        }

        if( destweights == nullptr && toutdegree > 0 )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**nullptr %s",
                "destweights"
                );
            goto fn_fail;
        }
    }

    //
    // Get sources and destinations
    //
    if( sources != nullptr )
    {
        CopyMemory( sources, dist_graph_ptr->topo.dist_graph.in, tindegree * sizeof(int) );
    }
    
    if( destinations != nullptr )
    {
        CopyMemory( destinations, dist_graph_ptr->topo.dist_graph.out, toutdegree * sizeof(int) );
    }

    //
    // Get source weights and destination weights
    //
    if( dist_graph_ptr->topo.dist_graph.is_weighted )
    {
        if( sourceweights != MPI_UNWEIGHTED && sourceweights != nullptr )
        {
            CopyMemory(
                sourceweights,
                dist_graph_ptr->topo.dist_graph.in_weights,
                sizeof(int) * tindegree
                );
        }

        if( destweights != MPI_UNWEIGHTED && destweights != nullptr )
        {
            CopyMemory(
                destweights,
                dist_graph_ptr->topo.dist_graph.out_weights,
                sizeof(int) * toutdegree
                );
        }
    }

    TraceLeave_MPI_Dist_graph_neighbors(sources, sourceweights, destinations, destweights);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_dist_graph_neighbors %C %d %d",
            comm,
            maxindegree,
            maxoutdegree
            )
        );
    TraceError(MPI_Dist_graph_neighbors, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Dist_graph_create_adjacent - Returns a handle to a new communicator to
                                 which the distributed graph topology information 
                                 is attached.

Input Parameters:
+ comm_old      - input communicator (handle)
. indegree      - size of sources and sourceweights arrays (non-negative integer)
. sources       - ranks of processes for which the calling process is a
                  destination (array of non-negative integers)
. sourceweights - weights of the edges into the calling
                  process (array of non-negative integers or MPI_UNWEIGHTED)
. outdegree     - size of destinations and destweights arrays (non-negative integer)
. destinations  - ranks of processes for which the calling process is a
                  source (array of non-negative integers)
. destweights   - weights of the edges out of the calling process 
                 (array of non-negative integers or MPI_UNWEIGHTED)
. info          - hints on optimization and interpretation of weights (handle)
- reorder       - the ranks may be reordered (true) or not (false) (logical)

Output Parameters:
. comm_dist_graph - communicator with distributed graph topology (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Dist_graph_create_adjacent(
    _In_ MPI_Comm comm_old,
    _In_range_(>=, 0) int indegree,
    _In_reads_opt_(indegree) const int sources[],
    _In_reads_opt_(indegree) const int sourceweights[],
    _In_range_(>=, 0) int outdegree,
    _In_reads_opt_(outdegree) const int destinations[],
    _In_reads_opt_(outdegree) const int destweights[],
    _In_ MPI_Info info,
    _In_range_(0, 1) int reorder,
    _Out_ MPI_Comm *comm_dist_graph
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Dist_graph_create_adjacent(comm_old, indegree, outdegree, info, reorder);

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm_old, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( comm_dist_graph == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**nullptr %s",
            "comm_dist_graph"
            );
        goto fn_fail;
    }

    if( indegree < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**argneg %s %d",
            "indegree",
            indegree
            );
        goto fn_fail;
    }

    int comm_size = comm_ptr->remote_size;
    if( indegree > comm_size )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**topotoolarge %d %d",
            indegree,
            comm_size
            );
        goto fn_fail;
    }

    if( outdegree < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**argneg %s %d",
            "outdegree",
            outdegree
            );
        goto fn_fail;
    }

    if( outdegree > comm_size )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**topotoolarge %d %d",
            outdegree,
            comm_size
            );
        goto fn_fail;
    }

    if( (sourceweights == MPI_UNWEIGHTED && destweights != MPI_UNWEIGHTED) ||
        (destweights == MPI_UNWEIGHTED && sourceweights != MPI_UNWEIGHTED) )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**unweightedboth"
            );
        goto fn_fail;
    }

    if( indegree > 0 )
    {
        if( sources == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**nullptr %s",
                "sources"
                );
            goto fn_fail;
        }

        if( sourceweights == MPI_WEIGHTS_EMPTY || sourceweights == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**invalidarg %s %s",
                "sourceweights",
                "Cannot use MPI_WEIGHTS_EMPTY or NULL"
                );
            goto fn_fail;
        }

        //
        // Check if each source and sourceweight entry is valid
        //
        for( int i = indegree - 1; i >= 0; i-- )
        {
            if( sources[i] < 0 || sources[i] >= comm_size )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_ARG,
                    "**argarrayrange %s %d %d %d",
                    "sources",
                    i,
                    sources[i],
                    comm_size
                    );
                goto fn_fail;
            }

            if( sourceweights != MPI_UNWEIGHTED )
            {
                if( sourceweights[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE(
                        MPI_ERR_ARG,
                        "**argarrayneg %s %d %d",
                        "sourceweights",
                        i,
                        sourceweights[i]
                        );
                    goto fn_fail;
                }
            }
        }
    }

    if( outdegree > 0 )
    {
        if( destinations == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**nullptr %s",
                "destinations"
                );
            goto fn_fail;
        }

        if( destweights == MPI_WEIGHTS_EMPTY || destweights == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**invalidarg %s %s",
                "destweights",
                "Cannot use MPI_WEIGHTS_EMPTY or NULL"
                );
            goto fn_fail;
        }

        //
        // Check if each destination and destination weight entry is valid
        //
        for( int i = outdegree - 1; i >= 0; i-- )
        {
            if( destinations[i] < 0 || destinations[i] > comm_size )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_ARG,
                    "**argarrayrange %s %d %d %d",
                    "destinations",
                    i,
                    destinations[i],
                    comm_size
                    );
                goto fn_fail;
            }

            if( destweights != MPI_UNWEIGHTED )
            {
                if( destweights[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE(
                        MPI_ERR_ARG,
                        "**argarrayneg %s %d %d",
                        "destweights",
                        i,
                        destweights[i]
                        );
                    goto fn_fail;
                }
            }
        }
    }

    mpi_errno = MPIR_Dist_graph_create_adjacent(
        comm_ptr,
        indegree,
        sources,
        sourceweights,
        outdegree,
        destinations,
        destweights,
        info_ptr,
        reorder,
        comm_dist_graph
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Dist_graph_create_adjacent(*comm_dist_graph);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_dist_graph_create_adjacent %C %d %p %p %d %p %p %d %d",
            comm_old,
            indegree,
            sources,
            sourceweights,
            outdegree,
            destinations,
            destweights,
            info,
            reorder
            )
        );
    TraceError(MPI_Dist_graph_create_adjacent, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Dist_graph_create - Returns a handle to a new
                        communicator to which the distributed graph topology 
                        information is attached.

Input Parameters:
+ comm_old     - input communicator (handle)
. n            - number of source nodes for which this process specifies edges 
                 (non-negative integer)
. sources      - array containing the n source nodes for which this process 
                 specifies edges (array of non-negative integers)
. degrees      - array specifying the number of destinations for each source node 
                 in the source node array (array of non-negative integers)
. destinations - destination nodes for the source nodes in the source node 
                 array (array of non-negative integers)
. weights      - weights for source to destination edges (array of non-negative 
                 integers or MPI_UNWEIGHTED)
. info         - hints on optimization and interpretation of weights (handle)
- reorder      - the process may be reordered (true) or not (false) (logical)

Output Parameters:
. comm_dist_graph - communicator with distributed graph topology added (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Dist_graph_create(
    _In_ MPI_Comm comm_old,
    _In_range_(>=, 0) int n,
    _In_reads_opt_(n) const int sources[],
    _In_reads_opt_(n) const int degrees[],
    _In_opt_ const int destinations[],
    _In_opt_ const int weights[],
    _In_ MPI_Info info,
    _In_range_(0, 1) int reorder,
    _Out_ MPI_Comm *comm_dist_graph
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Dist_graph_create(comm_old, n, info, reorder);

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm_old, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( n < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( 
            MPI_ERR_ARG,
            "**argneg %s %d",
            "n",
            n
            );
        goto fn_fail;
    }

    int comm_size = comm_ptr->remote_size;
    if( n > 0 )
    {
        if( sources == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**nullptr %s",
                "sources"
                );
            goto fn_fail;
        }

        if( degrees == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_ARG,
                "**nullptr %s",
                "degrees"
                );
            goto fn_fail;
        }

        int dest_count = 0;
        for( int i = n - 1; i >= 0; i-- )
        {
            if( sources[i] < 0 || sources[i] >= comm_size )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_ARG,
                    "**argarrayrange %s %d %d %d",
                    "sources",
                    i,
                    sources[i],
                    comm_size
                    );
                goto fn_fail;
            }

            if( degrees[i] < 0 )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_ARG,
                    "**argarrayneg %s %d %d",
                    "degrees",
                    i,
                    degrees[i]
                    );
                goto fn_fail;
            }

            if( degrees[i] > 0 )
            {
                dest_count += degrees[i];
            }
        }

        if( dest_count > 0 )
        {
            if( destinations == nullptr )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_ARG,
                    "**nullptr %s",
                    "destinations"
                    );
                goto fn_fail;
            }

            if( weights == nullptr || weights == MPI_WEIGHTS_EMPTY )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_ARG,
                    "**invalidarg %s %s",
                    "weights",
                    "Cannot use MPI_WEIGHTS_EMPTY or NULL"
                    );
                goto fn_fail;
            }

            for( int i = dest_count - 1; i >= 0; i-- )
            {
                if( destinations[i] < 0 || destinations[i] >= comm_size )
                {
                    mpi_errno = MPIU_ERR_CREATE(
                        MPI_ERR_ARG,
                        "**argarrayrange %s %d %d %d",
                        "destinations",
                        i,
                        destinations[i],
                        comm_size
                        );
                    goto fn_fail;
                }

                if( weights != MPI_UNWEIGHTED && weights != nullptr)
                {
                    if( weights[i] < 0 )
                    {
                        mpi_errno = MPIU_ERR_CREATE(
                            MPI_ERR_ARG,
                            "**argarrayneg %s %d %d",
                            "weights",
                            i,
                            weights[i]
                            );
                        goto fn_fail;
                    }
                }

            }
        }
    }

    mpi_errno = MPIR_Dist_graph_create(
        comm_ptr,
        n,
        const_cast<int*>(sources),
        const_cast<int*>(degrees),
        const_cast<int*>(destinations),
        const_cast<int*>(weights),
        info_ptr,
        reorder,
        comm_dist_graph
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Dist_graph_create(*comm_dist_graph);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_dist_graph_create %C %d %p %p %p %p %d %d",
            comm_old,
            n,
            sources,
            degrees,
            destinations,
            weights,
            info,
            reorder
            )
        );
    TraceError(MPI_Dist_graph_create, mpi_errno);
    goto fn_exit;
}
