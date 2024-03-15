// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef COLL_H
#define COLL_H

/* ------------------------------------------------------------------------- */
/* FIXME: Also for mpicoll.h, in src/mpi/coll?  */
/* ------------------------------------------------------------------------- */
/* thresholds to switch between long and short vector algorithms for
   collective operations */
/* FIXME: Should there be a way to (a) update/compute these at configure time
   and (b) provide runtime control?  Should these be MPIR_xxx_DEFAULT
   instead? */

/*
   Expose the collective switch over points as environment variable.
   These defines become the default constants
*/
#define MPIR_BCAST_SHORT_MSG_DEFAULT                    524288
#define MPIR_BCAST_LONG_MSG_DEFAULT                     2097152
#define MPIR_BCAST_MIN_PROCS_DEFAULT                    8
#define MPIR_ALLTOALL_SHORT_MSG_DEFAULT                 4096
#define MPIR_ALLTOALL_MEDIUM_MSG_DEFAULT                16384
#define MPIR_ALLTOALLV_SHORT_MSG_DEFAULT                2048
#define MPIR_REDSCAT_COMMUTATIVE_LONG_MSG_DEFAULT       524288
#define MPIR_REDSCAT_NONCOMMUTATIVE_SHORT_MSG_DEFAULT   512
#define MPIR_ALLGATHER_SHORT_MSG_DEFAULT                32768
#define MPIR_ALLGATHER_LONG_MSG_DEFAULT                 524288
#define MPIR_REDUCE_SHORT_MSG_DEFAULT                   65536
#define MPIR_ALLREDUCE_SHORT_MSG_DEFAULT                262144
#define MPIR_SCATTER_VSMALL_MSG_DEFAULT                 2048
#define MPIR_SCATTER_SHORT_MSG_DEFAULT                  65536  /* for intercommunicator scatter */
#define MPIR_GATHER_VSMALL_MSG_DEFAULT                  1024
#define MPIR_GATHER_SHORT_MSG_DEFAULT                   2048   /* for intercommunicator gather */

#define MPIR_BCAST_SMP_THRESHOLD_DEFAULT                0
#define MPIR_BCAST_SMP_CEILING_DEFAULT                  INT_MAX
#define MPIR_REDUCE_SMP_THRESHOLD_DEFAULT               0
#define MPIR_REDUCE_SMP_CEILING_DEFAULT                 8192
#define MPIR_ALLREDUCE_SMP_THRESHOLD_DEFAULT            0
#define MPIR_ALLREDUCE_SMP_CEILING_DEFAULT              262144

#define MPIR_ALLTOALL_PAIR_ALGO_MSG_THRESHOLD_DEFAULT   262144
#define MPIR_ALLTOALL_PAIR_ALGO_PROCS_THRESHOLD_DEFAULT 64

#define _mpi_when_root_(_comm, _root, _annot)                               \
    _When_(                                                                 \
        _root == MPI_ROOT && _comm->comm_kind == MPID_INTERCOMM, _annot     \
        )                                                                   \
    _When_(                                                                 \
        _comm->rank != MPI_PROC_NULL && _root == _comm->rank && _comm->comm_kind != MPID_INTERCOMM, _annot  \
        )


#define _mpi_when_not_root_(_comm, _root, _annot)                           \
    _When_(                                                                 \
        _root > 0 && _comm->comm_kind == MPID_INTERCOMM, _annot             \
        )                                                                   \
    _When_(                                                                 \
        _root != _comm->rank && _comm->comm_kind != MPID_INTERCOMM, _annot  \
        )


//
// These functions are used in the implementation of collective
// operations. They are wrappers around MPID send/recv functions. They do
// sends/receives by setting the communicator type bits in the tag.
//
MPI_RESULT
MPIC_Send(
    _In_opt_ const void*       buf,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, comm_ptr->remote_size - 1)
             int               dest,
    _In_     int               tag,
    _In_     const MPID_Comm*  comm_ptr
    );


MPI_RESULT
MPIC_Recv(
    _Out_opt_ void*            buf,
    _In_range_(>=, 0)
             int               count,
    _In_      TypeHandle       datatype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              source,
    _In_      int              tag,
    _In_      const MPID_Comm* comm_ptr,
    _Out_     MPI_Status*      status
    );


MPI_RESULT
MPIC_Sendrecv(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              dest,
    _In_      int              sendtag,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              source,
    _In_      int              recvtag,
    _In_      const MPID_Comm* comm_ptr,
    _Out_     MPI_Status*      status
    );


MPI_RESULT
MPIR_Localcopy(
    _In_opt_          const void*      sendbuf,
    _In_range_(>=, 0) int              sendcount,
    _In_              TypeHandle       sendtype,
    _Out_opt_         void*            recvbuf,
    _In_range_(>=, 0) int              recvcount,
    _In_              TypeHandle       recvtype
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Bcast_intra(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
             void              *buffer,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, comm_ptr->remote_size - 1)
             int               root,
    _In_     const MPID_Comm*  comm_ptr
    );


//
// Collective tuner needs access to the non-HA version for comparisson.
//
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Bcast_intra_flat(
    _When_(root == comm_ptr->rank, _In_opt_)
    _When_(root != comm_ptr->rank, _Out_opt_)
             BYTE*             buffer,
    _In_range_(>=, 0)
             MPIDI_msg_sz_t    nbytes,
    _In_range_(0, comm_ptr->remote_size - 1)
             int               root,
    _In_     const MPID_Comm*  comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Bcast_inter(
    _When_(root == MPI_ROOT, _In_opt_)
    _When_(root >= 0, _Out_opt_)
            void               *buffer,
    _In_range_(>=, 0)
            int                count,
    _In_    TypeHandle         datatype,
    _mpi_coll_rank_(root)
            int                root,
    _In_    const MPID_Comm*   comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IbcastBuildIntra(
    _When_(root == pComm->rank, _In_opt_)
    _When_(root != pComm->rank, _Out_opt_)
             void              *buffer,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, pComm->remote_size - 1)
             int               root,
    _In_     MPID_Comm*        pComm,
    _Outptr_
             MPID_Request**    ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibcast_intra(
    _When_(root == pComm->rank, _In_opt_)
    _When_(root != pComm->rank, _Out_opt_)
             void              *buffer,
    _In_range_(>=, 0)
             int               count,
    _In_     TypeHandle        datatype,
    _In_range_(0, pComm->remote_size - 1)
             int               root,
    _In_     MPID_Comm*        pComm,
    _Outptr_
             MPID_Request**    ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
IbcastBuildInter(
    _When_(root == MPI_ROOT, _In_opt_)
    _When_(root >= 0, _Out_opt_)
            void               *buffer,
    _In_range_(>=, 0)
            int                count,
    _In_    TypeHandle         datatype,
    _mpi_coll_rank_(root)
            int                root,
    _In_    MPID_Comm*         pComm,
    _Outptr_
            MPID_Request**     ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibcast_inter(
    _When_(root == MPI_ROOT, _In_opt_)
    _When_(root >= 0, _Out_opt_)
            void               *buffer,
    _In_range_(>=, 0)
            int                count,
    _In_     TypeHandle        datatype,
    _mpi_coll_rank_(root)
            int                root,
    _In_    MPID_Comm*         pComm,
    _Outptr_
            MPID_Request**     ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IscatterBuildIntra(
    _When_(root == pComm->rank, _In_opt_)
             const void*     sendbuf,
    _When_(root == pComm->rank, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(((pComm->rank == root && sendcnt != 0) || (pComm->rank != root && recvcnt != 0)) || _Old_(recvbuf) != MPI_IN_PLACE, _Out_opt_)
             void*           recvbuf,
    _When_(root != pComm->rank || recvbuf != MPI_IN_PLACE, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _In_range_(0, pComm->remote_size - 1)
             int             root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    tag,
    _Outptr_ MPID_Request**  ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iscatter_intra(
    _When_(root == pComm->rank, _In_opt_)
             const void*     sendbuf,
    _When_(root == pComm->rank, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(root != pComm->rank || _Old_(recvbuf) != MPI_IN_PLACE, _Out_opt_)
             void*           recvbuf,
    _When_(root != pComm->rank || recvbuf != MPI_IN_PLACE, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _In_range_(0, pComm->remote_size - 1)
             int             root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    tag,
    _Outptr_ MPID_Request**  ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Iscatter_inter(
    _When_(root == MPI_ROOT, _In_opt_)
             const void*     sendbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
             int             sendcnt,
    _In_     TypeHandle      sendtype,
    _When_(root >= 0, _Out_opt_)
             void*           recvbuf,
    _When_(root >= 0, _In_range_(>=, 0))
             int             recvcnt,
    _In_     TypeHandle      recvtype,
    _mpi_coll_rank_(root)
             int             root,
    _In_     MPID_Comm*      pComm,
    _In_     unsigned int    tag,
    _Outptr_ MPID_Request**  ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Gather_intra(
    _mpi_when_not_root_(comm_ptr, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_not_root_(comm_ptr, root, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(comm_ptr, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(comm_ptr, root, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Gather_inter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root >= 0, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root) int  root,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IgatherBuildIntra(
    _mpi_when_not_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_not_root_(pComm, root, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(pComm, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(pComm, root, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _In_range_(0, pComm->remote_size - 1)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Igather_intra(
    _mpi_when_not_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_not_root_(pComm, root, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(pComm, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(pComm, root, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _In_range_(0, pComm->remote_size - 1)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Igather_inter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root >= 0, _In_range_(>=, 0))
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _When_(root == MPI_ROOT, _In_range_(>=, 0))
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


MPI_RESULT
MPIR_Gatherv(
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root == comm_ptr->rank && _Old_(sendbuf) != MPI_IN_PLACE && sendcnt >= 0, _In_opt_)
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root != comm_ptr->rank && sendcnt >= 0, _In_opt_)
    _When_(comm_ptr->comm_kind == MPID_INTERCOMM && root > 0 && sendcnt >= 0, _In_opt_)
              const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(comm_ptr, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(comm_ptr, root, _In_)
              const int        recvcnts[],
    _mpi_when_root_(comm_ptr, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    );


MPI_RESULT
IgathervBuildBoth(
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(sendbuf) != MPI_IN_PLACE && sendcnt >= 0, _In_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && sendcnt >= 0, _In_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && sendcnt >= 0, _In_opt_)
              const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(pComm, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        recvcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


MPI_RESULT
MPIR_Igatherv(
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(sendbuf) != MPI_IN_PLACE && sendcnt >= 0, _In_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && sendcnt >= 0, _In_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && sendcnt >= 0, _In_opt_)
              const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcnt,
    _In_      TypeHandle       sendtype,
    _mpi_when_root_(pComm, root, _Out_opt_)
              void*            recvbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        recvcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_intra(
    _In_opt_  const void*      sendbuf,
    _When_(root == comm_ptr->rank, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    );


//
// Collective tuner needs access to the non-HA version for comparisson.
//
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_intra_flat(
    _In_opt_  const void*      sendbuf,
    _When_(root == comm_ptr->rank, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_range_(0, comm_ptr->remote_size - 1)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_inter(
    _When_(root > 0, _In_opt_)
              const void*      sendbuf,
    _When_( root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _mpi_coll_rank_(root) int  root,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IreduceBuildIntra(
    _In_opt_  const void*      sendbuf,
    _When_( root == pComm->rank, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >= , 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_range_( 0, pComm->remote_size - 1 )
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ireduce_intra(
    _In_opt_  const void*      sendbuf,
    _When_(root == pComm->rank, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_range_(0, pComm->remote_size - 1)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
IreduceBuildInter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _When_( root != MPI_PROC_NULL, _In_ )
              MPID_Op*         pOp,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ireduce_inter(
    _When_(root >= 0, _In_opt_)
              const void*      sendbuf,
    _When_(root == MPI_ROOT, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _When_( root != MPI_PROC_NULL, _In_ )
              MPID_Op*         pOp,
    _mpi_coll_rank_(root) int  root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


MPI_RESULT
MPIR_Scatterv(
    _mpi_when_root_(comm_ptr, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_root_(comm_ptr, root, _In_)
              const int        sendcnts[],
    _mpi_when_root_(comm_ptr, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       sendtype,
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root == comm_ptr->rank && _Old_(recvbuf) != MPI_IN_PLACE && recvcnt > 0, _Out_opt_)
    _When_(comm_ptr->comm_kind != MPID_INTERCOMM && root != comm_ptr->rank && recvcnt > 0, _Out_opt_)
    _When_(comm_ptr->comm_kind == MPID_INTERCOMM && root > 0 && recvcnt > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      const MPID_Comm* comm_ptr
    );


MPI_RESULT
IscattervBuildBoth(
    _mpi_when_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        sendcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       sendtype,
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(recvbuf) != MPI_IN_PLACE && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && recvcnt > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


MPI_RESULT
MPIR_Iscatterv(
    _mpi_when_root_(pComm, root, _In_opt_)
              const void*      sendbuf,
    _mpi_when_root_(pComm, root, _In_)
              const int        sendcnts[],
    _mpi_when_root_(pComm, root, _In_opt_)
              const int        displs[],
    _In_      TypeHandle       sendtype,
    _When_(pComm->comm_kind != MPID_INTERCOMM && root == pComm->rank && _Old_(recvbuf) != MPI_IN_PLACE && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind != MPID_INTERCOMM && root != pComm->rank && recvcnt > 0, _Out_opt_)
    _When_(pComm->comm_kind == MPID_INTERCOMM && root > 0 && recvcnt > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcnt,
    _In_      TypeHandle       recvtype,
    _mpi_coll_rank_(root)
              int              root,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Barrier_intra(
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Barrier_inter(
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IbarrierBuildIntra(
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibarrier_intra(
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ibarrier_inter(
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_block_intra(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_intra(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_block_inter(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    );


MPI_RESULT
MPIR_Ireduce_scatter_block(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      MPID_Comm*       comm_ptr,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Reduce_scatter_inter(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ireduce_scatter_intra(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ireduce_scatter_inter(
    _In_opt_  const void*      sendbuf,
    _Out_opt_ void*            recvbuf,
    _In_      const int*       recvcnts,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allreduce_intra(
    _In_opt_  const BYTE*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              BYTE*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    );


//
// Collective tuner needs access to the non-HA version for comparisson.
//
_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allreduce_intra_flat(
    _In_opt_  const BYTE*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              BYTE*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Allreduce_inter(
    _In_opt_  const BYTE*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              BYTE*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iallreduce_intra(
    _In_opt_  const void*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              void*            recvbuf,
    _In_range_( >=, 0 )
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Iallreduce_inter(
    _In_opt_  const void*      sendbuf,
    _When_( count > 0, _Out_opt_ )
              void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      MPID_Op*         pOp,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoall_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _In_      unsigned int     tag,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoall_inter(
    _In_opt_    const void*      sendbuf,
    _In_range_(>=, 0)
                int              sendcount,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_range_(>=, 0)
                int              recvcount,
    _In_        TypeHandle       recvtype,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
IalltoallvBuildIntra(
    _In_opt_    const void*      sendbuf,
    _In_        const int*       sendcnts,
    _In_        const int*       sdispls,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_        const int*       recvcnts,
    _In_        const int*       rdispls,
    _In_        TypeHandle       recvtype,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
IalltoallvBuildInter(
    _In_opt_    const void*      sendbuf,
    _In_opt_    const int*       sendcnts,
    _In_opt_    const int*       sdispls,
    _In_        TypeHandle       sendtype,
    _Inout_opt_ void*            recvbuf,
    _In_opt_    const int*       recvcnts,
    _In_opt_    const int*       rdispls,
    _In_        TypeHandle       recvtype,
    _In_        MPID_Comm*       pComm,
    _In_        unsigned int     tag,
    _Outptr_    MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgather_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgather_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _When_(recvcount > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iallgather_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Iallgather_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _When_(recvcount > 0, _Out_opt_)
              void*            recvbuf,
    _In_range_(>=, 0)
              int              recvcount,
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgatherv_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Allgatherv_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _Out_opt_ void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      const MPID_Comm* comm_ptr
    );


_Pre_satisfies_(pComm->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iallgatherv_intra(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(pComm->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Iallgatherv_inter(
    _In_opt_  const void*      sendbuf,
    _In_range_(>=, 0)
              int              sendcount,
    _In_      TypeHandle       sendtype,
    _In_opt_  void*            recvbuf,
    _In_      const int        recvcounts[],
    _In_      const int        displs[],
    _In_      TypeHandle       recvtype,
    _In_      MPID_Comm*       pComm,
    _Outptr_  MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallv_intra(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        TypeHandle         sendtype,
    _Inout_opt_ void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        TypeHandle         recvtype,
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallv_inter(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        TypeHandle         sendtype,
    _Inout_opt_ void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        TypeHandle         recvtype,
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind == MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallw_intra(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        const TypeHandle   sendtypes[],
    _Out_opt_   void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        const TypeHandle   recvtypes[],
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Ialltoallw_inter(
    _In_opt_    const void*        sendbuf,
    _In_        const int          sendcnts[],
    _In_        const int          sdispls[],
    _In_        const TypeHandle   sendtypes[],
    _Out_opt_   void*              recvbuf,
    _In_        const int          recvcnts[],
    _In_        const int          rdispls[],
    _In_        const TypeHandle   recvtypes[],
    _In_        const MPID_Comm*   comm_ptr,
    _In_        unsigned int       tag,
    _Outptr_    MPID_Request**     ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iscan(
    _In_opt_    const void*      sendbuf,
    _Inout_opt_ void*            recvbuf,
    _In_range_(>=, 0)
                int              count,
    _In_        TypeHandle       datatype,
    _In_        const MPID_Op*   pOp,
    _In_        MPID_Comm*       comm_ptr,
    _In_        int              tag,
    _Outptr_    MPID_Request**   ppRequest
    );


_Pre_satisfies_(comm_ptr->comm_kind != MPID_INTERCOMM)
MPI_RESULT
MPIR_Iexscan(
    _In_opt_  const void*      sendbuf,
    _In_opt_  void*            recvbuf,
    _In_range_(>=, 0)
              int              count,
    _In_      TypeHandle       datatype,
    _In_      const MPID_Op*   pOp,
    _In_      MPID_Comm*       comm_ptr,
    _In_      int              tag,
    _Outptr_  MPID_Request**   ppRequest
    );

#endif // COLL_H
