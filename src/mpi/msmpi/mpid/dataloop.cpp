// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "dataloopp.h"

/* Dataloops
 *
 * The functions here are used for the creation, copying, update, and display
 * of DLOOP_Dataloop structures and trees of these structures.
 *
 * Currently we store trees of dataloops in contiguous regions of memory.  They
 * are stored in such a way that subtrees are also stored contiguously.  This
 * makes it somewhat easier to copy these subtrees around.  Keep this in mind
 * when looking at the functions below.
 *
 * The structures used in this file are defined in mpid_datatype.h.  There is
 * no separate mpid_dataloop.h at this time.
 *
 * OPTIMIZATIONS:
 *
 * There are spots in the code with OPT tags that indicate where we could
 * optimize particular calculations or avoid certain checks.
 *
 * NOTES:
 *
 * Don't have locks in place at this time!
 */

/* Some functions in this file are responsible for allocation of space for
 * dataloops.  These structures include the dataloop structure itself
 * followed by a sequence of variable-sized arrays, depending on the loop
 * kind.  For example, a dataloop of kind DLOOP_KIND_INDEXED has a
 * dataloop structure followed by an array of block sizes and then an array
 * of offsets.
 *
 * For efficiency and ease of cleanup (preserving a single free at
 * deallocation), we want to allocate this memory as a single large chunk.
 * However, we must perform some alignment of the components of this chunk
 * in order to obtain correct and efficient operation across all platforms.
 */


/*@
  Dataloop_free - deallocate the resources used to store a dataloop

  Input Parameters:
. dataloop - pointer to dataloop structure
@*/
_Success_(*dataloop == nullptr)
_At_(*dataloop,_Pre_maybenull_)
void
MPID_Dataloop_free(
    _Inout_ _Outptr_result_maybenull_ DLOOP_Dataloop** dataloop
    )
{
    if (*dataloop == NULL) return;

    memset(*dataloop, 0, sizeof(DLOOP_Dataloop_common));
    MPIU_Free(*dataloop);
    *dataloop = NULL;
    return;
}
/*@
  Dataloop_copy - Copy an arbitrary dataloop structure, updating
  pointers as necessary

  Input Parameters:
+ dest   - pointer to destination region
. src    - pointer to original dataloop structure
- size   - size of dataloop structure

  This routine parses the dataloop structure as it goes in order to
  determine what exactly it needs to update.

  Notes:
  It assumes that the source dataloop was allocated in our usual way;
  this means that the entire dataloop is in a contiguous region and that
  the root of the tree is first in the array.

  This has some implications:
+ we can use a contiguous copy mechanism to copy the majority of the
  structure
- all pointers in the region are relative to the start of the data region
  the first dataloop in the array is the root of the tree
@*/
void
MPID_Dataloop_copy(
    _Out_writes_bytes_all_(size) DLOOP_Dataloop*       dest,
    _In_reads_bytes_(size)       const DLOOP_Dataloop* src,
    _In_                         int                   size
    )
{
    DLOOP_Offset ptrdiff;

    /* copy region first */
    memcpy(dest, src, size);

    /* Calculate difference in starting locations. DLOOP_Dataloop_update()
     * then traverses the new structure and updates internal pointers by
     * adding this difference to them. This way we can just copy the
     * structure, including pointers, in one big block.
     */
    ptrdiff = (char *) dest - (char *) src;

    /* traverse structure updating pointers */
    MPID_Dataloop_update(dest, ptrdiff);

    return;
}


/*@
  Dataloop_update - update pointers after a copy operation

  Input Parameters:
+ dataloop - pointer to loop to update
- ptrdiff - value indicating offset between old and new pointer values

  This function is used to recursively update all the pointers in a
  dataloop tree.
@*/
void
MPID_Dataloop_update(
    _Inout_ DLOOP_Dataloop* dataloop,
    _In_    DLOOP_Offset    ptrdiff
    )
{
    /* OPT: only declare these variables down in the Struct case */
    int i;
    DLOOP_Dataloop **looparray;

    switch(dataloop->kind & DLOOP_KIND_MASK)
    {
        case DLOOP_KIND_CONTIG:
        case DLOOP_KIND_VECTOR:
            /*
             * All these really ugly assignments are really of the form:
             *
             * ((char *) dataloop->loop_params.c_t.loop) += ptrdiff;
             *
             * However, some compilers spit out warnings about casting on the
             * LHS, so we get this much nastier form instead (using common
             * struct for contig and vector):
             */
            dataloop->loop_params.cm_t.dataloop = (DLOOP_Dataloop *)
                ((char *) dataloop->loop_params.cm_t.dataloop + ptrdiff);

            if (!(dataloop->kind & DLOOP_FINAL_MASK))
                MPID_Dataloop_update(dataloop->loop_params.cm_t.dataloop, ptrdiff);
            break;

        case DLOOP_KIND_BLOCKINDEXED:
            dataloop->loop_params.bi_t.offset_array = (DLOOP_Offset *)
                ((char *) dataloop->loop_params.bi_t.offset_array + ptrdiff);
            dataloop->loop_params.bi_t.dataloop = (DLOOP_Dataloop *)
                ((char *) dataloop->loop_params.bi_t.dataloop + ptrdiff);

            if (!(dataloop->kind & DLOOP_FINAL_MASK))
                MPID_Dataloop_update(dataloop->loop_params.bi_t.dataloop, ptrdiff);
            break;

        case DLOOP_KIND_INDEXED:
            dataloop->loop_params.i_t.blocksize_array = (DLOOP_Count *)
                ((char *) dataloop->loop_params.i_t.blocksize_array + ptrdiff);
            dataloop->loop_params.i_t.offset_array = (DLOOP_Offset *)
                ((char *) dataloop->loop_params.i_t.offset_array + ptrdiff);
            dataloop->loop_params.i_t.dataloop = (DLOOP_Dataloop *)
                ((char *) dataloop->loop_params.i_t.dataloop + ptrdiff);

            if (!(dataloop->kind & DLOOP_FINAL_MASK))
                MPID_Dataloop_update(dataloop->loop_params.i_t.dataloop, ptrdiff);
            break;

        case DLOOP_KIND_STRUCT:
            dataloop->loop_params.s_t.blocksize_array = (DLOOP_Count *)
                ((char *) dataloop->loop_params.s_t.blocksize_array + ptrdiff);
            dataloop->loop_params.s_t.offset_array = (DLOOP_Offset *)
                ((char *) dataloop->loop_params.s_t.offset_array + ptrdiff);
            dataloop->loop_params.s_t.dataloop_array = (DLOOP_Dataloop **)
                ((char *) dataloop->loop_params.s_t.dataloop_array + ptrdiff);

            /* fix the N dataloop pointers too */
            looparray = dataloop->loop_params.s_t.dataloop_array;
            for (i=0; i < dataloop->loop_params.s_t.count; i++)
            {
                looparray[i] = (DLOOP_Dataloop *)
                    ((char *) looparray[i] + ptrdiff);
            }

            if (dataloop->kind & DLOOP_FINAL_MASK) break;

            for (i=0; i < dataloop->loop_params.s_t.count; i++)
            {
                MPID_Dataloop_update(looparray[i], ptrdiff);
            }
            break;
        default:
            MPIU_Assert(0);
            break;
    }
    return;
}

/*@
  Dataloop_alloc - allocate the resources used to store a dataloop with
                   no old loops associated with it.

  Input Parameters:
+ kind          - kind of dataloop to allocate
. count         - number of elements in dataloop (kind dependent)
. new_loop_p    - address at which to store new dataloop pointer
- new_loop_sz_p - pointer to integer in which to store new loop size

  Notes:
  The count parameter passed into this function will often be different
  from the count passed in at the MPI layer due to optimizations.
@*/
MPI_RESULT
MPID_Dataloop_alloc(
    _In_                                       int              kind,
    _In_                                       DLOOP_Count      count,
    _Outptr_result_bytebuffer_(*new_loop_sz_p) DLOOP_Dataloop** new_loop_p,
    _Out_                                      int*             new_loop_sz_p
    )
{
    return MPID_Dataloop_alloc_and_copy(kind,
                                        count,
                                        NULL,
                                        0,
                                        new_loop_p,
                                        new_loop_sz_p);
}

/*@
  Dataloop_alloc_and_copy - allocate the resources used to store a
                            dataloop and copy in old dataloop as
                            appropriate

  Input Parameters:
+ kind          - kind of dataloop to allocate
. count         - number of elements in dataloop (kind dependent)
. old_loop      - pointer to old dataloop (or NULL for none)
. old_loop_sz   - size of old dataloop (should be zero if old_loop is NULL)
. new_loop_p    - address at which to store new dataloop pointer
- new_loop_sz_p - pointer to integer in which to store new loop size

  Notes:
  The count parameter passed into this function will often be different
  from the count passed in at the MPI layer.
@*/
MPI_RESULT
MPID_Dataloop_alloc_and_copy(
    _In_                                       int                   kind,
    _In_                                       DLOOP_Count           count,
    _In_reads_bytes_opt_(old_loop_sz)          const DLOOP_Dataloop* old_loop,
    _In_                                       int                   old_loop_sz,
    _Outptr_result_bytebuffer_(*new_loop_sz_p) DLOOP_Dataloop**      new_loop_p,
    _Out_                                      int*                  new_loop_sz_p
    )
{
    int new_loop_sz = 0;
    int align_sz = HAVE_MAX_STRUCT_ALIGNMENT;
    int epsilon;
    int loop_sz = sizeof(DLOOP_Dataloop);
    int off_sz = 0, blk_sz = 0, ptr_sz = 0, extent_sz = 0;

    char *pos;
    DLOOP_Dataloop *new_loop;

    if (old_loop != NULL)
    {
        MPIU_Assert((old_loop_sz % align_sz) == 0);
    }

    /* calculate the space that we actually need for everything */
    switch (kind)
    {
        case DLOOP_KIND_STRUCT:
            /* need space for dataloop pointers and extents */
            ptr_sz = count * sizeof(DLOOP_Dataloop *);
            extent_sz = count * sizeof(DLOOP_Offset);
            __fallthrough;
        case DLOOP_KIND_INDEXED:
            /* need space for block sizes */
            blk_sz = count * sizeof(DLOOP_Count);
            __fallthrough;
        case DLOOP_KIND_BLOCKINDEXED:
            /* need space for block offsets */
            off_sz = count * sizeof(DLOOP_Offset);
            __fallthrough;
        case DLOOP_KIND_CONTIG:
        case DLOOP_KIND_VECTOR:
            break;
        default:
            MPIU_Assert(0);
    }

    /* pad everything that we're going to allocate */
    epsilon = loop_sz % align_sz;
    if (epsilon) loop_sz += align_sz - epsilon;

    epsilon = off_sz % align_sz;
    if (epsilon) off_sz += align_sz - epsilon;

    epsilon = blk_sz % align_sz;
    if (epsilon) blk_sz += align_sz - epsilon;

    epsilon = ptr_sz % align_sz;
    if (epsilon) ptr_sz += align_sz - epsilon;

    epsilon = extent_sz % align_sz;
    if (epsilon) extent_sz += align_sz - epsilon;

    new_loop_sz += loop_sz + off_sz + blk_sz + ptr_sz +
        extent_sz + old_loop_sz;

    /* allocate space */
    new_loop = (DLOOP_Dataloop *) MPIU_Malloc(new_loop_sz);
    if (new_loop == NULL)
    {
        return MPIU_ERR_NOMEM();
    }

    /* set all the pointers in the new dataloop structure */
    switch (kind)
    {
        case DLOOP_KIND_STRUCT:
            /* order is:
             * - pointers
             * - blocks
             * - offsets
             * - extents
             */
            new_loop->loop_params.s_t.dataloop_array =
                (DLOOP_Dataloop **) (((char *) new_loop) + loop_sz);
            new_loop->loop_params.s_t.blocksize_array =
                (DLOOP_Count *) (((char *) new_loop) + loop_sz + ptr_sz);
            new_loop->loop_params.s_t.offset_array =
                (DLOOP_Offset *) (((char *) new_loop) + loop_sz +
                                  ptr_sz + blk_sz);
            new_loop->loop_params.s_t.el_extent_array =
                (DLOOP_Offset *) (((char *) new_loop) + loop_sz +
                                  ptr_sz + blk_sz + off_sz);
            break;
        case DLOOP_KIND_INDEXED:
            /* order is:
             * - blocks
             * - offsets
             */
            new_loop->loop_params.i_t.blocksize_array =
                (DLOOP_Count *) (((char *) new_loop) + loop_sz);
            new_loop->loop_params.i_t.offset_array =
                (DLOOP_Offset *) (((char *) new_loop) + loop_sz + blk_sz);
            if (old_loop == NULL)
            {
                new_loop->loop_params.i_t.dataloop = NULL;
            }
            else
            {
                new_loop->loop_params.i_t.dataloop =
                    (DLOOP_Dataloop *) (((char *) new_loop) +
                                        (new_loop_sz - old_loop_sz));
            }
            break;
        case DLOOP_KIND_BLOCKINDEXED:
            new_loop->loop_params.bi_t.offset_array =
                (DLOOP_Offset *) (((char *) new_loop) + loop_sz);
            if (old_loop == NULL)
            {
                new_loop->loop_params.bi_t.dataloop = NULL;
            }
            else
            {
                new_loop->loop_params.bi_t.dataloop =
                    (DLOOP_Dataloop *) (((char *) new_loop) +
                                        (new_loop_sz - old_loop_sz));
            }
            break;
        case DLOOP_KIND_CONTIG:
            if (old_loop == NULL)
            {
                new_loop->loop_params.c_t.dataloop = NULL;
            }
            else
            {
                new_loop->loop_params.c_t.dataloop =
                    (DLOOP_Dataloop *) (((char *) new_loop) +
                                        (new_loop_sz - old_loop_sz));
            }
            break;
        case DLOOP_KIND_VECTOR:
            if (old_loop == NULL)
            {
                new_loop->loop_params.v_t.dataloop = NULL;
            }
            else
            {
                new_loop->loop_params.v_t.dataloop =
                    (DLOOP_Dataloop *) (((char *) new_loop) +
                                        (new_loop_sz - old_loop_sz));
            }
            break;
        default:
            MPIU_Assert(0);
    }

    pos = ((char *) new_loop) + (new_loop_sz - old_loop_sz);
    if (old_loop != NULL)
    {
        MPID_Dataloop_copy( (DLOOP_Dataloop*)pos, old_loop, old_loop_sz);
    }

    *new_loop_p    = new_loop;
    *new_loop_sz_p = new_loop_sz;
    return MPI_SUCCESS;
}

/*@
  Dataloop_struct_alloc - allocate the resources used to store a dataloop and
                          copy in old dataloop as appropriate.  this version
                          is specifically for use when a struct dataloop is
                          being created; the space to hold old dataloops in
                          this case must be described back to the
                          implementation in order for efficient copying.

  Input Parameters:
+ count         - number of elements in dataloop (kind dependent)
. old_loop_sz   - size of old dataloop (should be zero if old_loop is NULL)
. basic_ct      - number of basic types for which new dataloops are needed
. old_loop_p    - address at which to store pointer to old loops
. new_loop_p    - address at which to store new struct dataloop pointer
- new_loop_sz_p - address at which to store new loop size

  Notes:
  The count parameter passed into this function will often be different
  from the count passed in at the MPI layer due to optimizations.

  The caller is responsible for filling in the region pointed to by
  old_loop_p (count elements).
@*/
MPI_RESULT
MPID_Dataloop_struct_alloc(
    _In_                                       DLOOP_Count count,
    _In_                                       int old_loop_sz,
    _In_                                       int basic_ct,
    _Outptr_                                   DLOOP_Dataloop** old_loop_p,
    _Outptr_result_bytebuffer_(*new_loop_sz_p) DLOOP_Dataloop** new_loop_p,
    _Out_                                      int* new_loop_sz_p
    )
{
    int new_loop_sz = 0;
    int align_sz = HAVE_MAX_STRUCT_ALIGNMENT;
    int epsilon;
    int loop_sz = sizeof(DLOOP_Dataloop);
    int off_sz, blk_sz, ptr_sz, extent_sz, basic_sz;

    DLOOP_Dataloop *new_loop;

    /* calculate the space that we actually need for everything */
    ptr_sz    = count * sizeof(DLOOP_Dataloop *);
    extent_sz = count * sizeof(DLOOP_Offset);
    blk_sz    = count * sizeof(DLOOP_Count);
    off_sz    = count * sizeof(DLOOP_Offset);
    basic_sz  = sizeof(DLOOP_Dataloop);

    /* pad everything that we're going to allocate */
    epsilon = loop_sz % align_sz;
    if (epsilon) loop_sz += align_sz - epsilon;

    epsilon = off_sz % align_sz;
    if (epsilon) off_sz += align_sz - epsilon;

    epsilon = blk_sz % align_sz;
    if (epsilon) blk_sz += align_sz - epsilon;

    epsilon = ptr_sz % align_sz;
    if (epsilon) ptr_sz += align_sz - epsilon;

    epsilon = extent_sz % align_sz;
    if (epsilon) extent_sz += align_sz - epsilon;

    epsilon = basic_sz % align_sz;
    if (epsilon) basic_sz += align_sz - epsilon;

    /* note: we pad *each* basic type dataloop, because the
     * code used to create them assumes that we're going to
     * do that.
     */

    new_loop_sz += loop_sz + off_sz + blk_sz + ptr_sz +
        extent_sz + (basic_ct * basic_sz) + old_loop_sz;

    /* allocate space */
    new_loop = (DLOOP_Dataloop *) MPIU_Malloc(new_loop_sz);
    if (new_loop == NULL)
    {
        return MPIU_ERR_CREATE(MPI_ERR_NO_MEM, "**allocmem" );
    }

    /* set all the pointers in the new dataloop structure */
    new_loop->loop_params.s_t.dataloop_array = (DLOOP_Dataloop **)
        (((char *) new_loop) + loop_sz);
    new_loop->loop_params.s_t.blocksize_array = (DLOOP_Count *)
        (((char *) new_loop) + loop_sz + ptr_sz);
    new_loop->loop_params.s_t.offset_array = (DLOOP_Offset *)
        (((char *) new_loop) + loop_sz + ptr_sz + blk_sz);
    new_loop->loop_params.s_t.el_extent_array = (DLOOP_Offset *)
        (((char *) new_loop) + loop_sz + ptr_sz + blk_sz + off_sz);

    *old_loop_p = (DLOOP_Dataloop *)
        (((char *) new_loop) + loop_sz + ptr_sz + blk_sz + off_sz + extent_sz);
    *new_loop_p = new_loop;
    *new_loop_sz_p = new_loop_sz;

    return MPI_SUCCESS;
}

/*@
  Dataloop_dup - make a copy of a dataloop

  Returns 0 on success, -1 on failure.
@*/
MPI_RESULT
MPID_Dataloop_dup(
    _In_reads_bytes_(old_loop_sz)       const DLOOP_Dataloop* old_loop,
    _In_                                int                   old_loop_sz,
    _Outptr_result_bytebuffer_(old_loop_sz) DLOOP_Dataloop**  new_loop_p
    )
{
    DLOOP_Dataloop *new_loop;

    MPIU_Assert(old_loop != NULL);
    MPIU_Assert(old_loop_sz > 0);

    new_loop = (DLOOP_Dataloop *) MPIU_Malloc(old_loop_sz);
    if (new_loop == NULL)
    {
        return MPIU_ERR_CREATE(MPI_ERR_NO_MEM, "**allocmem" );
    }

    MPID_Dataloop_copy(new_loop, old_loop, old_loop_sz);
    *new_loop_p = new_loop;
    return MPI_SUCCESS;
}

/*@
  Dataloop_stream_size - return the size of the data described by the dataloop

  Input Parameters:
+ dl_p   - pointer to dataloop for which we will return the size
- sizefn - function for determining size of types in the corresponding stream
           (passing NULL will instead result in el_size values being used)

@*/
DLOOP_Offset
MPID_Dataloop_stream_size(
    _In_ const struct DLOOP_Dataloop *dl_p,
    _In_ DLOOP_Offset (*sizefn)(DLOOP_Type el_type)
    )
{
    DLOOP_Offset tmp_sz, tmp_ct = 1;

    for (;;)
    {
        if ((dl_p->kind & DLOOP_KIND_MASK) == DLOOP_KIND_STRUCT)
        {
            int i;

            tmp_sz = 0;
            for (i=0; i < dl_p->loop_params.s_t.count; i++)
            {
                tmp_sz += dl_p->loop_params.s_t.blocksize_array[i] *
                   MPID_Dataloop_stream_size(dl_p->loop_params.s_t.dataloop_array[i], sizefn);
            }
            return tmp_sz * tmp_ct;
        }

        switch (dl_p->kind & DLOOP_KIND_MASK)
        {
                case DLOOP_KIND_CONTIG:
                    tmp_ct *= dl_p->loop_params.c_t.count;
                    break;
                case DLOOP_KIND_VECTOR:
                    tmp_ct *=
                        dl_p->loop_params.v_t.count * dl_p->loop_params.v_t.blocksize;
                    break;
                case DLOOP_KIND_BLOCKINDEXED:
                    tmp_ct *= dl_p->loop_params.bi_t.count *
                        dl_p->loop_params.bi_t.blocksize;
                    break;
                case DLOOP_KIND_INDEXED:
                    tmp_ct *= dl_p->loop_params.i_t.total_blocks;
                break;
                default:
                    MPIU_Assert(0);
                    break;
        }

        if (dl_p->kind & DLOOP_FINAL_MASK)
        {
            break;
        }

        MPIU_Assert(dl_p->loop_params.cm_t.dataloop != NULL);
        dl_p = dl_p->loop_params.cm_t.dataloop;
    }

    /* call fn for size using bottom type, or use size if fnptr is NULL */
    tmp_sz = ((sizefn) ? sizefn(dl_p->el_type) : dl_p->el_size);

    return tmp_sz * tmp_ct;
}


/*@
  DLOOP_Dataloop_create_named - create a dataloop for a "named" type
  if necessary.

  "named" types are ones for which MPI_Type_get_envelope() returns a
  combiner of MPI_COMBINER_NAMED. some types that fit this category,
  such as MPI_SHORT_INT, have multiple elements with potential gaps
  and padding. these types need dataloops for correct processing.
@*/
static MPI_RESULT
DLOOP_Dataloop_create_named(
    _In_                                MPI_Datatype     type,
    _Outptr_result_bytebuffer_maybenull_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    )
{
    DLOOP_Dataloop *dlp;

    /* special case: pairtypes need dataloops too.
     *
     * note: not dealing with MPI_2INT because size == extent
     *       in all cases for that type.
     *
     * note: MPICH2 always precreates these, so we will never call
     *       Dataloop_create_pairtype() from here in the MPICH2
     *       case.
     */
    if (type == MPI_FLOAT_INT || type == MPI_DOUBLE_INT ||
        type == MPI_LONG_INT || type == MPI_SHORT_INT ||
        type == MPI_LONG_DOUBLE_INT)
    {
        DLOOP_Handle_get_loopptr_macro(type, dlp, flag);
        if (dlp != NULL)
        {
            /* dataloop already created; just return it. */
            *dlp_p = dlp;
            DLOOP_Handle_get_loopsize_macro(type, *dlsz_p, flag);
            DLOOP_Handle_get_loopdepth_macro(type, *dldepth_p, flag);
            return MPI_SUCCESS;
        }

        return MPID_Dataloop_create_pairtype(type,
                                             dlp_p,
                                             dlsz_p,
                                             dldepth_p,
                                             flag);
    }

    /* no other combiners need dataloops; exit. */
    *dlp_p = NULL;
    *dlsz_p = 0;
    *dldepth_p = 0;
    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Dataloop_create(
    _In_                                MPI_Datatype     type,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    )
{
    MPI_RESULT mpierrno;
    int i;

    int nr_ints, nr_aints, nr_types, combiner;
    MPI_Datatype *types;
    int *ints;
    MPI_Aint *aints;

    DLOOP_Dataloop *old_dlp = NULL;
    int old_dlsz=0, old_dldepth=0;

    int dummy1, dummy2, dummy3, type0_combiner, ndims;
    MPI_Datatype tmptype;

    MPI_Aint stride;
    MPI_Aint *disps;

    MPID_Type_get_envelope(type, &nr_ints, &nr_aints, &nr_types, &combiner);

    /* some named types do need dataloops; handle separately. */
    if (combiner == MPI_COMBINER_NAMED)
    {
        return DLOOP_Dataloop_create_named(type, dlp_p, dlsz_p, dldepth_p, flag);
    }
    else if (combiner == MPI_COMBINER_F90_REAL ||
             combiner == MPI_COMBINER_F90_COMPLEX ||
             combiner == MPI_COMBINER_F90_INTEGER)
    {
        return MPID_Dataloop_create_contiguous(
            1,
            DLOOP_Handle_get_basic_type_macro(type),
            dlp_p,
            dlsz_p,
            dldepth_p,
            flag );
    }

    /* Q: should we also check for "hasloop", or is the COMBINER
     *    check above enough to weed out everything that wouldn't
     *    have a loop?
     */
    DLOOP_Handle_get_loopptr_macro(type, old_dlp, flag);
    if (old_dlp != NULL)
    {
        /* dataloop already created; just return it. */
        *dlp_p = old_dlp;
        DLOOP_Handle_get_loopsize_macro(type, *dlsz_p, flag);
        DLOOP_Handle_get_loopdepth_macro(type, *dldepth_p, flag);
        return MPI_SUCCESS;
    }

    MPID_Type_access_contents(type, &ints, &aints, &types);

    /* first check for zero count on types where that makes sense */
    switch(combiner)
    {
        case MPI_COMBINER_CONTIGUOUS:
        case MPI_COMBINER_VECTOR:
        case MPI_COMBINER_HVECTOR_INTEGER:
        case MPI_COMBINER_HVECTOR:
        case MPI_COMBINER_INDEXED_BLOCK:
        case MPI_COMBINER_INDEXED:
        case MPI_COMBINER_HINDEXED_BLOCK:
        case MPI_COMBINER_HINDEXED_INTEGER:
        case MPI_COMBINER_HINDEXED:
        case MPI_COMBINER_STRUCT_INTEGER:
        case MPI_COMBINER_STRUCT:
            if (ints[0] == 0)
            {
                mpierrno = MPID_Dataloop_create_contiguous(0,
                                                           MPI_INT,
                                                           dlp_p,
                                                           dlsz_p,
                                                           dldepth_p,
                                                           flag);
                goto exit;
            }
            break;
        default:
            break;
    }

    /* recurse, processing types "below" this one before processing
     * this one, if those type don't already have dataloops.
     *
     * note: in the struct case below we'll handle any additional
     *       types "below" the current one.
     */
    MPID_Type_get_envelope(types[0], &dummy1, &dummy2, &dummy3,
                           &type0_combiner);

    if (type0_combiner != MPI_COMBINER_NAMED)
    {
        DLOOP_Handle_get_loopptr_macro(types[0], old_dlp, flag);
        if (old_dlp == NULL)
        {
            /* no dataloop already present; create and store one */
            mpierrno = MPID_Dataloop_create(types[0],
                                            &old_dlp,
                                            &old_dlsz,
                                            &old_dldepth,
                                            flag);
            if( mpierrno != MPI_SUCCESS )
            {
                goto exit;
            }

            DLOOP_Handle_set_loopptr_macro(types[0], old_dlp, flag);
            DLOOP_Handle_set_loopsize_macro(types[0], old_dlsz, flag);
            DLOOP_Handle_set_loopdepth_macro(types[0], old_dldepth, flag);
        }
        else
        {
            DLOOP_Handle_get_loopsize_macro(types[0], old_dlsz, flag);
            DLOOP_Handle_get_loopdepth_macro(types[0], old_dldepth, flag);
        }
    }

    switch(combiner)
    {
        case MPI_COMBINER_DUP:
            if (type0_combiner != MPI_COMBINER_NAMED)
            {
                mpierrno = MPID_Dataloop_dup(old_dlp, old_dlsz, dlp_p);
                *dlsz_p    = old_dlsz;
                *dldepth_p = old_dldepth;
            }
            else
            {
                mpierrno = MPID_Dataloop_create_contiguous(1,
                                                           types[0],
                                                           dlp_p, dlsz_p,
                                                           dldepth_p,
                                                           flag);
            }
            break;
        case MPI_COMBINER_RESIZED:
            if (type0_combiner != MPI_COMBINER_NAMED)
            {
                mpierrno = MPID_Dataloop_dup(old_dlp, old_dlsz, dlp_p);
                *dlsz_p    = old_dlsz;
                *dldepth_p = old_dldepth;
            }
            else
            {
                mpierrno = MPID_Dataloop_create_contiguous(1,
                                                           types[0],
                                                           dlp_p, dlsz_p,
                                                           dldepth_p,
                                                           flag);
                if( mpierrno == MPI_SUCCESS )
                {
                    (*dlp_p)->el_extent = aints[1]; /* extent */
                }
            }
            break;
        case MPI_COMBINER_CONTIGUOUS:
            mpierrno = MPID_Dataloop_create_contiguous(ints[0] /* count */,
                                                       types[0] /* oldtype */,
                                                       dlp_p, dlsz_p,
                                                       dldepth_p,
                                                       flag);
            break;
        case MPI_COMBINER_VECTOR:
            mpierrno = MPID_Dataloop_create_vector(ints[0] /* count */,
                                                   ints[1] /* blklen */,
                                                   ints[2] /* stride */,
                                                   0 /* stride not bytes */,
                                                   types[0] /* oldtype */,
                                                   dlp_p, dlsz_p, dldepth_p,
                                                   flag);
            break;
        case MPI_COMBINER_HVECTOR_INTEGER:
        case MPI_COMBINER_HVECTOR:
            /* fortran hvector has integer stride in bytes */
            if (combiner == MPI_COMBINER_HVECTOR_INTEGER)
            {
                stride = (MPI_Aint) ints[2];
            }
            else
            {
                stride = aints[0];
            }

            mpierrno = MPID_Dataloop_create_vector(ints[0] /* count */,
                                                   ints[1] /* blklen */,
                                                   stride,
                                                   1 /* stride in bytes */,
                                                   types[0] /* oldtype */,
                                                   dlp_p, dlsz_p, dldepth_p,
                                                   flag);
            break;
        case MPI_COMBINER_INDEXED_BLOCK:
            mpierrno = MPID_Dataloop_create_blockindexed(ints[0] /* count */,
                                                         ints[1] /* blklen */,
                                                         &ints[2] /* disps */,
                                                         0 /* disp not bytes */,
                                                         types[0] /* oldtype */,
                                                         dlp_p, dlsz_p,
                                                         dldepth_p,
                                                         flag);
            break;
        case MPI_COMBINER_INDEXED:
            mpierrno = MPID_Dataloop_create_indexed(ints[0] /* count */,
                                                    &ints[1] /* blklens */,
                                                    &ints[ints[0]+1] /* disp */,
                                                    0 /* disp not in bytes */,
                                                    types[0] /* oldtype */,
                                                    dlp_p, dlsz_p, dldepth_p,
                                                    flag);
            break;
        case MPI_COMBINER_HINDEXED_BLOCK:
            mpierrno = MPID_Dataloop_create_blockindexed(
                ints[0],  // count
                ints[1],  // blklen
                aints,    // disp_array
                1,        // dispinbytes
                types[0], // oldtype
                dlp_p,
                dlsz_p,
                dldepth_p,
                flag
                );
            break;
        case MPI_COMBINER_HINDEXED_INTEGER:
        case MPI_COMBINER_HINDEXED:
            if (combiner == MPI_COMBINER_HINDEXED_INTEGER)
            {
                disps = MPIU_Malloc_objn(ints[0], MPI_Aint);
                if( disps == NULL )
                {
                    mpierrno = MPIU_ERR_NOMEM();
                    goto exit;
                }

                for (i=0; i < ints[0]; i++)
                {
                    disps[i] = (MPI_Aint) ints[ints[0] + 1 + i];
                }
            }
            else
            {
                disps = aints;
            }

            mpierrno = MPID_Dataloop_create_indexed(ints[0] /* count */,
                                                    &ints[1] /* blklens */,
                                                    disps,
                                                    1 /* disp in bytes */,
                                                    types[0] /* oldtype */,
                                                    dlp_p, dlsz_p, dldepth_p,
                                                    flag);

            if (combiner == MPI_COMBINER_HINDEXED_INTEGER)
            {
                MPIU_Free(disps);
            }

            break;
        case MPI_COMBINER_STRUCT_INTEGER:
        case MPI_COMBINER_STRUCT:
            for (i = 1; i < ints[0]; i++)
            {
                int type_combiner;
                NMPI_Type_get_envelope(types[i], &dummy1, &dummy2, &dummy3,
                                       &type_combiner);

                if (type_combiner != MPI_COMBINER_NAMED)
                {
                    DLOOP_Handle_get_loopptr_macro(types[i], old_dlp, flag);
                    if (old_dlp == NULL)
                    {
                        mpierrno = MPID_Dataloop_create(types[i],
                                                        &old_dlp,
                                                        &old_dlsz,
                                                        &old_dldepth,
                                                        flag);
                        if( mpierrno != MPI_SUCCESS )
                        {
                            goto exit;
                        }
                        DLOOP_Handle_set_loopptr_macro(types[i], old_dlp,
                                                        flag);
                        DLOOP_Handle_set_loopsize_macro(types[i], old_dlsz,
                                                        flag);
                        DLOOP_Handle_set_loopdepth_macro(types[i], old_dldepth,
                                                            flag);
                    }
                }
            }
            if (combiner == MPI_COMBINER_STRUCT_INTEGER)
            {
                disps = MPIU_Malloc_objn(ints[0], MPI_Aint);
                if( disps == NULL )
                {
                    mpierrno = MPIU_ERR_NOMEM();
                    goto exit;
                }

                for (i=0; i < ints[0]; i++)
                {
                    disps[i] = (MPI_Aint) ints[ints[0] + 1 + i];
                }
            }
            else
            {
                disps = aints;
            }

            mpierrno = MPID_Dataloop_create_struct(ints[0] /* count */,
                                                   &ints[1] /* blklens */,
                                                   disps,
                                                   types /* oldtype array */,
                                                   dlp_p, dlsz_p, dldepth_p,
                                                   flag);

            if (combiner == MPI_COMBINER_STRUCT_INTEGER)
            {
                MPIU_Free(disps);
            }
            break;
        case MPI_COMBINER_SUBARRAY:
            ndims = ints[0];
            mpierrno = MPID_Type_convert_subarray(ndims,
                                                  &ints[1] /* sizes */,
                                                  &ints[1+ndims] /* subsizes */,
                                                  &ints[1+2*ndims] /* starts */,
                                                  ints[1+3*ndims] /* order */,
                                                  types[0],
                                                  &tmptype);
            if( mpierrno != MPI_SUCCESS )
            {
                goto exit;
            }

            mpierrno = MPID_Dataloop_create(tmptype,
                                            dlp_p,
                                            dlsz_p,
                                            dldepth_p,
                                            flag);

            NMPI_Type_free(&tmptype);
            break;
        case MPI_COMBINER_DARRAY:
            ndims = ints[2];
            mpierrno = MPID_Type_convert_darray(ints[0] /* size */,
                                                ints[1] /* rank */,
                                                ndims,
                                                &ints[3] /* gsizes */,
                                                &ints[3+ndims] /*distribs */,
                                                &ints[3+2*ndims] /* dargs */,
                                                &ints[3+3*ndims] /* psizes */,
                                                ints[3+4*ndims] /* order */,
                                                types[0],
                                                &tmptype);
            if( mpierrno != MPI_SUCCESS )
            {
                goto exit;
            }

            mpierrno = MPID_Dataloop_create(tmptype,
                                            dlp_p,
                                            dlsz_p,
                                            dldepth_p,
                                            flag);

            NMPI_Type_free(&tmptype);
            break;
        default:
            MPIU_Assert(0);
            mpierrno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**intern");
            break;
    }

 exit:

    /* for now we just leave the intermediate dataloops in place.
     * could remove them to save space if we wanted.
     */

    return mpierrno;
}


/* DLOOP_Type_blockindexed_array_copy
 *
 * Unlike the indexed version, this one does not compact adjacent
 * blocks, because that would really mess up the blockindexed type!
 */
static void
DLOOP_Type_blockindexed_array_copy(
    _In_                    DLOOP_Count   count,
    _In_                    const void*   in_disp_array,
    _Out_writes_all_(count) DLOOP_Offset* out_disp_array,
    _In_                    int           dispinbytes,
    _In_                    DLOOP_Offset  old_extent
    )
{
    int i;
    if (!dispinbytes)
    {
        for (i=0; i < count; i++)
        {
            out_disp_array[i] =
                ((DLOOP_Offset) ((int *) in_disp_array)[i]) * old_extent;
        }
    }
    else
    {
        for (i=0; i < count; i++)
        {
            out_disp_array[i] =
                ((DLOOP_Offset) ((MPI_Aint *) in_disp_array)[i]);
        }
    }
    return;
}


static DLOOP_Count
DLOOP_Type_blockindexed_count_contig(
    _In_ DLOOP_Count  count,
    _In_ DLOOP_Count  blklen,
    _In_ const void*  disp_array,
    _In_ int          dispinbytes,
    _In_ DLOOP_Offset old_extent
    )
{
    int i, contig_count = 1;

    if (!dispinbytes)
    {
        /* this is from the MPI type, is of type int */
        DLOOP_Offset cur_tdisp = (DLOOP_Offset) ((int *) disp_array)[0];

        for (i=1; i < count; i++)
        {
            DLOOP_Offset next_tdisp = (DLOOP_Offset) ((int *) disp_array)[i];

            if (cur_tdisp + blklen != next_tdisp)
            {
                contig_count++;
            }
            cur_tdisp = next_tdisp;
        }
    }
    else
    {
        /* this is from the MPI type, is of type MPI_Aint */
        DLOOP_Offset cur_bdisp = (DLOOP_Offset) ((MPI_Aint *) disp_array)[0];

        for (i=1; i < count; i++)
        {
            DLOOP_Offset next_bdisp =
                (DLOOP_Offset) ((MPI_Aint *) disp_array)[i];

            if (cur_bdisp + (DLOOP_Offset) blklen * old_extent != next_bdisp)
            {
                contig_count++;
            }
            cur_bdisp = next_bdisp;
        }
    }
    return contig_count;
}


/*@
   Dataloop_create_blockindexed - create blockindexed dataloop

   Arguments:
+  int count
.  void *displacement_array (array of either MPI_Aints or ints)
.  int displacement_in_bytes (boolean)
.  MPI_Datatype old_type
.  DLOOP_Dataloop **output_dataloop_ptr
.  int output_dataloop_size
.  int output_dataloop_depth
-  int flag

.N Errors
.N Returns 0 on success, -1 on failure.
@*/
MPI_RESULT
MPID_Dataloop_create_blockindexed(
    _In_                                int              icount,
    _In_                                int              iblklen,
    _In_                                const void*      disp_array,
    _In_                                int              dispinbytes,
    _In_                                DLOOP_Type       oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    )
{
    MPI_RESULT mpi_errno;
    int is_builtin, is_vectorizable = 1;
    int i, new_loop_sz, old_loop_depth;

    DLOOP_Count contig_count, count, blklen;
    DLOOP_Offset old_extent, eff_disp0, eff_disp1, last_stride;
    DLOOP_Dataloop *new_dlp;

    count  = (DLOOP_Count) icount; /* avoid subsequent casting */
    blklen = (DLOOP_Count) iblklen;

    /* if count or blklen are zero, handle with contig code, call it a int */
    if (count == 0 || blklen == 0)
    {
        mpi_errno = MPID_Dataloop_create_contiguous(0,
                                                    MPI_INT,
                                                    dlp_p,
                                                    dlsz_p,
                                                    dldepth_p,
                                                    flag);
        return mpi_errno;
    }

    is_builtin = (DLOOP_Handle_hasloop_macro(oldtype)) ? 0 : 1;

    if (is_builtin)
    {
        old_extent = DLOOP_Handle_get_size_macro(oldtype);
        old_loop_depth = 0;
    }
    else
    {
        old_extent = DLOOP_Handle_get_extent_macro(oldtype);
        DLOOP_Handle_get_loopdepth_macro(oldtype, old_loop_depth, flag);
    }

    contig_count = DLOOP_Type_blockindexed_count_contig(count,
                                                        blklen,
                                                        disp_array,
                                                        dispinbytes,
                                                        old_extent);

    /* optimization:
     *
     * if contig_count == 1 and block starts at displacement 0,
     * store it as a contiguous rather than a blockindexed dataloop.
     */
    if ((contig_count == 1) &&
        ((!dispinbytes && ((int *) disp_array)[0] == 0) ||
         (dispinbytes && ((MPI_Aint *) disp_array)[0] == 0)))
    {
        return MPID_Dataloop_create_contiguous(icount * iblklen,
                                               oldtype,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    /* optimization:
     *
     * if contig_count == 1 store it as a blockindexed with one
     * element rather than as a lot of individual blocks.
     */
    if (contig_count == 1)
    {
        /* adjust count and blklen and drop through */
        blklen *= count;
        count = 1;
        iblklen *= icount;
        icount = 1;
    }

    /* optimization:
     *
     * if displacements start at zero and result in a fixed stride,
     * store it as a vector rather than a blockindexed dataloop.
     */
    eff_disp0 = (dispinbytes) ? ((DLOOP_Offset) ((MPI_Aint *) disp_array)[0]) :
        (((DLOOP_Offset) ((int *) disp_array)[0]) * old_extent);

    if (count > 1 && eff_disp0 == (DLOOP_Offset) 0)
    {
        eff_disp1 = (dispinbytes) ?
            ((DLOOP_Offset) ((MPI_Aint *) disp_array)[1]) :
            (((DLOOP_Offset) ((int *) disp_array)[1]) * old_extent);
        last_stride = eff_disp1 - eff_disp0;

        for (i=2; i < count; i++)
        {
            eff_disp0 = eff_disp1;
            eff_disp1 = (dispinbytes) ?
                ((DLOOP_Offset) ((MPI_Aint *) disp_array)[i]) :
                (((DLOOP_Offset) ((int *) disp_array)[i]) * old_extent);
            if (eff_disp1 - eff_disp0 != last_stride)
            {
                is_vectorizable = 0;
                break;
            }
        }
        if (is_vectorizable)
        {
            return MPID_Dataloop_create_vector(count,
                                               blklen,
                                               last_stride,
                                               1, /* strideinbytes */
                                               oldtype,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
        }
    }

    /* TODO: optimization:
     *
     * if displacements result in a fixed stride, but first displacement
     * is not zero, store it as a blockindexed (blklen == 1) of a vector.
     */

    /* TODO: optimization:
     *
     * if a blockindexed of a contig, absorb the contig into the blocklen
     * parameter and keep the same overall depth
     */

    /* otherwise storing as a blockindexed dataloop */

    /* Q: HOW CAN WE TELL IF IT IS WORTH IT TO STORE AS AN
     * INDEXED WITH FEWER CONTIG BLOCKS (IF CONTIG_COUNT IS SMALL)?
     */

    if (is_builtin)
    {
        mpi_errno = MPID_Dataloop_alloc(DLOOP_KIND_BLOCKINDEXED,
                                        count,
                                        &new_dlp,
                                        &new_loop_sz);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
        new_dlp->kind = DLOOP_KIND_BLOCKINDEXED | DLOOP_FINAL_MASK;

        new_dlp->el_size   = old_extent;
        new_dlp->el_extent = old_extent;
        new_dlp->el_type   = oldtype;
    }
    else
    {
        DLOOP_Dataloop *old_loop_ptr = NULL;
        int old_loop_sz = 0;

        DLOOP_Handle_get_loopptr_macro(oldtype, old_loop_ptr, flag);
        DLOOP_Handle_get_loopsize_macro(oldtype, old_loop_sz, flag);

        mpi_errno = MPID_Dataloop_alloc_and_copy(DLOOP_KIND_BLOCKINDEXED,
                                                 count,
                                                 old_loop_ptr,
                                                 old_loop_sz,
                                                 &new_dlp,
                                                 &new_loop_sz);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        new_dlp->kind = DLOOP_KIND_BLOCKINDEXED;

        new_dlp->el_size = DLOOP_Handle_get_size_macro(oldtype);
        new_dlp->el_extent = DLOOP_Handle_get_extent_macro(oldtype);
        new_dlp->el_type = DLOOP_Handle_get_basic_type_macro(oldtype);
    }

    new_dlp->loop_params.bi_t.count     = count;
    new_dlp->loop_params.bi_t.blocksize = blklen;

    /* copy in displacement parameters
     *
     * regardless of dispinbytes, we store displacements in bytes in loop.
     */
    DLOOP_Type_blockindexed_array_copy(count,
                                       disp_array,
                                       new_dlp->loop_params.bi_t.offset_array,
                                       dispinbytes,
                                       old_extent);

    *dlp_p     = new_dlp;
    *dlsz_p    = new_loop_sz;
    *dldepth_p = old_loop_depth + 1;

    return MPI_SUCCESS;
}


/*@
   Dataloop_contiguous - create the dataloop representation for a
   contiguous datatype

   Arguments:
+  int icount,
.  MPI_Datatype oldtype,
.  DLOOP_Dataloop **dlp_p,
.  int *dlsz_p,
.  int *dldepth_p,
-  int flag

.N Errors
.N Returns 0 on success, -1 on failure.
@*/
MPI_RESULT
MPID_Dataloop_create_contiguous(
    _In_                                int              icount,
    _In_                                MPI_Datatype     oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    )
{
    MPI_RESULT mpi_errno;
    DLOOP_Count count;
    int is_builtin;
    int new_loop_sz, new_loop_depth;

    DLOOP_Dataloop *new_dlp;

    count = (DLOOP_Count) icount; /* avoid subsequent casting */

    is_builtin = (DLOOP_Handle_hasloop_macro(oldtype)) ? 0 : 1;

    if (is_builtin)
    {
        new_loop_depth = 1;

        mpi_errno = MPID_Dataloop_alloc(DLOOP_KIND_CONTIG,
                                       count,
                                       &new_dlp,
                                       &new_loop_sz);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        DLOOP_Offset basic_sz = DLOOP_Handle_get_size_macro(oldtype);
        new_dlp->kind = DLOOP_KIND_CONTIG | DLOOP_FINAL_MASK;

        new_dlp->el_size   = basic_sz;
        new_dlp->el_extent = new_dlp->el_size;
        new_dlp->el_type   = oldtype;

        new_dlp->loop_params.c_t.count = count;
    }
    else
    {
        int old_loop_sz = 0, old_loop_depth = 0;
        DLOOP_Offset old_size = 0, old_extent = 0;
        DLOOP_Dataloop *old_loop_ptr;

        DLOOP_Handle_get_loopsize_macro(oldtype, old_loop_sz, flag);
        DLOOP_Handle_get_loopdepth_macro(oldtype, old_loop_depth, flag);
        DLOOP_Handle_get_loopptr_macro(oldtype, old_loop_ptr, flag);

        if(old_loop_ptr == NULL)
        {
            return MPI_ERR_ARG;
        }

        old_size = DLOOP_Handle_get_size_macro(oldtype);
        old_extent = DLOOP_Handle_get_extent_macro(oldtype);

        /* if we have a simple combination of contigs, coalesce */
        if (((old_loop_ptr->kind & DLOOP_KIND_MASK) == DLOOP_KIND_CONTIG)
            && (old_size == old_extent))
        {
            /* make a copy of the old loop and multiply the count */
            mpi_errno = MPID_Dataloop_dup(old_loop_ptr,
                                         old_loop_sz,
                                         &new_dlp);
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }

            new_dlp->loop_params.c_t.count *= count;

            new_loop_sz = old_loop_sz;
            DLOOP_Handle_get_loopdepth_macro(oldtype, new_loop_depth, flag);
        }
        else
        {
            new_loop_depth = old_loop_depth + 1;
            /* allocate space for new loop including copy of old */
            mpi_errno = MPID_Dataloop_alloc_and_copy(DLOOP_KIND_CONTIG,
                                                    count,
                                                    old_loop_ptr,
                                                    old_loop_sz,
                                                    &new_dlp,
                                                    &new_loop_sz);
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
            new_dlp->kind = DLOOP_KIND_CONTIG;
            new_dlp->el_size = old_size;
            new_dlp->el_extent = old_extent;
            new_dlp->el_type = DLOOP_Handle_get_basic_type_macro(oldtype);

            new_dlp->loop_params.c_t.count = count;
        }
    }

    *dlp_p     = new_dlp;
    *dlsz_p    = new_loop_sz;
    *dldepth_p = new_loop_depth;

    return MPI_SUCCESS;
}


/* DLOOP_Type_indexed_array_copy()
 *
 * Copies arrays into place, combining adjacent contiguous regions and
 * dropping zero-length regions.
 *
 * Extent passed in is for the original type.
 *
 * Output displacements are always output in bytes, while block
 * lengths are always output in terms of the base type.
 */
static void
DLOOP_Type_indexed_array_copy(
    _In_                DLOOP_Count   count,
    _In_reads_(count)   const int*    in_blklen_array,
    _In_reads_(count)   const void*   in_disp_array,
    _Out_writes_(count) DLOOP_Count*  out_blklen_array,
    _Out_writes_(count) DLOOP_Offset* out_disp_array,
    _In_                int           dispinbytes,
    _In_                DLOOP_Offset  old_extent
    )
{
    DLOOP_Count i, cur_idx = 0;

    out_blklen_array[0] = (DLOOP_Count) in_blklen_array[0];

    if (!dispinbytes)
    {
        out_disp_array[0] = (DLOOP_Offset)
            ((int *) in_disp_array)[0] * old_extent;

        for (i = 1; i < count; i++)
        {
            if (in_blklen_array[i] == 0)
            {
                continue;
            }
            else if (out_disp_array[cur_idx] +
                     ((DLOOP_Offset) out_blklen_array[cur_idx]) * old_extent ==
                     ((DLOOP_Offset) ((int *) in_disp_array)[i]) * old_extent)
            {
                /* adjacent to current block; add to block */
                out_blklen_array[cur_idx] += (DLOOP_Count) in_blklen_array[i];
            }
            else
            {
                cur_idx++;
                out_disp_array[cur_idx] =
                    ((DLOOP_Offset) ((int *) in_disp_array)[i]) * old_extent;
                out_blklen_array[cur_idx] = in_blklen_array[i];
            }
        }
    }
    else /* input displacements already in bytes */
    {
        out_disp_array[0] = (DLOOP_Offset) ((MPI_Aint *) in_disp_array)[0];

        for (i = 1; i < count; i++)
        {
            if (in_blklen_array[i] == 0)
            {
                continue;
            }
            else if (out_disp_array[cur_idx] +
                     ((DLOOP_Offset) out_blklen_array[cur_idx]) * old_extent ==
                     ((DLOOP_Offset) ((MPI_Aint *) in_disp_array)[i]))
            {
                /* adjacent to current block; add to block */
                out_blklen_array[cur_idx] += in_blklen_array[i];
            }
            else
            {
                cur_idx++;
                out_disp_array[cur_idx] =
                    (DLOOP_Offset) ((MPI_Aint *) in_disp_array)[i];
                out_blklen_array[cur_idx] = (DLOOP_Count) in_blklen_array[i];
            }
        }
    }

    return;
}


/* DLOOP_Type_indexed_count_contig()
 *
 * Determines the actual number of contiguous blocks represented by the
 * blocklength/displacement arrays.  This might be less than count (as
 * few as 1).
 *
 * Extent passed in is for the original type.
 */
static DLOOP_Count
DLOOP_Type_indexed_count_contig(
    _In_              DLOOP_Count  count,
    _In_reads_(count) const int*   blocklength_array,
    _In_reads_(count) const void*  displacement_array,
    _In_              int          dispinbytes,
    _In_              DLOOP_Offset old_extent
    )
{
    DLOOP_Count i, contig_count = 1;
    DLOOP_Count cur_blklen = (DLOOP_Count) blocklength_array[0];

    if (!dispinbytes)
    {
        DLOOP_Offset cur_tdisp =
            (DLOOP_Offset) ((int *) displacement_array)[0];

        for (i = 1; i < count; i++)
        {
            if (blocklength_array[i] == 0)
            {
                continue;
            }
            else if (cur_tdisp + cur_blklen ==
                     (DLOOP_Offset) ((int *) displacement_array)[i])
            {
                /* adjacent to current block; add to block */
                cur_blklen += (DLOOP_Count) blocklength_array[i];
            }
            else
            {
                cur_tdisp  = (DLOOP_Offset) ((int *) displacement_array)[i];
                cur_blklen = (DLOOP_Count) blocklength_array[i];
                contig_count++;
            }
        }
    }
    else
    {
        DLOOP_Offset cur_bdisp =
            (DLOOP_Offset) ((MPI_Aint *) displacement_array)[0];

        for (i = 1; i < count; i++)
        {
            if (blocklength_array[i] == 0)
            {
                continue;
            }
            else if (cur_bdisp + cur_blklen * old_extent ==
                     (DLOOP_Offset) ((MPI_Aint *) displacement_array)[i])
            {
                /* adjacent to current block; add to block */
                cur_blklen += (DLOOP_Count) blocklength_array[i];
            }
            else
            {
                cur_bdisp  =
                    (DLOOP_Offset) ((MPI_Aint *) displacement_array)[i];
                cur_blklen = (DLOOP_Count) blocklength_array[i];
                contig_count++;
            }
        }
    }
    return contig_count;
}


/*@
   DLOOP_Dataloop_create_indexed

   Arguments:
+  int icount
.  int *iblocklength_array
.  void *displacement_array (either ints or MPI_Aints)
.  int dispinbytes
.  MPI_Datatype oldtype
.  DLOOP_Dataloop **dlp_p
.  int *dlsz_p
.  int *dldepth_p
-  int flag

.N Errors
.N Returns 0 on success, -1 on error.
@*/
MPI_RESULT
MPID_Dataloop_create_indexed(
    _In_                                int              icount,
    _In_reads_(icount)                  const int*       blocklength_array,
    _In_                                const void*      displacement_array,
    _In_                                int              dispinbytes,
    _In_                                MPI_Datatype     oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    )
{
    int is_builtin;
    int i, new_loop_sz, old_loop_depth, blksz;

    DLOOP_Count old_type_count = 0, contig_count, count;
    DLOOP_Offset old_extent;
    struct DLOOP_Dataloop *new_dlp;

    count = (DLOOP_Count) icount; /* avoid subsequent casting */

    /* if count is zero, handle with contig code, call it an int */
    if (count == 0)
    {
        return MPID_Dataloop_create_contiguous(0,
                                               MPI_INT,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    is_builtin = (DLOOP_Handle_hasloop_macro(oldtype)) ? 0 : 1;

    if (is_builtin)
    {
        old_extent = DLOOP_Handle_get_extent_macro(oldtype);
        old_loop_depth = 0;
    }
    else
    {
        old_extent = DLOOP_Handle_get_extent_macro(oldtype);
        DLOOP_Handle_get_loopdepth_macro(oldtype, old_loop_depth, flag);
    }

    for (i=0; i < count; i++)
    {
        old_type_count += (DLOOP_Count) blocklength_array[i];
    }

    contig_count = DLOOP_Type_indexed_count_contig(count,
                                                   blocklength_array,
                                                   displacement_array,
                                                   dispinbytes,
                                                   old_extent);

    /* if contig_count is zero (no data), handle with contig code */
    if (contig_count == 0)
    {
        return MPID_Dataloop_create_contiguous(0,
                                               MPI_INT,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    /* optimization:
     *
     * if contig_count == 1 and block starts at displacement 0,
     * store it as a contiguous rather than an indexed dataloop.
     */
    if ((contig_count == 1) &&
        ((!dispinbytes && ((int *) displacement_array)[0] == 0) ||
         (dispinbytes && ((MPI_Aint *) displacement_array)[0] == 0)))
    {
        return MPID_Dataloop_create_contiguous((int) old_type_count,
                                               oldtype,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    /* optimization:
     *
     * if contig_count == 1 (and displacement != 0), store this as
     * a single element blockindexed rather than a lot of individual
     * blocks.
     */
    if (contig_count == 1)
    {
        return MPID_Dataloop_create_blockindexed(1,
                                                 (int) old_type_count,
                                                 displacement_array,
                                                 dispinbytes,
                                                 oldtype,
                                                 dlp_p,
                                                 dlsz_p,
                                                 dldepth_p,
                                                 flag);
    }

    /* optimization:
     *
     * if block length is the same for all blocks, store it as a
     * blockindexed rather than an indexed dataloop.
     */
    blksz = blocklength_array[0];
    for (i=1; i < count; i++)
    {
        if (blocklength_array[i] != blksz)
        {
            blksz--;
            break;
        }
    }
    if (blksz == blocklength_array[0])
    {
        return MPID_Dataloop_create_blockindexed(icount,
                                                 blksz,
                                                 displacement_array,
                                                 dispinbytes,
                                                 oldtype,
                                                 dlp_p,
                                                 dlsz_p,
                                                 dldepth_p,
                                                 flag);
    }

    /* note: blockindexed looks for the vector optimization */

    /* TODO: optimization:
     *
     * if an indexed of a contig, absorb the contig into the blocklen array
     * and keep the same overall depth
     */

    /* otherwise storing as an indexed dataloop */

    if (is_builtin)
    {
        MPID_Dataloop_alloc(DLOOP_KIND_INDEXED,
                                       count,
                                       &new_dlp,
                                       &new_loop_sz);
        if (!new_dlp) return -1;

        new_dlp->kind = DLOOP_KIND_INDEXED | DLOOP_FINAL_MASK;

        new_dlp->el_size   = old_extent;
        new_dlp->el_extent = old_extent;
        new_dlp->el_type   = oldtype;
    }
    else
    {
        DLOOP_Dataloop *old_loop_ptr = NULL;
        int old_loop_sz = 0;

        DLOOP_Handle_get_loopptr_macro(oldtype, old_loop_ptr, flag);
        DLOOP_Handle_get_loopsize_macro(oldtype, old_loop_sz, flag);

        MPID_Dataloop_alloc_and_copy(DLOOP_KIND_INDEXED,
                                                contig_count,
                                                old_loop_ptr,
                                                old_loop_sz,
                                                &new_dlp,
                                                &new_loop_sz);
        if (!new_dlp) return -1;

        new_dlp->kind = DLOOP_KIND_INDEXED;

        new_dlp->el_size = DLOOP_Handle_get_size_macro(oldtype);
        new_dlp->el_extent = DLOOP_Handle_get_extent_macro(oldtype);
        new_dlp->el_type = DLOOP_Handle_get_basic_type_macro(oldtype);
    }

    new_dlp->loop_params.i_t.count        = contig_count;
    new_dlp->loop_params.i_t.total_blocks = old_type_count;

    /* copy in blocklength and displacement parameters (in that order)
     *
     * regardless of dispinbytes, we store displacements in bytes in loop.
     */
    DLOOP_Type_indexed_array_copy(count,
                                  blocklength_array,
                                  displacement_array,
                                  new_dlp->loop_params.i_t.blocksize_array,
                                  new_dlp->loop_params.i_t.offset_array,
                                  dispinbytes,
                                  old_extent);

    *dlp_p     = new_dlp;
    *dlsz_p    = new_loop_sz;
    *dldepth_p = old_loop_depth + 1;

    return MPI_SUCCESS;
}


#define PAIRTYPE_CONTENTS(mt1_,ut1_,mt2_,ut2_)                          \
    {                                                                   \
        struct { ut1_ a; ut2_ b; } foo;                                 \
        disps[0] = 0;                                                   \
        disps[1] = (MPI_Aint) ((char *) &foo.b - (char *) &foo.a);      \
        types[0] = mt1_;                                                \
        types[1] = mt2_;                                                \
    }


/*@
   Dataloop_create_pairtype - create dataloop for a pairtype

   Arguments:
+  MPI_Datatype type - the pairtype
.  DLOOP_Dataloop **output_dataloop_ptr
.  int output_dataloop_size
.  int output_dataloop_depth
-  int flag

.N Errors
.N Returns 0 on success, -1 on failure.

   Note:
   This function simply creates the appropriate input parameters for
   use with Dataloop_create_struct and then calls that function.

   This same function could be used to create dataloops for any type
   that actually consists of two distinct elements.
@*/
MPI_RESULT
MPID_Dataloop_create_pairtype(
    _In_                                MPI_Datatype     type,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int               flag
    )
{
    int blocks[2] = { 1, 1 };
    MPI_Aint disps[2];
    MPI_Datatype types[2];

    MPIU_Assert(type == MPI_FLOAT_INT || type == MPI_DOUBLE_INT ||
                 type == MPI_LONG_INT || type == MPI_SHORT_INT ||
                type == MPI_LONG_DOUBLE_INT || type == MPI_2INT);

    switch(type)
    {
        case MPI_FLOAT_INT:
            PAIRTYPE_CONTENTS(MPI_FLOAT, float, MPI_INT, int);
            break;
        case MPI_DOUBLE_INT:
            PAIRTYPE_CONTENTS(MPI_DOUBLE, double, MPI_INT, int);
            break;
        case MPI_LONG_INT:
            PAIRTYPE_CONTENTS(MPI_LONG, long, MPI_INT, int);
            break;
        case MPI_SHORT_INT:
            PAIRTYPE_CONTENTS(MPI_SHORT, short, MPI_INT, int);
            break;
        case MPI_LONG_DOUBLE_INT:
            PAIRTYPE_CONTENTS(MPI_LONG_DOUBLE, long double, MPI_INT, int);
            break;
        case MPI_2INT:
            PAIRTYPE_CONTENTS(MPI_INT, int, MPI_INT, int);
            break;
    }

    return MPID_Dataloop_create_struct(2,
                                                  blocks,
                                                  disps,
                                                  types,
                                                  dlp_p,
                                                  dlsz_p,
                                                  dldepth_p,
                                                  flag);
}


static MPI_RESULT
DLOOP_Dataloop_create_unique_type_struct(
    _In_                                int               count,
    _In_reads_(count)                   const int*        blklens,
    _In_reads_(count)                   const MPI_Aint*   disps,
    _In_                                const DLOOP_Type* oldtypes,
    _In_                                int               type_pos,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop**  dlp_p,
    _Out_                               int*              dlsz_p,
    _Out_                               int*              dldepth_p,
    _In_                                int               flag
    )
{
    /* the same type used more than once in the array; type_pos
     * indexes to the first of these.
     */
    int i, *tmp_blklens, cur_pos = 0;
    DLOOP_Offset *tmp_disps;

    /* count is an upper bound on number of type instances */
    tmp_blklens = MPIU_Malloc_objn(count, int);
    if (!tmp_blklens)
    {
        return MPIU_ERR_NOMEM();
    }

    tmp_disps = MPIU_Malloc_objn(count, DLOOP_Offset);
    if (!tmp_disps)
    {
        MPIU_Free(tmp_blklens);
        /* TODO: ??? */
        return MPIU_ERR_NOMEM();
    }

    for (i=type_pos; i < count; i++)
    {
        if (oldtypes[i] == oldtypes[type_pos] && blklens != 0)
        {
            tmp_blklens[cur_pos] = blklens[i];
            tmp_disps[cur_pos]   = disps[i];
            cur_pos++;
        }
    }

    MPI_RESULT mpi_errno = MPID_Dataloop_create_indexed(cur_pos,
                                                        tmp_blklens,
                                                        tmp_disps,
                                                         1, /* disp in bytes */
                                                        oldtypes[type_pos],
                                                        dlp_p,
                                                        dlsz_p,
                                                        dldepth_p,
                                                        flag);

    MPIU_Free(tmp_blklens);
    MPIU_Free(tmp_disps);

    return mpi_errno;
}


static MPI_RESULT
DLOOP_Dataloop_create_basic_all_bytes_struct(
    _In_                                int               count,
    _In_reads_(count)                   const int*        blklens,
    _In_reads_(count)                   const MPI_Aint*   disps,
    _In_                                const DLOOP_Type* oldtypes,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop**  dlp_p,
    _Out_                               int*              dlsz_p,
    _Out_                               int*              dldepth_p,
    _In_                                int               flag
    )
{
    int i, cur_pos = 0;
    int *tmp_blklens;
    MPI_Aint *tmp_disps;

    /* count is an upper bound on number of type instances */
    tmp_blklens = MPIU_Malloc_objn(count, int);

    if (!tmp_blklens)
    {
        return MPIU_ERR_NOMEM();
    }

    tmp_disps = MPIU_Malloc_objn(count, MPI_Aint);

    if (!tmp_disps)
    {
        MPIU_Free(tmp_blklens);
        return MPIU_ERR_NOMEM();
    }

    for (i=0; i < count; i++)
    {
        if (oldtypes[i] != MPI_LB && oldtypes[i] != MPI_UB && blklens[i] != 0)
        {
            DLOOP_Offset sz;

            sz = DLOOP_Handle_get_size_macro(oldtypes[i]);
            tmp_blklens[cur_pos] = (int) sz * blklens[i];
            tmp_disps[cur_pos]   = disps[i];
            cur_pos++;
        }
    }
    MPI_RESULT mpi_errno = MPID_Dataloop_create_indexed(cur_pos,
                                                        tmp_blklens,
                                                        tmp_disps,
                                                        1, /* disp in bytes */
                                                        MPI_BYTE,
                                                        dlp_p,
                                                        dlsz_p,
                                                        dldepth_p,
                                                        flag);

    MPIU_Free(tmp_blklens);
    MPIU_Free(tmp_disps);

    return mpi_errno;
}


static MPI_RESULT
DLOOP_Dataloop_create_flattened_struct(
    _In_                                int               count,
    _In_reads_(count)                   const int*        blklens,
    _In_reads_(count)                   const MPI_Aint*   disps,
    _In_                                const DLOOP_Type* oldtypes,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop**  dlp_p,
    _Out_                               int*              dlsz_p,
    _Out_                               int*              dldepth_p,
    _In_                                int               flag
    )
{
    /* arbitrary types, convert to bytes and use indexed */
    MPI_RESULT mpi_errno;
    int i, *tmp_blklens, nr_blks = 0;
    MPI_Aint *tmp_disps; /* since we're calling another fn that takes
                            this type as an input parameter */
    DLOOP_Offset bytes;
    DLOOP_Segment *segp;

    int first_ind, last_ind;

    segp = MPID_Segment_alloc();
    if (!segp)
    {
        return MPIU_ERR_NOMEM();
    }

    /* use segment code once to count contiguous regions */
    for (i=0; i < count; i++)
    {
        /* ignore type elements with a zero blklen */
        if (blklens[i] == 0) continue;

        if (!DLOOP_Handle_hasloop_macro(oldtypes[i]) &&
            (oldtypes[i] != MPI_LB &&
             oldtypes[i] != MPI_UB))
        {
            nr_blks++;
        }
        else /* derived type; get a count of contig blocks */
        {
            DLOOP_Count tmp_nr_blks;
            //
            // If the derived type has some data to contribute, add to flattened representation.
            //
            if((blklens[i] > 0) &&
               (DLOOP_Handle_get_size_macro(oldtypes[i]) > 0))
            {
                MPID_Segment_init(NULL,
                                             (DLOOP_Count) blklens[i],
                                             oldtypes[i],
                                             segp,
                                             flag);
                bytes = SEGMENT_IGNORE_LAST;

                MPID_Segment_count_contig_blocks(segp,
                                                            0,
                                                            &bytes,
                                                            &tmp_nr_blks);

                nr_blks += tmp_nr_blks;
            }
        }
    }

    nr_blks += 2; /* safety measure */

    tmp_blklens = MPIU_Malloc_objn(nr_blks, int);
    if (!tmp_blklens)
    {
        MPID_Segment_free(segp);
        return MPIU_ERR_NOMEM();
    }

    tmp_disps = MPIU_Malloc_objn(nr_blks, MPI_Aint);
    if (!tmp_disps)
    {
        MPIU_Free(tmp_blklens);
        MPID_Segment_free(segp);
        return MPIU_ERR_NOMEM();
    }

    /* use segment code again to flatten the type */
    first_ind = 0;
    for (i=0; i < count; i++)
    {
        /* we're going to use the segment code to flatten the type.
         * we put in our displacement as the buffer location, and use
         * the blocklength as the count value to get N contiguous copies
         * of the type.
         *
         * Note that we're going to get back values in bytes, so that will
         * be our new element type.
         */
        if (oldtypes[i] != MPI_UB &&
            oldtypes[i] != MPI_LB &&
            blklens[i] != 0 &&
            (!DLOOP_Handle_hasloop_macro(oldtypes[i]) ||
              DLOOP_Handle_get_size_macro(oldtypes[i]) > 0))
        {
            mpi_errno = MPID_Segment_init(reinterpret_cast<const void*> (disps[i]),
                                          (DLOOP_Count) blklens[i],
                                          oldtypes[i],
                                          segp,
                                          0);
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_exit;
            }

            last_ind = nr_blks - first_ind;
            bytes = SEGMENT_IGNORE_LAST;
            MPID_Segment_mpi_flatten(segp,
                                                0,
                                                &bytes,
                                                &tmp_blklens[first_ind],
                                                &tmp_disps[first_ind],
                                                &last_ind);
            first_ind += last_ind;
        }
    }
    nr_blks = first_ind;

    mpi_errno = MPID_Dataloop_create_indexed(nr_blks,
                                             tmp_blklens,
                                             tmp_disps,
                                             1, /* disp in bytes */
                                             MPI_BYTE,
                                             dlp_p,
                                             dlsz_p,
                                             dldepth_p,
                                             flag);
fn_exit:
    MPID_Segment_free(segp);
    MPIU_Free(tmp_blklens);
    MPIU_Free(tmp_disps);

    return mpi_errno;
}


/*@
  Dataloop_create_struct - create the dataloop representation for a
  struct datatype

  Input Parameters:
+ count - number of blocks in vector
. blklens - number of elements in each block
. disps - offsets of blocks from start of type in bytes
- oldtypes - types (using handle) of datatypes on which vector is based

  Output Parameters:
+ dlp_p - pointer to address in which to place pointer to new dataloop
- dlsz_p - pointer to address in which to place size of new dataloop

  Return Value:
  0 on success, -1 on failure.

  Notes:
  This function relies on others, like Dataloop_create_indexed, to create
  types in some cases. This call (like all the rest) takes int blklens
  and MPI_Aint displacements, so it's possible to overflow when working
  with a particularly large struct type in some cases. This isn't detected
  or corrected in this code at this time.

@*/
MPI_RESULT
MPID_Dataloop_create_struct(
    _In_                                int               count,
    _In_reads_(count)                   const int*        blklens,
    _In_reads_(count)                   const MPI_Aint*   disps,
    _In_                                const DLOOP_Type* oldtypes,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop**  dlp_p,
    _Out_                               int*              dlsz_p,
    _Out_                               int*              dldepth_p,
    _In_                                int               flag
    )
{
    int i, nr_basics = 0, nr_derived = 0, type_pos = 0;

    DLOOP_Type first_basic = MPI_DATATYPE_NULL,
        first_derived = MPI_DATATYPE_NULL;

    /* variables used in general case only */
    int loop_idx, new_loop_sz, new_loop_depth;
    int old_loop_sz = 0, old_loop_depth = 0;

    DLOOP_Dataloop *new_dlp, *curpos;

    /* if count is zero, handle with contig code, call it a int */
    if (count == 0)
    {
        return MPID_Dataloop_create_contiguous(0,
                                               MPI_INT,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    /* browse the old types and characterize */
    for (i=0; i < count; i++)
    {
        /* ignore type elements with a zero blklen */
        if (blklens[i] == 0) continue;

        if (oldtypes[i] != MPI_LB && oldtypes[i] != MPI_UB)
        {
            if (!DLOOP_Handle_hasloop_macro(oldtypes[i]))
            {
                if (nr_basics == 0)
                {
                    first_basic = oldtypes[i];
                    type_pos = i;
                }
                else if (oldtypes[i] != first_basic)
                {
                    first_basic = MPI_DATATYPE_NULL;
                }
                nr_basics++;
            }
            else /* derived type */
            {
                if (nr_derived == 0)
                {
                    first_derived = oldtypes[i];
                    type_pos = i;
                }
                else if (oldtypes[i] != first_derived)
                {
                    first_derived = MPI_DATATYPE_NULL;
                }
                nr_derived++;
            }
        }
    }

    /* note on optimizations:
     *
     * because LB, UB, and extent calculations are handled as part of
     * the Datatype, we can safely ignore them in all our calculations
     * here.
     */

    /* optimization:
     *
     * if there were only MPI_LBs and MPI_UBs in the struct type,
     * treat it as a zero-element contiguous (just as count == 0).
     */
    if (nr_basics == 0 && nr_derived == 0)
    {
        return MPID_Dataloop_create_contiguous(0,
                                               MPI_INT,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    /* optimization:
     *
     * if there is only one unique instance of a type in the struct, treat it
     * as a blockindexed type.
     *
     * notes:
     *
     * if the displacement happens to be zero, the blockindexed code will
     * optimize this into a contig.
     */
    if (nr_basics + nr_derived == 1)
    {
        /* type_pos is index to only real type in array */
        return MPID_Dataloop_create_blockindexed(
            1, /* count */
            blklens[type_pos],
            &disps[type_pos],
            1, /* displacement in bytes */
            oldtypes[type_pos],
            dlp_p,
            dlsz_p,
            dldepth_p,
            flag);
    }

    /* optimization:
     *
     * if there only one unique type (more than one instance) in the
     * struct, treat it as an indexed type.
     *
     * notes:
     *
     * this will apply to a single type with an LB/UB, as those
     * are handled elsewhere.
     *
     */
    if (((nr_derived == 0) && (first_basic != MPI_DATATYPE_NULL)) ||
        ((nr_basics == 0) && (first_derived != MPI_DATATYPE_NULL)))
    {
        return DLOOP_Dataloop_create_unique_type_struct(count,
                                                        blklens,
                                                        disps,
                                                        oldtypes,
                                                        type_pos,
                                                        dlp_p,
                                                        dlsz_p,
                                                        dldepth_p,
                                                        flag);
    }

    /* optimization:
     *
     * if there are no derived types and caller indicated either a
     * homogeneous system or the "all bytes" conversion, convert
     * everything to bytes and use an indexed type.
     */
    if (nr_derived == 0 && flag == DLOOP_DATALOOP_HOMOGENEOUS)
    {
        return DLOOP_Dataloop_create_basic_all_bytes_struct(count,
                                                            blklens,
                                                            disps,
                                                            oldtypes,
                                                            dlp_p,
                                                            dlsz_p,
                                                            dldepth_p,
                                                            flag);
    }

    /* optimization:
     *
     * if caller asked for homogeneous or all bytes representation,
     * flatten the type and store it as an indexed type so that
     * there are no branches in the dataloop tree.
     */
    if (flag == DLOOP_DATALOOP_HOMOGENEOUS)
    {
        return DLOOP_Dataloop_create_flattened_struct(count,
                                                      blklens,
                                                      disps,
                                                      oldtypes,
                                                      dlp_p,
                                                      dlsz_p,
                                                      dldepth_p,
                                                      flag);
    }

    /* scan through types and gather derived type info */
    for (i=0; i < count; i++)
    {
        /* ignore type elements with a zero blklen */
        if (blklens[i] == 0) continue;

        if (DLOOP_Handle_hasloop_macro(oldtypes[i]))
        {
            int tmp_loop_depth, tmp_loop_sz;

            DLOOP_Handle_get_loopdepth_macro(oldtypes[i], tmp_loop_depth, flag);
            DLOOP_Handle_get_loopsize_macro(oldtypes[i], tmp_loop_sz, flag);

            if (tmp_loop_depth > old_loop_depth)
            {
                old_loop_depth = tmp_loop_depth;
            }
            old_loop_sz += tmp_loop_sz;
        }
    }

    /* general case below: 2 or more distinct types that are either
     * basics or derived, and for which we want to preserve the types
     * themselves.
     */

    if (nr_basics > 0)
    {
        /* basics introduce an extra level of depth, so if our new depth
         * must be at least 2 if there are basics.
         */
        new_loop_depth = ((old_loop_depth+1) > 2) ? (old_loop_depth+1) : 2;
    }
    else
    {
        new_loop_depth = old_loop_depth + 1;
    }

    MPI_RESULT mpi_errno = MPID_Dataloop_struct_alloc((DLOOP_Count) nr_basics + nr_derived,
                                                      old_loop_sz,
                                                      nr_basics,
                                                      &curpos,
                                                      &new_dlp,
                                                      &new_loop_sz);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    new_dlp->kind = DLOOP_KIND_STRUCT;
    new_dlp->el_size = -1; /* not valid for struct */
    new_dlp->el_extent = -1; /* not valid for struct; see el_extent_array */
    new_dlp->el_type = MPI_DATATYPE_NULL; /* not valid for struct */

    new_dlp->loop_params.s_t.count = (DLOOP_Count) nr_basics + nr_derived;

    /* note: curpos points to first byte in "old dataloop" region of
     * newly allocated space.
     */

    for (i=0, loop_idx = 0; i < count; i++)
    {
        /* ignore type elements with a zero blklen */
        if (blklens[i] == 0) continue;

        if (!DLOOP_Handle_hasloop_macro(oldtypes[i]))
        {
            DLOOP_Dataloop *dummy_dlp;
            int dummy_sz, dummy_depth;

            /* LBs and UBs already taken care of -- skip them */
            if (oldtypes[i] == MPI_LB || oldtypes[i] == MPI_UB)
            {
                continue;
            }

            /* build a contig dataloop for this basic and point to that
             *
             * optimization:
             *
             * push the count (blklen) from the struct down into the
             * contig so we can process more at the leaf.
             */
            mpi_errno = MPID_Dataloop_create_contiguous(blklens[i],
                                                        oldtypes[i],
                                                        &dummy_dlp,
                                                        &dummy_sz,
                                                        &dummy_depth,
                                                        flag);

            if (mpi_errno != MPI_SUCCESS)
            {
                /* TODO: FREE ALLOCATED RESOURCES */
                return mpi_errno;
            }

            /* copy the new contig loop into place in the struct memory
             * region
             */
            MPID_Dataloop_copy(curpos, dummy_dlp, dummy_sz);
            new_dlp->loop_params.s_t.dataloop_array[loop_idx] = curpos;
            curpos = (DLOOP_Dataloop *) ((char *) curpos + dummy_sz);

            /* we stored the block size in the contig -- use 1 here */
            new_dlp->loop_params.s_t.blocksize_array[loop_idx] = 1;
            new_dlp->loop_params.s_t.el_extent_array[loop_idx] =
                ((DLOOP_Offset) blklens[i]) * dummy_dlp->el_extent;
            MPID_Dataloop_free(&dummy_dlp);
        }
        else
        {
            DLOOP_Dataloop *old_loop_ptr;
            DLOOP_Offset old_extent;

            DLOOP_Handle_get_loopptr_macro(oldtypes[i], old_loop_ptr, flag);
            DLOOP_Handle_get_loopsize_macro(oldtypes[i], old_loop_sz, flag);
            old_extent = DLOOP_Handle_get_extent_macro(oldtypes[i]);

            MPID_Dataloop_copy(curpos, old_loop_ptr, old_loop_sz);
            new_dlp->loop_params.s_t.dataloop_array[loop_idx] = curpos;
            curpos = (DLOOP_Dataloop *) ((char *) curpos + old_loop_sz);

            new_dlp->loop_params.s_t.blocksize_array[loop_idx] =
                (DLOOP_Count) blklens[i];
            new_dlp->loop_params.s_t.el_extent_array[loop_idx] =
                old_extent;
        }
        new_dlp->loop_params.s_t.offset_array[loop_idx] =
            (DLOOP_Offset) disps[i];
        loop_idx++;
    }

    *dlp_p     = new_dlp;
    *dlsz_p    = new_loop_sz;
    *dldepth_p = new_loop_depth;

    return MPI_SUCCESS;
}


/*@
   Dataloop_create_vector

   Arguments:
+  int icount
.  int iblocklength
.  MPI_Aint astride
.  int strideinbytes
.  MPI_Datatype oldtype
.  DLOOP_Dataloop **dlp_p
.  int *dlsz_p
.  int *dldepth_p
-  int flag

   Returns 0 on success, -1 on failure.

@*/
MPI_RESULT
MPID_Dataloop_create_vector(
    _In_                                int              icount,
    _In_                                int              iblocklength,
    _In_                                MPI_Aint         astride,
    _In_                                int              strideinbytes,
    _In_                                MPI_Datatype     oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    )
{
    MPI_RESULT mpi_errno;
    int is_builtin;
    int new_loop_sz, new_loop_depth;

    DLOOP_Count count, blocklength;
    DLOOP_Offset stride;
    DLOOP_Dataloop *new_dlp;

    count       = (DLOOP_Count) icount; /* avoid subsequent casting */
    blocklength = (DLOOP_Count) iblocklength;
    stride      = (DLOOP_Offset) astride;

    /* if count or blocklength are zero, handle with contig code,
     * call it a int
     */
    if (count == 0 || blocklength == 0)
    {

        return MPID_Dataloop_create_contiguous(0,
                                               MPI_INT,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    /* optimization:
     *
     * if count == 1, store as a contiguous rather than a vector dataloop.
     */
    if (count == 1)
    {
        return MPID_Dataloop_create_contiguous(iblocklength,
                                               oldtype,
                                               dlp_p,
                                               dlsz_p,
                                               dldepth_p,
                                               flag);
    }

    is_builtin = (DLOOP_Handle_hasloop_macro(oldtype)) ? 0 : 1;

    if (is_builtin)
    {
        new_loop_sz = sizeof(DLOOP_Dataloop);
        new_loop_depth = 1;
    }
    else
    {
        int old_loop_sz = 0, old_loop_depth = 0;

        DLOOP_Handle_get_loopsize_macro(oldtype, old_loop_sz, flag);
        DLOOP_Handle_get_loopdepth_macro(oldtype, old_loop_depth, flag);

        /* TODO: ACCOUNT FOR PADDING IN LOOP_SZ HERE */
        new_loop_sz = sizeof(DLOOP_Dataloop) + old_loop_sz;
        new_loop_depth = old_loop_depth + 1;
    }

    if (is_builtin)
    {
        DLOOP_Offset basic_sz = 0;

        mpi_errno = MPID_Dataloop_alloc(DLOOP_KIND_VECTOR,
                                        count,
                                        &new_dlp,
                                        &new_loop_sz);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        basic_sz = DLOOP_Handle_get_size_macro(oldtype);
        new_dlp->kind = DLOOP_KIND_VECTOR | DLOOP_FINAL_MASK;

        new_dlp->el_size   = basic_sz;
        new_dlp->el_extent = new_dlp->el_size;
        new_dlp->el_type   = oldtype;
    }
    else /* user-defined base type (oldtype) */
    {
        DLOOP_Dataloop *old_loop_ptr;
        int old_loop_sz = 0;

        DLOOP_Handle_get_loopptr_macro(oldtype, old_loop_ptr, flag);
        DLOOP_Handle_get_loopsize_macro(oldtype, old_loop_sz, flag);

        mpi_errno = MPID_Dataloop_alloc_and_copy(DLOOP_KIND_VECTOR,
                                                 count,
                                                 old_loop_ptr,
                                                 old_loop_sz,
                                                 &new_dlp,
                                                 &new_loop_sz);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        new_dlp->kind = DLOOP_KIND_VECTOR;
        new_dlp->el_size = DLOOP_Handle_get_size_macro(oldtype);
        new_dlp->el_extent = DLOOP_Handle_get_extent_macro(oldtype);
        new_dlp->el_type = DLOOP_Handle_get_basic_type_macro(oldtype);
    }

    /* vector-specific members
     *
     * stride stored in dataloop is always in bytes for local rep of type
     */
    new_dlp->loop_params.v_t.count     = count;
    new_dlp->loop_params.v_t.blocksize = blocklength;
    new_dlp->loop_params.v_t.stride    = (strideinbytes) ? stride :
        stride * new_dlp->el_extent;

    *dlp_p     = new_dlp;
    *dlsz_p    = new_loop_sz;
    *dldepth_p = new_loop_depth;

    return MPI_SUCCESS;
}
