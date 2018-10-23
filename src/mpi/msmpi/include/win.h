// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef WIN_H
#define WIN_H

//
// window lock states
//
typedef enum MPIDI_Win_Lock_state_t
{
    MPIDI_LOCKED_NOT_LOCKED,
    MPIDI_LOCKED_LOCK_REQUESTED,
    MPIDI_LOCKED_EXCLUSIVE = MPI_LOCK_EXCLUSIVE,
    MPIDI_LOCKED_SHARED = MPI_LOCK_SHARED,

} MPIDI_Win_Lock_state_t;

//
// operation that was called to lock the window
//
typedef enum MPIDI_Win_Lock_op_t
{
    MPIDI_WIN_NOT_LOCKED,
    MPIDI_WIN_LOCK_TARGET,
    MPIDI_WIN_LOCK_ALL
} MPIDI_Win_Lock_op_t;

//
// Information needed to use a window on shared memory.
//
struct MPIDI_WinShmInfo
{
    int m_processCount; // Number of processes that access shm

    bool m_bMemBlocksContig; // Indicates if mem blocks for different ranks are contiguous

    struct MappedShmInfo
    {
        void *m_pBase;  // Starting address of the mapped view for a process.
                        // m_pBase is null if shm hasn't been mapped
        MPI_Aint m_size;
        int m_dispUnit;
    } *m_pMappedShmInfo;

    //
    // Pair of source id-handle information + local handle
    // There is single ShmSrcInfo if memory is contig, m_processCount ShmSrcInfos otherwise
    //
    struct ShmSrcInfo
    {
        DWORD m_idSrcProcess;   // Id of process that created the shared mem
        HANDLE m_hSrcShm;       // Handle of shared mem in the creating process context
        HANDLE m_hDestShm;      // Handle of shm in the context of the process
    } *m_pShmSrcInfo;

    MPIDI_WinShmInfo()
    {
        m_pMappedShmInfo = nullptr;
        m_pShmSrcInfo = nullptr;
    }

    int Initialize(
        _In_ bool bContig,
        _In_ int nCommSize
        );

    void CleanUpMemory();

    void ReleaseHandles();

    inline int GetSrcInfoCount() const
    {
        return m_bMemBlocksContig ? 1 : m_processCount;
    }

    int MapView(_In_range_(>=, 0) int rank);

    MPI_RESULT
    GetMappedView(
        _In_range_(>=, 0) int rank,
        _Out_ MPI_Aint *pSize,
        _Out_ int *pDispUnit,
        _Outptr_ void **ppBase
        );

};

struct MPIDI_Win_t
{
    //
    // Type of memory where this window resides
    // False if memory for the window was allocated by the user
    // True if  memory was allocated as part of MPI_Win_allocate call, thus needs to be deleted
    //
    bool isAllocated;

    //
    // Type of memory where this window resides
    // False if window is local and rma should be over the channel
    // True if window is already on shared memory, processes can directly access
    //
    bool isShm;

    //
    // Valid only for shared memory windows
    //
    MPIDI_WinShmInfo shmInfo;

    //
    // completion counter for operations targeting this window
    //
    volatile int my_counter;

    //
    // array of handles to the window objects of all processes
    //
    MPI_Win *all_win_handles;

    //
    // list of outstanding RMA requests
    //
    struct MPIDI_RMA_ops *rma_ops_list;

    //
    // the type of lock that has been granted to this process (as source) for passive target rma
    // array of comm_ptr->size values
    //
    volatile int* remote_lock_state;

    //
    // true if RMA operations on target have been completed - for passive target rma
    // array of comm_ptr->size values
    //
    volatile int* remote_ops_done;

    //
    // operation used to lock the window
    //
    volatile MPIDI_Win_Lock_op_t lock_op_used;

    //
    // number of operations requested for each target
    // array of comm_ptr->size values
    //
    int* requested_ops_count;

    //
    // true if a error has been found on that window for passive target rma
    //
    volatile bool error_found;
    volatile MPI_RESULT rma_error;

    //
    // current lock state on this window (as target) (none, shared, exclusive)
    //
    volatile MPIDI_Win_Lock_state_t lock_state;

    //
    // shared look ref count
    //
    volatile int shared_lock_ref_cnt;

    //
    // list of unsatisfied locks
    //
    struct MPIDI_Win_lock_queue* lock_queue;

    //
    // array containing the nunber of passive target puts/accums issued from
    // this process to other processes.
    //
    int* pt_rma_puts_accs;

    //
    // no. of passive target puts/accums that this process has completed as target
    //
    volatile int my_pt_rma_puts_accs;
};


struct MPIDI_Win_memory_region_t
{
    MPI_Aint base;
    MPI_Aint size;
    MPIDI_Win_memory_region_t* next;
};


/* Windows */
/*S
  MPID_Win - Description of the Window Object data structure.

  Module:
  Win-DS

  Notes:
  The following 3 keyvals are defined for attributes on all MPI
  Window objects\:
.vb
 MPI_WIN_SIZE
 MPI_WIN_BASE
 MPI_WIN_DISP_UNIT
.ve
  These correspond to the values in 'length', 'start_address', and
  'disp_unit'.

  The communicator in the window is the same communicator that the user
  provided to 'MPI_Win_create' (not a dup).  However, each intracommunicator
  has a special context id that may be used if MPI communication is used
  by the implementation to implement the RMA operations.

  There is no separate window group; the group of the communicator should be
  used.

  Question:
  Should a 'MPID_Win' be defined after 'MPID_Segment' in case the device
  wants to
  store a queue of pending put/get operations, described with 'MPID_Segment'
  (or 'MPID_Request')s?

  S*/
struct MPID_Win
{
    int           handle;        /* value of MPI_Win for this structure */
    volatile long  ref_count;
    int fence_cnt;               /* 0 = no fence has been called; 1 = fence has been called */
    MPID_Errhandler *errhandler; /* Pointer to the error handler structure */
    void *base;
    MPI_Aint size;
    MPIDI_Win_memory_region_t *regions;
    int disp_unit;               /* Displacement unit of *local* window */
    MPID_Attribute *attributes;
    MPID_Group *start_group_ptr; /* group passed in MPI_Win_start */
    int start_assert;            /* assert passed to MPI_Win_start */
    MPID_Comm* comm_ptr;         /* communicator of window (dup) */
    MPI_LOCK winLock;
    int createFlavor;
    int winModel;

    /* These are COPIES of the values so that addresses to them
       can be returned as attributes.  They are initialized by the
       MPI_Win_get_attr function */
    int copyDispUnit;
    MPI_Aint copySize;
    void *copyBase;

    /* Other, device-specific information */
    MPIDI_Win_t dev;

    const char* name;

public:
    const char* GetDefaultName() const;
};

extern MPIU_Object_alloc_t MPID_Win_mem;
/* Preallocated win objects */
extern MPID_Win MPID_Win_direct[];


/*@
  MPID_Win_xxxx - One sided operations

  Input Parameter:

  Return Value:
  MPI error code.

  Notes:
  MPID Windows operations.

  Module:
  Request

  @*/

MPI_RESULT
MPID_Win_allocate_shared(
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_opt_ const MPID_Info *pInfo,
    _In_ const MPID_Comm *pComm,
    _Outptr_ void **ppBase,
    _Outptr_ MPID_Win **ppWin
    );

MPI_RESULT
MPID_Win_shared_query(
    _In_ MPID_Win *pWin,
    _In_range_(>, MPI_PROC_NULL) int rank,
    _Out_ MPI_Aint *pSize,
    _Out_ int *pDispUnit,
    _Outptr_ void **ppBase
    );

MPI_RESULT MPID_Win_fence(int assert, MPID_Win *win_ptr);

MPI_RESULT
MPID_Win_Put(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr
    );

MPI_RESULT
MPID_Win_Get(
    _Out_opt_ void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr
    );

MPI_RESULT
MPID_Win_Accumulate(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr
    );

MPI_RESULT
MPID_Win_Get_accumulate(
    _In_opt_ const void *origin_addr,
    _In_range_(>= , 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _Out_opt_ void *result_addr,
    _In_range_(>= , 0) int result_count,
    _In_ TypeHandle result_datatype,
    _In_range_(>= , 0) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_range_(>= , 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr
);

MPI_RESULT
MPID_Win_Rput(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
    );

MPI_RESULT
MPID_Win_Rget(
    _Out_opt_ void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
    );

MPI_RESULT
MPID_Win_Raccumulate(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
    );

MPI_RESULT
MPID_Win_Rget_accumulate(
    _In_opt_ const void *origin_addr,
    _In_range_(>= , 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _Out_opt_ void *result_addr,
    _In_range_(>= , 0) int result_count,
    _In_ TypeHandle result_datatype,
    _In_range_(>= , 0) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_range_(>= , 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
);

MPI_RESULT
MPID_Win_Compare_and_swap(
    _In_ const void *origin_addr,
    _In_ const void *compare_addr,
    _In_ void *result_addr,
    _In_ TypeHandle datatype,
    _In_range_(>= , 0) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_ MPID_Win *win_ptr
);

MPI_RESULT MPID_Win_free(MPID_Win*);
MPI_RESULT MPID_Win_test(const MPID_Win *win_ptr, int *flag);
MPI_RESULT MPID_Win_wait(const MPID_Win *win_ptr);
MPI_RESULT MPID_Win_complete(MPID_Win *win_ptr);
MPI_RESULT MPID_Win_post(const MPID_Group *group_ptr, int assert, MPID_Win *win_ptr);
MPI_RESULT MPID_Win_start(MPID_Group *group_ptr, int assert, MPID_Win *win_ptr);
MPI_RESULT MPID_Win_lock(int lock_type, int dest, int assert, MPID_Win *win_ptr);
MPI_RESULT MPID_Win_lock_all(int assert, MPID_Win *win_ptr);
MPI_RESULT MPID_Win_unlock(int dest, MPID_Win *win_ptr);
MPI_RESULT MPID_Win_unlock_all(MPID_Win *win_ptr);
MPI_RESULT MPID_Win_flush(int dest, MPID_Win *win_ptr, bool isLocal);
MPI_RESULT MPID_Win_flush_all(MPID_Win *win_ptr, bool isLocal);

//
// Functions that handle the creation of one sided communication window
//
MPI_RESULT
MPID_Win_create(
    _In_opt_ void *pBase,
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_opt_ const MPID_Info * pInfo,
    _In_ const MPID_Comm * pComm,
    _In_ bool isAllocated,
    _In_ bool isShm,
    _Outptr_ MPID_Win **ppWin
    );

MPI_RESULT
MPID_Win_create_noncontig(
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_ const MPID_Comm *pComm,
    _Inout_ MPID_Win *pWin
    );

MPI_RESULT
MPID_Win_create_contig(
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_ const MPID_Comm *pComm,
    _Inout_ MPID_Win *pWin
    );

MPI_RESULT
MPID_Win_allocate(
    _In_range_(>= , 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_opt_ MPID_Info *pInfo,
    _In_ const MPID_Comm *pComm,
    _Out_ void *ppBase,
    _Outptr_ MPID_Win **ppWin
    );

MPI_RESULT
MPID_Win_attach(
    _In_ MPID_Win *win_ptr,
    _In_ MPI_Aint address,
    _In_range_(>=, 0) MPI_Aint size
    );

MPI_RESULT
MPID_Win_detach(
    _In_ MPID_Win *win_ptr,
    _In_ MPI_Aint address
    );

/*
 * Good Memory (may be required for passive target operations on MPI_Win)
 */

/*@
  MPID_Alloc_mem - Allocate memory suitable for passive target RMA operations

  Input Parameter:
+ size - Number of types to allocate.
- info - Info object

  Return value:
  Pointer to the allocated memory.  If the memory is not available,
  returns null.

  Notes:
  This routine is used to implement 'MPI_Alloc_mem'.  It is for that reason
  that there is no communicator argument.

  This memory may `only` be freed with 'MPID_Free_mem'.

  This is a `local`, not a collective operation.  It functions more like a
  good form of 'malloc' than collective shared-memory allocators such as
  the 'shmalloc' found on SGI systems.

  Implementations of this routine may wish to use 'MPID_Memory_register'.
  However, this routine has slighly different requirements, so a separate
  entry point is provided.

  Question:
  Since this takes an info object, should there be an error routine in the
  case that the info object contains an error?

  Module:
  Win
  @*/
void *MPID_Alloc_mem( size_t size, MPID_Info *info );

/*@
  MPID_Free_mem - Frees memory allocated with 'MPID_Alloc_mem'

  Input Parameter:
. ptr - Pointer to memory allocated by 'MPID_Alloc_mem'.

  Return value:
  'MPI_SUCCESS' if memory was successfully freed; an MPI error code otherwise.

  Notes:
  The return value is provided because it may not be easy to validate the
  value of 'ptr' without attempting to free the memory.

  Module:
  Win
  @*/
MPI_RESULT MPID_Free_mem( void *ptr );


//
// Retrieves the value for a given built-in attribute, and returns
// whether the value was written to the pValue parameter.
//
_Success_( return == TRUE )
BOOL
MPID_Win_get_attr(
    _In_ MPID_Win* pWin,
    _In_ int hKey,
    _Outptr_result_maybenull_ void** pValue
    );

#endif // WIN_H
