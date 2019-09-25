// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "dataloopp.h"
#include "veccpy.h"


/* Notes on functions:
 *
 * There are a few different sets of functions here:
 * - DLOOP_Segment_manipulate() - uses a "piece" function to perform operations
 *   using segments (piece functions defined elsewhere)
 * - MPID_ functions - these define the externally visible interface
 *   to segment functionality
 */

static inline DLOOP_Count DLOOP_Stackelm_blocksize(const struct DLOOP_Dataloop_stackelm *elmp);
static inline DLOOP_Offset DLOOP_Stackelm_offset(const struct DLOOP_Dataloop_stackelm *elmp);
static inline void DLOOP_Stackelm_load(struct DLOOP_Dataloop_stackelm *elmp,
                                       struct DLOOP_Dataloop *dlp,
                                       int branch_flag);
/* Segment_init
 *
 * buf    - datatype buffer location
 * count  - number of instances of the datatype in the buffer
 * handle - handle for datatype (could be derived or not)
 * segp   - pointer to previously allocated segment structure
 * flag   - flag indicating which optimizations are valid
 *          should be one of DLOOP_DATALOOP_HOMOGENEOUS, _HETEROGENEOUS,
 *          of _ALL_BYTES.
 *
 * Notes:
 * - Assumes that the segment has been allocated.
 * - Older MPICH2 code may pass "0" to indicate HETEROGENEOUS or "1" to
 *   indicate HETEROGENEOUS.
 *
 */
MPI_RESULT
MPID_Segment_init(
    const DLOOP_Buffer buf,
    DLOOP_Count count,
    DLOOP_Handle handle,
    struct DLOOP_Segment *segp,
    int flag)
{
    DLOOP_Offset elmsize = 0;
    int i, depth = 0;
    int branch_detected = 0;

    struct DLOOP_Dataloop_stackelm *elmp;
    struct DLOOP_Dataloop *dlp = 0, *sblp = &segp->builtin_loop;

    MPIU_Assert(flag == DLOOP_DATALOOP_HETEROGENEOUS ||
                 flag == DLOOP_DATALOOP_HOMOGENEOUS);

    if (!DLOOP_Handle_hasloop_macro(handle))
    {
        /* simplest case; datatype has no loop (basic) */

        elmsize = DLOOP_Handle_get_size_macro(handle);

        sblp->kind = DLOOP_KIND_CONTIG | DLOOP_FINAL_MASK;
        sblp->loop_params.c_t.count = count;
        sblp->loop_params.c_t.dataloop = 0;
        sblp->el_size = elmsize;
        sblp->el_type = DLOOP_Handle_get_basic_type_macro(handle);
        sblp->el_extent = DLOOP_Handle_get_extent_macro(handle);

        dlp = sblp;
        depth = 1;
    }
    else if (count == 0)
    {
        /* only use the builtin */
        sblp->kind = DLOOP_KIND_CONTIG | DLOOP_FINAL_MASK;
        sblp->loop_params.c_t.count = 0;
        sblp->loop_params.c_t.dataloop = 0;
        sblp->el_size = 0;
        sblp->el_extent = 0;

        dlp = sblp;
        depth = 1;
    }
    else if (count == 1)
    {
        /* don't use the builtin */
        DLOOP_Handle_get_loopptr_macro(handle, dlp, flag);
        DLOOP_Handle_get_loopdepth_macro(handle, depth, flag);
    }
    else
    {
        /* default: need to use builtin to handle contig; must check
         * loop depth first
         */
        DLOOP_Dataloop *oldloop; /* loop from original type, before new count */
        DLOOP_Offset type_size, type_extent;
        DLOOP_Type el_type;

        DLOOP_Handle_get_loopdepth_macro(handle, depth, flag);
        if (depth >= DLOOP_MAX_DATATYPE_DEPTH)
        {
            return MPIU_ERR_NOMEM();
        }


        DLOOP_Handle_get_loopptr_macro(handle, oldloop, flag);
        MPIU_Assert(oldloop != NULL);
        type_size = DLOOP_Handle_get_size_macro(handle);
        type_extent = DLOOP_Handle_get_extent_macro(handle);
        el_type = DLOOP_Handle_get_basic_type_macro(handle);

        if (depth == 1 && ((oldloop->kind & DLOOP_KIND_MASK) == DLOOP_KIND_CONTIG))
        {
            if (type_size == type_extent)
            {
                /* use a contig */
                sblp->kind                     = DLOOP_KIND_CONTIG | DLOOP_FINAL_MASK;
                sblp->loop_params.c_t.count    = count * oldloop->loop_params.c_t.count;
                sblp->loop_params.c_t.dataloop = NULL;
                sblp->el_size                  = oldloop->el_size;
                sblp->el_extent                = oldloop->el_extent;
                sblp->el_type                  = oldloop->el_type;
            }
            else
            {
                /* use a vector, with extent of original type becoming the stride */
                sblp->kind                      = DLOOP_KIND_VECTOR | DLOOP_FINAL_MASK;
                sblp->loop_params.v_t.count     = count;
                sblp->loop_params.v_t.blocksize = oldloop->loop_params.c_t.count;
                sblp->loop_params.v_t.stride    = type_extent;
                sblp->loop_params.v_t.dataloop  = NULL;
                sblp->el_size                   = oldloop->el_size;
                sblp->el_extent                 = oldloop->el_extent;
                sblp->el_type                   = oldloop->el_type;
            }
        }
        else
        {
            /* general case */
            sblp->kind                     = DLOOP_KIND_CONTIG;
            sblp->loop_params.c_t.count    = count;
            sblp->loop_params.c_t.dataloop = oldloop;
            sblp->el_size                  = type_size;
            sblp->el_extent                = type_extent;
            sblp->el_type                  = el_type;

            depth++; /* we're adding to the depth with the builtin */
        }

        dlp = sblp;
    }

    /* initialize the rest of the segment values */
    segp->handle = handle;
    segp->ptr = (char*) buf;
    segp->stream_off = 0;
    segp->cur_sp = 0;
    segp->valid_sp = 0;

    /* initialize the first stackelm in its entirety */
    elmp = &(segp->stackelm[0]);
    DLOOP_Stackelm_load(elmp, dlp, 0);
    branch_detected = elmp->may_require_reloading;

    /* Fill in parameters not set by DLOOP_Stackelm_load */
    elmp->orig_offset = 0;
    elmp->curblock    = elmp->orig_block;
    /* DLOOP_Stackelm_offset assumes correct orig_count, curcount, loop_p */
    elmp->curoffset   = /* elmp->orig_offset + */ DLOOP_Stackelm_offset(elmp);

    /* get pointer to next dataloop */
    i = 1;
    while (!(dlp->kind & DLOOP_FINAL_MASK))
    {
        switch (dlp->kind & DLOOP_KIND_MASK)
        {
            case DLOOP_KIND_CONTIG:
            case DLOOP_KIND_VECTOR:
            case DLOOP_KIND_BLOCKINDEXED:
            case DLOOP_KIND_INDEXED:
                dlp = dlp->loop_params.cm_t.dataloop;
                break;
            case DLOOP_KIND_STRUCT:
                dlp = dlp->loop_params.s_t.dataloop_array[0];
                break;
            default:
                MPIU_Assert(0);
                break;
        }

        /* loop_p, orig_count, orig_block, and curcount are all filled by us now.
         * the rest are filled in at processing time.
         */
        elmp = &(segp->stackelm[i]);

        DLOOP_Stackelm_load(elmp, dlp, branch_detected);
        branch_detected = elmp->may_require_reloading;
        i++;
    }

    segp->valid_sp = depth-1;

    return MPI_SUCCESS;
}

/* Segment_alloc
 *
 */
struct DLOOP_Segment * MPID_Segment_alloc(void)
{
    return MPIU_Malloc_obj(DLOOP_Segment);
}

/* Segment_free
 *
 * Input Parameters:
 * segp - pointer to segment
 */
void MPID_Segment_free(struct DLOOP_Segment *segp)
{
    MPIU_Free(segp);
    return;
}

/* DLOOP_Segment_manipulate - do something to a segment
 *
 * If you think of all the data to be manipulated (packed, unpacked, whatever),
 * as a stream of bytes, it's easier to understand how first and last fit in.
 *
 * This function does all the work, calling the piecefn passed in when it
 * encounters a datatype element which falls into the range of first..(last-1).
 *
 * piecefn can be NULL, in which case this function doesn't do anything when it
 * hits a region.  This is used internally for repositioning within this stream.
 *
 * last is a byte offset to the byte just past the last byte in the stream
 * to operate on.  this makes the calculations all over MUCH cleaner.
 *
 * stream_off, stream_el_size, first, and last are all working in terms of the
 * types and sizes for the stream, which might be different from the local sizes
 * (in the heterogeneous case).
 *
 * This is a horribly long function.  Too bad; it's complicated :)! -- Rob
 *
 * NOTE: THIS IMPLEMENTATION CANNOT HANDLE STRUCT DATALOOPS.
 */

__forceinline
DLOOP_Offset DloopSegmentUpdate(
    _In_ DLOOP_Segment *segp,
         int           cur_sp,
         int           valid_sp,
         DLOOP_Offset  stream_off )
{
    segp->cur_sp     = cur_sp;
    segp->valid_sp   = valid_sp;
    segp->stream_off = stream_off;
    return stream_off;
}

__forceinline
DLOOP_Dataloop_stackelm*  DloopSegmentGetValues(
    _In_  DLOOP_Segment*            segp,
    _In_  const DLOOP_Offset*       lastp,
    _Out_ int*                      cur_sp,
    _Out_ int*                      valid_sp,
    _Out_ DLOOP_Offset*             stream_off,
    _Out_ DLOOP_Offset*             last )
{
    *cur_sp     = segp->cur_sp;
    *valid_sp   = segp->valid_sp;
    *stream_off = segp->stream_off;
    *last       = *lastp;
    return &(segp->stackelm[segp->cur_sp]);
}

#define DLOOP_STACKELM_BLOCKINDEXED_OFFSET(elmp_, curcount_) \
(elmp_)->loop_p->loop_params.bi_t.offset_array[(curcount_)]

#define DLOOP_STACKELM_INDEXED_OFFSET(elmp_, curcount_) \
(elmp_)->loop_p->loop_params.i_t.offset_array[(curcount_)]

#define DLOOP_STACKELM_INDEXED_BLOCKSIZE(elmp_, curcount_) \
(elmp_)->loop_p->loop_params.i_t.blocksize_array[(curcount_)]

#define DLOOP_STACKELM_STRUCT_OFFSET(elmp_, curcount_) \
(elmp_)->loop_p->loop_params.s_t.offset_array[(curcount_)]

#define DLOOP_STACKELM_STRUCT_BLOCKSIZE(elmp_, curcount_) \
(elmp_)->loop_p->loop_params.s_t.blocksize_array[(curcount_)]

#define DLOOP_STACKELM_STRUCT_EL_EXTENT(elmp_, curcount_) \
(elmp_)->loop_p->loop_params.s_t.el_extent_array[(curcount_)]

#define DLOOP_STACKELM_STRUCT_DATALOOP(elmp_, curcount_) \
(elmp_)->loop_p->loop_params.s_t.dataloop_array[(curcount_)]


void
MPID_Segment_manipulate(
    _In_ struct DLOOP_Segment *segp,
    DLOOP_Offset first,
    _Inout_ DLOOP_Offset *lastp,
    int (*contigfn) (DLOOP_Offset *blocks_p,
                     DLOOP_Type el_type,
                     DLOOP_Offset rel_off,
                     DLOOP_Buffer bufp,
                     void *v_paramp),
    int (*vectorfn) (DLOOP_Offset *blocks_p,
                     DLOOP_Count count,
                     DLOOP_Count blklen,
                     DLOOP_Offset stride,
                     DLOOP_Type el_type,
                     DLOOP_Offset rel_off,
                     DLOOP_Buffer bufp,
                     void *v_paramp),
    int (*blkidxfn) (DLOOP_Offset *blocks_p,
                     DLOOP_Count count,
                     DLOOP_Count blklen,
                     DLOOP_Offset *offsetarray,
                     DLOOP_Type el_type,
                     DLOOP_Offset rel_off,
                     DLOOP_Buffer bufp,
                     void *v_paramp),
    int (*indexfn) (DLOOP_Offset *blocks_p,
                    DLOOP_Count count,
                    DLOOP_Count *blockarray,
                    DLOOP_Offset *offsetarray,
                    DLOOP_Type el_type,
                    DLOOP_Offset rel_off,
                    DLOOP_Buffer bufp,
                    void *v_paramp),
    DLOOP_Offset (*sizefn) (DLOOP_Type el_type),
    void *pieceparams
    )
{
    /* these four are the "local values": cur_sp, valid_sp, last, stream_off */
    int cur_sp, valid_sp;
    DLOOP_Offset last, stream_off;

    struct DLOOP_Dataloop_stackelm *cur_elmp;
    enum { PF_NULL, PF_CONTIG, PF_VECTOR, PF_BLOCKINDEXED, PF_INDEXED } piecefn_type = PF_NULL;

    cur_elmp = DloopSegmentGetValues( segp,
                                      lastp,
                                      &cur_sp,
                                      &valid_sp,
                                      &stream_off,
                                      &last );

    if (first == *lastp)
    {
        /* nothing to do */
        return;
    }

    /* first we ensure that stream_off and first are in the same spot */
    if (first != stream_off)
    {
        if (first < stream_off)
        {
            //
            // Reset the values for the DLOOP segment
            //
            segp->stream_off             = 0;
            segp->cur_sp                 = 0;
            segp->stackelm[0].curcount   = segp->stackelm[0].orig_count;
            segp->stackelm[0].orig_block =
                DLOOP_Stackelm_blocksize( &segp->stackelm[0] );
            segp->stackelm[0].curblock   = segp->stackelm[0].orig_block;
            segp->stackelm[0].curoffset  = segp->stackelm[0].orig_offset +
                DLOOP_Stackelm_offset( &segp->stackelm[0] );

            cur_elmp                     = &(segp->stackelm[0]);
            stream_off                   = 0;
        }

        if (first != stream_off)
        {
            DLOOP_Offset tmp_last = first;

            /* use manipulate function with a NULL piecefn to advance
             * stream offset
             */
            MPID_Segment_manipulate(segp,
                                               stream_off,
                                               &tmp_last,
                                               NULL, /* contig fn */
                                               NULL, /* vector fn */
                                               NULL, /* blkidx fn */
                                               NULL, /* index fn */
                                               sizefn,
                                               NULL);

            /* verify that we're in the right location */
            MPIU_Assert( tmp_last == first );
        }

        cur_elmp = DloopSegmentGetValues( segp,
                                          lastp,
                                          &cur_sp,
                                          &valid_sp,
                                          &stream_off,
                                          &last );
    }

    for (;;)
    {
        if (cur_elmp->loop_p->kind & DLOOP_FINAL_MASK)
        {
            int piecefn_indicated_exit = -1;
            DLOOP_Offset myblocks, local_el_size, stream_el_size;
            DLOOP_Type el_type;

            /* structs are never finals (leaves) */
            MPIU_Assert((cur_elmp->loop_p->kind & DLOOP_KIND_MASK) !=
                   DLOOP_KIND_STRUCT);

            /* pop immediately on zero count */
            if (cur_elmp->curcount == 0)
            {
                //
                // Pop DLOOP segment and exit if necessary
                //
                if( cur_sp == 0 )
                {
                    *lastp = DloopSegmentUpdate( segp,
                                                 cur_sp,
                                                 valid_sp,
                                                 stream_off );
                    return;
                }
                --cur_sp;
                --cur_elmp;
            }

            /* size on this system of the int, double, etc. that is
             * the elementary type.
             */
            local_el_size  = cur_elmp->loop_p->el_size;
            el_type        = cur_elmp->loop_p->el_type;
            stream_el_size = (sizefn) ? sizefn(el_type) : local_el_size;

            /* calculate number of elem. types to work on and function to use.
             * default is to use the contig piecefn (if there is one).
             */
            myblocks = cur_elmp->curblock;
            piecefn_type = (contigfn ? PF_CONTIG : PF_NULL);

            /* check for opportunities to use other piecefns */
            switch (cur_elmp->loop_p->kind & DLOOP_KIND_MASK)
            {
                case DLOOP_KIND_CONTIG:
                    break;
                case DLOOP_KIND_BLOCKINDEXED:
                    /* only use blkidx piecefn if at start of blkidx type */
                    if (blkidxfn &&
                        cur_elmp->orig_block == cur_elmp->curblock &&
                        cur_elmp->orig_count == cur_elmp->curcount)
                    {
                        /* TODO: RELAX CONSTRAINTS */
                        myblocks = cur_elmp->curblock * cur_elmp->curcount;
                        piecefn_type = PF_BLOCKINDEXED;
                    }
                    break;
                case DLOOP_KIND_INDEXED:
                    /* only use index piecefn if at start of the index type.
                     *   count test checks that we're on first block.
                     *   block test checks that we haven't made progress on first block.
                     */
                    if (indexfn &&
                        cur_elmp->orig_count == cur_elmp->curcount &&
                        cur_elmp->curblock == DLOOP_STACKELM_INDEXED_BLOCKSIZE(cur_elmp, 0))
                    {
                        /* TODO: RELAX CONSTRAINT ON COUNT? */
                        myblocks = cur_elmp->loop_p->loop_params.i_t.total_blocks;
                        piecefn_type = PF_INDEXED;
                    }
                    break;
                case DLOOP_KIND_VECTOR:
                    /* only use the vector piecefn if at the start of a
                     * contiguous block.
                     */
                    if (vectorfn && cur_elmp->orig_block == cur_elmp->curblock)
                    {
                        myblocks = cur_elmp->curblock * cur_elmp->curcount;
                        piecefn_type = PF_VECTOR;
                    }
                    break;
                default:
                    MPIU_Assert(0);
                    break;
            }

            /* enforce the last parameter if necessary by reducing myblocks */
            if (last != SEGMENT_IGNORE_LAST &&
                (stream_off + (myblocks * stream_el_size) > last))
            {
                myblocks = ((last - stream_off) / stream_el_size);
                if (myblocks == 0)
                {
                    *lastp = DloopSegmentUpdate( segp,
                                                 cur_sp,
                                                 valid_sp,
                                                 stream_off );
                    return;
                }
            }

            /* call piecefn to perform data manipulation */
            switch (piecefn_type)
            {
                case PF_NULL:
                    piecefn_indicated_exit = 0;
                    break;
                case PF_CONTIG:
                    MPIU_Assert(myblocks <= cur_elmp->curblock);
                    piecefn_indicated_exit =
                        contigfn(&myblocks,
                                 el_type,
                                 cur_elmp->curoffset, /* relative to segp->ptr */
                                 segp->ptr, /* start of buffer (from segment) */
                                 pieceparams);
                    break;
                case PF_VECTOR:
                    piecefn_indicated_exit =
OACR_WARNING_SUPPRESS(26500, "Suppress false anvil warning.")
                        vectorfn(&myblocks,
                                 cur_elmp->curcount,
                                 cur_elmp->orig_block,
                                 cur_elmp->loop_p->loop_params.v_t.stride,
                                 el_type,
                                 cur_elmp->curoffset,
                                 segp->ptr,
                                 pieceparams);
                    break;
                case PF_BLOCKINDEXED:
                    piecefn_indicated_exit =
OACR_WARNING_SUPPRESS(26500, "Suppress false anvil warning.")
                        blkidxfn(&myblocks,
                                 cur_elmp->curcount,
                                 cur_elmp->orig_block,
                                 cur_elmp->loop_p->loop_params.bi_t.offset_array,
                                 el_type,
                                 cur_elmp->orig_offset, /* blkidxfn adds offset */
                                 segp->ptr,
                                 pieceparams);
                    break;
                case PF_INDEXED:
                    piecefn_indicated_exit =
OACR_WARNING_SUPPRESS(26500, "Suppress false anvil warning.")
                        indexfn(&myblocks,
                                cur_elmp->curcount,
                                cur_elmp->loop_p->loop_params.i_t.blocksize_array,
                                cur_elmp->loop_p->loop_params.i_t.offset_array,
                                el_type,
                                cur_elmp->orig_offset, /* indexfn adds offset value */
                                segp->ptr,
                                pieceparams);
                    break;
            }

            /* update local values based on piecefn returns (myblocks and
             * piecefn_indicated_exit)
             */
            MPIU_Assert(piecefn_indicated_exit >= 0);
            MPIU_Assert(myblocks >= 0);
            stream_off += myblocks * stream_el_size;

            /* myblocks of 0 or less than cur_elmp->curblock indicates
             * that we should stop processing and return.
             */
            if (myblocks == 0)
            {
                *lastp = DloopSegmentUpdate( segp,
                                             cur_sp,
                                             valid_sp,
                                             stream_off );
                return;
            }
            else if (myblocks < cur_elmp->curblock)
            {
                cur_elmp->curoffset += myblocks * local_el_size;
                cur_elmp->curblock  -= (DLOOP_Count)myblocks;

                *lastp = DloopSegmentUpdate( segp,
                                             cur_sp,
                                             valid_sp,
                                             stream_off );
                return;
            }

            /* myblocks >= cur_elmp->curblock */

            int count_index = 0;

            /* this assumes we're either *just* processing the last parts
             * of the current block, or we're processing as many blocks as
             * we like starting at the beginning of one.
             */

            switch (cur_elmp->loop_p->kind & DLOOP_KIND_MASK)
            {
            case DLOOP_KIND_INDEXED:
                while (myblocks > 0 && myblocks >= cur_elmp->curblock)
                {
                    myblocks -= cur_elmp->curblock;
                    cur_elmp->curcount--;
                    MPIU_Assert(cur_elmp->curcount >= 0);

                    count_index = cur_elmp->orig_count -
                        cur_elmp->curcount;
                    cur_elmp->curblock =
                        DLOOP_STACKELM_INDEXED_BLOCKSIZE(cur_elmp,
                                                         count_index);
                }

                if (cur_elmp->curcount == 0)
                {
                    /* don't bother to fill in values; we're popping anyway */
                    MPIU_Assert(myblocks == 0);

                    //
                    // Pop DLOOP segment and exit if necessary
                    //
                    if( cur_sp == 0 )
                    {
                        *lastp = DloopSegmentUpdate( segp,
                                                     cur_sp,
                                                     valid_sp,
                                                     stream_off );
                        return;
                    }
                    --cur_sp;
                    --cur_elmp;
                }
                else
                {
                    cur_elmp->orig_block = cur_elmp->curblock;
                    cur_elmp->curoffset  = cur_elmp->orig_offset +
                        DLOOP_STACKELM_INDEXED_OFFSET(cur_elmp,
                                                      count_index);

                    cur_elmp->curblock  -= (DLOOP_Count)myblocks;
                    cur_elmp->curoffset += myblocks * local_el_size;
                }
                break;
            case DLOOP_KIND_VECTOR:
                /* this math relies on assertions at top of code block */
                cur_elmp->curcount -= (DLOOP_Count)myblocks / cur_elmp->curblock;
                if (cur_elmp->curcount == 0)
                {
                    MPIU_Assert(myblocks % cur_elmp->curblock == 0);

                    //
                    // Pop DLOOP segment and exit if necessary
                    //
                    if( cur_sp == 0 )
                    {
                        *lastp = DloopSegmentUpdate( segp,
                                                     cur_sp,
                                                     valid_sp,
                                                     stream_off );
                        return;
                    }
                    --cur_sp;
                    --cur_elmp;
                }
                else
                {
                    /* this math relies on assertions at top of code block */
                    cur_elmp->curblock = cur_elmp->orig_block - ((DLOOP_Count)myblocks % cur_elmp->curblock);
                    /* new offset = original offset +
                     *              stride * whole blocks +
                     *              leftover bytes
                     */
                    cur_elmp->curoffset = cur_elmp->orig_offset +
                        ((cur_elmp->orig_count - cur_elmp->curcount) *
                         cur_elmp->loop_p->loop_params.v_t.stride) +
                        ((cur_elmp->orig_block - cur_elmp->curblock) *
                         local_el_size);
                }
                break;
            case DLOOP_KIND_CONTIG:
                /* contigs that reach this point have always been
                 * completely processed
                 */
                MPIU_Assert(myblocks == cur_elmp->curblock &&
                            cur_elmp->curcount == 1);

                //
                // Pop DLOOP segment and exit if necessary
                //
                if( cur_sp == 0 )
                {
                    *lastp = DloopSegmentUpdate( segp,
                                                 cur_sp,
                                                 valid_sp,
                                                 stream_off );
                    return;
                }
                --cur_sp;
                --cur_elmp;
                break;
            case DLOOP_KIND_BLOCKINDEXED:
                while (myblocks > 0 && myblocks >= cur_elmp->curblock)
                {
                    myblocks -= cur_elmp->curblock;
                    cur_elmp->curcount--;
                    MPIU_Assert(cur_elmp->curcount >= 0);

                    count_index = cur_elmp->orig_count -
                        cur_elmp->curcount;
                    cur_elmp->curblock = cur_elmp->orig_block;
                }
                if (cur_elmp->curcount == 0)
                {
                    /* popping */
                    MPIU_Assert(myblocks == 0);

                    //
                    // Pop DLOOP segment and exit if necessary
                    //
                    if( cur_sp == 0 )
                    {
                        *lastp = DloopSegmentUpdate( segp,
                                                     cur_sp,
                                                     valid_sp,
                                                     stream_off );
                        return;
                    }
                    --cur_sp;
                    --cur_elmp;
                }
                else
                {
                    /* cur_elmp->orig_block = cur_elmp->curblock; */
                    cur_elmp->curoffset = cur_elmp->orig_offset +
                        DLOOP_STACKELM_BLOCKINDEXED_OFFSET(cur_elmp, count_index);
                    cur_elmp->curblock  -= (DLOOP_Count)myblocks;
                    cur_elmp->curoffset += myblocks * local_el_size;
                }
                break;
            }

            if (piecefn_indicated_exit)
            {
                /* piece function indicated that we should quit processing */
                *lastp = DloopSegmentUpdate( segp,
                                             cur_sp,
                                             valid_sp,
                                             stream_off );
                return;
            }
        } /* end of if leaf */
        else if (cur_elmp->curblock == 0)
        {
            cur_elmp->curcount--;

            /* new block.  for indexed and struct reset orig_block.
             * reset curblock for all types
             */
            switch (cur_elmp->loop_p->kind & DLOOP_KIND_MASK)
            {
                case DLOOP_KIND_CONTIG:
                case DLOOP_KIND_VECTOR:
                case DLOOP_KIND_BLOCKINDEXED:
                    break;
                case DLOOP_KIND_INDEXED:
                    cur_elmp->orig_block =
                        DLOOP_STACKELM_INDEXED_BLOCKSIZE(cur_elmp, cur_elmp->curcount ? cur_elmp->orig_count - cur_elmp->curcount : 0);
                    break;
                case DLOOP_KIND_STRUCT:
                    cur_elmp->orig_block =
                        DLOOP_STACKELM_STRUCT_BLOCKSIZE(cur_elmp, cur_elmp->curcount ? cur_elmp->orig_count - cur_elmp->curcount : 0);
                    break;
                default:
                    MPIU_Assert(0);
                    break;
            }
            cur_elmp->curblock = cur_elmp->orig_block;

            if (cur_elmp->curcount == 0)
            {
                if( cur_sp == 0 )
                {
                    *lastp = DloopSegmentUpdate( segp,
                                                 cur_sp,
                                                 valid_sp,
                                                 stream_off );
                    return;
                }
                --cur_sp;
                --cur_elmp;
            }
        }
        else /* push the stackelm */
        {
            DLOOP_Dataloop_stackelm *next_elmp;
            int count_index, block_index;

            count_index = cur_elmp->orig_count - cur_elmp->curcount;
            block_index = cur_elmp->orig_block - cur_elmp->curblock;

            /* reload the next stackelm if necessary */
            next_elmp = &(segp->stackelm[cur_sp + 1]);
            if (cur_elmp->may_require_reloading)
            {
                DLOOP_Dataloop *load_dlp = NULL;
                switch (cur_elmp->loop_p->kind & DLOOP_KIND_MASK)
                {
                    case DLOOP_KIND_CONTIG:
                    case DLOOP_KIND_VECTOR:
                    case DLOOP_KIND_BLOCKINDEXED:
                    case DLOOP_KIND_INDEXED:
                        load_dlp = cur_elmp->loop_p->loop_params.cm_t.dataloop;
                        break;
                    case DLOOP_KIND_STRUCT:
                        load_dlp = DLOOP_STACKELM_STRUCT_DATALOOP(cur_elmp,
                                                                  count_index);
                        break;
                    default:
                        MPIU_Assert(0);
                        break;
                }

                DLOOP_Stackelm_load(next_elmp,
                                    load_dlp,
                                    1);
            }

            /* set orig_offset and all cur values for new stackelm.
             * this is done in two steps: first set orig_offset based on
             * current stackelm, then set cur values based on new stackelm.
             */
            switch (cur_elmp->loop_p->kind & DLOOP_KIND_MASK)
            {
                case DLOOP_KIND_CONTIG:
                    next_elmp->orig_offset = cur_elmp->curoffset +
                        block_index * cur_elmp->loop_p->el_extent;
                    break;
                case DLOOP_KIND_VECTOR:
                    /* note: stride is in bytes */
                    next_elmp->orig_offset = cur_elmp->orig_offset +
                        count_index * cur_elmp->loop_p->loop_params.v_t.stride +
                        block_index * cur_elmp->loop_p->el_extent;
                    break;
                case DLOOP_KIND_BLOCKINDEXED:
                    next_elmp->orig_offset = cur_elmp->orig_offset +
                        block_index * cur_elmp->loop_p->el_extent +
                        DLOOP_STACKELM_BLOCKINDEXED_OFFSET(cur_elmp,
                                                           count_index);
                    break;
                case DLOOP_KIND_INDEXED:
                    next_elmp->orig_offset = cur_elmp->orig_offset +
                        block_index * cur_elmp->loop_p->el_extent +
                        DLOOP_STACKELM_INDEXED_OFFSET(cur_elmp, count_index);
                    break;
                case DLOOP_KIND_STRUCT:
                    next_elmp->orig_offset = cur_elmp->orig_offset +
                        block_index * DLOOP_STACKELM_STRUCT_EL_EXTENT(cur_elmp, count_index) +
                        DLOOP_STACKELM_STRUCT_OFFSET(cur_elmp, count_index);
                    break;
                default:
                    MPIU_Assert(0);
                    break;
            }

            switch (next_elmp->loop_p->kind & DLOOP_KIND_MASK)
            {
                case DLOOP_KIND_CONTIG:
                case DLOOP_KIND_VECTOR:
                    next_elmp->curcount  = next_elmp->orig_count;
                    next_elmp->curblock  = next_elmp->orig_block;
                    next_elmp->curoffset = next_elmp->orig_offset;
                    break;
                case DLOOP_KIND_BLOCKINDEXED:
                    next_elmp->curcount  = next_elmp->orig_count;
                    next_elmp->curblock  = next_elmp->orig_block;
                    next_elmp->curoffset = next_elmp->orig_offset +
                        DLOOP_STACKELM_BLOCKINDEXED_OFFSET(next_elmp, 0);
                    break;
                case DLOOP_KIND_INDEXED:
                    next_elmp->curcount  = next_elmp->orig_count;
                    next_elmp->curblock  =
                        DLOOP_STACKELM_INDEXED_BLOCKSIZE(next_elmp, 0);
                    next_elmp->curoffset = next_elmp->orig_offset +
                        DLOOP_STACKELM_INDEXED_OFFSET(next_elmp, 0);
                    break;
                case DLOOP_KIND_STRUCT:
                    next_elmp->curcount = next_elmp->orig_count;
                    next_elmp->curblock =
                        DLOOP_STACKELM_STRUCT_BLOCKSIZE(next_elmp, 0);
                    next_elmp->curoffset = next_elmp->orig_offset +
                        DLOOP_STACKELM_STRUCT_OFFSET(next_elmp, 0);
                    break;
                default:
                    MPIU_Assert(0);
                    break;
            }

            cur_elmp->curblock--;

            //
            // Push Dloop segment
            //
            ++cur_sp;
            ++cur_elmp;

        } /* end of else push the stackelm */
    } /* end of for (;;) */
}

/* DLOOP_Stackelm_blocksize - returns block size for stackelm based on current
 * count in stackelm.
 *
 * NOTE: loop_p, orig_count, and curcount members of stackelm MUST be correct
 * before this is called!
 *
 */
static inline DLOOP_Count DLOOP_Stackelm_blocksize(const struct DLOOP_Dataloop_stackelm *elmp)
{
    const struct DLOOP_Dataloop *dlp = elmp->loop_p;

    switch(dlp->kind & DLOOP_KIND_MASK)
    {
        case DLOOP_KIND_CONTIG:
            /* NOTE: we're dropping the count into the
             * blksize field for contigs, as described
             * in the init call.
             */
            return dlp->loop_params.c_t.count;
            break;
        case DLOOP_KIND_VECTOR:
            return dlp->loop_params.v_t.blocksize;
            break;
        case DLOOP_KIND_BLOCKINDEXED:
            return dlp->loop_params.bi_t.blocksize;
            break;
        case DLOOP_KIND_INDEXED:
            return dlp->loop_params.i_t.blocksize_array[elmp->orig_count - elmp->curcount];
            break;
        case DLOOP_KIND_STRUCT:
            return dlp->loop_params.s_t.blocksize_array[elmp->orig_count - elmp->curcount];
            break;
        default:
            MPIU_Assert(0);
            break;
    }
    return -1;
}

/* DLOOP_Stackelm_offset - returns starting offset (displacement) for stackelm
 * based on current count in stackelm.
 *
 * NOTE: loop_p, orig_count, and curcount members of stackelm MUST be correct
 * before this is called!
 *
 * also, this really is only good at init time for vectors and contigs
 * (all the time for indexed) at the moment.
 *
 */
static inline DLOOP_Offset DLOOP_Stackelm_offset(const struct DLOOP_Dataloop_stackelm *elmp)
{
    const struct DLOOP_Dataloop *dlp = elmp->loop_p;

    switch(dlp->kind & DLOOP_KIND_MASK)
    {
        case DLOOP_KIND_VECTOR:
        case DLOOP_KIND_CONTIG:
            return 0;
            break;
        case DLOOP_KIND_BLOCKINDEXED:
            return dlp->loop_params.bi_t.offset_array[elmp->orig_count - elmp->curcount];
            break;
        case DLOOP_KIND_INDEXED:
            return dlp->loop_params.i_t.offset_array[elmp->orig_count - elmp->curcount];
            break;
        case DLOOP_KIND_STRUCT:
            return dlp->loop_params.s_t.offset_array[elmp->orig_count - elmp->curcount];
            break;
        default:
            MPIU_Assert(0);
            break;
    }
    return -1;
}

/* DLOOP_Stackelm_load
 * loop_p, orig_count, orig_block, and curcount are all filled by us now.
 * the rest are filled in at processing time.
 */
static inline void DLOOP_Stackelm_load(struct DLOOP_Dataloop_stackelm *elmp,
                                       struct DLOOP_Dataloop *dlp,
                                       int branch_flag)
{
    elmp->loop_p = dlp;

    if ((dlp->kind & DLOOP_KIND_MASK) == DLOOP_KIND_CONTIG)
    {
        elmp->orig_count = 1; /* put in blocksize instead */
    }
    else
    {
        elmp->orig_count = dlp->loop_params.count;
    }

    if (branch_flag || (dlp->kind & DLOOP_KIND_MASK) == DLOOP_KIND_STRUCT)
    {
        elmp->may_require_reloading = 1;
    }
    else
    {
        elmp->may_require_reloading = 0;
    }

    /* required by DLOOP_Stackelm_blocksize */
    elmp->curcount = elmp->orig_count;

    elmp->orig_block = DLOOP_Stackelm_blocksize(elmp);
    /* TODO: GO AHEAD AND FILL IN CURBLOCK? */
}


/* MPID_Segment_piece_params
*
* This structure is used to pass function-specific parameters into our
* segment processing function.  This allows us to get additional parameters
* to the functions it calls without changing the prototype.
*/
struct MPID_Segment_piece_params
{
    union
    {
        struct
        {
            char *pack_buffer;
        } pack;
        struct
        {
            MPID_IOV *vectorp;
            int index;
            int length;
        } pack_vector;
        struct
        {
            INT64 *offp;
            int *sizep; /* see notes in Segment_flatten header */
            int index;
            int length;
        } flatten;
        struct
        {
            char *last_loc;
            int count;
        } contig_blocks;
        struct
        {
            char *unpack_buffer;
        } unpack;
        struct
        {
            int stream_off;
        } print;
    } u;
};

/* prototypes of internal functions */
static int MPID_Segment_vector_pack_to_iov(DLOOP_Offset *blocks_p,
                                       int count,
                                       int blksz,
                                       DLOOP_Offset stride,
                                       DLOOP_Type el_type,
                                       DLOOP_Offset rel_off,
                                       void *bufp,
                                       void *v_paramp);

static int MPID_Segment_contig_pack_to_iov(DLOOP_Offset *blocks_p,
                                           DLOOP_Type el_type,
                                           DLOOP_Offset rel_off,
                                           void *bufp,
                                           void *v_paramp);

static int MPID_Segment_contig_flatten(DLOOP_Offset *blocks_p,
                                   DLOOP_Type el_type,
                                   DLOOP_Offset rel_off,
                                   void *bufp,
                                   void *v_paramp);

static int MPID_Segment_vector_flatten(DLOOP_Offset *blocks_p,
                                   int count,
                                   int blksz,
                                   DLOOP_Offset stride,
                                   DLOOP_Type el_type,
                                   DLOOP_Offset rel_off, /* offset into buffer */
                                   void *bufp, /* start of buffer */
                                   void *v_paramp);

/********** EXTERNALLY VISIBLE FUNCTIONS FOR TYPE MANIPULATION **********/

/* MPID_Segment_pack_vector
*
* Parameters:
* segp    - pointer to segment structure
* first   - first byte in segment to pack
* lastp   - in/out parameter describing last byte to pack (and afterwards
*           the last byte _actually_ packed)
*           NOTE: actually returns index of byte _after_ last one packed
* vectorp - pointer to (off, len) pairs to fill in
* lengthp - in/out parameter describing length of array (and afterwards
*           the amount of the array that has actual data)
*/
void MPID_Segment_pack_vector(struct DLOOP_Segment *segp,
                          DLOOP_Offset first,
                          DLOOP_Offset *lastp,
                          MPID_IOV *vectorp,
                          int *lengthp)
{
    struct MPID_Segment_piece_params packvec_params;

    packvec_params.u.pack_vector.vectorp = vectorp;
    packvec_params.u.pack_vector.index   = 0;
    packvec_params.u.pack_vector.length  = *lengthp;

    MPIU_Assert(*lengthp > 0);

    MPID_Segment_manipulate(segp,
                        first,
                        lastp,
                        MPID_Segment_contig_pack_to_iov,
                        MPID_Segment_vector_pack_to_iov,
                        NULL, /* blkidx fn */
                        NULL, /* index fn */
                        NULL,
                        &packvec_params);

    /* last value already handled by MPID_Segment_manipulate */
    *lengthp = packvec_params.u.pack_vector.index;
}

/* MPID_Segment_unpack_vector
*
* Q: Should this be any different from pack vector?
*/
void MPID_Segment_unpack_vector(struct DLOOP_Segment *segp,
                            DLOOP_Offset first,
                            DLOOP_Offset *lastp,
                            MPID_IOV *vectorp,
                            int *lengthp)
{
    MPID_Segment_pack_vector(segp, first, lastp, vectorp, lengthp);
}

/* MPID_Segment_flatten
*
* offp    - pointer to array to fill in with offsets
* sizep   - pointer to array to fill in with sizes
* lengthp - pointer to value holding size of arrays; # used is returned
*
* Internally, index is used to store the index of next array value to fill in.
*
* TODO: MAKE SIZES Aints IN ROMIO, CHANGE THIS TO USE INTS TOO.
*/
void MPID_Segment_flatten(struct DLOOP_Segment *segp,
                      DLOOP_Offset first,
                      DLOOP_Offset *lastp,
                      DLOOP_Offset *offp,
                      int *sizep,
                      DLOOP_Offset *lengthp)
{
    struct MPID_Segment_piece_params packvec_params;

    packvec_params.u.flatten.offp = (INT64 *) offp;
    packvec_params.u.flatten.sizep = sizep;
    packvec_params.u.flatten.index   = 0;
    packvec_params.u.flatten.length  = (int)*lengthp;

    MPIU_Assert(*lengthp > 0);

    MPID_Segment_manipulate(segp,
                        first,
                        lastp,
                        MPID_Segment_contig_flatten,
                        MPID_Segment_vector_flatten,
                        NULL, /* blkidx fn */
                        NULL,
                        NULL,
                        &packvec_params);

    /* last value already handled by MPID_Segment_manipulate */
    *lengthp = packvec_params.u.flatten.index;
}


/*
* EVERYTHING BELOW HERE IS USED ONLY WITHIN THIS FILE
*/

/********** FUNCTIONS FOR CREATING AN IOV DESCRIBING BUFFER **********/

/* MPID_Segment_contig_pack_to_iov
*/
static int MPID_Segment_contig_pack_to_iov(DLOOP_Offset *blocks_p,
                                           DLOOP_Type el_type,
                                           DLOOP_Offset rel_off,
                                           void *bufp,
                                           void *v_paramp)
{
    int el_size, last_idx;
    DLOOP_Offset size;
    const char *last_end = NULL;
    struct MPID_Segment_piece_params *paramp = (struct MPID_Segment_piece_params*)v_paramp;

    el_size = MPID_Datatype_get_basic_size(el_type);
    size = *blocks_p * (DLOOP_Offset) el_size;

    last_idx = paramp->u.pack_vector.index - 1;
    if (last_idx >= 0)
    {
        last_end = ((char *) paramp->u.pack_vector.vectorp[last_idx].buf) +
            paramp->u.pack_vector.vectorp[last_idx].len;
    }

    if ((last_idx == paramp->u.pack_vector.length-1) &&
        (last_end != ((char *) bufp + rel_off)))
    {
        /* we have used up all our entries, and this region doesn't fit on
         * the end of the last one.  setting blocks to 0 tells manipulation
         * function that we are done (and that we didn't process any blocks).
         */
        *blocks_p = 0;
        return 1;
    }
    else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
    {
        /* add this size to the last vector rather than using up another one */
        paramp->u.pack_vector.vectorp[last_idx].len += (int)size;
    }
    else
    {
        paramp->u.pack_vector.vectorp[last_idx+1].buf = (char *) bufp + rel_off;
        paramp->u.pack_vector.vectorp[last_idx+1].len = (int)size;
        paramp->u.pack_vector.index++;
    }
    return 0;
}

/* MPID_Segment_vector_pack_to_iov
 *
 * Input Parameters:
 * blocks_p - [inout] pointer to a count of blocks (total, for all noncontiguous pieces)
 * count    - # of noncontiguous regions
 * blksz    - size of each noncontiguous region
 * stride   - distance in bytes from start of one region to start of next
 * el_type - elemental type (e.g. MPI_INT)
 * ...
 *
 * Note: this is only called when the starting position is at the beginning
 * of a whole block in a vector type.
 */
static int MPID_Segment_vector_pack_to_iov(DLOOP_Offset *blocks_p,
                                           int count,
                                           int blksz,
                                           DLOOP_Offset stride,
                                           DLOOP_Type el_type,
                                           DLOOP_Offset rel_off, /* offset into buffer */
                                           void *bufp, /* start of buffer */
                                           void *v_paramp)
{
    int i, basic_size;
    DLOOP_Offset size, blocks_left;
    struct MPID_Segment_piece_params *paramp = (struct MPID_Segment_piece_params*)v_paramp;

    basic_size = MPID_Datatype_get_basic_size(el_type);
    blocks_left = *blocks_p;

    for (i=0; i < count && blocks_left > 0; i++)
    {
        int last_idx;
        const char *last_end = NULL;

        if (blocks_left > blksz)
        {
            size = blksz * basic_size;
            blocks_left -= blksz;
        }
        else
        {
            /* last pass */
            size = blocks_left * basic_size;
            blocks_left = 0;
        }

        last_idx = paramp->u.pack_vector.index - 1;
        if (last_idx >= 0)
        {
            last_end = ((char *) paramp->u.pack_vector.vectorp[last_idx].buf) +
                paramp->u.pack_vector.vectorp[last_idx].len;
        }

        if ((last_idx == paramp->u.pack_vector.length-1) &&
            (last_end != ((char *) bufp + rel_off)))
        {
            /* we have used up all our entries, and this one doesn't fit on
             * the end of the last one.
             */
            *blocks_p -= (blocks_left + (size / basic_size));

            return 1;
        }
        else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
        {
            /* add this size to the last vector rather than using up new one */
            paramp->u.pack_vector.vectorp[last_idx].len += (int)size;
        }
        else
        {
            paramp->u.pack_vector.vectorp[last_idx+1].buf =
                (char *) bufp + rel_off;
            paramp->u.pack_vector.vectorp[last_idx+1].len = (int)size;
            paramp->u.pack_vector.index++;
        }

        rel_off += stride;

    }

    /* if we get here then we processed ALL the blocks; don't need to update
     * blocks_p
     */
    MPIU_Assert(blocks_left == 0);
    return 0;
}

/********** FUNCTIONS FOR FLATTENING A TYPE **********/

/* MPID_Segment_contig_flatten
 */
static int MPID_Segment_contig_flatten(DLOOP_Offset *blocks_p,
                                       DLOOP_Type el_type,
                                       DLOOP_Offset rel_off,
                                       void *bufp,
                                       void *v_paramp)
{
    OACR_USE_PTR( blocks_p );
    OACR_USE_PTR( bufp );
    int index, el_size;
    DLOOP_Offset size;
    struct MPID_Segment_piece_params *paramp = (struct MPID_Segment_piece_params*)v_paramp;

    el_size = MPID_Datatype_get_basic_size(el_type);
    size = *blocks_p * (DLOOP_Offset) el_size;
    index = paramp->u.flatten.index;

    if (index > 0 && ((DLOOP_Offset) bufp + rel_off) ==
        ((paramp->u.flatten.offp[index - 1]) +
         (DLOOP_Offset) paramp->u.flatten.sizep[index - 1]))
    {
        /* add this size to the last vector rather than using up another one */
        paramp->u.flatten.sizep[index - 1] += (int)size;
    }
    else
    {
        paramp->u.flatten.offp[index] =  ((INT64) (MPI_Aint) bufp) + (INT64) rel_off;
        paramp->u.flatten.sizep[index] = (int)size;

        paramp->u.flatten.index++;
        /* check to see if we have used our entire vector buffer, and if so
         * return 1 to stop processing
         */
        if (paramp->u.flatten.index == paramp->u.flatten.length)
        {
            return 1;
        }
    }
    return 0;
}

/* MPID_Segment_vector_flatten
 *
 * Notes:
 * - this is only called when the starting position is at the beginning
 *   of a whole block in a vector type.
 * - this was a virtual copy of MPID_Segment_pack_to_iov; now it has improvements
 *   that MPID_Segment_pack_to_iov needs.
 * - we return the number of blocks that we did process in region pointed to by
 *   blocks_p.
 */
static int MPID_Segment_vector_flatten(DLOOP_Offset *blocks_p,
                                       int count,
                                       int blksz,
                                       DLOOP_Offset stride,
                                       DLOOP_Type el_type,
                                       DLOOP_Offset rel_off, /* offset into buffer */
                                       void *bufp, /* start of buffer */
                                       void *v_paramp)
{
    OACR_USE_PTR( bufp );
    int i, basic_size;
    DLOOP_Offset size, blocks_left;
    struct MPID_Segment_piece_params *paramp = (struct MPID_Segment_piece_params*)v_paramp;

    basic_size = MPID_Datatype_get_basic_size(el_type);
    blocks_left = *blocks_p;

    for (i=0; i < count && blocks_left > 0; i++)
    {
        int index = paramp->u.flatten.index;

        if (blocks_left > blksz)
        {
            size = blksz * (DLOOP_Offset) basic_size;
            blocks_left -= blksz;
        }
        else
        {
            /* last pass */
            size = blocks_left * basic_size;
            blocks_left = 0;
        }

        if (index > 0 && ((DLOOP_Offset) bufp + rel_off) ==
            ((paramp->u.flatten.offp[index - 1]) + (DLOOP_Offset) paramp->u.flatten.sizep[index - 1]))
        {
            /* add this size to the last region rather than using up another one */
            paramp->u.flatten.sizep[index - 1] += (int)size;
        }
        else if (index < paramp->u.flatten.length)
        {
            /* take up another region */
            paramp->u.flatten.offp[index]  = (DLOOP_Offset) bufp + rel_off;
            paramp->u.flatten.sizep[index] = (int)size;
            paramp->u.flatten.index++;
        }
        else
        {
            /* we tried to add to the end of the last region and failed; add blocks back in */
            *blocks_p = *blocks_p - blocks_left + (size / basic_size);
            return 1;
        }
        rel_off += stride;

    }
    MPIU_Assert(blocks_left == 0);
    return 0;
}


static int MPID_Segment_contig_m2m(DLOOP_Offset *blocks_p,
                                       DLOOP_Type el_type,
                                       DLOOP_Offset rel_off,
                                       void* /*bufp*/,
                                       void *v_paramp)
{
    OACR_USE_PTR( blocks_p );
    DLOOP_Offset el_size; /* DLOOP_Count? */
    DLOOP_Offset size;
    struct MPID_m2m_params *paramp = (struct MPID_m2m_params *)v_paramp;

    el_size = DLOOP_Handle_get_size_macro(el_type);
    size = *blocks_p * el_size;

    if (paramp->direction == DLOOP_M2M_TO_USERBUF)
    {
        memcpy((char *) (paramp->userbuf + rel_off), paramp->streambuf, size);
    }
    else
    {
        memcpy(paramp->streambuf, (char *) (paramp->userbuf + rel_off), size);
    }
    paramp->streambuf += size;
    return 0;
}



//
// Disable compiler warnings about constant-valued conditional expressions
//
#pragma warning( push )
#pragma warning( disable : 4127 )

/* Segment_vector_m2m
 *
 * Note: this combines both packing and unpacking functionality.
 *
 * Note: this is only called when the starting position is at the beginning
 * of a whole block in a vector type.
 */
static int MPID_Segment_vector_m2m(DLOOP_Offset *blocks_p,
                                       DLOOP_Count /*count*/,
                                       DLOOP_Count blksz,
                                       DLOOP_Offset stride,
                                       DLOOP_Type el_type,
                                       DLOOP_Offset rel_off, /* offset into buffer */
                                       void* /*bufp*/,
                                       void *v_paramp)
{
    OACR_USE_PTR( blocks_p );
    DLOOP_Count i, blocks_left, whole_count;
    DLOOP_Offset el_size;
    struct MPID_m2m_params *paramp = (struct MPID_m2m_params *)v_paramp;
    char *cbufp;

    cbufp = paramp->userbuf + rel_off;
    el_size = DLOOP_Handle_get_size_macro(el_type);

    whole_count = (blksz > 0) ? (DLOOP_Count)(*blocks_p / blksz) : 0;
    blocks_left = (blksz > 0) ? (DLOOP_Count)(*blocks_p % blksz) : 0;

    if (paramp->direction == DLOOP_M2M_TO_USERBUF)
    {
        if (el_size == 8 &&
            MPIR_ALIGN8_TEST(paramp->streambuf,cbufp))
        {
            MPIDI_Copy_to_vec<INT64>(&paramp->streambuf, &cbufp, stride,
                                       blksz, whole_count);
            MPIDI_Copy_to_vec<INT64>(&paramp->streambuf, &cbufp, 0,
                                       blocks_left, 1);
        }
        else if (el_size == 4 &&
                 MPIR_ALIGN4_TEST(paramp->streambuf,cbufp))
        {
            MPIDI_Copy_to_vec<INT32>(&paramp->streambuf, &cbufp, stride,
                                       blksz, whole_count);
            MPIDI_Copy_to_vec<INT32>(&paramp->streambuf, &cbufp, 0,
                                       blocks_left, 1);
        }
        else if (el_size == 2)
        {
            MPIDI_Copy_to_vec<INT16>(&paramp->streambuf, &cbufp, stride,
                                       blksz, whole_count);
            MPIDI_Copy_to_vec<INT16>(&paramp->streambuf, &cbufp, 0,
                                       blocks_left, 1);
        }
        else
        {
            for (i=0; i < whole_count; i++)
            {
                memcpy(cbufp, paramp->streambuf, blksz * el_size);
                paramp->streambuf += blksz * el_size;
                cbufp += stride;
            }
            if (blocks_left)
            {
                memcpy(cbufp, paramp->streambuf, blocks_left * el_size);
                paramp->streambuf += blocks_left * el_size;
            }
        }
    }
    else /* M2M_FROM_USERBUF */
    {
        if (el_size == 8 &&
            MPIR_ALIGN8_TEST(cbufp,paramp->streambuf))
        {
            MPIDI_Copy_from_vec<INT64>(&cbufp, &paramp->streambuf, stride,
                                         blksz, whole_count);
            MPIDI_Copy_from_vec<INT64>(&cbufp, &paramp->streambuf, 0,
                                         blocks_left, 1);
        }
        else if (el_size == 4 &&
                 MPIR_ALIGN4_TEST(cbufp,paramp->streambuf))
        {
            MPIDI_Copy_from_vec<INT32>(&cbufp, &paramp->streambuf, stride,
                                         blksz, whole_count);
            MPIDI_Copy_from_vec<INT32>(&cbufp, &paramp->streambuf, 0,
                                         blocks_left, 1);
        }
        else if (el_size == 2)
        {
            MPIDI_Copy_from_vec<INT16>(&cbufp, &paramp->streambuf, stride,
                                         blksz, whole_count);
            MPIDI_Copy_from_vec<INT16>(&cbufp, &paramp->streambuf, 0,
                                         blocks_left, 1);
        }
        else
        {
            for (i=0; i < whole_count; i++)
            {
                memcpy(paramp->streambuf, cbufp, blksz * el_size);
                paramp->streambuf += blksz * el_size;
                cbufp += stride;
            }
            if (blocks_left)
            {
                memcpy(paramp->streambuf, cbufp, blocks_left * el_size);
                paramp->streambuf += blocks_left * el_size;
            }
        }
    }

    return 0;
}

/* MPID_Segment_blkidx_m2m
 */
static int
MPID_Segment_blkidx_m2m(
    _In_ DLOOP_Offset *blocks_p,
    DLOOP_Count /*count*/,
    DLOOP_Count blocklen,
    _In_ DLOOP_Offset *offsetarray,
    DLOOP_Type el_type,
    DLOOP_Offset rel_off,
    void* /*bufp*/,
    _In_ void *v_paramp
    )
{
    OACR_USE_PTR( blocks_p );
    OACR_USE_PTR( offsetarray );

    DLOOP_Count curblock = 0;
    DLOOP_Offset el_size;
    DLOOP_Count blocks_left = (DLOOP_Count)*blocks_p;
    char *cbufp;
    struct MPID_m2m_params *paramp = (struct MPID_m2m_params *)v_paramp;

    el_size = DLOOP_Handle_get_size_macro(el_type);

    while (blocks_left)
    {
        char *src, *dest;

        cbufp = paramp->userbuf + rel_off + offsetarray[curblock];

        if (blocklen > blocks_left) blocklen = blocks_left;

        if (paramp->direction == DLOOP_M2M_TO_USERBUF)
        {
            src  = paramp->streambuf;
            dest = cbufp;
        }
        else
        {
            src  = cbufp;
            dest = paramp->streambuf;
        }

        /* note: MPIDI_Copy_from_vec modifies src  and dest buffer pointers, so we must reset */
        if (el_size == 8 &&
            MPIR_ALIGN8_TEST(src, dest))
        {
            MPIDI_Copy_from_vec<INT64>(&src, &dest, 0, blocklen, 1);
        }
        else if (el_size == 4 &&
                 MPIR_ALIGN4_TEST(src,dest))
        {
            MPIDI_Copy_from_vec<INT32>(&src, &dest, 0, blocklen, 1);
        }
        else if (el_size == 2)
        {
            MPIDI_Copy_from_vec<INT16>(&src, &dest, 0, blocklen, 1);
        }
        else
        {
            memcpy(dest, src, blocklen * el_size);
        }

        paramp->streambuf += blocklen * el_size;
        blocks_left -= blocklen;
        curblock++;
    }

    return 0;
}

/* MPID_Segment_index_m2m
 */
static int
MPID_Segment_index_m2m(
    _In_ DLOOP_Offset *blocks_p,
    DLOOP_Count /*count*/,
    _In_ DLOOP_Count *blockarray,
    _In_ DLOOP_Offset *offsetarray,
    DLOOP_Type el_type,
    DLOOP_Offset rel_off,
    void* /*bufp*/,
    _In_ void *v_paramp
    )
{
    OACR_USE_PTR( blocks_p );
    OACR_USE_PTR( blockarray );
    OACR_USE_PTR( offsetarray );

    int curblock = 0;
    DLOOP_Offset el_size;
    DLOOP_Count cur_block_sz;
    DLOOP_Count blocks_left = (DLOOP_Count)*blocks_p;
    char *cbufp;
    struct MPID_m2m_params *paramp = (struct MPID_m2m_params *)v_paramp;

    el_size = DLOOP_Handle_get_size_macro(el_type);

    while (blocks_left)
    {
        char *src, *dest;

        cur_block_sz = blockarray[curblock];

        cbufp = paramp->userbuf + rel_off + offsetarray[curblock];

        if (cur_block_sz > blocks_left) cur_block_sz = blocks_left;

        if (paramp->direction == DLOOP_M2M_TO_USERBUF)
        {
            src  = paramp->streambuf;
            dest = cbufp;
        }
        else
        {
            src  = cbufp;
            dest = paramp->streambuf;
        }

        /* note: MPIDI_Copy_from_vec modifies src  and dest buffer pointers, so we must reset */
        if (el_size == 8 &&
            MPIR_ALIGN8_TEST(src, dest))
        {
            MPIDI_Copy_from_vec<INT64>(&src, &dest, 0, cur_block_sz, 1);
        }
        else if (el_size == 4 &&
                 MPIR_ALIGN4_TEST(src,dest))
        {
            MPIDI_Copy_from_vec<INT32>(&src, &dest, 0, cur_block_sz, 1);
        }
        else if (el_size == 2)
        {
            MPIDI_Copy_from_vec<INT16>(&src, &dest, 0, cur_block_sz, 1);
        }
        else
        {
            memcpy(dest, src, cur_block_sz * el_size);
        }

        paramp->streambuf += cur_block_sz * el_size;
        blocks_left -= cur_block_sz;
        curblock++;
    }

    return 0;
}


//
// Re-enable compiler warnings about constant-valued conditional expressions
//
#pragma warning( pop )


void MPID_Segment_pack(DLOOP_Segment *segp,
                                  DLOOP_Offset   first,
                                  DLOOP_Offset  *lastp,
                                  void *streambuf)
{
    struct MPID_m2m_params params;

    /* experimenting with discarding buf value in the segment, keeping in
     * per-use structure instead. would require moving the parameters around a
     * bit.
     */
    params.userbuf   = segp->ptr;
    params.streambuf = (char *)streambuf;
    params.direction = DLOOP_M2M_FROM_USERBUF;

    MPID_Segment_manipulate(segp, first, lastp,
                                       MPID_Segment_contig_m2m,
                                       MPID_Segment_vector_m2m,
                                       MPID_Segment_blkidx_m2m,
                                       MPID_Segment_index_m2m,
                                       NULL, /* size fn */
                                       &params);
    return;
}

void MPID_Segment_unpack(DLOOP_Segment *segp,
                                    DLOOP_Offset   first,
                                    DLOOP_Offset  *lastp,
                                    const void *streambuf)
{
    struct MPID_m2m_params params;

    /* experimenting with discarding buf value in the segment, keeping in
     * per-use structure instead. would require moving the parameters around a
     * bit.
     */
    params.userbuf   = segp->ptr;
    params.streambuf = (char *)streambuf;
    params.direction = DLOOP_M2M_TO_USERBUF;

    MPID_Segment_manipulate(segp, first, lastp,
                                       MPID_Segment_contig_m2m,
                                       MPID_Segment_vector_m2m,
                                       MPID_Segment_blkidx_m2m,
                                       MPID_Segment_index_m2m,
                                       NULL, /* size fn */
                                       &params);
    return;
}

struct MPID_contig_blocks_params
{
    DLOOP_Count  count;
    DLOOP_Offset last_loc;
};

/* MPID_Segment_contig_count_block
 *
 * Note: because bufp is just an offset, we can ignore it in our
 *       calculations of # of contig regions.
 */
static int DLOOP_Segment_contig_count_block(DLOOP_Offset *blocks_p,
                                            DLOOP_Type el_type,
                                            DLOOP_Offset rel_off,
                                            DLOOP_Buffer /*bufp*/,
                                            void *v_paramp)
{
    OACR_USE_PTR( blocks_p );
    DLOOP_Offset size, el_size;
    struct MPID_contig_blocks_params *paramp = (struct MPID_contig_blocks_params *)v_paramp;

    MPIU_Assert(*blocks_p > 0);

    el_size = DLOOP_Handle_get_size_macro(el_type);
    size = *blocks_p * el_size;

    if (paramp->count > 0 && rel_off == paramp->last_loc)
    {
        /* this region is adjacent to the last */
        paramp->last_loc += size;
    }
    else
    {
        /* new region */
        paramp->last_loc = rel_off + size;
        paramp->count++;
    }
    return 0;
}

/* DLOOP_Segment_vector_count_block
 *
 * Input Parameters:
 * blocks_p - [inout] pointer to a count of blocks (total, for all noncontiguous pieces)
 * count    - # of noncontiguous regions
 * blksz    - size of each noncontiguous region
 * stride   - distance in bytes from start of one region to start of next
 * el_type - elemental type (e.g. MPI_INT)
 * ...
 *
 * Note: this is only called when the starting position is at the beginning
 * of a whole block in a vector type.
 */
static int DLOOP_Segment_vector_count_block(DLOOP_Offset* /*blocks_p*/,
                                            DLOOP_Count count,
                                            DLOOP_Count blksz,
                                            DLOOP_Offset stride,
                                            DLOOP_Type el_type,
                                            DLOOP_Offset rel_off, /* offset into buffer */
                                            void* /*bufp*/,
                                            void *v_paramp)
{
    DLOOP_Count new_blk_count;
    DLOOP_Offset size, el_size;
    struct MPID_contig_blocks_params *paramp = (struct MPID_contig_blocks_params *)v_paramp;

    MPIU_Assert(count > 0 && blksz > 0);

    el_size = DLOOP_Handle_get_size_macro(el_type);
    size = el_size * blksz;
    new_blk_count = count;

    /* if size == stride, then blocks are adjacent to one another */
    if (size == stride) new_blk_count = 1;

    if (paramp->count > 0 && rel_off == paramp->last_loc)
    {
        /* first block sits at end of last block */
        new_blk_count--;
    }

    paramp->last_loc = rel_off + (count-1) * stride + size;
    paramp->count += new_blk_count;
    return 0;
}

/* DLOOP_Segment_blkidx_count_block
 *
 * Note: this is only called when the starting position is at the
 * beginning of a whole block in a blockindexed type.
 */
static int DLOOP_Segment_blkidx_count_block(DLOOP_Offset* /*blocks_p*/,
                                            DLOOP_Count count,
                                            DLOOP_Count blksz,
                                            _In_count_(count) DLOOP_Offset *offsetarray,
                                            DLOOP_Type el_type,
                                            DLOOP_Offset rel_off,
                                            void* /*bufp*/,
                                            void *v_paramp)
{
    OACR_USE_PTR( offsetarray );
    DLOOP_Count i, new_blk_count;
    DLOOP_Offset size, el_size, last_loc;
    struct MPID_contig_blocks_params *paramp = (struct MPID_contig_blocks_params *)v_paramp;

    MPIU_Assert(count > 0 && blksz > 0);

    el_size = DLOOP_Handle_get_size_macro(el_type);
    size = el_size * blksz;
    new_blk_count = count;

    if (paramp->count > 0 && ((rel_off + offsetarray[0]) == paramp->last_loc))
    {
        /* first block sits at end of last block */
        new_blk_count--;
    }

    last_loc = rel_off + offsetarray[0] + size;
    for (i=1; i < count; i++)
    {
        if (last_loc == rel_off + offsetarray[i]) new_blk_count--;

        last_loc = rel_off + offsetarray[i] + size;
    }

    paramp->last_loc = last_loc;
    paramp->count += new_blk_count;
    return 0;
}

/* DLOOP_Segment_index_count_block
 *
 * Note: this is only called when the starting position is at the
 * beginning of a whole block in an indexed type.
 */
static int DLOOP_Segment_index_count_block(DLOOP_Offset* /*blocks_p*/,
                                           DLOOP_Count count,
                                           _In_count_(count) DLOOP_Count *blockarray,
                                           _In_count_(count) DLOOP_Offset *offsetarray,
                                           DLOOP_Type el_type,
                                           DLOOP_Offset rel_off,
                                           void* /*bufp*/,
                                           void *v_paramp)
{
    OACR_USE_PTR( blockarray );
    OACR_USE_PTR( offsetarray );
    DLOOP_Count new_blk_count;
    DLOOP_Offset el_size, last_loc;
    struct MPID_contig_blocks_params *paramp = (struct MPID_contig_blocks_params *)v_paramp;

    MPIU_Assert(count > 0);

    el_size = DLOOP_Handle_get_size_macro(el_type);
    new_blk_count = count;

    if (paramp->count > 0 && ((rel_off + offsetarray[0]) == paramp->last_loc))
    {
        /* first block sits at end of last block */
        new_blk_count--;
    }

    /* Note: when we build an indexed type we combine adjacent regions,
     *       so we're not going to go through and check every piece
     *       separately here. if someone else were building indexed
     *       dataloops by hand, then the loop here might be necessary.
     *       DLOOP_Count i and DLOOP_Offset size would need to be
     *       declared above.
     */
    last_loc = rel_off + offsetarray[count-1] + blockarray[count-1] * el_size;

    paramp->last_loc = last_loc;
    paramp->count += new_blk_count;
    return 0;
}

/* DLOOP_Segment_count_contig_blocks()
 *
 * Count number of contiguous regions in segment between first and last.
 */
void MPID_Segment_count_contig_blocks(DLOOP_Segment *segp,
                                                 DLOOP_Offset first,
                                                 DLOOP_Offset *lastp,
                                                 DLOOP_Count *countp)
{
    struct MPID_contig_blocks_params params;

    params.count    = 0;
    params.last_loc = 0;

    MPID_Segment_manipulate(segp,
                                       first,
                                       lastp,
                                       DLOOP_Segment_contig_count_block,
                                       DLOOP_Segment_vector_count_block,
                                       DLOOP_Segment_blkidx_count_block,
                                       DLOOP_Segment_index_count_block,
                                       NULL, /* size fn */
                                       (void *) &params);

    *countp = params.count;
    return;
}

/********** FUNCTIONS FOR FLATTENING INTO MPI OFFSETS AND BLKLENS  **********/

/* Segment_mpi_flatten
 *
 * Flattens into a set of blocklengths and displacements, as in an
 * MPI hindexed type. Note that we use appropriately-sized variables
 * in the associated params structure for this reason.
 *
 * NOTE: blocks will be in units of bytes when returned.
 *
 * WARNING: there's potential for overflow here as we convert from
 *          various types into an index of bytes.
 */
struct MPID_mpi_flatten_params
{
    int       index, length;
    MPI_Aint  last_end;
    int      *blklens;
    MPI_Aint *disps;
};

/* DLOOP_Segment_contig_mpi_flatten
 *
 */
static int DLOOP_Segment_contig_mpi_flatten(DLOOP_Offset *blocks_p,
                                            DLOOP_Type el_type,
                                            DLOOP_Offset rel_off,
                                            void *bufp,
                                            void *v_paramp)
{
    int last_idx;
    DLOOP_Offset size, el_size;
    const char *last_end = NULL;
    struct MPID_mpi_flatten_params *paramp = (struct MPID_mpi_flatten_params *)v_paramp;

    el_size = DLOOP_Handle_get_size_macro(el_type);
    size = *blocks_p * el_size;

    last_idx = paramp->index - 1;
    if (last_idx >= 0)
    {
        last_end = ((char *) paramp->disps[last_idx]) +
            paramp->blklens[last_idx];
    }

    if ((last_idx == paramp->length-1) &&
        (last_end != ((char *) bufp + rel_off)))
    {
        /* we have used up all our entries, and this region doesn't fit on
         * the end of the last one.  setting blocks to 0 tells manipulation
         * function that we are done (and that we didn't process any blocks).
         */
        *blocks_p = 0;
        return 1;
    }
    else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
    {
        /* add this size to the last vector rather than using up another one */
        paramp->blklens[last_idx] += (int)size;
    }
    else
    {
        paramp->disps[last_idx+1]   = (MPI_Aint) ((char *) bufp + rel_off);
        paramp->blklens[last_idx+1] = (int)size;
        paramp->index++;
    }
    return 0;
}

/* DLOOP_Segment_vector_mpi_flatten
 *
 * Input Parameters:
 * blocks_p - [inout] pointer to a count of blocks (total, for all noncontiguous pieces)
 * count    - # of noncontiguous regions
 * blksz    - size of each noncontiguous region
 * stride   - distance in bytes from start of one region to start of next
 * el_type - elemental type (e.g. MPI_INT)
 * ...
 *
 * Note: this is only called when the starting position is at the beginning
 * of a whole block in a vector type.
 *
 * TODO: MAKE THIS CODE SMARTER, USING THE SAME GENERAL APPROACH AS IN THE
 *       COUNT BLOCK CODE ABOVE.
 */
static int DLOOP_Segment_vector_mpi_flatten(DLOOP_Offset *blocks_p,
                                            DLOOP_Count count,
                                            DLOOP_Count blksz,
                                            DLOOP_Offset stride,
                                            DLOOP_Type el_type,
                                            DLOOP_Offset rel_off, /* offset into buffer */
                                            void *bufp, /* start of buffer */
                                            void *v_paramp)
{
    DLOOP_Count i, blocks_left;
    DLOOP_Offset size, el_size;
    struct MPID_mpi_flatten_params *paramp = (struct MPID_mpi_flatten_params *)v_paramp;

    el_size = DLOOP_Handle_get_size_macro(el_type);
    blocks_left = (DLOOP_Count)*blocks_p;

    for (i=0; i < count && blocks_left > 0; i++)
    {
        int last_idx;
        const char *last_end = NULL;

        if (blocks_left > blksz)
        {
            size = blksz * el_size;
            blocks_left -= blksz;
        }
        else
        {
            /* last pass */
            size = blocks_left * el_size;
            blocks_left = 0;
        }

        last_idx = paramp->index - 1;
        if (last_idx >= 0)
        {
            last_end = ((char *) paramp->disps[last_idx]) +
                paramp->blklens[last_idx];
        }

        if ((last_idx == paramp->length-1) &&
            (last_end != ((char *) bufp + rel_off)))
        {
            /* we have used up all our entries, and this one doesn't fit on
             * the end of the last one.
             */
            *blocks_p -= (blocks_left + (size / el_size));
            return 1;
        }
        else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
        {
            /* add this size to the last vector rather than using up new one */
            paramp->blklens[last_idx] += (int)size;
        }
        else
        {
            paramp->disps[last_idx+1]   = (MPI_Aint) ((char *) bufp + rel_off);
            paramp->blklens[last_idx+1] = (int)size;
            paramp->index++;
        }

        rel_off += stride;
    }

    /* if we get here then we processed ALL the blocks; don't need to update
     * blocks_p
     */

    MPIU_Assert(blocks_left == 0);
    return 0;
}

/* MPID_Segment_mpi_flatten - flatten a type into a representation
 *                            appropriate for passing to hindexed create.
 *
 * Parameters:
 * segp    - pointer to segment structure
 * first   - first byte in segment to pack
 * lastp   - in/out parameter describing last byte to pack (and afterwards
 *           the last byte _actually_ packed)
 *           NOTE: actually returns index of byte _after_ last one packed
 * blklens, disps - the usual blocklength and displacement arrays for MPI
 * lengthp - in/out parameter describing length of array (and afterwards
 *           the amount of the array that has actual data)
 */
void MPID_Segment_mpi_flatten(DLOOP_Segment *segp,
                                         DLOOP_Offset first,
                                         DLOOP_Offset *lastp,
                                         int *blklens,
                                         MPI_Aint *disps,
                                         int *lengthp)
{
    struct MPID_mpi_flatten_params params;

    MPIU_Assert(*lengthp > 0);

    params.index   = 0;
    params.length  = *lengthp;
    params.blklens = blklens;
    params.disps   = disps;

    MPID_Segment_manipulate(segp,
                                       first,
                                       lastp,
                                       DLOOP_Segment_contig_mpi_flatten,
                                       DLOOP_Segment_vector_mpi_flatten,
                                       NULL, /* DLOOP_Segment_blkidx_mpi_flatten, */
                                       NULL, /* DLOOP_Segment_index_mpi_flatten, */
                                       NULL,
                                       &params);

    /* last value already handled by MPID_Segment_manipulate */
    *lengthp = params.index;
    return;
}


static inline int is_float_type(DLOOP_Type el_type)
{
    return ((el_type == MPI_FLOAT) || (el_type == MPI_DOUBLE) ||
            (el_type == MPI_LONG_DOUBLE) ||
            (el_type == MPI_DOUBLE_PRECISION) ||
            (el_type == MPI_COMPLEX) || (el_type == MPI_DOUBLE_COMPLEX)) ||
            (el_type == MPI_REAL4) || (el_type == MPI_REAL8) ||
            (el_type == MPI_C_COMPLEX) || (el_type == MPI_C_FLOAT_COMPLEX) ||
            (el_type == MPI_C_DOUBLE_COMPLEX) || (el_type == MPI_C_LONG_DOUBLE_COMPLEX) ||
            (el_type == MPI_REAL);
/*             (el_type == MPI_REAL16)); */
}


//
// Converts an integer from one data representation to another.
//
// Care needs to be taken when converting to a smaller size
// such that the sign is preserved, and magnitude properly
// represented.
//
static MPI_RESULT ext32_integer_convert_resize(
    BYTE* /*pDest*/,
    BYTE* /*pSrc*/,
    int /*destElementSize*/,
    int /*srcElementSize*/,
    size_t /*count*/
    )
{
    /* TODO */
    return MPI_ERR_CONVERSION;
}


//
// Converts a floating-point value from one data representation to another.
//
// Care needs to be taken when converting to a smaller size
// such that the sign is preserved, and magnitude properly
// represented.
//
static MPI_RESULT ext32_float_convert_resize(
    BYTE* /*pDest*/,
    BYTE* /*pSrc*/,
    int /*destElementSize*/,
    int /*srcElementSize*/,
    size_t /*count*/
    )
{
    /* TODO */
    return MPI_ERR_CONVERSION;
}


USHORT Swap( USHORT src )
{
    return _byteswap_ushort( src );
}


ULONG Swap( ULONG src )
{
    return _byteswap_ulong( src );
}


UINT64 Swap( UINT64 src )
{
    return _byteswap_uint64( src );
}


typedef struct _UINT128
{
    UINT64 lowPart;
    UINT64 highPart;
} UINT128;


UINT128 Swap( UINT128 src )
{
    UINT128 temp;
    temp.highPart = _byteswap_uint64( src.lowPart );
    temp.lowPart = _byteswap_uint64( src.highPart );
    return temp;
}


template<typename T>
void SwapLoop( 
    _Out_writes_(count) T*     dest,
    _In_reads_(count)   T*     src,
    _In_                size_t count
    )
{
    for( size_t i = 0; i < count; i++ )
    {
        dest[i] = Swap( src[i] );
    }
}


static MPI_RESULT
MPID_Segment_contig_convert_ext32(
    _Out_bytecap_(destElementSize * count) BYTE* pDest,
    _In_bytecount_(srcElementSize * count) BYTE* pSrc,
    int destElementSize,
    int srcElementSize,
    MPI_Datatype type,
    size_t count
    )
{
    MPIU_Assert( pDest != nullptr );
    MPIU_Assert( pSrc != nullptr);
    MPIU_Assert( srcElementSize != 0 );
    MPIU_Assert( destElementSize != 0 );

    /* TODO: DEAL WITH CASE WHERE ALL DATA DOESN'T FIT! */
    if ((srcElementSize == destElementSize))
    {
        //
        // If the source and destination elements are the same size, we just need
        // to byteswap all the values, and we don't need to care about whether
        // the data is integers or floating point values.
        //
        switch( srcElementSize )
        {
        case 1:
            memcpy(
                pDest,
                pSrc,
                count
                );
            break;

        case 2:
            SwapLoop<USHORT>(
                reinterpret_cast<USHORT*>(pDest),
                reinterpret_cast<USHORT*>(pSrc),
                count
                );
            break;

        case 4:
            SwapLoop<ULONG>(
                reinterpret_cast<ULONG*>(pDest),
                reinterpret_cast<ULONG*>(pSrc),
                count
                );
            break;

        case 8:
            SwapLoop<UINT64>(
                reinterpret_cast<UINT64*>(pDest),
                reinterpret_cast<UINT64*>(pSrc),
                count
                );
            break;

        case 16:
            //
            // MPI_DOUBLE_COMPLEX and similar types are 16 bytes.
            //
            SwapLoop<UINT128>(
                reinterpret_cast<UINT128*>(pDest),
                reinterpret_cast<UINT128*>(pSrc),
                count
                );
            break;

        default:
            return MPI_ERR_CONVERSION;
        }
        return 0;
    }

    if (is_float_type(type))
    {
        //
        // All other float types have the same size, so should have been handled above.
        //
        MPIU_Assert( type == MPI_LONG_DOUBLE || type == MPI_C_LONG_DOUBLE_COMPLEX );

        return ext32_float_convert_resize(
            pDest,
            pSrc,
            destElementSize,
            srcElementSize,
            count
            );
    }
    else
    {
        //
        // None of the integer values supported have a different size.
        //
        MPIU_Assert( type == MPI_DATATYPE_NULL );
        return ext32_integer_convert_resize(
            pDest,
            pSrc,
            destElementSize,
            srcElementSize,
            count
            );
    }
}

//
// MPID_Segment_contig_pack_ext32 is passed in as an argument in MPID_Segment_manipulate.
// Even though this function can treat blocks_p as const, other functions passed into in
// MPID_Segment_manipulate cannot. Hence keeping blocks_p as non const. (same comment for unpack)
//
OACR_WARNING_DISABLE(NONCONST_BUFFER_PARAM, "param blocks_p is not const to match the function signature");

static MPI_RESULT
MPID_Segment_contig_pack_ext32(
    DLOOP_Offset *blocks_p,
    DLOOP_Type el_type,
    DLOOP_Offset rel_off,
    void *bufp,
    void *v_paramp
    )
{
    struct MPID_Segment_piece_params *paramp =
        static_cast<struct MPID_Segment_piece_params*>(v_paramp);

    int src_el_size = MPID_Datatype_get_basic_size(el_type);
    int dest_el_size = static_cast<int>(MPIDI_Datatype_get_basic_size_external32(el_type));
    MPIU_Assert(dest_el_size != 0);

    BYTE* pSrc = reinterpret_cast<BYTE*>(bufp) + rel_off;
    BYTE* pDest = reinterpret_cast<BYTE*>(paramp->u.pack.pack_buffer);

    size_t count = *blocks_p;

    MPI_RESULT mpi_errno = MPID_Segment_contig_convert_ext32(
        pDest,
        pSrc,
        dest_el_size,
        src_el_size,
        el_type,
        count
        );

    if( mpi_errno == MPI_SUCCESS )
    {
        paramp->u.pack.pack_buffer += (dest_el_size * count);
    }

    return mpi_errno;
}

static int
MPID_Segment_contig_unpack_ext32(
    DLOOP_Offset *blocks_p,
    DLOOP_Type el_type,
    DLOOP_Offset rel_off,
    _Inout_ void *bufp,
    _Inout_ void *v_paramp
    )
{
    struct MPID_Segment_piece_params *paramp =
        static_cast<struct MPID_Segment_piece_params*>(v_paramp);

    int src_el_size = static_cast<int>(MPIDI_Datatype_get_basic_size_external32(el_type));
    int dest_el_size = MPID_Datatype_get_basic_size(el_type);

    MPIU_Assert(dest_el_size != 0);

    BYTE* pSrc = reinterpret_cast<BYTE*>(paramp->u.unpack.unpack_buffer);
    BYTE* pDest = reinterpret_cast<BYTE*>(bufp) + rel_off;

    size_t count = *blocks_p;

    int mpi_errno = MPID_Segment_contig_convert_ext32(
        pDest,
        pSrc,
        dest_el_size,
        src_el_size,
        el_type,
        count
        );

    if( mpi_errno == MPI_SUCCESS )
    {
        paramp->u.unpack.unpack_buffer += (dest_el_size * count);
    }

    return mpi_errno;
}
OACR_WARNING_ENABLE(NONCONST_BUFFER_PARAM, "param blocks_p is not const to match the function signature");

void
MPID_Segment_pack_external32(
    DLOOP_Segment *segp,
    DLOOP_Offset first,
    DLOOP_Offset *lastp,
    void *pack_buffer
    )
{
    struct MPID_Segment_piece_params pack_params;

    pack_params.u.pack.pack_buffer = (char*)pack_buffer;
    MPID_Segment_manipulate(segp,
                            first,
                            lastp,
                            MPID_Segment_contig_pack_ext32,
                            NULL, /* MPID_Segment_vector_pack_external32_to_buf, */
                            NULL, /* blkidx */
                            NULL, /* MPID_Segment_index_pack_external32_to_buf, */
                            MPIDI_Datatype_get_basic_size_external32,
                            &pack_params);

    return;
}

void
MPID_Segment_unpack_external32(
    DLOOP_Segment *segp,
    DLOOP_Offset first,
    DLOOP_Offset *lastp,
    DLOOP_Buffer unpack_buffer)
{
    struct MPID_Segment_piece_params pack_params;

    pack_params.u.unpack.unpack_buffer = (char*)unpack_buffer;
    MPID_Segment_manipulate(segp,
                            first,
                            lastp,
                            MPID_Segment_contig_unpack_ext32,
                            NULL, /* MPID_Segment_vector_unpack_external32_to_buf, */
                            NULL, /* blkidx */
                            NULL, /* MPID_Segment_index_unpack_external32_to_buf, */
                            MPIDI_Datatype_get_basic_size_external32,
                            &pack_params);

    return;
}
