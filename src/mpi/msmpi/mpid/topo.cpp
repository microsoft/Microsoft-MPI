// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2015 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* Keyval for topology information */
static int MPIR_Topology_keyval = MPI_KEYVAL_INVALID;

/* Local functions */
static MPI_RESULT MPIR_Topology_copy_fn ( MPI_Comm, int, void *, void *, void *,
                                   int * );
static MPI_RESULT MPIR_Topology_delete_fn ( MPI_Comm, int, void *, void * );
static int MPIR_Topology_finalize ( void * );

/*
  Return a poiner to the topology structure on a communicator.
  Returns null if no topology structure is defined
*/
_Success_(return != nullptr)
MPIR_Topology*
MPIR_Topology_get(
    _In_ const MPID_Comm* comm_ptr
    )
{
    MPIR_Topology *topo_ptr;
    int flag;

    if (MPIR_Topology_keyval == MPI_KEYVAL_INVALID)
    {
        return 0;
    }

    (void)NMPI_Comm_get_attr(comm_ptr->handle, MPIR_Topology_keyval,
                             &topo_ptr, &flag );
    if (flag)
        return topo_ptr;

    return 0;
}

MPI_RESULT
MPIR_Topology_put(
    _In_opt_ const MPID_Comm* comm_ptr,
    _In_ MPIR_Topology* topo_ptr
    )
{
    if (comm_ptr == nullptr)
    {
        return MPI_SUCCESS;
    }

    MPI_RESULT mpi_errno;

    if (MPIR_Topology_keyval == MPI_KEYVAL_INVALID)
    {
        /* Create a new keyval */
        /* FIXME - thread safe code needs a thread lock here, followed
           by another test on the keyval to see if a different thread
           got there first */
        mpi_errno = NMPI_Comm_create_keyval( MPIR_Topology_copy_fn,
                                             MPIR_Topology_delete_fn,
                                             &MPIR_Topology_keyval, 0 );
        /* Register the finalize handler */
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        MPIR_Add_finalize( MPIR_Topology_finalize, (void*)0,
                           MPIR_FINALIZE_CALLBACK_PRIO-1);
    }

    mpi_errno = NMPI_Comm_set_attr(comm_ptr->handle, MPIR_Topology_keyval,
                                   topo_ptr );
    return mpi_errno;
}


static int MPIR_Topology_finalize( void* /*p*/ )
{
    if (MPIR_Topology_keyval != MPI_KEYVAL_INVALID)
    {
        /* Just in case */
        NMPI_Comm_free_keyval( &MPIR_Topology_keyval );
    }

    return 0;
}

/* The keyval copy and delete functions must handle copying and deleting
   the associated topology structures

   We can reduce the number of allocations by making a single allocation
   of enough integers for all fields (including the ones in the structure)
   and freeing the single object later.
*/
static MPI_RESULT
MPIR_Topology_copy_fn (
    MPI_Comm /*comm*/,
    int /*keyval*/,
    void* /*extra_data*/,
    void *attr_in,
    void *attr_out,
    int *flag )
{
    OACR_USE_PTR( attr_in );
    const MPIR_Topology *old_topology = (const MPIR_Topology *)attr_in;
    MPIR_Topology *copy_topology;

    if (old_topology->kind == MPI_CART)
    {
        int ndims = old_topology->topo.cart.ndims;
        size_t cbTopo = sizeof(MPIR_Topology) + (sizeof(int) * 3 * ndims);
        copy_topology = static_cast<MPIR_Topology*>( MPIU_Malloc( cbTopo ) );
        if( copy_topology == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        CopyMemory( copy_topology, old_topology, cbTopo );
        copy_topology->topo.cart.dims = reinterpret_cast<int*>(copy_topology + 1);
        copy_topology->topo.cart.periodic = copy_topology->topo.cart.dims + ndims;
        copy_topology->topo.cart.position =  copy_topology->topo.cart.periodic + ndims;
    }
    else if (old_topology->kind == MPI_GRAPH)
    {
        int nnodes = old_topology->topo.graph.nnodes;
        int nedges = old_topology->topo.graph.nedges;
        size_t cbTopo = sizeof(MPIR_Topology) + (sizeof(int) * (nnodes + nedges));
        copy_topology = static_cast<MPIR_Topology*>( MPIU_Malloc( cbTopo ) );
        if( copy_topology == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        CopyMemory( copy_topology, old_topology, cbTopo );
        copy_topology->topo.graph.index = reinterpret_cast<int*>(copy_topology + 1);
        copy_topology->topo.graph.edges = copy_topology->topo.graph.index + nnodes;
    }
    else if (old_topology->kind == MPI_DIST_GRAPH)
    {
        int indegree = old_topology->topo.dist_graph.indegree;
        int outdegree = old_topology->topo.dist_graph.outdegree;
        int topo_size = sizeof(int) * (indegree + outdegree);
        int isweighted = ( old_topology->topo.dist_graph.is_weighted ) ? 1 : 0;
        if( isweighted != 0 )
        {
             topo_size *=2;
        }

        int cbTopo = sizeof(MPIR_Topology) + topo_size;
        copy_topology = static_cast<MPIR_Topology*>( MPIU_Malloc( cbTopo ) );
        if( copy_topology == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        CopyMemory( copy_topology, old_topology, cbTopo );

        int* inout_data_ptr = reinterpret_cast<int*>(copy_topology + 1);
        if( indegree > 0 )
        {
            copy_topology->topo.dist_graph.in = inout_data_ptr;
        }
        if( outdegree > 0 )
        {
            copy_topology->topo.dist_graph.out = inout_data_ptr + indegree;
        }

        int* inout_weights_ptr = inout_data_ptr + indegree + outdegree;
        if( isweighted != 0 )
        {
            if( indegree > 0 )
            {
                copy_topology->topo.dist_graph.in_weights = inout_weights_ptr;
            }
            if( outdegree > 0 )
            {
                copy_topology->topo.dist_graph.out_weights = inout_weights_ptr + indegree;
            }
        }
    }
    else
    {
        /* Unknown topology */
        return MPI_ERR_TOPOLOGY;
    }

    *(void **)attr_out = (void *)copy_topology;
    *flag = 1;

    return MPI_SUCCESS;
}

static MPI_RESULT
MPIR_Topology_delete_fn (
    MPI_Comm /*comm*/,
    int /*keyval*/,
    void *attr_val,
    void* /*extra_data*/ )
{
    MPIR_Topology *topology = (MPIR_Topology *)attr_val;

    /* FIXME - free the attribute data structure */

    if( topology->kind != MPI_CART && 
        topology->kind != MPI_GRAPH && 
        topology->kind != MPI_DIST_GRAPH)
    {
        return MPI_ERR_TOPOLOGY;
    }

    MPIU_Free( topology );
    return MPI_SUCCESS;
}

//
// Because we store factors with their multiplicities, a small array can
// store all of the factors for a large number (grows *faster* than n
// factorial).  By setting MAX_FACTORS to 16, all possible numbers in
// unsigned 64-bit input are contained
//
#define MAX_FACTORS 16
/* 2^20 is a millon */
#define MAX_DIMS    20

typedef struct Factors { int val, cnt; } Factors;

/* Return the factors of n and their multiplicity in factors; the number of
   distinct factors is the return value and the total number of factors,
   including multiplicities, is returned in ndivisors */
#define NUM_PRIMES 168
#define MAX_PRIMES 997
  static int primes[NUM_PRIMES] =
           {2,    3,    5,    7,   11,   13,   17,   19,   23,   29,
           31,   37,   41,   43,   47,   53,   59,   61,   67,   71,
           73,   79,   83,   89,   97,  101,  103,  107,  109,  113,
          127,  131,  137,  139,  149,  151,  157,  163,  167,  173,
          179,  181,  191,  193,  197,  199,  211,  223,  227,  229,
          233,  239,  241,  251,  257,  263,  269,  271,  277,  281,
          283,  293,  307,  311,  313,  317,  331,  337,  347,  349,
          353,  359,  367,  373,  379,  383,  389,  397,  401,  409,
          419,  421,  431,  433,  439,  443,  449,  457,  461,  463,
          467,  479,  487,  491,  499,  503,  509,  521,  523,  541,
          547,  557,  563,  569,  571,  577,  587,  593,  599,  601,
          607,  613,  617,  619,  631,  641,  643,  647,  653,  659,
          661,  673,  677,  683,  691,  701,  709,  719,  727,  733,
          739,  743,  751,  757,  761,  769,  773,  787,  797,  809,
          811,  821,  823,  827,  829,  839,  853,  857,  859,  863,
          877,  881,  883,  887,  907,  911,  919,  929,  937,  941,
          947,  953,  967,  971,  977,  983,  991,  997};

//
// Prime number decomposition
// factors put in ascending order
// Search from bottom, as they are most likely candidates
//
static int MPIR_Factor(
    _In_range_(>, 0)       int n,
    _Out_cap_(MAX_FACTORS) Factors factors[],
    _Out_                  int *ndivisors)
{
    int primeIndex, nfactors, nall, ceiling;
    unsigned long bitscan;

    //
    // If 1, 0 or negative, bail
    //
    if (n <= 1)
    {
        factors[0].val = n;
        factors[0].cnt = 1;
        *ndivisors = 1;
        return 1;
    }

    nfactors = 0;
    nall = 0;

    //
    // Check factor of 2
    //
    _BitScanForward(&bitscan, n);
    if (bitscan > 0)
    {
        factors[0].val = 2;
        factors[0].cnt = bitscan;
        n >>= bitscan;
        if (n == 1)
        {
            //
            // n was PowerOf2
            //
            *ndivisors = bitscan;
            return 1;
        }
        nall = bitscan;
        nfactors++;
    }

    //
    // use pseudo-square root of the PowerOf2Ceiling as a safe ceiling limit
    // 'pseudo', because bitscan odd cuts lower than even; start 1 bit higher
    // to be safe.  Otherwise, perfect squares of primes immediately below
    // an odd PowerOf2Ceiling can be missed, e.g. 289 or 361
    //
    _BitScanReverse(&bitscan, n);
    ceiling = 4 << (bitscan / 2);
    if (ceiling > MAX_PRIMES)
    {
        ceiling = MAX_PRIMES;
    }

    primeIndex = 0;
    do
    {
        int target = primes[++primeIndex];

        if ((n % target) == 0)
        {
            //
            // Aliquot divisor found, determine exponent
            //
            int count = 0;
            do
            {
                n /= target;
                count++;
            } while ((n % target) == 0);

            nall += count;
            factors[nfactors].cnt = count;
            factors[nfactors++].val = target;
        }
        if ((target >= ceiling) && (n != 1))
        {
            //
            // n still has a factor above the ceiling
            // This is either prime with a count of 1
            // and the ceiling has found it quickly,
            // or greater than MAX_PRIMES**2
            //
            nall++;
            factors[nfactors].val = n;
            factors[nfactors++].cnt = 1;
            n = 1;
        }
    } while (n != 1);

    *ndivisors = nall;
    return nfactors;
}

/*
   Given a collection of factors from the factors routine and a number of
   required values, combine the elements in factors into "needed" elements
   of the array chosen.  These are non-increasing and so can be used directly
   in setting values in the dims array in MPIR_Dims_create.

   Algorithm (very simple)

   target_size = nnodes / ndims needed.
   Accumulate factors, starting from the bottom,
   until the target size is met or exceeded.
   Put all of the remaining factors into the last dimension
   (recompute target_size with each step, since we may
   miss the target by a wide margin.

   A much more sophisticated code would try to balance
   the number of nodes assigned to each dimension, possibly
   in concert with guidelines from the device about "good"
   sizes

 */

/* FIXME: This routine does not work well with non-powers of two (and
   probably with collections of different factors.  For example,
   factoring squares like 6*6 or 7*7 in to 2 dimensions doesn't yield the
   expected results (18,2) and (49,1) in current tests */

/*
   To give more reasonable values of the chosen factors (in the chosen array)
   from the factors, we first distribute the factors among the dimensions
   equally.

   Note that since this is only used in the case where factors are the
   factors of nnnodes, we don't need to use the value of nnodes.
 */

static int MPIR_ChooseFactors( int nfactors, const Factors factors[],
                               int needed, _Out_cap_(needed) int chosen[] )
{
    int i, j;

    /* Initialize the chosen factors to all 1 */
    for (i=0; i<needed; i++)
    {
        chosen[i] = 1;
    }
    /* For each of the available factors, there are factors[].cnt
       copies of each of the unique factors.  These are assigned in
       modified round robin fashion to each of the "chosen" entries.
       The modification is to combine with the smallest current term
       Note that the factors are in increasing order. */

    i = 0;  /* We don't reset the count so that the factors are
               distributed among the dimensions */
    for (j = nfactors - 1; j >= 0; j--)
    {
        int cnt = factors[j].cnt;
        int val = factors[j].val;
        /* For each of the factors that we add, try to keep the
           entries balanced. */
        while (cnt--)
        {
            /* Find the smallest current entry */
            int ii, cMin, iMin;
            iMin = 0;
            cMin = chosen[0];
            for (ii=1; ii<needed; ii++)
            {
                if (chosen[ii] < cMin)
                {
                    cMin = chosen[ii];
                    iMin = ii;
                }
            }
            if (chosen[i] > iMin) i = iMin;

            chosen[i] *= val;
            i++;
            if (i >= needed) i = 0;
        }
    }

    /* Second, sort the chosen array in non-increasing order.  Use
       a simple bubble sort because the number of elements is always small */
    for (i=0; i<needed-1; i++)
    {
        for (j=i+1; j<needed; j++)
        {
            if (chosen[j] > chosen[i])
            {
                int tmp = chosen[i];
                chosen[i] = chosen[j];
                chosen[j] = tmp;
            }
        }
    }
    return 0;
}


MPI_RESULT
MPIR_Dims_create(
    _In_range_(>, 0)       int nnodes,
    _In_range_(>, 0)       int ndims,
    _Inout_updates_(ndims) int dims[]
    )
{
    Factors factors[MAX_FACTORS];
    int chosen[MAX_DIMS];
    int i, j;
    MPI_RESULT mpi_errno;
    int dims_needed, dims_product, nfactors, ndivisors=0;

    /* Find the number of unspecified dimensions in dims and the product
       of the positive values in dims */
    dims_needed  = 0;
    dims_product = 1;
    for (i=0; i<ndims; i++)
    {
        if (dims[i] < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DIMS,  "**argarrayneg %s %d %d", "dims", i, dims[i]);
            return mpi_errno;
        }
        if (dims[i] == 0)
        {
            dims_needed++;
        }
        else
        {
            dims_product *= dims[i];
        }
    }

    if (dims_needed > MAX_DIMS)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DIMS,  "**dimsmany %d %d", dims_needed, MAX_DIMS );
        return mpi_errno;
    }

    //
    // Can we factor nnodes by dims_product?
    //
    if ((nnodes % dims_product) != 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_DIMS, "**dimspartition");
        return mpi_errno;
    }

    if (!dims_needed)
    {
        /* Special case - all dimensions provided */
        return MPI_SUCCESS;
    }

    nnodes /= dims_product;

    /* Now, factor nnodes into dims_needed components.  We'd like these
       to match the underlying machine topology as much as possible.  In the
       absence of information about the machine topology, we can try to
       make the factors as close to each other as possible.

       The MPICH 1 version used donated code that was quite sophisticated
       and complex.  However, since it didn't take the system topology
       into account, it was more sophisticated that was perhaps warranted.
       In addition, useful values of nnodes for most MPI programs will be
       of the order 10-10000, and powers of two will be common.
    */

    /* Get the factors */
    nfactors = MPIR_Factor(nnodes, factors, &ndivisors);

    /* Divide into 3 major cases:
       1. Fewer divisors than needed dimensions.  Just use all of the
          factors up, setting the remaining dimensions to 1
       2. Only one distinct factor (typically 2) but with greater
          multiplicity.  Give each dimension a nearly equal size
       3. Other.  There are enough factors to divide among the dimensions.
          This is done in an ad hoc fashion
    */

/* DEBUG
    printf( "factors are (%d of them) with %d divisors\n", nfactors, ndivisors );
    for (j=0; j<nfactors; j++)
    {
        printf( "val = %d repeated %d\n", factors[j].val, factors[j].cnt );
    }
*/
    /* The MPI spec requires that the values that are set be in nonincreasing
       order (MPI-1, section 6.5).  */

    /* Distribute the factors among the dimensions */
    if (ndivisors <= dims_needed)
    {
        /* Just use the factors as needed.  */
        MPIR_ChooseFactors( nfactors, factors, dims_needed, chosen );
        j = 0;
        for (i=0; i<ndims; i++)
        {
            if (dims[i] == 0)
            {
                dims[i] = chosen[j++];
            }
        }
    }
    else
    {
        /* We must combine some of the factors */
        /* This is what the fancy code is for in the MPICH-1 code.
           If the number of distinct factors is 1 (e.g., a power of 2),
           then this code can be much simpler */
        /* NOT DONE */
        /* FIXME */
        if (nfactors == 1)
        {
            /* Special case for k**n, such as powers of 2 */
            int factor = factors[0].val;
            int cnt    = factors[0].cnt; /* Numver of factors left */
            int cnteach = ( cnt + dims_needed - 1 ) / dims_needed;
            int factor_each;

            factor_each = factor;
            for (i=1; i<cnteach; i++) factor_each *= factor;

            for (i=0; i<ndims; i++)
            {
                if (dims[i] == 0)
                {
                    if (cnt > cnteach)
                    {
                        dims[i] = factor_each;
                        cnt -= cnteach;
                    }
                    else if (cnt > 0)
                    {
                        factor_each = factor;
                        for (j=1; j<cnt; j++)
                            factor_each *= factor;
                        dims[i] = factor_each;
                        cnt = 0;
                    }
                    else
                    {
                        dims[i] = 1;
                    }
                }
            }
        }
        else
        {
            /* Here is the general case.  */
            MPIR_ChooseFactors( nfactors, factors, dims_needed,
                                chosen );
            j = 0;
            for (i=0; i<ndims; i++)
            {
                if (dims[i] == 0)
                {
                    dims[i] = chosen[j++];
                }
            }
        }
    }
    return MPI_SUCCESS;
}


MPI_RESULT
MPIR_Cart_create(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int ndims,
    _In_reads_(ndims) const int dims[],
    _In_reads_(ndims) const int periods[],
    _In_ int reorder,
    _Out_ MPI_Comm* comm_cart
    )
{
    int       i, newsize, rank, nranks;
    MPI_RESULT mpi_errno;
    MPID_Comm *newcomm_ptr = NULL;
    StackGuardPointer<MPIR_Topology, CppDeallocator<void>> cart_ptr;
    MPI_Comm ncomm;

    /* Check for invalid arguments */
    newsize = 1;
    for (i=0; i<ndims; i++)
    {
        newsize *= dims[i];
    }

    /* Use ERR_ARG instead of ERR_TOPOLOGY because there is no topology yet */
    if(newsize > comm_ptr->remote_size)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**cartdim %d %d", comm_ptr->remote_size, newsize);
        goto fn_fail;
    }

    if (ndims == 0)
    {
        //
        // specified as a 0D Cartesian topology in MPI 2.1.
        // Rank 0 returns a dup of COMM_SELF with the topology info attached.
        // Others return MPI_COMM_NULL.
        //
        rank = comm_ptr->rank;

        if (rank != 0)
        {
            *comm_cart = MPI_COMM_NULL;
            return MPI_SUCCESS;
        }

        mpi_errno = NMPI_Comm_dup(MPI_COMM_SELF, &ncomm);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }

        newcomm_ptr = CommPool::Get( ncomm );

        /* Create the topology structure */
        cart_ptr = static_cast<MPIR_Topology*>(
            MPIU_Malloc( sizeof(MPIR_Topology) )
            );
        if( cart_ptr == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        cart_ptr->kind               = MPI_CART;
        cart_ptr->topo.cart.nnodes   = 1;
        cart_ptr->topo.cart.ndims    = 0;

        /* make mallocs of size 1 int so that they get freed as part of the
        normal free mechanism */
        cart_ptr->topo.cart.dims = nullptr;
        cart_ptr->topo.cart.periodic = nullptr;
        cart_ptr->topo.cart.position = nullptr;
    }
    else
    {
        /* Create a new communicator as a duplicate of the input communicator
        (but do not duplicate the attributes) */
        if (reorder)
        {
            /* Allow the cart map routine to remap the assignment of ranks to
            processes */
            mpi_errno = NMPI_Cart_map( comm_ptr->handle, ndims, (int *)dims,
                (int *)periods, &rank );

            /* Create the new communicator with split, since we need to reorder
            the ranks (including the related internals, such as the connection
            tables */
            if (mpi_errno == MPI_SUCCESS)
            {
                mpi_errno = NMPI_Comm_split( comm_ptr->handle,
                    rank == MPI_UNDEFINED ? MPI_UNDEFINED : 1,
                    rank, &ncomm );
                if (mpi_errno == MPI_SUCCESS && ncomm != MPI_COMM_NULL)
                {
                    newcomm_ptr = CommPool::Get( ncomm );
                }
            }
        }
        else
        {
            mpi_errno = MPIR_Comm_copy( comm_ptr, newsize, &newcomm_ptr );
            rank   = comm_ptr->rank;
        }
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        /* If this process is not in the resulting communicator, return a
        null communicator and exit */
        if (rank >= newsize || rank == MPI_UNDEFINED)
        {
            *comm_cart = MPI_COMM_NULL;
            return MPI_SUCCESS;
        }

        MPIU_Assert(NULL != newcomm_ptr);

        /* Create the topololgy structure */
        cart_ptr = static_cast<MPIR_Topology*>(
            MPIU_Malloc( sizeof(MPIR_Topology) + (sizeof(int) * 3 * ndims) )
            );
        if( cart_ptr == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        cart_ptr->kind               = MPI_CART;
        cart_ptr->topo.cart.nnodes   = newsize;
        cart_ptr->topo.cart.ndims    = ndims;
        cart_ptr->topo.cart.dims = reinterpret_cast<int*>( cart_ptr + 1 );
        cart_ptr->topo.cart.periodic = cart_ptr->topo.cart.dims + ndims;
        cart_ptr->topo.cart.position = cart_ptr->topo.cart.periodic + ndims;
        nranks = newsize;
        for (i=0; i<ndims; i++)
        {
            cart_ptr->topo.cart.dims[i]     = dims[i];
            cart_ptr->topo.cart.periodic[i] = periods[i];
            nranks = nranks / dims[i];
            /* FIXME: nranks could be zero (?) */
            cart_ptr->topo.cart.position[i] = rank / nranks;
            rank = rank % nranks;
        }
    }

    /* Place this topology onto the communicator */
    mpi_errno = MPIR_Topology_put( newcomm_ptr, cart_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    cart_ptr.detach();
    *comm_cart = newcomm_ptr->handle;

    return mpi_errno;

  fn_fail:
    if( newcomm_ptr != NULL )
    {
        MPIR_Comm_release(newcomm_ptr, 0);
    }
    mpi_errno = MPIU_ERR_GET(
        mpi_errno,
        "**mpi_cart_create %C %d %p %p %d %p",
        comm_ptr,
        ndims,
        dims,
        periods,
        reorder,
        comm_cart
        );
    return mpi_errno;
}


MPI_RESULT
MPIR_Cart_map(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int ndims,
    _In_reads_(ndims) const int dims[],
    _In_opt_ const int /*periodic*/[],
    _Out_ int* newrank
    )
{
    int rank, nranks, i, size;

    /* Determine number of processes needed for topology */
    if(ndims == 0)
    {
        nranks = 1;
    }
    else
    {
        nranks = dims[0];
        for ( i=1; i<ndims; i++ )
            nranks *= dims[i];
    }

    size = comm_ptr->remote_size;

    /* Test that the communicator is large enough */
    if (size < nranks)
    {
        return MPIU_ERR_CREATE(MPI_ERR_DIMS,  "**topotoolarge %d %d", size, nranks );
    }

    /* Am I in this range? */
    rank = comm_ptr->rank;
    if ( rank < nranks )
    {
        /* This relies on the ranks *not* being reordered by the current
           Cartesian routines */
        *newrank = rank;
    }
    else
    {
        *newrank = MPI_UNDEFINED;
    }

    return MPI_SUCCESS;
}


MPI_RESULT
MPIR_Graph_map(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int nnodes,
    _In_reads_(nnodes) const int /*index*/[],
    _In_opt_ const int /*edges*/[],
    _Out_ int *newrank
    )
{
    /* This is the trivial version that does not remap any processes. */
    if (comm_ptr->rank < nnodes)
    {
        *newrank = comm_ptr->rank;
    }
    else
    {
        *newrank = MPI_UNDEFINED;
    }
    return MPI_SUCCESS;
}


MPI_RESULT
MPIR_Graph_create(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int nnodes,
    _In_reads_(nnodes) const int index[],
    _In_reads_(nnodes) const int edges[],
    _In_ int reorder,
    _Out_ MPI_Comm* comm_graph
    )
{
    MPI_RESULT mpi_errno;
    int i, nedges;
    MPID_Comm *newcomm_ptr = NULL;
    StackGuardPointer<MPIR_Topology, CppDeallocator<void>> graph_ptr;

    /* Set this to null in case there is an error */
    *comm_graph = MPI_COMM_NULL;

    /* Create a new communicator */
    if (reorder)
    {
        int nrank;
        MPI_Comm ncomm;

        /* Allow the cart map routine to remap the assignment of ranks to
           processes */
        mpi_errno = NMPI_Graph_map( comm_ptr->handle, nnodes,
                                    (int *)index, (int *)edges,
                                    &nrank );
        ON_ERROR_FAIL(mpi_errno);

        /* Create the new communicator with split, since we need to reorder
           the ranks (including the related internals, such as the connection
           tables */
        mpi_errno = NMPI_Comm_split( comm_ptr->handle,
                            nrank == MPI_UNDEFINED ? MPI_UNDEFINED : 1,
                            nrank, &ncomm );
        if (mpi_errno == MPI_SUCCESS && ncomm != MPI_COMM_NULL)
        {
            newcomm_ptr = CommPool::Get( ncomm );
        }
    }
    else
    {
        /* Just use the first nnodes processes in the communicator */
        mpi_errno = MPIR_Comm_copy( comm_ptr, nnodes, &newcomm_ptr );
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }


    /* If this process is not in the resulting communicator, return a
       null communicator and exit */
    if (newcomm_ptr == NULL)
    {
        *comm_graph = MPI_COMM_NULL;
        goto fn_exit;
    }

    nedges = index[nnodes-1];
    graph_ptr = static_cast<MPIR_Topology*>(
        MPIU_Malloc( sizeof(MPIR_Topology) + (sizeof(int) * (nnodes + nedges)) )
        );
    if( graph_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    graph_ptr->kind = MPI_GRAPH;
    graph_ptr->topo.graph.nnodes = nnodes;
    graph_ptr->topo.graph.nedges = nedges;
    graph_ptr->topo.graph.index = reinterpret_cast<int*>(graph_ptr + 1);
    graph_ptr->topo.graph.edges = graph_ptr->topo.graph.index + nnodes;
    for (i=0; i<nnodes; i++)
        graph_ptr->topo.graph.index[i] = index[i];
    for (i=0; i<nedges; i++)
        graph_ptr->topo.graph.edges[i] = edges[i];

    /* Finally, place the topology onto the new communicator and return the
       handle */
    mpi_errno = MPIR_Topology_put( newcomm_ptr, graph_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    graph_ptr.detach();

    *comm_graph = newcomm_ptr->handle;

    /* ... end of body of routine ... */

  fn_exit:
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        (MPID_Comm*)comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_graph_create %C %d %p %p %d %p",
            comm_ptr->handle, nnodes, index, edges, reorder, comm_graph
            )
        );
    goto fn_exit;
}


MPI_RESULT
MPIR_Dist_graph_create_adjacent(
    _In_ const MPID_Comm *comm_ptr,
    _In_range_(>=, 0) int indegree,
    _In_reads_(indegree) const int sources[],
    _In_reads_(indegree) const int sourceweights[],
    _In_range_(>=, 0) int outdegree,
    _In_reads_(outdegree) const int destinations[],
    _In_reads_(outdegree) const int destweights[],
    _In_opt_ MPID_Info* /*info*/,
    _In_opt_ int /*reorder*/,
    _Out_ MPI_Comm *comm_dist_graph
    )
{
    MPID_Comm *comm_dist_graph_ptr;
    StackGuardPointer<MPIR_Topology, CppDeallocator<void>> topo_ptr;

    //
    // Following the spirit of the old graph topo interface, attributes do not
    // propagate to the new communicator (see MPI-2.1 pp. 243 line 11)
    //
    MPI_RESULT mpi_errno = MPIR_Comm_copy( comm_ptr, comm_ptr->remote_size, &comm_dist_graph_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    bool isweighted = ( sourceweights != MPI_UNWEIGHTED ) ? true : false;
    
    int topo_size = sizeof(int) * ( indegree + outdegree );
    if( isweighted )
    {
        topo_size *=2;
    }

    //
    // Create the topology structure
    //
    topo_ptr = static_cast<MPIR_Topology*>(
        MPIU_Malloc( sizeof(MPIR_Topology) + topo_size )
        );
    if( topo_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    topo_ptr->kind = MPI_DIST_GRAPH;
    topo_ptr->topo.dist_graph.indegree = indegree;
    topo_ptr->topo.dist_graph.outdegree = outdegree;
    topo_ptr->topo.dist_graph.in = nullptr;
    topo_ptr->topo.dist_graph.out = nullptr;
    topo_ptr->topo.dist_graph.in_weights = nullptr;
    topo_ptr->topo.dist_graph.out_weights = nullptr;
    topo_ptr->topo.dist_graph.is_weighted = isweighted;

    int* inout_data_ptr = reinterpret_cast<int*>(topo_ptr + 1);
    if( indegree > 0 )
    {
        topo_ptr->topo.dist_graph.in = inout_data_ptr;
        CopyMemory( 
            topo_ptr->topo.dist_graph.in,
            sources,
            indegree * sizeof(int)
            );
    }

    if( outdegree > 0 )
    {
        topo_ptr->topo.dist_graph.out = inout_data_ptr + indegree;
        CopyMemory(
            topo_ptr->topo.dist_graph.out,
            destinations,
            outdegree * sizeof(int)
            );
    }

    int* inout_weights_ptr = inout_data_ptr + indegree + outdegree;
    if( isweighted )
    {
        if( indegree > 0 )
        {
            topo_ptr->topo.dist_graph.in_weights = inout_weights_ptr;
            CopyMemory(
                topo_ptr->topo.dist_graph.in_weights,
                sourceweights,
                indegree * sizeof(int)
                );
        }

        if( outdegree > 0 )
        {
            topo_ptr->topo.dist_graph.out_weights = inout_weights_ptr + indegree;
            CopyMemory(
                topo_ptr->topo.dist_graph.out_weights,
                destweights,
                outdegree * sizeof(int)
                );
        }
    }

    mpi_errno = MPIR_Topology_put(comm_dist_graph_ptr, topo_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    topo_ptr.detach();

    if (comm_dist_graph_ptr != nullptr)
    {
        *comm_dist_graph = comm_dist_graph_ptr->handle;
    }
    else
    {
        *comm_dist_graph = MPI_COMM_NULL;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    *comm_dist_graph = MPI_COMM_NULL;
    goto fn_exit;
}


MPI_RESULT
MPIR_Dist_graph_create(
    _In_ const MPID_Comm *comm_ptr,
    _In_range_(>=, 0) const int n,
    _In_reads_opt_(n) const int sources[],
    _In_reads_opt_(n) const int degrees[],
    _In_opt_ const int destinations[],
    _In_opt_ const int weights[],
    _In_opt_ MPID_Info* /*info*/,
    _In_opt_ int /*reorder*/,
    _Out_ MPI_Comm *comm_dist_graph
    )
{
    StackGuardPointer<MPIR_Topology, CppDeallocator<void>> topo_ptr;
    StackGuardPointer<int, CppDeallocator<void>>sendbuf_out;
    StackGuardPointer<int, CppDeallocator<void>>sendbuf_in;
    StackGuardPointer<int, CppDeallocator<void>>recvbuf_out;
    StackGuardPointer<int, CppDeallocator<void>>recvbuf_in;
    StackGuardPointer<int, CppDeallocator<void>>sendbuf_index;
    StackGuardPointer<int, CppDeallocator<void>>sendcount;
    StackGuardPointer<int, CppDeallocator<void>>recvcount;
    StackGuardPointer<int, CppDeallocator<void>>sdispls;
    StackGuardPointer<int, CppDeallocator<void>>rdispls;

    MPID_Comm *comm_dist_graph_ptr;
    int comm_size = comm_ptr->remote_size;

    //
    // Following the spirit of the old graph topo interface, attributes do not
    // propagate to the new communicator (see MPI-2.1 pp. 243 line 11)
    //
    MPI_RESULT mpi_errno = MPIR_Comm_copy( comm_ptr, comm_size, &comm_dist_graph_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // If the graph is unweighted then we need not allocate
    // space for the weights.
    //
    int einfo_size = 1;
    if( weights != MPI_UNWEIGHTED )
    {
        einfo_size++;
    }

    sendcount = static_cast<int*>(
        MPIU_Malloc( 2 * comm_size * sizeof(int) )
        );
    if( sendcount == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }
    ZeroMemory( sendcount, 2 * comm_size * sizeof(int) );

    //
    // sendcount[0 - (comm_size-1)]           - Holds count of  in-edge data to be sent to each rank.
    // sendcount[comm_size - (2*comm_size-1)] - Holds count of out-edge data to be sent to each rank. 
    //
    int index = 0;
    int i,j;
    unsigned sendbuf_size = 0;
    for( i = 0; i < n; i++ )
    {
        for( j = 0; j < degrees[i]; j++ )
        {
            sendcount[comm_size + sources[i]] += einfo_size;  // OUT EDGE
            sendcount[destinations[index]] += einfo_size;     // IN  EDGE
            sendbuf_size += static_cast<unsigned>(einfo_size);
            index++;
        }
    }

    //
    // Compute the data offset into the sendbuf_in/sendbuf_out arrays
    //
    sdispls = static_cast<int*>(
        MPIU_Malloc( 2 * comm_size * sizeof(int) )
        );
    if( sdispls == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    int prev_in = 0;
    int prev_out = 0;
    for( i = 0; i < comm_size; i++ )
    {
        if( sendcount[i] != 0 )
        {
            sdispls[i] = prev_in;
            prev_in += sendcount[i];
        }
        if( sendcount[comm_size + i] != 0 )
        {
            sdispls[comm_size + i] = prev_out;
            prev_out += sendcount[comm_size + i];
        }
    }

    //
    // Seperate the in-edge and out-edge info of other ranks, the current rank holds.
    // 
    sendbuf_in = static_cast<int*>(
        MPIU_Malloc( sendbuf_size * sizeof(int) )
        );
    if( sendbuf_in == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    sendbuf_out  = static_cast<int*>(
        MPIU_Malloc( sendbuf_size * sizeof(int) )
        );
    if( sendbuf_out == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    sendbuf_index = static_cast<int*>(
        MPIU_Malloc( 2 * comm_size * sizeof(int) )
        );
    if( sendbuf_index == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }
    ZeroMemory( sendbuf_index, 2 * comm_size * sizeof(int) );

    index = 0;
    int src, dst, d_weight;
    for( i = 0; i < n; i++ )
    {
        src = sources[i];
        for( j = 0; j < degrees[i]; j++ )
        {
            dst = destinations[index];
            sendbuf_in[sdispls[dst] + sendbuf_index[dst]] = src;
            sendbuf_out[sdispls[comm_size + src] + sendbuf_index[comm_size + src]] = dst;
            sendbuf_index[dst]++;
            sendbuf_index[comm_size + src]++;
 
            if( weights != MPI_UNWEIGHTED )
            {
                d_weight = weights[index];
                sendbuf_in[sdispls[dst] + sendbuf_index[dst]] = d_weight;
                sendbuf_out[sdispls[comm_size + src] + sendbuf_index[comm_size + src]] = d_weight;
                sendbuf_index[dst]++;
                sendbuf_index[comm_size + src]++;
            }
            index++;
        }
    }

    //
    // AlltoAll on the sendcount for the in-edge and out-edge will
    // give the recvcount for the in-edge and out-edge
    //
    recvcount = static_cast<int*>(
        MPIU_Malloc( 2 * comm_size * sizeof(int) )
        );
    if( recvcount == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    MPID_Request* creq_ptr;
    mpi_errno = MPIR_Ialltoall_intra(
        sendcount,
        1,
        g_hBuiltinTypes.MPI_Int,
        recvcount,
        1,
        g_hBuiltinTypes.MPI_Int,
        const_cast<MPID_Comm*>( comm_ptr ),
        MPIR_ALLTOALL_TAG,
        &creq_ptr
        );
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = MPIR_Wait( creq_ptr );
        creq_ptr->Release();
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPIR_Ialltoall_intra(
        sendcount + comm_size,
        1,
        g_hBuiltinTypes.MPI_Int,
        recvcount + comm_size,
        1,
        g_hBuiltinTypes.MPI_Int,
        const_cast<MPID_Comm*>( comm_ptr ),
        MPIR_ALLTOALL_TAG,
        &creq_ptr
        );
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = MPIR_Wait( creq_ptr );
        creq_ptr->Release();
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    rdispls = static_cast<int*>(
        MPIU_Malloc( 2 * comm_size * sizeof(int) )
        );
    if( rdispls == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }
    
    //
    // Calculate the data offset into the recvbuf_in/recvbuf_out arrays
    //
    int data_size_in = 0;
    int data_size_out = 0;
    for( i = 0; i < comm_size; i++ )
    {
        if( recvcount[i] != 0 )
        {
            rdispls[i] = data_size_in;
            data_size_in += recvcount[i];
        }
        if( recvcount[comm_size + i] != 0 )
        {
            rdispls[comm_size + i] = data_size_out;
            data_size_out += recvcount[comm_size + i];
        }
    }

    int indegree = data_size_in;
    int outdegree = data_size_out;
    if( weights != MPI_UNWEIGHTED )
    {
        indegree = data_size_in / 2;
        outdegree = data_size_out / 2;
    }

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

    //
    // Initialize the topology structure
    //
    int topo_size = sizeof(int) * (data_size_in + data_size_out);
    topo_ptr = static_cast<MPIR_Topology*>(
        MPIU_Malloc( sizeof(MPIR_Topology) + topo_size )
        );
    if( topo_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    topo_ptr->kind = MPI_DIST_GRAPH;
    topo_ptr->topo.dist_graph.indegree = indegree;
    topo_ptr->topo.dist_graph.outdegree = outdegree;
    topo_ptr->topo.dist_graph.in = nullptr;
    topo_ptr->topo.dist_graph.out = nullptr;
    topo_ptr->topo.dist_graph.in_weights = nullptr;
    topo_ptr->topo.dist_graph.out_weights = nullptr;
    topo_ptr->topo.dist_graph.is_weighted = ( weights != MPI_UNWEIGHTED ) ? true : false;

    int* inout_data_ptr = reinterpret_cast<int*>(topo_ptr + 1);
    int* inout_weights_ptr = inout_data_ptr + indegree + outdegree;

    if( indegree > 0 )
    {
        topo_ptr->topo.dist_graph.in = inout_data_ptr;
        if( weights != MPI_UNWEIGHTED )
        {
            topo_ptr->topo.dist_graph.in_weights = inout_weights_ptr;
        }
    }

    if( outdegree > 0 )
    {
        topo_ptr->topo.dist_graph.out = inout_data_ptr + indegree;
        if( weights != MPI_UNWEIGHTED )
        {
            topo_ptr->topo.dist_graph.out_weights = inout_weights_ptr + indegree;
        }
    }

    //
    // Exchange in-edge and out-edge topo data among all ranks
    //
    recvbuf_in = static_cast<int*>(
        MPIU_Malloc( data_size_in * sizeof(int) )
        );
    if( recvbuf_in == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    mpi_errno = MPIR_Ialltoallv_intra(
        sendbuf_in,
        sendcount,
        sdispls,
        g_hBuiltinTypes.MPI_Int,
        recvbuf_in,
        recvcount,
        rdispls,
        g_hBuiltinTypes.MPI_Int,
        comm_ptr,
        MPIR_ALLTOALLV_TAG,
        &creq_ptr
        );
    if (mpi_errno == MPI_SUCCESS)
    {
        mpi_errno = MPIR_Wait(creq_ptr);
        creq_ptr->Release();
    }
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    recvbuf_out  = static_cast<int*>(
        MPIU_Malloc( data_size_out * sizeof(int) )
        );
    if( recvbuf_out == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    mpi_errno = MPIR_Ialltoallv_intra(
        sendbuf_out,
        sendcount + comm_size,
        sdispls + comm_size,
        g_hBuiltinTypes.MPI_Int,
        recvbuf_out,
        recvcount + comm_size,
        rdispls + comm_size,
        g_hBuiltinTypes.MPI_Int,
        comm_ptr,
        MPIR_ALLTOALLV_TAG,
        &creq_ptr
        );
    if (mpi_errno == MPI_SUCCESS)
    {
        mpi_errno = MPIR_Wait(creq_ptr);
        creq_ptr->Release();
    }
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    //
    // Copy data from the recv buffer into the topo structure
    //
    if( weights != MPI_UNWEIGHTED )
    {
        j = 0;
        i = 0;
        for( index = 0; index < indegree; index++ )
        {
            topo_ptr->topo.dist_graph.in[j] = recvbuf_in[i++];
            topo_ptr->topo.dist_graph.in_weights[j++] = recvbuf_in[i++];
        }

        j = 0;
        i = 0;
        for( index = 0; index < outdegree; index++ )
        {
            topo_ptr->topo.dist_graph.out[j] = recvbuf_out[i++];
            topo_ptr->topo.dist_graph.out_weights[j++] = recvbuf_out[i++];
        }
    }
    else
    {
        CopyMemory( topo_ptr->topo.dist_graph.in, recvbuf_in, indegree * sizeof(int) );
        CopyMemory( topo_ptr->topo.dist_graph.out, recvbuf_out, outdegree * sizeof(int) );
    }

    mpi_errno =  MPIR_Topology_put( comm_dist_graph_ptr, topo_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    topo_ptr.detach();

    *comm_dist_graph = comm_dist_graph_ptr->handle;

  fn_exit:
    return mpi_errno;

  fn_fail:
    *comm_dist_graph = MPI_COMM_NULL;

    goto fn_exit;
}
