// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.


/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef DATALOOP_H
#define DATALOOP_H

/* These following dataloop-specific types will be used throughout the DLOOP
 * instance:
 */
#define DLOOP_Offset     MPI_Aint
#define DLOOP_Count      int
#define DLOOP_Handle     MPI_Datatype
#define DLOOP_Type       MPI_Datatype
#define DLOOP_Buffer     void *

/* Redefine all of the internal structures in terms of the prefix */
#define MPID_Dataloop               DLOOP_Dataloop
#define MPID_Segment                DLOOP_Segment

/* These flags are used at creation time to specify what types of
 * optimizations may be applied. They are also passed in at Segment_init
 * time to specify which dataloop to use.
 *
 * Note: The flag to MPID_Segment_init() was originally simply "hetero"
 * and was a boolean value (0 meaning homogeneous). Some MPICH2 code
 * may still rely on HOMOGENEOUS being "0" and HETEROGENEOUS being "1".
 */
#define DLOOP_DATALOOP_HOMOGENEOUS   0
#define DLOOP_DATALOOP_HETEROGENEOUS 1

/* NOTE: ASSUMING LAST TYPE IS SIGNED */
#define SEGMENT_IGNORE_LAST ((DLOOP_Offset) -1)
/*
 * Each of the MPI datatypes can be mapped into one of 5 very simple
 * loops.  This loop has the following parameters:
 * - count
 * - blocksize[]
 * - offset[]
 * - stride
 * - datatype[]
 *
 * where each [] indicates that a field may be *either* an array or a scalar.
 * For each such type, we define a struct that describes these parameters
 */

/*S
  DLOOP_Dataloop_contig - Description of a contiguous dataloop

  Fields:
+ count - Number of elements
- dataloop - Dataloop of the elements

  Module:
  Datatype
  S*/
typedef struct DLOOP_Dataloop_contig
{
    DLOOP_Count count;
    struct DLOOP_Dataloop *dataloop;
} DLOOP_Dataloop_contig;

/*S
  DLOOP_Dataloop_vector - Description of a vector or strided dataloop

  Fields:
+ count - Number of elements
. blocksize - Number of dataloops in each element
. stride - Stride (in bytes) between each block
- dataloop - Dataloop of each element

  Module:
  Datatype
  S*/
typedef struct DLOOP_Dataloop_vector
{
    DLOOP_Count count;
    struct DLOOP_Dataloop *dataloop;
    DLOOP_Count blocksize;
    DLOOP_Offset stride;
} DLOOP_Dataloop_vector;

/*S
  DLOOP_Dataloop_blockindexed - Description of a block-indexed dataloop

  Fields:
+ count - Number of blocks
. blocksize - Number of elements in each block
. offset_array - Array of offsets (in bytes) to each block
. total_blocks - count of total blocks in the array (cached value)
- dataloop - Dataloop of each element

  Module:
  Datatype

  S*/
typedef struct DLOOP_Dataloop_blockindexed
{
    DLOOP_Count count;
    struct DLOOP_Dataloop *dataloop;
    DLOOP_Count blocksize;
    DLOOP_Offset *offset_array;
} DLOOP_Dataloop_blockindexed;

/*S
  DLOOP_Dataloop_indexed - Description of an indexed dataloop

  Fields:
+ count - Number of blocks
. blocksize_array - Array giving the number of elements in each block
. offset_array - Array of offsets (in bytes) to each block
. total_blocks - count of total blocks in the array (cached value)
- dataloop - Dataloop of each element

  Module:
  Datatype

  S*/
typedef struct DLOOP_Dataloop_indexed
{
    DLOOP_Count count;
    struct DLOOP_Dataloop *dataloop;
    DLOOP_Count *blocksize_array;
    DLOOP_Offset *offset_array;
    DLOOP_Count total_blocks;
} DLOOP_Dataloop_indexed;

/*S
  DLOOP_Dataloop_struct - Description of a structure dataloop

  Fields:
+ count - Number of blocks
. blocksize_array - Array giving the number of elements in each block
. offset_array - Array of offsets (in bytes) to each block
- dataloop_array - Array of dataloops describing the elements of each block

  Module:
  Datatype

  S*/
typedef struct DLOOP_Dataloop_struct
{
    DLOOP_Count count;
    struct DLOOP_Dataloop **dataloop_array;
    DLOOP_Count            *blocksize_array;
    DLOOP_Offset           *offset_array;
    DLOOP_Offset           *el_extent_array; /* need more than one */
} DLOOP_Dataloop_struct;

/* In many cases, we need the count and the next dataloop item. This
   common structure gives a quick access to both.  Note that all other
   structures must use the same ordering of elements.
   Question: should we put the pointer first in case
   sizeof(pointer)>sizeof(int) ?
*/
typedef struct DLOOP_Dataloop_common
{
    DLOOP_Count count;
    struct DLOOP_Dataloop *dataloop;
} DLOOP_Dataloop_common;

/*S
  DLOOP_Dataloop - Description of the structure used to hold a dataloop
  description

  Fields:
+  kind - Describes the type of the dataloop.  This is divided into three
   separate bit fields\:
.vb
     Dataloop type (e.g., DLOOP_CONTIG etc.).  3 bits
     IsFinal (a "leaf" dataloop; see text) 1 bit
     Element Size (units for fields.) 2 bits
        Element size has 4 values
        0   - Elements are in units of bytes
        1   - Elements are in units of 2 bytes
        2   - Elements are in units of 4 bytes
        3   - Elements are in units of 8 bytes
.ve
  The dataloop type is one of 'DLOOP_CONTIG', 'DLOOP_VECTOR',
  'DLOOP_BLOCKINDEXED', 'DLOOP_INDEXED', or 'DLOOP_STRUCT'.
. loop_parms - A union containing the 5 dataloop structures, e.g.,
  'DLOOP_Dataloop_contig', 'DLOOP_Dataloop_vector', etc.  A sixth element in
  this union, 'count', allows quick access to the shared 'count' field in the
  five dataloop structure.
. extent - The extent of the dataloop

  Module:
  Datatype

  S*/
typedef struct DLOOP_Dataloop
{
    int kind;                  /* Contains both the loop type
                                  (contig, vector, blockindexed, indexed,
                                  or struct) and a bit that indicates
                                  whether the dataloop is a leaf type. */
    union
    {
        DLOOP_Count                 count;
        DLOOP_Dataloop_contig       c_t;
        DLOOP_Dataloop_vector       v_t;
        DLOOP_Dataloop_blockindexed bi_t;
        DLOOP_Dataloop_indexed      i_t;
        DLOOP_Dataloop_struct       s_t;
        DLOOP_Dataloop_common       cm_t;
    } loop_params;
    DLOOP_Offset el_size; /* I don't feel like dealing with the bit manip.
                           * needed to get the packed size right at the moment.
                           */
    DLOOP_Offset el_extent;
    DLOOP_Type   el_type;
} DLOOP_Dataloop;

#define DLOOP_FINAL_MASK  0x00000008
#define DLOOP_KIND_MASK   0x00000007
#define DLOOP_KIND_CONTIG 0x1
#define DLOOP_KIND_VECTOR 0x2
#define DLOOP_KIND_BLOCKINDEXED 0x3
#define DLOOP_KIND_INDEXED 0x4
#define DLOOP_KIND_STRUCT 0x5

/* The max datatype depth is the maximum depth of the stack used to
   evaluate datatypes.  It represents the length of the chain of
   datatype dependencies.  Defining this and testing when a datatype
   is created removes a test in the datatype evaluation loop. */
#define DLOOP_MAX_DATATYPE_DEPTH 16

/*S
  DLOOP_Dataloop_stackelm - Structure for an element of the stack used
  to process dataloops

  Fields:
+ curcount - Current loop count value (between 0 and
             loop.loop_params.count-1)
. orig_count - original count value (cached so we don't have to look it up)
. curoffset - Offset into memory relative to the pointer to the buffer
              passed in by the user.  Used to maintain our position as we
              move up and down the stack.  NEED MORE NOTES ON THIS!!!
. orig_offset - original offset, set before the stackelm is processed, so that
                we know where the offset was.  this is used in processing indexed
                types and possibly others.  it is set for all types, but not
                referenced in some cases.
. curblock - Current block value...NEED MORE NOTES ON THIS!!!
. orig_block - original block value (caches so we don't have to look it up);
               INVALID FOR INDEX AND STRUCT TYPES.
- loop_p  - pointer to Loop-based description of the dataloop

S*/
typedef struct DLOOP_Dataloop_stackelm
{
    int may_require_reloading; /* indicates that items below might
                                * need reloading (e.g. this is a struct)
                                */

    DLOOP_Count  curcount;
    DLOOP_Offset curoffset;
    DLOOP_Count  curblock;

    DLOOP_Count  orig_count;
    DLOOP_Offset orig_offset;
    DLOOP_Count  orig_block;

    struct DLOOP_Dataloop *loop_p;
} DLOOP_Dataloop_stackelm;

/*S
  DLOOP_Segment - Description of the Segment datatype

  Notes:
  This has no corresponding MPI datatype.

  Module:
  Segment

  Questions:
  Should this have an id for allocation and similarity purposes?
  S*/
typedef struct DLOOP_Segment
{
    char *ptr; /* pointer to datatype buffer */
    DLOOP_Handle handle;
    DLOOP_Offset stream_off; /* next offset into data stream resulting from datatype
                              * processing.  in other words, how many bytes have
                              * we created/used by parsing so far?  that amount + 1.
                              */
    DLOOP_Dataloop_stackelm stackelm[DLOOP_MAX_DATATYPE_DEPTH];
    int  cur_sp;   /* Current stack pointer when using dataloop */
    int  valid_sp; /* maximum valid stack pointer.  This is used to
                      maintain information on the stack after it has
                      been placed there by following the datatype field
                      in a DLOOP_Dataloop_st for any type except struct */

    struct DLOOP_Dataloop builtin_loop; /* used for both predefined types (which
                                          * won't have a loop already) and for
                                  * situations where a count is passed in
                                  * and we need to create a contig loop
                                  * to handle it
                                  */
    /* other, device-specific information */
} DLOOP_Segment;

/* Dataloop functions (dataloop.c) */
void
MPID_Dataloop_update(
    _Inout_ DLOOP_Dataloop* dataloop,
    _In_    DLOOP_Offset    ptrdiff
    );


DLOOP_Offset
MPID_Dataloop_stream_size(
    _In_ const struct DLOOP_Dataloop *dl_p,
    _In_ DLOOP_Offset (*sizefn)(DLOOP_Type el_type)
    );


MPI_RESULT
MPID_Dataloop_dup(
    _In_reads_bytes_(old_loop_sz)       const DLOOP_Dataloop* old_loop,
    _In_                                int                   old_loop_sz,
    _Outptr_result_bytebuffer_(old_loop_sz) DLOOP_Dataloop**  new_loop_p
    );


_Success_(*dataloop == nullptr)
_At_(*dataloop,_Pre_maybenull_)
void
MPID_Dataloop_free(
    _Inout_ _Outptr_result_maybenull_ DLOOP_Dataloop** dataloop
    );


/* Segment functions (segment.c) */
DLOOP_Segment * MPID_Segment_alloc(void);

void MPID_Segment_free(DLOOP_Segment *segp);

MPI_RESULT
MPID_Segment_init(
    const DLOOP_Buffer buf,
    DLOOP_Count count,
    DLOOP_Handle handle,
    DLOOP_Segment *segp,
    int hetero
    );

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
    );

void MPID_Segment_pack(struct DLOOP_Segment *segp,
                                  DLOOP_Offset   first,
                                  DLOOP_Offset  *lastp,
                                  void *streambuf);
void MPID_Segment_unpack(struct DLOOP_Segment *segp,
                                    DLOOP_Offset   first,
                                    DLOOP_Offset  *lastp,
                                    const void *streambuf);


MPI_RESULT
MPID_Dataloop_create(
    _In_                                MPI_Datatype     type,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    );


MPI_RESULT
MPID_Dataloop_create_pairtype(
    _In_ MPI_Datatype type,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int               flag
    );

#endif // DATALOOP_H
