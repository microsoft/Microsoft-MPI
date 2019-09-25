// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "mpiimpl.h"
#include "mpi_fortlogical.h"
#include "nmpi.h"
#include "mpifbind.h"

#if defined(__cplusplus)
extern "C" {
#endif

#pragma warning(disable:4100)


static void MPIAPI MPIR_Comm_errhandler_f_proxy(MPI_Comm_errhandler_fn* fn, MPI_Comm* comm, int* errcode, ...);

//
// MPI_INIT
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_init__ ( MPI_Fint *ierr );

void FORT_CALL mpi_init_ ( MPI_Fint *ierr )
{
    mpi_init__( ierr );
}

//
// PMPI_INIT
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_init__ ( MPI_Fint *ierr );

void FORT_CALL pmpi_init_ ( MPI_Fint *ierr )
{
    pmpi_init__( ierr );
}

//
// MPI_INIT_THREAD
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_init_thread__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL mpi_init_thread_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_init_thread__( v1, v2, ierr );
}

//
// PMPI_INIT_THREAD
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_init_thread__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL pmpi_init_thread_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_init_thread__( v1, v2, ierr );
}

//
// MPI_SEND
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_send__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL mpi_send_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_send__(v1,v2,v3,v4,v5,v6,ierr);
}

//
// PMPI_SEND
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_send__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL pmpi_send_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_send__(v1,v2,v3,v4,v5,v6,ierr);
}

//
// MPI_RECV
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_recv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_recv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_recv__(v1,v2,v3,v4,v5,v6,v7,ierr);
}

//
// PMPI_RECV
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_recv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_recv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_recv__(v1,v2,v3,v4,v5,v6,v7,ierr);
}

//
// MPI_GET_COUNT
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_count__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr );

void FORT_CALL mpi_get_count_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_get_count__(v1,v2,v3,ierr);
}

//
// PMPI_GET_COUNT
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_count__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr );

void FORT_CALL pmpi_get_count_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_get_count__(v1,v2,v3,ierr);
}

//
// MPI_BSEND
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_bsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL mpi_bsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_bsend__( v1, v2, v3, v4, v5, v6, ierr );
}

//
// PMPI_BSEND
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_bsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL pmpi_bsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_bsend__( v1, v2, v3, v4, v5, v6, ierr );
}

//
// MPI_SSEND
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ssend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL mpi_ssend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_ssend__( v1, v2, v3, v4, v5, v6, ierr );
}

//
// PMPI_SSEND
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ssend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL pmpi_ssend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_ssend__( v1, v2, v3, v4, v5, v6, ierr );
}

//
// MPI_RSEND
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_rsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL mpi_rsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_rsend__( v1, v2, v3, v4, v5, v6, ierr );
}

//
// PMPI_RSEND
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_rsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL pmpi_rsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_rsend__( v1, v2, v3, v4, v5, v6, ierr );
}

//
//MPI_BUFFER_ATTACH
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_buffer_attach__ ( void*v1, const MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL mpi_buffer_attach_ ( void*v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_buffer_attach__( v1, v2, ierr );
}

//
//PMPI_BUFFER_ATTACH
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_buffer_attach__ ( void*v1, const MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL pmpi_buffer_attach_ ( void*v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_buffer_attach__( v1, v2, ierr );
}

//
// mpi_buffer_detach
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_buffer_detach__ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL mpi_buffer_detach_ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_buffer_detach__( v1, v2, ierr );
}

//
// pmpi_buffer_detach
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_buffer_detach__ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL pmpi_buffer_detach_ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_buffer_detach__( v1, v2, ierr );
}

//
// mpi_isend
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_isend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_isend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_isend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
// pmpi_isend
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_isend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_isend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_isend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
// mpi_ibsend
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ibsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_ibsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_ibsend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
// pmpi_ibsend
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ibsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_ibsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_ibsend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//mpi_issend
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_issend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_issend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_issend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//pmpi_issend
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_issend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_issend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_issend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//mpi_irsend
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_irsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_irsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_irsend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//pmpi_irsend
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_irsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_irsend_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_irsend__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//mpi_irecv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_irecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_irecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_irecv__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//pmpi_irecv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_irecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_irecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_irecv__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
// mpi_wait
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_wait__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL mpi_wait_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_wait__( v1, v2, ierr );
}

//
// pmpi_wait
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_wait__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL pmpi_wait_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_wait__( v1, v2, ierr );
}

//
//mpi_test
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_test__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr );

void FORT_CALL mpi_test_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_test__( v1, v2, v3, ierr );
}

//
//pmpi_test
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_test__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr );

void FORT_CALL pmpi_test_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_test__( v1, v2, v3, ierr );
}

//
//mpi_request_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_request_free__ ( MPI_Fint *v1, MPI_Fint *ierr );

void FORT_CALL mpi_request_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_request_free__( v1, ierr );
}

//
//pmpi_request_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_request_free__ ( MPI_Fint *v1, MPI_Fint *ierr );

void FORT_CALL pmpi_request_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_request_free__( v1, ierr );
}

//
//mpi_waitany
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_waitany__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr );

void FORT_CALL mpi_waitany_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_waitany__( v1, v2,  v3, v4, ierr );
}

//
//pmpi_waitany
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_waitany__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr );

void FORT_CALL pmpi_waitany_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_waitany__( v1, v2,  v3, v4, ierr );
}

//
//mpi_testany
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_testany__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL mpi_testany_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_testany__( v1, v2,  v3, v4, v5, ierr );
}

//
//pmpi_testany
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_testany__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL pmpi_testany_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_testany__( v1, v2,  v3, v4, v5, ierr );
}

//
//mpi_waitall
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_waitall__ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, MPI_Fint *ierr );

void FORT_CALL mpi_waitall_ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_waitall__( v1, v2, v3, ierr );
}

//
//pmpi_waitall
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_waitall__ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, MPI_Fint *ierr );

void FORT_CALL pmpi_waitall_ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_waitall__( v1, v2, v3, ierr );
}

//
// mpi_testall
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_testall__ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *ierr );

void FORT_CALL mpi_testall_ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_testall__( v1, v2, v3, v4, ierr );
}

//
// pmpi_testall
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_testall__ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *ierr );

void FORT_CALL pmpi_testall_ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_testall__( v1, v2, v3, v4, ierr );
}

//
// mpi_waitsome
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_waitsome__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL mpi_waitsome_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_waitsome__( v1, v2, v3, v4, v5, ierr );
}

//
// pmpi_waitsome
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_waitsome__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL pmpi_waitsome_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_waitsome__( v1, v2, v3, v4, v5, ierr );
}

//
//mpi_testsome
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_testsome__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL mpi_testsome_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_testsome__( v1, v2, v3, v4, v5, ierr );
}

//
//pmpi_testsome
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_testsome__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL pmpi_testsome_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_testsome__( v1, v2, v3, v4, v5, ierr );
}

//
//mpi_iprobe
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iprobe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL mpi_iprobe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_iprobe__( v1, v2, v3, v4, v5, ierr );
}

//
//pmpi_iprobe
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iprobe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL pmpi_iprobe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_iprobe__( v1, v2, v3, v4, v5, ierr );
}

//
// mpi_probe
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_probe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr );

void FORT_CALL mpi_probe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_probe__( v1, v2, v3, v4, ierr );
}

//
// pmpi_probe
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_probe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr );

void FORT_CALL pmpi_probe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_probe__( v1, v2, v3, v4, ierr );
}

//
// mpi_improbe
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_improbe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL mpi_improbe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_improbe__( v1, v2, v3, v4, v5, v6, ierr );
}

//
// pmpi_improbe
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_improbe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL pmpi_improbe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_improbe__( v1, v2, v3, v4, v5, v6, ierr );
}

//
// mpi_mprobe
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_mprobe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL mpi_mprobe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_mprobe__( v1, v2, v3, v4, v5, ierr );
}

//
// pmpi_mprobe
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_mprobe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL pmpi_mprobe_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_mprobe__( v1, v2, v3, v4, v5, ierr );
}

//
// mpi_mrecv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_mrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL mpi_mrecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_mrecv__( v1, v2, v3, v4, v5, ierr );
}

//
// pmpi_mrecv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_mrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL pmpi_mrecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_mrecv__( v1, v2, v3, v4, v5, ierr );
}

//
// mpi_imrecv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_imrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL mpi_imrecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_imrecv__( v1, v2, v3, v4, v5, ierr );
}

//
// pmpi_imrecv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_imrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr );

void FORT_CALL pmpi_imrecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_imrecv__( v1, v2, v3, v4, v5, ierr );
}

//
// mpi_cancel
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cancel__ ( MPI_Fint *v1, MPI_Fint *ierr );

void FORT_CALL mpi_cancel_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_cancel__( v1, ierr );
}

//
// pmpi_cancel
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cancel__ ( MPI_Fint *v1, MPI_Fint *ierr );

void FORT_CALL pmpi_cancel_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_cancel__( v1, ierr );
}

//
// mpi_test_cancelled
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_test_cancelled__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL mpi_test_cancelled_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_test_cancelled__( v1, v2, ierr );
}

//
// pmpi_test_cancelled
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_test_cancelled__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void FORT_CALL pmpi_test_cancelled_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_test_cancelled__( v1, v2, ierr );
}

//
//mpi_send_init
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_send_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_send_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_send_init__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//pmpi_send_init
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_send_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_send_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_send_init__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//mpi_bsend_init
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_bsend_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_bsend_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_bsend_init__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//pmpi_bsend_init
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_bsend_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_bsend_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_bsend_init__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//mpi_ssend_init_
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ssend_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL mpi_ssend_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_ssend_init__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
//pmpi_ssend_init_
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ssend_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr );

void FORT_CALL pmpi_ssend_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_ssend_init__( v1, v2, v3, v4, v5, v6, v7, ierr );
}

//
// mpi_rsend_init
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_rsend_init__ ( _Out_cap_(*v5) void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_rsend_init_ ( _Out_cap_(*v5) void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_rsend_init__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_rsend_init
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_rsend_init__ ( _Out_cap_(*v5) void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_rsend_init_ ( _Out_cap_(*v5) void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_rsend_init__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_recv_init
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_recv_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_recv_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_recv_init__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_recv_init
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_recv_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_recv_init_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_recv_init__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_start
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_start__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_start_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_start__ (v1, ierr);
}

//
// pmpi_start
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_start__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_start_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_start__ (v1, ierr);
}

//
// mpi_startall
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_startall__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_startall_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_startall__ (v1, v2, ierr);
}

//
// pmpi_startall
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_startall__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_startall_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_startall__ (v1, v2, ierr);
}

//
// mpi_sendrecv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_sendrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, void*v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, MPI_Fint *v12, MPI_Fint *ierr ) ;

void FORT_CALL mpi_sendrecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, void*v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, MPI_Fint *v12, MPI_Fint *ierr )
{
    mpi_sendrecv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, ierr);
}

//
// pmpi_sendrecv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_sendrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, void*v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, MPI_Fint *v12, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_sendrecv_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, void*v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, MPI_Fint *v12, MPI_Fint *ierr )
{
    pmpi_sendrecv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, ierr);
}

//
// mpi_sendrecv_replace
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_sendrecv_replace__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_sendrecv_replace_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_sendrecv_replace__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_sendrecv_replace
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_sendrecv_replace__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_sendrecv_replace_ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_sendrecv_replace__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_type_contiguous
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_contiguous__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_contiguous_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_type_contiguous__ (v1, v2, v3, ierr);
}

//
// pmpi_type_contiguous
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_contiguous__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_contiguous_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_type_contiguous__ (v1, v2, v3, ierr);
}

//
// mpi_type_vector
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_vector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_vector_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_vector__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_vector
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_vector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_vector_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_vector__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_hvector
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_hvector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_hvector_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_hvector__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_hvector
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_hvector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_hvector_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_hvector__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_indexed
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_indexed__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_indexed_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_indexed__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_indexed
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_indexed__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_indexed_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_indexed__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_hindexed
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_hindexed__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_hindexed_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_hindexed__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_hindexed
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_hindexed__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_hindexed_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_hindexed__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_struct
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_struct__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_struct_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_struct__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_struct
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_struct__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_struct_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, const MPI_Fint * v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_struct__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_extent
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_extent__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_extent_ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    mpi_type_extent__ (v1, v2, ierr);
}

//
// pmpi_type_extent
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_extent__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_extent_ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    pmpi_type_extent__ (v1, v2, ierr);
}

//
// mpi_type_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_type_size__ (v1, v2, ierr);
}

//
// pmpi_type_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_type_size__ (v1, v2, ierr);
}

//
// mpi_type_size_x
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_size_x__ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_size_x_ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Fint *ierr )
{
    mpi_type_size_x__ (v1, v2, ierr);
}

//
// pmpi_type_size_x
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_size_x__ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_size_x_ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Fint *ierr )
{
    pmpi_type_size_x__ (v1, v2, ierr);
}

//
// mpi_type_lb
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_lb__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_lb_ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    mpi_type_lb__ (v1, v2, ierr);
}

//
// pmpi_type_lb
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_lb__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_lb_ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    pmpi_type_lb__ (v1, v2, ierr);
}

//
// mpi_type_ub
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_ub__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_ub_ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    mpi_type_ub__ (v1, v2, ierr);
}

//
// pmpi_type_ub
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_ub__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_ub_ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    pmpi_type_ub__ (v1, v2, ierr);
}

//
// mpi_type_commit
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_commit__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_commit_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_type_commit__ (v1, ierr);
}

//
// pmpi_type_commit
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_commit__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_commit_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_type_commit__ (v1, ierr);
}

//
// mpi_type_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_type_free__ (v1, ierr);
}

//
// pmpi_type_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_type_free__ (v1, ierr);
}

//
// mpi_get_elements
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_elements__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_get_elements_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_get_elements__ (v1, v2, v3, ierr);
}

//
// pmpi_get_elements
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_elements__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_get_elements_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_get_elements__ (v1, v2, v3, ierr);
}

//
// mpi_get_elements_x
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_elements_x__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_get_elements_x_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Count *v3, MPI_Fint *ierr )
{
    mpi_get_elements_x__ (v1, v2, v3, ierr);
}

//
// pmpi_get_elements_x
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_elements_x__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_get_elements_x_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Count *v3, MPI_Fint *ierr )
{
    pmpi_get_elements_x__ (v1, v2, v3, ierr);
}

//
// mpi_pack
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_pack__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v5) void *v4, const MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_pack_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v5) void *v4, const MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_pack__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_pack
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_pack__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v5) void *v4, const MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_pack_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v5) void *v4, const MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_pack__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_unpack
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_unpack__ ( _In_reads_bytes_(*v2) void *v1, const MPI_Fint *v2, MPI_Fint *v3, void*v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_unpack_ ( _In_reads_bytes_(*v2) void *v1, const MPI_Fint *v2, MPI_Fint *v3, void*v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_unpack__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_unpack
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_unpack__ ( _In_reads_bytes_(*v2) void *v1, const MPI_Fint *v2, MPI_Fint *v3, void*v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_unpack_ ( _In_reads_bytes_(*v2) void *v1, const MPI_Fint *v2, MPI_Fint *v3, void*v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_unpack__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_pack_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_pack_size__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_pack_size_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_pack_size__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_pack_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_pack_size__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_pack_size_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_pack_size__ (v1, v2, v3, v4, ierr);
}

//
// mpi_barrier
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_barrier__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_barrier_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_barrier__ (v1, ierr);
}

//
// pmpi_barrier
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_barrier__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_barrier_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_barrier__ (v1, ierr);
}

//
// mpi_bcast
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_bcast__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_bcast_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_bcast__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_bcast
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_bcast__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_bcast_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_bcast__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_gather
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_gather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_gather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    mpi_gather__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_gather
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_gather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_gather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    pmpi_gather__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_gatherv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_gatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_gatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_gatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_gatherv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_gatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_gatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_gatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_scatter
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_scatter__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_scatter_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    mpi_scatter__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_scatter
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_scatter__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_scatter_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    pmpi_scatter__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_scatterv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_scatterv__ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_scatterv_ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_scatterv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_scatterv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_scatterv__ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_scatterv_ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_scatterv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_allgather
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_allgather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_allgather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_allgather__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_allgather
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_allgather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_allgather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_allgather__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_allgatherv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_allgatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_allgatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    mpi_allgatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_allgatherv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_allgatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_allgatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    pmpi_allgatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_alltoall
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_alltoall__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_alltoall_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_alltoall__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_alltoall
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_alltoall__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_alltoall_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_alltoall__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_alltoallv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_alltoallv__ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_alltoallv_ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_alltoallv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_alltoallv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_alltoallv__ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_alltoallv_ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_alltoallv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_reduce
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_reduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_reduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_reduce__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_reduce
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_reduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_reduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_reduce__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_ireduce
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ireduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint* v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_ireduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint* v8, MPI_Fint *ierr )
{
    mpi_ireduce__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_ireduce
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ireduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint* v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_ireduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint* v8, MPI_Fint *ierr )
{
    pmpi_ireduce__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_reduce_local
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_reduce_local__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr) ;

void FORT_CALL mpi_reduce_local_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr)
{
    mpi_reduce_local__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_reduce_local
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_reduce_local__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr) ;

void FORT_CALL pmpi_reduce_local_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr)
{
    pmpi_reduce_local__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_iallgather
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iallgather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_iallgather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr )
{
    mpi_iallgather__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_iallgather
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iallgather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_iallgather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr )
{
    pmpi_iallgather__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_iallgatherv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iallgatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_iallgatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_iallgatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_iallgatherv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iallgatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_iallgatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_iallgatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_iallreduce
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iallreduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_iallreduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_iallreduce__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_iallreduce
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iallreduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_iallreduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_iallreduce__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_ialltoall
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ialltoall__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_ialltoall_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr )
{
    mpi_ialltoall__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_ialltoall
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ialltoall__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_ialltoall_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr )
{
    pmpi_ialltoall__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_ialltoallv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ialltoallv__ (void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr);

void FORT_CALL mpi_ialltoallv_ (void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr)
{
    mpi_ialltoallv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// pmpi_ialltoallv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ialltoallv__ (void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr);

void FORT_CALL pmpi_ialltoallv_ (void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr)
{
    pmpi_ialltoallv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// mpi_ibarrier
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ibarrier__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_ibarrier_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_ibarrier__ (v1, v2, ierr);
}

//
// pmpi_ibarrier
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ibarrier__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_ibarrier_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_ibarrier__ (v1, v2, ierr);
}

//
// mpi_ibcast
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ibcast__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_ibcast_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_ibcast__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_ibcast
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ibcast__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_ibcast_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_ibcast__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_iexscan
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iexscan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_iexscan_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_iexscan__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_iexscan
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iexscan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_iexscan_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_iexscan__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_igather
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_igather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_igather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_igather__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_igather
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_igather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_igather_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_igather__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_igatherv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_igatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL mpi_igatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    mpi_igatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// pmpi_igatherv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_igatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_igatherv_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    pmpi_igatherv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// mpi_ireduce_scatter
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ireduce_scatter__ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_ireduce_scatter_ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_ireduce_scatter__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_ireduce_scatter
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ireduce_scatter__ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_ireduce_scatter_ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_ireduce_scatter__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_ireduce_scatter_block
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ireduce_scatter_block__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_ireduce_scatter_block_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_ireduce_scatter_block__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_ireduce_scatter_block
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ireduce_scatter_block__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_ireduce_scatter_block_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_ireduce_scatter_block__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_iscan
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iscan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_iscan_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_iscan__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_iscan
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iscan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_iscan_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_iscan__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_iscatter
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iscatter__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_iscatter_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_iscatter__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_iscatter
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iscatter__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_iscatter_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_iscatter__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_iscatterv
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_iscatterv__ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL mpi_iscatterv_ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    mpi_iscatterv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// pmpi_iscatterv
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_iscatterv__ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_iscatterv_ ( void *v1, MPI_Fint *v2, MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    pmpi_iscatterv__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

/* The Fortran Op function prototype and calling convention */
typedef void (FORT_CALL MPI_F77_User_function)(const void *, void *, const MPI_Fint *, const MPI_Fint * );

/* Helper proxy function to thunk op function calls into Fortran calling convention */
static void MPIAPI MPIR_Op_f_proxy(MPI_User_function* uop, const void* invec, void* outvec, const int* len, const MPI_Datatype* dtype)
{
    ((MPI_F77_User_function*)uop)(invec, outvec, len, dtype);
}

//
// mpi_op_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_op_create__ ( MPI_User_function *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_op_create_ ( MPI_User_function *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_op_create__ (v1, v2, v3, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Op_set_proxy( *v3, MPIR_Op_f_proxy);
    }
}

//
// pmpi_op_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_op_create__ ( MPI_User_function *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_op_create_ ( MPI_User_function *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_op_create__ (v1, v2, v3, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Op_set_proxy( *v3, MPIR_Op_f_proxy);
    }
}

//
//mpi_op_commutative
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_op_commutative__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void MSMPI_FORT_CALL mpi_op_commutative_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_op_commutative__( v1, v2, ierr );
}

//
//pmpi_op_commutative
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_op_commutative__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr );

void MSMPI_FORT_CALL pmpi_op_commutative_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_op_commutative__( v1, v2, ierr );
}

//
// mpi_op_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_op_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_op_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_op_free__ (v1, ierr);
}

//
// pmpi_op_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_op_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_op_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_op_free__ (v1, ierr);
}

//
// mpi_allreduce
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_allreduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_allreduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_allreduce__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_allreduce
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_allreduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_allreduce_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_allreduce__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_reduce_scatter_block
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_reduce_scatter_block__(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr);

void FORT_CALL mpi_reduce_scatter_block_(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr)
{
    mpi_reduce_scatter_block__(v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_reduce_scatter_block
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_reduce_scatter_block__(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr);

void FORT_CALL pmpi_reduce_scatter_block_(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr)
{
    pmpi_reduce_scatter_block__(v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_reduce_scatter
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_reduce_scatter__ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_reduce_scatter_ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_reduce_scatter__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_reduce_scatter
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_reduce_scatter__ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_reduce_scatter_ ( void *v1, void *v2, MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_reduce_scatter__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_scan
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_scan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_scan_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_scan__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_scan
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_scan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_scan_ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_scan__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_group_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_group_size__ (v1, v2, ierr);
}

//
// pmpi_group_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_group_size__ (v1, v2, ierr);
}

//
// mpi_group_rank
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_rank_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_group_rank__ (v1, v2, ierr);
}

//
// pmpi_group_rank
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_rank_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_group_rank__ (v1, v2, ierr);
}

//
// mpi_group_translate_ranks
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_translate_ranks__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_translate_ranks_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_group_translate_ranks__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_group_translate_ranks
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_translate_ranks__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_translate_ranks_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_group_translate_ranks__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_group_compare
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_compare__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_compare_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_group_compare__ (v1, v2, v3, ierr);
}

//
// pmpi_group_compare
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_compare__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_compare_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_group_compare__ (v1, v2, v3, ierr);
}

//
// mpi_comm_group
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_group__ (v1, v2, ierr);
}

//
// pmpi_comm_group
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_group__ (v1, v2, ierr);
}

//
// mpi_group_union
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_union__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_union_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_group_union__ (v1, v2, v3, ierr);
}

//
// pmpi_group_union
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_union__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_union_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_group_union__ (v1, v2, v3, ierr);
}

//
// mpi_group_intersection
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_intersection__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_intersection_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_group_intersection__ (v1, v2, v3, ierr);
}

//
// pmpi_group_intersection
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_intersection__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_intersection_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_group_intersection__ (v1, v2, v3, ierr);
}

//
// mpi_group_difference
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_difference__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_difference_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_group_difference__ (v1, v2, v3, ierr);
}

//
// pmpi_group_difference
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_difference__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_difference_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_group_difference__ (v1, v2, v3, ierr);
}

//
// mpi_group_incl
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_incl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_incl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_group_incl__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_group_incl
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_incl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_incl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_group_incl__ (v1, v2, v3, v4, ierr);
}

//
// mpi_group_excl
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_excl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_excl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_group_excl__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_group_excl
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_excl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_excl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_group_excl__ (v1, v2, v3, v4, ierr);
}

//
// mpi_group_range_incl
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_range_incl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_range_incl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_group_range_incl__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_group_range_incl
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_range_incl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_range_incl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_group_range_incl__ (v1, v2, v3, v4, ierr);
}

//
// mpi_group_range_excl
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_range_excl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_range_excl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_group_range_excl__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_group_range_excl
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_range_excl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_range_excl_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_group_range_excl__ (v1, v2, v3, v4, ierr);
}

//
// mpi_group_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_group_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_group_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_group_free__ (v1, ierr);
}

//
// pmpi_group_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_group_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_group_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_group_free__ (v1, ierr);
}

//
// mpi_comm_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_size__ (v1, v2, ierr);
}

//
// pmpi_comm_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_size__ (v1, v2, ierr);
}

//
// mpi_comm_rank
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_rank_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_rank__ (v1, v2, ierr);
}

//
// pmpi_comm_rank
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_rank_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_rank__ (v1, v2, ierr);
}

//
// mpi_comm_compare
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_compare__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_compare_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_comm_compare__ (v1, v2, v3, ierr);
}

//
// pmpi_comm_compare
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_compare__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_compare_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_comm_compare__ (v1, v2, v3, ierr);
}

//
// mpi_comm_dup
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_dup_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_dup__ (v1, v2, ierr);
}

//
// pmpi_comm_dup
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_dup_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_dup__ (v1, v2, ierr);
}

//
// mpi_comm_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_comm_create__ (v1, v2, v3, ierr);
}

//
// pmpi_comm_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_comm_create__ (v1, v2, v3, ierr);
}

//
// mpi_comm_split
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_split__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_split_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_comm_split__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_comm_split
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_split__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_split_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_comm_split__ (v1, v2, v3, v4, ierr);
}

//
// mpi_comm_split_type
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_split_type__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_split_type_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_comm_split_type__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_comm_split_type
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_split_type__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_split_type_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_comm_split_type__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_comm_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_comm_free__ (v1, ierr);
}

//
// pmpi_comm_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_comm_free__ (v1, ierr);
}

//
// mpi_comm_test_inter
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_test_inter__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_test_inter_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_test_inter__ (v1, v2, ierr);
}

//
// pmpi_comm_test_inter
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_test_inter__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_test_inter_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_test_inter__ (v1, v2, ierr);
}

//
// mpi_comm_remote_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_remote_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_remote_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_remote_size__ (v1, v2, ierr);
}

//
// pmpi_comm_remote_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_remote_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_remote_size_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_remote_size__ (v1, v2, ierr);
}

//
// mpi_comm_remote_group
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_remote_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_remote_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_remote_group__ (v1, v2, ierr);
}

//
// pmpi_comm_remote_group
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_remote_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_remote_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_remote_group__ (v1, v2, ierr);
}

//
// mpi_intercomm_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_intercomm_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3,const  MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_intercomm_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3,const  MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_intercomm_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_intercomm_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_intercomm_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3,const  MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_intercomm_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3,const  MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_intercomm_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_intercomm_merge
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_intercomm_merge__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_intercomm_merge_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_intercomm_merge__ (v1, v2, v3, ierr);
}

//
// pmpi_intercomm_merge
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_intercomm_merge__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_intercomm_merge_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_intercomm_merge__ (v1, v2, v3, ierr);
}

//
// mpi_keyval_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_keyval_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_keyval_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_keyval_free__ (v1, ierr);
}

//
// pmpi_keyval_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_keyval_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_keyval_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_keyval_free__ (v1, ierr);
}

//
// mpi_attr_put
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_attr_put__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_attr_put_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    mpi_attr_put__ (v1, v2, v3, ierr);
}

//
// pmpi_attr_put
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_attr_put__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_attr_put_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    pmpi_attr_put__ (v1, v2, v3, ierr);
}

//
// mpi_attr_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_attr_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_attr_get_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_attr_get__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_attr_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_attr_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_attr_get_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_attr_get__ (v1, v2, v3, v4, ierr);
}

//
// mpi_attr_delete
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_attr_delete__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_attr_delete_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_attr_delete__ (v1, v2, ierr);
}

//
// pmpi_attr_delete
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_attr_delete__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_attr_delete_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_attr_delete__ (v1, v2, ierr);
}

//
// mpi_topo_test
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_topo_test__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_topo_test_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_topo_test__ (v1, v2, ierr);
}

//
// pmpi_topo_test
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_topo_test__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_topo_test_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_topo_test__ (v1, v2, ierr);
}

//
// mpi_cart_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cart_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cart_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_cart_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_cart_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cart_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cart_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_cart_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_dims_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_dims_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_dims_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_dims_create__ (v1, v2, v3, ierr);
}

//
// pmpi_dims_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_dims_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_dims_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_dims_create__ (v1, v2, v3, ierr);
}

//
// mpi_graph_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_graph_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_graph_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_graph_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_graph_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_graph_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_graph_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_graph_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_graphdims_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_graphdims_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_graphdims_get_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_graphdims_get__ (v1, v2, v3, ierr);
}

//
// pmpi_graphdims_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_graphdims_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_graphdims_get_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_graphdims_get__ (v1, v2, v3, ierr);
}

//
// mpi_graph_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_graph_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v3) MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_graph_get_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v3) MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_graph_get__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_graph_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_graph_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v3) MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_graph_get_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v3) MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_graph_get__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_cartdim_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cartdim_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cartdim_get_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_cartdim_get__ (v1, v2, ierr);
}

//
// pmpi_cartdim_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cartdim_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cartdim_get_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_cartdim_get__ (v1, v2, ierr);
}

//
// mpi_cart_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cart_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v2) MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cart_get_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v2) MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_cart_get__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_cart_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cart_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v2) MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cart_get_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v2) MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_cart_get__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_cart_rank
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cart_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cart_rank_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_cart_rank__ (v1, v2, v3, ierr);
}

//
// pmpi_cart_rank
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cart_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cart_rank_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_cart_rank__ (v1, v2, v3, ierr);
}

//
// mpi_cart_coords
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cart_coords__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v3) MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cart_coords_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v3) MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_cart_coords__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_cart_coords
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cart_coords__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v3) MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cart_coords_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v3) MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_cart_coords__ (v1, v2, v3, v4, ierr);
}

//
// mpi_graph_neighbors_count
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_graph_neighbors_count__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_graph_neighbors_count_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_graph_neighbors_count__ (v1, v2, v3, ierr);
}

//
// pmpi_graph_neighbors_count
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_graph_neighbors_count__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_graph_neighbors_count_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_graph_neighbors_count__ (v1, v2, v3, ierr);
}

//
// mpi_graph_neighbors
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_graph_neighbors__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_graph_neighbors_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_graph_neighbors__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_graph_neighbors
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_graph_neighbors__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_graph_neighbors_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_graph_neighbors__ (v1, v2, v3, v4, ierr);
}

//
// mpi_cart_shift
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cart_shift__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cart_shift_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_cart_shift__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_cart_shift
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cart_shift__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cart_shift_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_cart_shift__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_cart_sub
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cart_sub__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cart_sub_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_cart_sub__ (v1, v2, v3, ierr);
}

//
// pmpi_cart_sub
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cart_sub__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cart_sub_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_cart_sub__ (v1, v2, v3, ierr);
}

//
// mpi_cart_map
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_cart_map__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_cart_map_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_cart_map__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_cart_map
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_cart_map__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_cart_map_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_cart_map__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_graph_map
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_graph_map__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_graph_map_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_graph_map__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_graph_map
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_graph_map__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_graph_map_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_graph_map__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_dist_graph_neighbors_count
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_dist_graph_neighbors_count__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_dist_graph_neighbors_count_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_dist_graph_neighbors_count__ (v1, v2, v3, v4, ierr);
} 
//
// pmpi_dist_graph_neighbors_count
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_dist_graph_neighbors_count__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_dist_graph_neighbors_count_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_dist_graph_neighbors_count__ (v1, v2, v3, v4, ierr);
}

//
// mpi_dist_graph_neighbors
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_dist_graph_neighbors__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _Out_cap_(*v5) MPI_Fint *v6, _Out_cap_(*v5) MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_dist_graph_neighbors_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _Out_cap_(*v5) MPI_Fint *v6, _Out_cap_(*v5) MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_dist_graph_neighbors__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_dist_graph_neighbors
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_dist_graph_neighbors__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _Out_cap_(*v5) MPI_Fint *v6, _Out_cap_(*v5) MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_dist_graph_neighbors_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _Out_cap_(*v5) MPI_Fint *v6, _Out_cap_(*v5) MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_dist_graph_neighbors__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_dist_graph_create_adjacent
// 
FORT_IMPORT void MSMPI_FORT_CALL mpi_dist_graph_create_adjacent__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _In_count_(*v5) MPI_Fint *v6, _In_count_(*v5) MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL mpi_dist_graph_create_adjacent_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _In_count_(*v5) MPI_Fint *v6, _In_count_(*v5) MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    mpi_dist_graph_create_adjacent__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// pmpi_dist_graph_create_adjacent
// 
FORT_IMPORT void MSMPI_FORT_CALL pmpi_dist_graph_create_adjacent__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _In_count_(*v5) MPI_Fint *v6, _In_count_(*v5) MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_dist_graph_create_adjacent_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _In_count_(*v5) MPI_Fint *v6, _In_count_(*v5) MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    pmpi_dist_graph_create_adjacent__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// mpi_dist_graph_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_dist_graph_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_dist_graph_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_dist_graph_create__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_dist_graph_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_dist_graph_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_dist_graph_create_ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_dist_graph_create__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_get_processor_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_processor_name__ ( _In_count_(d1) char *v1, MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_get_processor_name_ ( _In_count_(d1) char *v1 FORT_MIXED_LEN(d1), MPI_Fint *v2, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_get_processor_name__ (v1, v2, ierr, d1);
}

//
// pmpi_get_processor_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_processor_name__ ( _In_count_(d1) char *v1, MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_get_processor_name_ ( _In_count_(d1) char *v1 FORT_MIXED_LEN(d1), MPI_Fint *v2, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_get_processor_name__ (v1, v2, ierr, d1);
}

//
// msmpi_get_bsend_overhead
//
FORT_IMPORT void MSMPI_FORT_CALL msmpi_get_bsend_overhead__ ( MPI_Fint *size ) ;

void FORT_CALL msmpi_get_bsend_overhead_ ( MPI_Fint *size )
{
    msmpi_get_bsend_overhead__ (size);
}

//
// pmsmpi_get_bsend_overhead
//
FORT_IMPORT void MSMPI_FORT_CALL pmsmpi_get_bsend_overhead__ ( MPI_Fint *size ) ;

void FORT_CALL pmsmpi_get_bsend_overhead_ ( MPI_Fint *size )
{
    pmsmpi_get_bsend_overhead__ (size);
}

//
// msmpi_get_version
//
FORT_IMPORT void MSMPI_FORT_CALL msmpi_get_version__ ( MPI_Fint *version ) ;

void FORT_CALL msmpi_get_version_ ( MPI_Fint *version )
{
    msmpi_get_version__ (version);
}

//
// pmsmpi_get_version
//
FORT_IMPORT void MSMPI_FORT_CALL pmsmpi_get_version__ ( MPI_Fint *version ) ;

void FORT_CALL pmsmpi_get_version_ ( MPI_Fint *version )
{
    pmsmpi_get_version__ (version);
}

//
// mpi_get_version
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_version__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_get_version_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_get_version__ (v1, v2, ierr);
}

//
// pmpi_get_version
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_version__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_get_version_ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_get_version__ (v1, v2, ierr);
}

//
// mpi_get_library_version
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_library_version__ ( _In_count_(d1) char *v1, MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_get_library_version_ ( _In_count_(d1) char *v1 FORT_MIXED_LEN(d1), MPI_Fint *v2, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_get_library_version__ (v1, v2, ierr, d1);
}

//
// pmpi_get_library_version
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_library_version__ ( _In_count_(d1) char *v1, MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_get_library_version_ ( _In_count_(d1) char *v1 FORT_MIXED_LEN(d1), MPI_Fint *v2, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_get_library_version__ (v1, v2, ierr, d1);
}

//
// mpi_errhandler_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_errhandler_create__ ( MPI_Handler_function*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_errhandler_create_ ( MPI_Handler_function*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_errhandler_create__ (v1, v2, ierr);
}

//
// pmpi_errhandler_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_errhandler_create__ ( MPI_Handler_function*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_errhandler_create_ ( MPI_Handler_function*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_errhandler_create__ (v1, v2, ierr);
}

//
// mpi_errhandler_set
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_errhandler_set__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_errhandler_set_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_errhandler_set__ (v1, v2, ierr);
}

//
// pmpi_errhandler_set
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_errhandler_set__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_errhandler_set_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_errhandler_set__ (v1, v2, ierr);
}

//
// mpi_errhandler_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_errhandler_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_errhandler_get_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_errhandler_get__ (v1, v2, ierr);
}

//
// pmpi_errhandler_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_errhandler_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_errhandler_get_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_errhandler_get__ (v1, v2, ierr);
}

//
// mpi_errhandler_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_errhandler_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_errhandler_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_errhandler_free__ (v1, ierr);
}

//
// pmpi_errhandler_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_errhandler_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_errhandler_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_errhandler_free__ (v1, ierr);
}

//
// mpi_error_string
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_error_string__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_error_string_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_error_string__ (v1, v2, v3, ierr, d2);
}

//
// pmpi_error_string
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_error_string__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_error_string_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_error_string__ (v1, v2, v3, ierr, d2);
}

//
// mpi_error_class
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_error_class__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_error_class_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_error_class__ (v1, v2, ierr);
}

//
// pmpi_error_class
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_error_class__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_error_class_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_error_class__ (v1, v2, ierr);
}

//
// mpi_finalize
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_finalize__ ( MPI_Fint *ierr ) ;

void FORT_CALL mpi_finalize_ ( MPI_Fint *ierr )
{
    mpi_finalize__ (ierr);
}

//
// pmpi_finalize
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_finalize__ ( MPI_Fint *ierr ) ;

void FORT_CALL pmpi_finalize_ ( MPI_Fint *ierr )
{
    pmpi_finalize__ (ierr);
}

//
// mpi_initialized
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_initialized__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_initialized_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_initialized__ (v1, ierr);
}

//
// pmpi_initialized
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_initialized__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_initialized_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_initialized__ (v1, ierr);
}

//
// mpi_abort
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_abort__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_abort_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_abort__ (v1, v2, ierr);
}

//
// pmpi_abort
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_abort__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_abort_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_abort__ (v1, v2, ierr);
}

//
// mpi_close_port
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_close_port__ ( _In_count_(d1) const char *v1, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_close_port_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_close_port__ (v1, ierr, d1);
}

//
// pmpi_close_port
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_close_port__ ( _In_count_(d1) const char *v1, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_close_port_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_close_port__ (v1, ierr, d1);
}

//
// mpi_comm_accept
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_accept__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_comm_accept_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_comm_accept__ (v1, v2, v3, v4, v5, ierr, d1);
}

//
// pmpi_comm_accept
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_accept__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_comm_accept_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_comm_accept__ (v1, v2, v3, v4, v5, ierr, d1);
}

//
// mpi_comm_connect
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_connect__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_comm_connect_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_comm_connect__ (v1, v2, v3, v4, v5, ierr, d1);
}

//
// pmpi_comm_connect
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_connect__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_comm_connect_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_comm_connect__ (v1, v2, v3, v4, v5, ierr, d1);
}

//
// mpi_comm_disconnect
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_disconnect__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_disconnect_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_comm_disconnect__ (v1, ierr);
}

//
// pmpi_comm_disconnect
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_disconnect__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_disconnect_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_comm_disconnect__ (v1, ierr);
}

//
// mpi_comm_get_parent
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_get_parent__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_get_parent_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_comm_get_parent__ (v1, ierr);
}

//
// pmpi_comm_get_parent
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_get_parent__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_get_parent_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_comm_get_parent__ (v1, ierr);
}

//
// mpi_comm_join
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_join__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_join_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_join__ (v1, v2, ierr);
}

//
// pmpi_comm_join
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_join__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_join_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_join__ (v1, v2, ierr);
}

//
// mpi_comm_spawn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_spawn__ ( _In_count_(d1) const char *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, _Out_cap_(*v3) MPI_Fint v8[], MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d2 ) ;

void FORT_CALL mpi_comm_spawn_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, _Out_cap_(*v3) MPI_Fint v8[], MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d2) )
{
    mpi_comm_spawn__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr, d1, d2);
}

//
// pmpi_comm_spawn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_spawn__ ( _In_count_(d1) const char *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, _Out_cap_(*v3) MPI_Fint v8[], MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d2 ) ;

void FORT_CALL pmpi_comm_spawn_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, _Out_cap_(*v3) MPI_Fint v8[], MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d2) )
{
    pmpi_comm_spawn__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr, d1, d2);
}

//
// mpi_comm_spawn_multiple
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_spawn_multiple__ ( const MPI_Fint *v1, _In_reads_(d2) const char *v2, _In_reads_(d3) const char *v3, _In_reads_(*v1) MPI_Fint v4[], _In_reads_(*v1) MPI_Info v5[], const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, _Out_opt_ MPI_Fint v9[], MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d3 ) ;

void FORT_CALL mpi_comm_spawn_multiple_ ( const MPI_Fint *v1, _In_reads_(d2) const char *v2 FORT_MIXED_LEN(d2), _In_reads_(d3) const char *v3 FORT_MIXED_LEN(d3), _In_reads_(*v1) MPI_Fint v4[], _In_reads_(*v1) MPI_Info v5[], const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, _Out_opt_ MPI_Fint v9[], MPI_Fint *ierr FORT_END_LEN(d2) FORT_END_LEN(d3) )
{
    mpi_comm_spawn_multiple__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr, d2, d3);
}

//
// pmpi_comm_spawn_multiple
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_spawn_multiple__ ( const MPI_Fint *v1, _In_reads_(d2) const char *v2, _In_reads_(d3) const char *v3, _In_reads_(*v1) MPI_Fint v4[], _In_reads_(*v1) MPI_Info v5[], const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, _Out_opt_ MPI_Fint v9[], MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d3 ) ;

void FORT_CALL pmpi_comm_spawn_multiple_ ( const MPI_Fint *v1, _In_reads_(d2) const char *v2 FORT_MIXED_LEN(d2), _In_reads_(d3) const char *v3 FORT_MIXED_LEN(d3), _In_reads_(*v1) MPI_Fint v4[], _In_reads_(*v1) MPI_Info v5[], const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, _Out_opt_ MPI_Fint v9[], MPI_Fint *ierr FORT_END_LEN(d2) FORT_END_LEN(d3) )
{
    pmpi_comm_spawn_multiple__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr, d2, d3);
}

//
// mpi_lookup_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_lookup_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 ) ;

void FORT_CALL mpi_lookup_name_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, _Out_cap_(d3) char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d3) )
{
    mpi_lookup_name__ (v1, v2, v3, ierr, d1, d3);
}

//
// pmpi_lookup_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_lookup_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 ) ;

void FORT_CALL pmpi_lookup_name_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, _Out_cap_(d3) char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d3) )
{
    pmpi_lookup_name__ (v1, v2, v3, ierr, d1, d3);
}

//
//mpi_open_port
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_open_port__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *ierr, MPI_Fint d2 );

void FORT_CALL mpi_open_port_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_open_port__ (v1,v2,ierr,d2);
}

//
//pmpi_open_port
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_open_port__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *ierr, MPI_Fint d2 );

void FORT_CALL pmpi_open_port_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_open_port__ (v1,v2,ierr,d2);
}

//
// mpi_publish_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_publish_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 ) ;

void FORT_CALL mpi_publish_name_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, _In_count_(d3) const char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d3) )
{
    mpi_publish_name__ (v1, v2, v3, ierr, d1, d3);
}

//
// pmpi_publish_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_publish_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 ) ;

void FORT_CALL pmpi_publish_name_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, _In_count_(d3) const char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d3) )
{
    pmpi_publish_name__ (v1, v2, v3, ierr, d1, d3);
}

//
// mpi_unpublish_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_unpublish_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 ) ;

void FORT_CALL mpi_unpublish_name_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, _In_count_(d3) const char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d3) )
{
    mpi_unpublish_name__ (v1, v2, v3, ierr, d1, d3);
}

//
// pmpi_unpublish_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_unpublish_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 ) ;

void FORT_CALL pmpi_unpublish_name_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, _In_count_(d3) const char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d1) FORT_END_LEN(d3) )
{
    pmpi_unpublish_name__ (v1, v2, v3, ierr, d1, d3);
}

//
// mpi_accumulate
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_accumulate__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_accumulate_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_accumulate__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_accumulate
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_accumulate__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_accumulate_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_accumulate__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_raccumulate
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_raccumulate__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL mpi_raccumulate_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    mpi_raccumulate__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// pmpi_raccumulate
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_raccumulate__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_raccumulate_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    pmpi_raccumulate__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// mpi_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_get_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    mpi_get__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_get_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    pmpi_get__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_rget
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_rget__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr );

void FORT_CALL mpi_rget_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_rget__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_rget
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_rget__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr );

void FORT_CALL pmpi_rget_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_rget__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_put
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_put__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL mpi_put_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    mpi_put__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// pmpi_put
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_put__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_put_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    pmpi_put__ (v1, v2, v3, v4, v5, v6, v7, v8, ierr);
}

//
// mpi_rput
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_rput__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr );

void FORT_CALL mpi_rput_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_rput__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_rput
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_rput__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr );

void FORT_CALL pmpi_rput_ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_rput__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_get_accumulate
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_accumulate__(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *ierr);

void FORT_CALL mpi_get_accumulate_(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *ierr)
{
    mpi_get_accumulate__(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, ierr);
}

//
// pmpi_get_accumulate
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_accumulate__(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *ierr);

void FORT_CALL pmpi_get_accumulate_(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *ierr)
{
    pmpi_get_accumulate__(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, ierr);
}

//
// mpi_rget_accumulate
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_rget_accumulate__(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *v13, MPI_Fint *ierr);

void FORT_CALL mpi_rget_accumulate_(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *v13, MPI_Fint *ierr)
{
    mpi_rget_accumulate__(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, ierr);
}

//
// pmpi_rget_accumulate
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_rget_accumulate__(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *v13, MPI_Fint *ierr);

void FORT_CALL pmpi_rget_accumulate_(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *v13, MPI_Fint *ierr)
{
    pmpi_rget_accumulate__(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, ierr);
}

//
// mpi_fetch_and_op
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_fetch_and_op__(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr);

void FORT_CALL mpi_fetch_and_op_(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr)
{
    mpi_fetch_and_op__(v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_fetch_and_op
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_fetch_and_op__(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr);

void FORT_CALL pmpi_fetch_and_op_(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr)
{
    pmpi_fetch_and_op__(v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_compare_and_swap
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_compare_and_swap__(void *v1, void *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr);

void FORT_CALL mpi_compare_and_swap_(void *v1, void *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr)
{
    mpi_compare_and_swap__(v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_compare_and_swap
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_compare_and_swap__(void *v1, void *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr);

void FORT_CALL pmpi_compare_and_swap_(void *v1, void *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr)
{
    pmpi_compare_and_swap__(v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_win_complete
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_complete__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_complete_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_win_complete__ (v1, ierr);
}

//
// pmpi_win_complete
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_complete__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_complete_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_win_complete__ (v1, ierr);
}

//
// mpi_win_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_create__ ( void*v1, const MPI_Aint * v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_create_ ( void*v1, const MPI_Aint * v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_win_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_win_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_create__ ( void*v1, const MPI_Aint * v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_create_ ( void*v1, const MPI_Aint * v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_win_create__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_win_allocate
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_allocate__ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL mpi_win_allocate_ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_win_allocate__ ( v1, v2, v3, v4, v5, v6, ierr );
}

//
// pmpi_win_allocate
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_allocate__ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr );

void FORT_CALL pmpi_win_allocate_ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_win_allocate__ ( v1, v2, v3, v4, v5, v6, ierr );
}

//
// mpi_win_allocate_shared
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_allocate_shared__ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_allocate_shared_ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_win_allocate_shared__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_win_allocate_shared
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_allocate_shared__ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_allocate_shared_ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_win_allocate_shared__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_win_create_dynamic
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_create_dynamic__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_create_dynamic_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_win_create_dynamic__ (v1, v2, v3, ierr);
}

//
// pmpi_win_create_dynamic
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_create_dynamic__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_create_dynamic_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_win_create_dynamic__ (v1, v2, v3, ierr);
}

//
// mpi_win_attach
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_attach__( MPI_Fint *v1, void *v2, const MPI_Aint *v3, MPI_Fint *ierr );

void FORT_CALL mpi_win_attach_ ( MPI_Fint *v1, void *v2, const MPI_Aint *v3, MPI_Fint *ierr )
{
    mpi_win_attach__ (v1, v2, v3, ierr);
}

//
// pmpi_win_attach
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_attach__ ( MPI_Fint *v1, void *v2, const MPI_Aint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_attach_ ( MPI_Fint *v1, void *v2, const MPI_Aint *v3, MPI_Fint *ierr )
{
    pmpi_win_attach__ (v1, v2, v3, ierr);
}

//
// mpi_win_detach
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_detach__ ( MPI_Fint *v1, void *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_detach_ ( MPI_Fint *v1, void *v2, MPI_Fint *ierr )
{
    mpi_win_detach__ (v1, v2, ierr);
}

//
// pmpi_win_detach
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_detach__ ( MPI_Fint *v1, void *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_detach_ ( MPI_Fint *v1, void *v2, MPI_Fint *ierr )
{
    pmpi_win_detach__ (v1, v2, ierr);
}

//
// mpi_win_shared_query
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_shared_query__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Aint *v3, MPI_Fint *v4, void *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_shared_query_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Aint *v3, MPI_Fint *v4, void *v5, MPI_Fint *ierr )
{
    mpi_win_shared_query__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_win_shared_query
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_shared_query__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Aint *v3, MPI_Fint *v4, void *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_shared_query_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Aint *v3, MPI_Fint *v4, void *v5, MPI_Fint *ierr )
{
    pmpi_win_shared_query__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_win_fence
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_fence__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_fence_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_fence__ (v1, v2, ierr);
}

//
// pmpi_win_fence
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_fence__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_fence_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_fence__ (v1, v2, ierr);
}

//
// mpi_win_flush
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_flush__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_flush_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_flush__ (v1, v2, ierr);
}

//
// pmpi_win_flush
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_flush__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_flush_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_flush__ (v1, v2, ierr);
}

//
// mpi_win_flush_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_flush_all__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL mpi_win_flush_all_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    mpi_win_flush_all__(v1, ierr);
}

//
// pmpi_win_flush_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_flush_all__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL pmpi_win_flush_all_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    pmpi_win_flush_all__(v1, ierr);
}

//
// mpi_win_flush_local
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_flush_local__(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr);

void FORT_CALL mpi_win_flush_local_(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr)
{
    mpi_win_flush_local__(v1, v2, ierr);
}

//
// pmpi_win_flush_local
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_flush_local__(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr);

void FORT_CALL pmpi_win_flush_local_(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr)
{
    pmpi_win_flush_local__(v1, v2, ierr);
}

//
// mpi_win_flush_local_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_flush_local_all__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL mpi_win_flush_local_all_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    mpi_win_flush_local_all__(v1, ierr);
}

//
// pmpi_win_flush_local_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_flush_local_all__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL pmpi_win_flush_local_all_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    pmpi_win_flush_local_all__(v1, ierr);
}

//
// mpi_win_sync
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_sync__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL mpi_win_sync_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    mpi_win_sync__(v1, ierr);
}

//
// pmpi_win_sync
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_sync__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL pmpi_win_sync_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    pmpi_win_sync__(v1, ierr);
}

//
// mpi_win_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_win_free__ (v1, ierr);
}

//
// pmpi_win_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_win_free__ (v1, ierr);
}

//
// mpi_win_get_group
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_get_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_get_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_get_group__ (v1, v2, ierr);
}

//
// pmpi_win_get_group
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_get_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_get_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_get_group__ (v1, v2, ierr);
}

//
// mpi_win_lock
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_lock__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_lock_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_win_lock__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_win_lock
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_lock__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_lock_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_win_lock__ (v1, v2, v3, v4, ierr);
}

//
// mpi_win_lock_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_lock_all__(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr);

void FORT_CALL mpi_win_lock_all_(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr)
{
    mpi_win_lock_all__(v1, v2, ierr);
}

//
// pmpi_win_lock_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_lock_all__(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr);

void FORT_CALL pmpi_win_lock_all_(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr)
{
    pmpi_win_lock_all__(v1, v2, ierr);
}

//
// mpi_win_post
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_post__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_post_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_win_post__ (v1, v2, v3, ierr);
}

//
// pmpi_win_post
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_post__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_post_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_win_post__ (v1, v2, v3, ierr);
}

//
// mpi_win_start
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_start__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_start_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_win_start__ (v1, v2, v3, ierr);
}

//
// pmpi_win_start
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_start__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_start_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_win_start__ (v1, v2, v3, ierr);
}

//
// mpi_win_test
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_test__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_test_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_test__ (v1, v2, ierr);
}

//
// pmpi_win_test
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_test__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_test_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_test__ (v1, v2, ierr);
}

//
// mpi_win_unlock
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_unlock__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_unlock_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_unlock__ (v1, v2, ierr);
}

//
// pmpi_win_unlock
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_unlock__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_unlock_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_unlock__ (v1, v2, ierr);
}

//
// mpi_win_unlock_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_unlock_all__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL mpi_win_unlock_all_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    mpi_win_unlock_all__(v1, ierr);
}

//
// pmpi_win_unlock_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_unlock_all__(const MPI_Fint *v1, MPI_Fint *ierr);

void FORT_CALL pmpi_win_unlock_all_(const MPI_Fint *v1, MPI_Fint *ierr)
{
    pmpi_win_unlock_all__(v1, ierr);
}

//
// mpi_win_wait
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_wait__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_wait_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_win_wait__ (v1, ierr);
}

//
// pmpi_win_wait
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_wait__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_wait_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_win_wait__ (v1, ierr);
}

//
// mpi_alltoallw
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_alltoallw__ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL mpi_alltoallw_ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *ierr )
{
    mpi_alltoallw__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// pmpi_alltoallw
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_alltoallw__ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_alltoallw_ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *ierr )
{
    pmpi_alltoallw__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, ierr);
}

//
// mpi_ialltoallw
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_ialltoallw__ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL mpi_ialltoallw_ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    mpi_ialltoallw__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// pmpi_ialltoallw
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_ialltoallw__ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_ialltoallw_ ( void*v1, MPI_Fint v2[], MPI_Fint v3[], MPI_Fint v4[], void*v5, MPI_Fint v6[], MPI_Fint v7[], MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    pmpi_ialltoallw__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// mpi_exscan
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_exscan__ ( void*v1, void*v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_exscan_ ( void*v1, void*v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_exscan__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_exscan
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_exscan__ ( void*v1, void*v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_exscan_ ( void*v1, void*v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_exscan__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_add_error_class
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_add_error_class__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_add_error_class_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_add_error_class__ (v1, ierr);
}

//
// pmpi_add_error_class
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_add_error_class__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_add_error_class_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_add_error_class__ (v1, ierr);
}

//
// mpi_add_error_code
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_add_error_code__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_add_error_code_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_add_error_code__ (v1, v2, ierr);
}

//
// pmpi_add_error_code
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_add_error_code__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_add_error_code_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_add_error_code__ (v1, v2, ierr);
}

//
// mpi_add_error_string
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_add_error_string__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_add_error_string_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_add_error_string__ (v1, v2, ierr, d2);
}

//
// pmpi_add_error_string
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_add_error_string__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_add_error_string_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_add_error_string__ (v1, v2, ierr, d2);
}

//
// mpi_comm_call_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_call_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_call_errhandler__ (v1, v2, ierr);
}

//
// pmpi_comm_call_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_call_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_call_errhandler__ (v1, v2, ierr);
}

/* The F90 attr copy function prototype and calling convention */
typedef void (FORT_CALL F90_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *,MPI_Aint *, MPI_Fint *, MPI_Fint *);

/* Helper proxy function to thunk the attr copy function call into F90 calling convention */
static
int
MPIAPI
MPIR_Comm_copy_attr_f90_proxy(
    MPI_Comm_copy_attr_function* user_function,
    MPI_Comm comm,
    int keyval,
    void* extra_state,
    void* value,
    void** new_value,
    int* flag
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)comm;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Aint fvalue = (MPI_Aint)value;
    MPI_Aint* fextra  = (MPI_Aint*)extra_state;
    MPI_Aint fnew = 0;
    MPI_Fint fflag = 0;

    ((F90_CopyFunction*)user_function)( &fhandle, &fkeyval, fextra, &fvalue, &fnew, &fflag, &ierr );

    *flag = fflag;
    *new_value = (void*)fnew;
    return ierr;
}


/* The F90 attr delete function prototype and calling convention */
typedef void (FORT_CALL F90_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *, MPI_Fint *);

/* Helper proxy function to thunk the attr delete function call into F77 calling convention */
static
int
MPIAPI
MPIR_Comm_delete_attr_f90_proxy(
    MPI_Comm_delete_attr_function* user_function,
    MPI_Comm comm,
    int keyval,
    void* value,
    void* extra_state
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)comm;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Aint fvalue = (MPI_Aint)value;
    MPI_Aint* fextra  = (MPI_Aint*)extra_state;

    ((F90_DeleteFunction*)user_function)( &fhandle, &fkeyval, &fvalue, fextra, &ierr );
    return ierr;
}

//
// mpi_comm_create_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_create_keyval__ ( MPI_Comm_copy_attr_function *v1, MPI_Comm_delete_attr_function *v2, MPI_Fint *v3, void *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_create_keyval_ ( MPI_Comm_copy_attr_function *v1, MPI_Comm_delete_attr_function *v2, MPI_Fint *v3, void *v4, MPI_Fint *ierr )
{
    mpi_comm_create_keyval__ (v1, v2, v3, v4, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Comm_copy_attr_f90_proxy, MPIR_Comm_delete_attr_f90_proxy );
    }
}

//
// pmpi_comm_create_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_create_keyval__ ( MPI_Comm_copy_attr_function *v1, MPI_Comm_delete_attr_function *v2, MPI_Fint *v3, void *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_create_keyval_ ( MPI_Comm_copy_attr_function *v1, MPI_Comm_delete_attr_function *v2, MPI_Fint *v3, void *v4, MPI_Fint *ierr )
{
    pmpi_comm_create_keyval__ (v1, v2, v3, v4, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Comm_copy_attr_f90_proxy, MPIR_Comm_delete_attr_f90_proxy );
    }
}

//
// mpi_comm_delete_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_delete_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_delete_attr__ (v1, v2, ierr);
}

//
// pmpi_comm_delete_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_delete_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_delete_attr__ (v1, v2, ierr);
}

//
// mpi_comm_free_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_free_keyval_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_comm_free_keyval__ (v1, ierr);
}

//
// pmpi_comm_free_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_free_keyval_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_comm_free_keyval__ (v1, ierr);
}

//
// mpi_comm_get_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_get_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_comm_get_attr__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_comm_get_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_get_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_comm_get_attr__ (v1, v2, v3, v4, ierr);
}

//
// mpi_comm_get_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_comm_get_name_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_comm_get_name__ (v1, v2, v3, ierr, d2);
}

//
// pmpi_comm_get_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_comm_get_name_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_comm_get_name__ (v1, v2, v3, ierr, d2);
}

//
// mpi_comm_set_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_set_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    mpi_comm_set_attr__ (v1, v2, v3, ierr);
}

//
// pmpi_comm_set_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_set_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    pmpi_comm_set_attr__ (v1, v2, v3, ierr);
}

//
// mpi_comm_set_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_comm_set_name_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_comm_set_name__ (v1, v2, ierr, d2);
}

//
// pmpi_comm_set_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_comm_set_name_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_comm_set_name__ (v1, v2, ierr, d2);
}

//
// mpi_file_call_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_call_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_call_errhandler__ (v1, v2, ierr);
}

//
// pmpi_file_call_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_call_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_call_errhandler__ (v1, v2, ierr);
}

//
// mpi_grequest_complete
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_grequest_complete__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_grequest_complete_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_grequest_complete__ (v1, ierr);
}

//
// pmpi_grequest_complete
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_grequest_complete__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_grequest_complete_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_grequest_complete__ (v1, ierr);
}

/* The Fortran Grequest query function prototype and calling convention */
typedef void (FORT_CALL MPIR_Grequest_f77_query_function)(void*, MPI_Status*, MPI_Fint*);

/* Helper proxy function to thunk Grequest cancel function call into Fortran calling convention */
static
int
MPIAPI
MPIR_Grequest_query_f_proxy(
    MPI_Grequest_query_function* user_function,
    void* extra_state,
    MPI_Status* status
    )
{
    MPI_Fint ferr;
    ((MPIR_Grequest_f77_query_function*)user_function)(extra_state, status, &ferr);
    return ferr;
}


/* The Fortran Grequest free function prototype and calling convention */
typedef void (FORT_CALL MPIR_Grequest_f77_free_function)(void*, MPI_Fint*);

/* Helper proxy function to thunk Grequest cancel function call into Fortran calling convention */
static
int
MPIAPI
MPIR_Grequest_free_f_proxy(
    MPI_Grequest_free_function* user_function,
    void* extra_state
    )
{
    MPI_Fint ferr;
    ((MPIR_Grequest_f77_free_function*)user_function)(extra_state, &ferr);
    return ferr;
}


/* The Fortran Grequest cancel function prototype and calling convention */
typedef void (FORT_CALL MPIR_Grequest_f77_cancel_function)(void*, int*, MPI_Fint*);

/* Helper proxy function to thunk Grequest cancel function call into Fortran calling convention */
static
int
MPIAPI
MPIR_Grequest_cancel_f_proxy(
    MPI_Grequest_cancel_function* user_function,
    void* extra_state,
    int complete
    )
{
    MPI_Fint ferr;
    MPI_Fint fcomplete = complete;
    ((MPIR_Grequest_f77_cancel_function*)user_function)(extra_state, &fcomplete, &ferr);
    return ferr;
}

//
// mpi_grequest_start
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_grequest_start__ ( MPI_Grequest_query_function*v1, MPI_Grequest_free_function*v2, MPI_Grequest_cancel_function*v3, void*v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_grequest_start_ ( MPI_Grequest_query_function*v1, MPI_Grequest_free_function*v2, MPI_Grequest_cancel_function*v3, void*v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_grequest_start__ (v1, v2, v3, v4, v5, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Grequest_set_proxy( *v5, MPIR_Grequest_query_f_proxy, MPIR_Grequest_free_f_proxy, MPIR_Grequest_cancel_f_proxy);
    }
}

//
// pmpi_grequest_start
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_grequest_start__ ( MPI_Grequest_query_function*v1, MPI_Grequest_free_function*v2, MPI_Grequest_cancel_function*v3, void*v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_grequest_start_ ( MPI_Grequest_query_function*v1, MPI_Grequest_free_function*v2, MPI_Grequest_cancel_function*v3, void*v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_grequest_start__ (v1, v2, v3, v4, v5, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Grequest_set_proxy( *v5, MPIR_Grequest_query_f_proxy, MPIR_Grequest_free_f_proxy, MPIR_Grequest_cancel_f_proxy);
    }
}

//
// mpi_is_thread_main
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_is_thread_main__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_is_thread_main_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_is_thread_main__ (v1, ierr);
}

//
// pmpi_is_thread_main
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_is_thread_main__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_is_thread_main_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_is_thread_main__ (v1, ierr);
}

//
// mpi_query_thread
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_query_thread__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_query_thread_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_query_thread__ (v1, ierr);
}

//
// pmpi_query_thread
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_query_thread__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_query_thread_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_query_thread__ (v1, ierr);
}

//
// mpi_status_set_cancelled
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_status_set_cancelled__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_status_set_cancelled_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_status_set_cancelled__ (v1, v2, ierr);
}

//
// pmpi_status_set_cancelled
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_status_set_cancelled__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_status_set_cancelled_ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_status_set_cancelled__ (v1, v2, ierr);
}

//
// mpi_status_set_elements
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_status_set_elements__ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_status_set_elements_ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_status_set_elements__ (v1, v2, v3, ierr);
}

//
// pmpi_status_set_elements
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_status_set_elements__ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_status_set_elements_ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_status_set_elements__ (v1, v2, v3, ierr);
}

//
// mpi_status_set_elements_x
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_status_set_elements_x__ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_status_set_elements_x_ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Count *v3, MPI_Fint *ierr )
{
    mpi_status_set_elements_x__ (v1, v2, v3, ierr);
}

//
// pmpi_status_set_elements_x
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_status_set_elements_x__ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_status_set_elements_x_ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Count *v3, MPI_Fint *ierr )
{
    pmpi_status_set_elements_x__ (v1, v2, v3, ierr);
}

/* The F90 attr copy function prototype and calling convention */
typedef void (FORT_CALL F90_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *,MPI_Aint *, MPI_Fint *, MPI_Fint *);

/* Helper proxy function to thunk the attr copy function call into F90 calling convention */
static
int
MPIAPI
MPIR_Type_copy_attr_f90_proxy(
    MPI_Type_copy_attr_function* user_function,
    MPI_Datatype datatype,
    int keyval,
    void* extra_state,
    void* value,
    void** new_value,
    int* flag
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)datatype;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Aint fvalue = (MPI_Aint)value;
    MPI_Aint* fextra  = (MPI_Aint*)extra_state;
    MPI_Aint fnew = 0;
    MPI_Fint fflag = 0;

    ((F90_CopyFunction*)user_function)( &fhandle, &fkeyval, fextra, &fvalue, &fnew, &fflag, &ierr );

    *flag = fflag;
    *new_value = (void*)fnew;
    return ierr;
}


/* The F90 attr delete function prototype and calling convention */
typedef void (FORT_CALL F90_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *, MPI_Fint *);

/* Helper proxy function to thunk the attr delete function call into F77 calling convention */
static
int
MPIAPI
MPIR_Type_delete_attr_f90_proxy(
    MPI_Type_delete_attr_function* user_function,
    MPI_Datatype datatype,
    int keyval,
    void* value,
    void* extra_state
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)datatype;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Aint fvalue = (MPI_Aint)value;
    MPI_Aint* fextra  = (MPI_Aint*)extra_state;

    ((F90_DeleteFunction*)user_function)( &fhandle, &fkeyval, &fvalue, fextra, &ierr );
    return ierr;
}

//
// mpi_type_create_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_keyval__ ( MPI_Type_copy_attr_function*v1, MPI_Type_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_keyval_ ( MPI_Type_copy_attr_function*v1, MPI_Type_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    mpi_type_create_keyval__ (v1, v2, v3, v4, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Type_copy_attr_f90_proxy, MPIR_Type_delete_attr_f90_proxy );
    }
}

//
// pmpi_type_create_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_keyval__ ( MPI_Type_copy_attr_function*v1, MPI_Type_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_keyval_ ( MPI_Type_copy_attr_function*v1, MPI_Type_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    pmpi_type_create_keyval__ (v1, v2, v3, v4, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Type_copy_attr_f90_proxy, MPIR_Type_delete_attr_f90_proxy );
    }
}

//
// mpi_type_delete_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_delete_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_type_delete_attr__ (v1, v2, ierr);
}

//
// pmpi_type_delete_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_delete_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_type_delete_attr__ (v1, v2, ierr);
}

//
// mpi_type_dup
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_dup_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_type_dup__ (v1, v2, ierr);
}

//
// pmpi_type_dup
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_dup_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_type_dup__ (v1, v2, ierr);
}

//
// mpi_type_free_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_free_keyval_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_type_free_keyval__ (v1, ierr);
}

//
// pmpi_type_free_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_free_keyval_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_type_free_keyval__ (v1, ierr);
}

//
// mpi_type_get_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_get_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_type_get_attr__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_type_get_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_get_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_type_get_attr__ (v1, v2, v3, v4, ierr);
}

//
// mpi_type_get_contents
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_contents__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v2) MPI_Fint v5[], _Out_cap_(*v3) MPI_Aint * v6, _Out_cap_(*v4) MPI_Fint v7[], MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_get_contents_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v2) MPI_Fint v5[], _Out_cap_(*v3) MPI_Aint * v6, _Out_cap_(*v4) MPI_Fint v7[], MPI_Fint *ierr )
{
    mpi_type_get_contents__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_type_get_contents
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_contents__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v2) MPI_Fint v5[], _Out_cap_(*v3) MPI_Aint * v6, _Out_cap_(*v4) MPI_Fint v7[], MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_get_contents_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v2) MPI_Fint v5[], _Out_cap_(*v3) MPI_Aint * v6, _Out_cap_(*v4) MPI_Fint v7[], MPI_Fint *ierr )
{
    pmpi_type_get_contents__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_type_get_envelope
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_envelope__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_get_envelope_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_get_envelope__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_get_envelope
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_envelope__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_get_envelope_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_get_envelope__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_get_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_type_get_name_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_type_get_name__ (v1, v2, v3, ierr, d2);
}

//
// pmpi_type_get_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_type_get_name_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_type_get_name__ (v1, v2, v3, ierr, d2);
}

//
// mpi_type_set_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_set_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    mpi_type_set_attr__ (v1, v2, v3, ierr);
}

//
// pmpi_type_set_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_set_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    pmpi_type_set_attr__ (v1, v2, v3, ierr);
}

//
// mpi_type_set_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_type_set_name_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_type_set_name__ (v1, v2, ierr, d2);
}

//
// pmpi_type_set_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_type_set_name_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_type_set_name__ (v1, v2, ierr, d2);
}

//
// mpi_type_match_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_match_size__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_match_size_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_type_match_size__ (v1, v2, v3, ierr);
}

//
// pmpi_type_match_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_match_size__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_match_size_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_type_match_size__ (v1, v2, v3, ierr);
}

//
// mpi_win_call_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_call_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_call_errhandler__ (v1, v2, ierr);
}

//
// pmpi_win_call_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_call_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_call_errhandler__ (v1, v2, ierr);
}

/* The F90 attr copy function prototype and calling convention */
typedef void (FORT_CALL F90_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *,MPI_Aint *, MPI_Fint *, MPI_Fint *);

/* Helper proxy function to thunk the attr copy function call into F90 calling convention */
static
int
MPIAPI
MPIR_Win_copy_attr_f90_proxy(
    MPI_Win_copy_attr_function* user_function,
    MPI_Win win,
    int keyval,
    void* extra_state,
    void* value,
    void** new_value,
    int* flag
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)win;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Aint fvalue = (MPI_Aint)value;
    MPI_Aint* fextra  = (MPI_Aint*)extra_state;
    MPI_Aint fnew = 0;
    MPI_Fint fflag = 0;

    ((F90_CopyFunction*)user_function)( &fhandle, &fkeyval, fextra, &fvalue, &fnew, &fflag, &ierr );

    *flag = fflag;
    *new_value = (void*)fnew;
    return ierr;
}


/* The F90 attr delete function prototype and calling convention */
typedef void (FORT_CALL F90_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *, MPI_Fint *);

/* Helper proxy function to thunk the attr delete function call into F77 calling convention */
static
int
MPIAPI
MPIR_Win_delete_attr_f90_proxy(
    MPI_Win_delete_attr_function* user_function,
    MPI_Win win,
    int keyval,
    void* value,
    void* extra_state
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)win;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Aint fvalue = (MPI_Aint)value;
    MPI_Aint* fextra  = (MPI_Aint*)extra_state;

    ((F90_DeleteFunction*)user_function)( &fhandle, &fkeyval, &fvalue, fextra, &ierr );
    return ierr;
}

//
// mpi_win_create_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_create_keyval__ ( MPI_Win_copy_attr_function*v1, MPI_Win_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_create_keyval_ ( MPI_Win_copy_attr_function*v1, MPI_Win_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    mpi_win_create_keyval__ (v1, v2, v3, v4, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Win_copy_attr_f90_proxy, MPIR_Win_delete_attr_f90_proxy );
    }
}

//
// pmpi_win_create_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_create_keyval__ ( MPI_Win_copy_attr_function*v1, MPI_Win_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_create_keyval_ ( MPI_Win_copy_attr_function*v1, MPI_Win_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    pmpi_win_create_keyval__ (v1, v2, v3, v4, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Win_copy_attr_f90_proxy, MPIR_Win_delete_attr_f90_proxy );
    }
}

//
// mpi_win_delete_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_delete_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_delete_attr__ (v1, v2, ierr);
}

//
// pmpi_win_delete_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_delete_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_delete_attr__ (v1, v2, ierr);
}

//
// mpi_win_free_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_free_keyval_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_win_free_keyval__ (v1, ierr);
}

//
// pmpi_win_free_keyval
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_free_keyval_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_win_free_keyval__ (v1, ierr);
}

//
// mpi_win_get_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_get_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_win_get_attr__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_win_get_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_get_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_win_get_attr__ (v1, v2, v3, v4, ierr);
}

//
// mpi_win_get_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_win_get_name_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_win_get_name__ (v1, v2, v3, ierr, d2);
}

//
// pmpi_win_get_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_win_get_name_ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_win_get_name__ (v1, v2, v3, ierr, d2);
}

//
// mpi_win_set_attr
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_set_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    mpi_win_set_attr__ (v1, v2, v3, ierr);
}

//
// pmpi_win_set_attr
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_set_attr_ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    pmpi_win_set_attr__ (v1, v2, v3, ierr);
}

//
// mpi_win_set_name
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_win_set_name_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_win_set_name__ (v1, v2, ierr, d2);
}

//
// pmpi_win_set_name
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_win_set_name_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_win_set_name__ (v1, v2, ierr, d2);
}

//
// mpi_alloc_mem
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_alloc_mem__ ( const MPI_Aint * v1, const MPI_Fint *v2, void*v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_alloc_mem_ ( const MPI_Aint * v1, const MPI_Fint *v2, void*v3, MPI_Fint *ierr )
{
    mpi_alloc_mem__ (v1, v2, v3, ierr);
}

//
// pmpi_alloc_mem
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_alloc_mem__ ( const MPI_Aint * v1, const MPI_Fint *v2, void*v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_alloc_mem_ ( const MPI_Aint * v1, const MPI_Fint *v2, void*v3, MPI_Fint *ierr )
{
    pmpi_alloc_mem__ (v1, v2, v3, ierr);
}

/* The Fortran Comm errhandler function prototype and calling convention */
/* Do not add ... as many Fotran compilers do not support it leading to a link error with stdcall */
typedef void (FORT_CALL MPI_F77_Comm_errhandler_fn)(MPI_Fint*, MPI_Fint*);

/* Helper proxy function to thunk comm errhandler function calls into Fortran calling convention */
static void MPIAPI MPIR_Comm_errhandler_f_proxy(MPI_Comm_errhandler_fn* fn, MPI_Comm* comm, int* errcode, ...)
{
    ((MPI_F77_Comm_errhandler_fn*)fn)((MPI_Fint*)comm, (MPI_Fint*)errcode);
}

//
// mpi_comm_create_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_create_errhandler__ ( MPI_Comm_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_create_errhandler_ ( MPI_Comm_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_create_errhandler__ (v1, v2, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Comm_errhandler_set_proxy( *v2, MPIR_Comm_errhandler_f_proxy);
    }
}

//
// pmpi_comm_create_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_create_errhandler__ ( MPI_Comm_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_create_errhandler_ ( MPI_Comm_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_create_errhandler__ (v1, v2, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Comm_errhandler_set_proxy( *v2, MPIR_Comm_errhandler_f_proxy);
    }
}

//
// mpi_comm_get_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_get_errhandler_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_get_errhandler__ (v1, v2, ierr);
}

//
// pmpi_comm_get_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_get_errhandler_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_get_errhandler__ (v1, v2, ierr);
}

//
// mpi_comm_set_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_set_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_comm_set_errhandler__ (v1, v2, ierr);
}

//
// pmpi_comm_set_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_set_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_comm_set_errhandler__ (v1, v2, ierr);
}

/* The Fortran File errhandler function prototype and calling convention */
/* Do not add ... as many Fotran compilers do not support it leading to a link error with stdcall */
typedef void (FORT_CALL MPI_F77_File_errhandler_fn)(MPI_Fint*, MPI_Fint*);

/* Helper proxy function to thunk file errhandler function calls into Fortran calling convention */
static void MPIAPI MPIR_File_errhandler_f_proxy(MPI_File_errhandler_fn* fn, MPI_File* file, int* errcode, ...)
{
    ((MPI_F77_File_errhandler_fn*)fn)((MPI_Fint*)file, (MPI_Fint*)errcode);
}

//
// mpi_file_create_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_create_errhandler__ ( MPI_File_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_create_errhandler_ ( MPI_File_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_create_errhandler__ (v1, v2, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_File_errhandler_set_proxy( *v2, MPIR_File_errhandler_f_proxy);
    }
}

//
// pmpi_file_create_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_create_errhandler__ ( MPI_File_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_create_errhandler_ ( MPI_File_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_create_errhandler__ (v1, v2, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_File_errhandler_set_proxy( *v2, MPIR_File_errhandler_f_proxy);
    }
}

//
// mpi_file_get_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_errhandler_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_get_errhandler__ (v1, v2, ierr);
}

//
// pmpi_file_get_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_errhandler_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_get_errhandler__ (v1, v2, ierr);
}

//
// mpi_file_set_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_set_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_set_errhandler__ (v1, v2, ierr);
}

//
// pmpi_file_set_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_set_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_set_errhandler__ (v1, v2, ierr);
}

//
// mpi_finalized
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_finalized__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_finalized_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_finalized__ (v1, ierr);
}

//
// pmpi_finalized
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_finalized__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_finalized_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_finalized__ (v1, ierr);
}

//
// mpi_free_mem
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_free_mem__ ( void*v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_free_mem_ ( void*v1, MPI_Fint *ierr )
{
    mpi_free_mem__ (v1, ierr);
}

//
// pmpi_free_mem
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_free_mem__ ( void*v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_free_mem_ ( void*v1, MPI_Fint *ierr )
{
    pmpi_free_mem__ (v1, ierr);
}

//
// mpi_info_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_create__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_info_create_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_info_create__ (v1, ierr);
}

//
// pmpi_info_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_create__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_info_create_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_info_create__ (v1, ierr);
}

//
// mpi_info_delete
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_delete__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_info_delete_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_info_delete__ (v1, v2, ierr, d2);
}

//
// pmpi_info_delete
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_delete__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_info_delete_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_info_delete__ (v1, v2, ierr, d2);
}

//
// mpi_info_dup
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_info_dup_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_info_dup__ (v1, v2, ierr);
}

//
// pmpi_info_dup
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_info_dup_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_info_dup__ (v1, v2, ierr);
}

//
// mpi_info_free
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_info_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_info_free__ (v1, ierr);
}

//
// pmpi_info_free
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_free__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_info_free_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_info_free__ (v1, ierr);
}

//
// mpi_info_get
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_get__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, _Out_cap_(d4) char *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d4 ) ;

void FORT_CALL mpi_info_get_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), const MPI_Fint *v3, _Out_cap_(d4) char *v4 FORT_MIXED_LEN(d4), MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d2) FORT_END_LEN(d4) )
{
    mpi_info_get__ (v1, v2, v3, v4, v5, ierr, d2, d4);
}

//
// pmpi_info_get
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_get__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, _Out_cap_(d4) char *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d4 ) ;

void FORT_CALL pmpi_info_get_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), const MPI_Fint *v3, _Out_cap_(d4) char *v4 FORT_MIXED_LEN(d4), MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d2) FORT_END_LEN(d4) )
{
    pmpi_info_get__ (v1, v2, v3, v4, v5, ierr, d2, d4);
}

//
// mpi_info_get_nkeys
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_get_nkeys__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_info_get_nkeys_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_info_get_nkeys__ (v1, v2, ierr);
}

//
// pmpi_info_get_nkeys
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_get_nkeys__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_info_get_nkeys_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_info_get_nkeys__ (v1, v2, ierr);
}

//
// mpi_info_get_nthkey
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_get_nthkey__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3, MPI_Fint *ierr, MPI_Fint d3 ) ;

void FORT_CALL mpi_info_get_nthkey_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d3) )
{
    mpi_info_get_nthkey__ (v1, v2, v3, ierr, d3);
}

//
// pmpi_info_get_nthkey
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_get_nthkey__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3, MPI_Fint *ierr, MPI_Fint d3 ) ;

void FORT_CALL pmpi_info_get_nthkey_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d3) )
{
    pmpi_info_get_nthkey__ (v1, v2, v3, ierr, d3);
}

//
// mpi_info_get_valuelen
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_get_valuelen__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_info_get_valuelen_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_info_get_valuelen__ (v1, v2, v3, v4, ierr, d2);
}

//
// pmpi_info_get_valuelen
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_get_valuelen__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_info_get_valuelen_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_info_get_valuelen__ (v1, v2, v3, v4, ierr, d2);
}

//
// mpi_info_set
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_info_set__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d3 ) ;

void FORT_CALL mpi_info_set_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), _In_count_(d3) const char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d2) FORT_END_LEN(d3) )
{
    mpi_info_set__ (v1, v2, v3, ierr, d2, d3);
}

//
// pmpi_info_set
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_info_set__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d3 ) ;

void FORT_CALL pmpi_info_set_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), _In_count_(d3) const char *v3 FORT_MIXED_LEN(d3), MPI_Fint *ierr FORT_END_LEN(d2) FORT_END_LEN(d3) )
{
    pmpi_info_set__ (v1, v2, v3, ierr, d2, d3);
}

//
// mpi_pack_external
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_pack_external__ ( _In_count_(d1) const char *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v6) void *v5, const MPI_Aint * v6, MPI_Aint * v7, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_pack_external_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), void *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v6) void *v5, const MPI_Aint * v6, MPI_Aint * v7, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_pack_external__ (v1, v2, v3, v4, v5, v6, v7, ierr, d1);
}

//
// pmpi_pack_external
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_pack_external__ ( _In_count_(d1) const char *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v6) void *v5, const MPI_Aint * v6, MPI_Aint * v7, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_pack_external_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), void *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v6) void *v5, const MPI_Aint * v6, MPI_Aint * v7, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_pack_external__ (v1, v2, v3, v4, v5, v6, v7, ierr, d1);
}

//
// mpi_pack_external_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_pack_external_size__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Aint * v4, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_pack_external_size_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, const MPI_Fint *v3, MPI_Aint * v4, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_pack_external_size__ (v1, v2, v3, v4, ierr, d1);
}

//
// pmpi_pack_external_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_pack_external_size__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Aint * v4, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_pack_external_size_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, const MPI_Fint *v3, MPI_Aint * v4, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_pack_external_size__ (v1, v2, v3, v4, ierr, d1);
}

//
// mpi_request_get_status
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_request_get_status__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_request_get_status_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_request_get_status__ (v1, v2, v3, ierr);
}

//
// pmpi_request_get_status
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_request_get_status__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_request_get_status_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_request_get_status__ (v1, v2, v3, ierr);
}

//
// mpi_status_c2f
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_status_c2f__ ( MPI_Fint *v1, MPI_Fint*v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_status_c2f_ ( MPI_Fint *v1, MPI_Fint*v2, MPI_Fint *ierr )
{
    mpi_status_c2f__ (v1, v2, ierr);
}

//
// pmpi_status_c2f
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_status_c2f__ ( MPI_Fint *v1, MPI_Fint*v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_status_c2f_ ( MPI_Fint *v1, MPI_Fint*v2, MPI_Fint *ierr )
{
    pmpi_status_c2f__ (v1, v2, ierr);
}

//
// mpi_status_f2c
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_status_f2c__ ( MPI_Fint*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_status_f2c_ ( MPI_Fint*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_status_f2c__ (v1, v2, ierr);
}

//
// pmpi_status_f2c
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_status_f2c__ ( MPI_Fint*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_status_f2c_ ( MPI_Fint*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_status_f2c__ (v1, v2, ierr);
}

//
// mpi_type_create_darray
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_darray__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint v4[], _Out_cap_(*v3) MPI_Fint v5[], _Out_cap_(*v3) MPI_Fint v6[], _Out_cap_(*v3) MPI_Fint v7[], const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_darray_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint v4[], _Out_cap_(*v3) MPI_Fint v5[], _Out_cap_(*v3) MPI_Fint v6[], _Out_cap_(*v3) MPI_Fint v7[], const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    mpi_type_create_darray__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// pmpi_type_create_darray
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_darray__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint v4[], _Out_cap_(*v3) MPI_Fint v5[], _Out_cap_(*v3) MPI_Fint v6[], _Out_cap_(*v3) MPI_Fint v7[], const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_darray_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint v4[], _Out_cap_(*v3) MPI_Fint v5[], _Out_cap_(*v3) MPI_Fint v6[], _Out_cap_(*v3) MPI_Fint v7[], const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    pmpi_type_create_darray__ (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, ierr);
}

//
// mpi_type_create_hindexed
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_hindexed__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint v2[], _Out_cap_(*v1) MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_hindexed_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint v2[], _Out_cap_(*v1) MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_create_hindexed__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_create_hindexed
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_hindexed__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint v2[], _Out_cap_(*v1) MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_hindexed_ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint v2[], _Out_cap_(*v1) MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_create_hindexed__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_create_hindexed_block
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_hindexed_block__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_hindexed_block_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_create_hindexed_block__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_create_hindexed_block
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_hindexed_block__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_hindexed_block_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_create_hindexed_block__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_create_hvector
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_hvector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_hvector_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_create_hvector__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_create_hvector
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_hvector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_hvector_ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_create_hvector__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_create_indexed_block
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_indexed_block__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_indexed_block_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_create_indexed_block__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_create_indexed_block
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_indexed_block__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_indexed_block_ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_create_indexed_block__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_create_resized
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_resized__ ( const MPI_Fint *v1, const MPI_Aint * v2, const MPI_Aint * v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_resized_ ( const MPI_Fint *v1, const MPI_Aint * v2, const MPI_Aint * v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_type_create_resized__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_type_create_resized
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_resized__ ( const MPI_Fint *v1, const MPI_Aint * v2, const MPI_Aint * v3, MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_resized_ ( const MPI_Fint *v1, const MPI_Aint * v2, const MPI_Aint * v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_type_create_resized__ (v1, v2, v3, v4, ierr);
}

//
// mpi_type_create_struct
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_struct__ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Aint * v3, _In_count_(*v1) MPI_Fint v4[], MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_struct_ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Aint * v3, _In_count_(*v1) MPI_Fint v4[], MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_type_create_struct__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_type_create_struct
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_struct__ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Aint * v3, _In_count_(*v1) MPI_Fint v4[], MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_struct_ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Aint * v3, _In_count_(*v1) MPI_Fint v4[], MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_type_create_struct__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_type_create_subarray
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_subarray__ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Fint v3[], _In_count_(*v1) MPI_Fint v4[], const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_subarray_ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Fint v3[], _In_count_(*v1) MPI_Fint v4[], const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    mpi_type_create_subarray__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_type_create_subarray
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_subarray__ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Fint v3[], _In_count_(*v1) MPI_Fint v4[], const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_subarray_ ( const MPI_Fint *v1, _In_count_(*v1) MPI_Fint v2[], _In_count_(*v1) MPI_Fint v3[], _In_count_(*v1) MPI_Fint v4[], const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    pmpi_type_create_subarray__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_type_get_extent
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_extent__ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_get_extent_ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr )
{
    mpi_type_get_extent__ (v1, v2, v3, ierr);
}

//
// pmpi_type_get_extent
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_extent__ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_get_extent_ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr )
{
    pmpi_type_get_extent__ (v1, v2, v3, ierr);
}

//
// mpi_type_get_extent_x
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_extent_x__ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_get_extent_x_ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr )
{
    mpi_type_get_extent_x__ (v1, v2, v3, ierr);
}

//
// pmpi_type_get_extent_x
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_extent_x__ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_get_extent_x_ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr )
{
    pmpi_type_get_extent_x__ (v1, v2, v3, ierr);
}

//
// mpi_type_get_true_extent
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_true_extent__ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_get_true_extent_ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr )
{
    mpi_type_get_true_extent__ (v1, v2, v3, ierr);
}

//
// pmpi_type_get_true_extent
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_true_extent__ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_get_true_extent_ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr )
{
    pmpi_type_get_true_extent__ (v1, v2, v3, ierr);
}

//
// mpi_type_get_true_extent_x
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_get_true_extent_x__ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_get_true_extent_x_ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr )
{
    mpi_type_get_true_extent_x__ (v1, v2, v3, ierr);
}

//
// pmpi_type_get_true_extent_x
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_get_true_extent_x__ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_get_true_extent_x_ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Count *v3, MPI_Fint *ierr )
{
    pmpi_type_get_true_extent_x__ (v1, v2, v3, ierr);
}

//
// mpi_unpack_external
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_unpack_external__ ( _In_count_(d1) const char *v1, _In_bytecount_(*v3) void *v2, const MPI_Aint * v3, MPI_Aint * v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_unpack_external_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), _In_bytecount_(*v3) void *v2, const MPI_Aint * v3, MPI_Aint * v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_unpack_external__ (v1, v2, v3, v4, v5, v6, v7, ierr, d1);
}

//
// pmpi_unpack_external
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_unpack_external__ ( _In_count_(d1) const char *v1, _In_bytecount_(*v3) void *v2, const MPI_Aint * v3, MPI_Aint * v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_unpack_external_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), _In_bytecount_(*v3) void *v2, const MPI_Aint * v3, MPI_Aint * v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_unpack_external__ (v1, v2, v3, v4, v5, v6, v7, ierr, d1);
}

/* The Fortran Win errhandler function prototype and calling convention */
/* Do not add ... as many Fotran compilers do not support it leading to a link error with stdcall */
typedef void (FORT_CALL MPI_F77_Win_errhandler_fn)(MPI_Fint*, MPI_Fint*);

/* Helper proxy function to thunk win errhandler function calls into Fortran calling convention */
static void MPIAPI MPIR_Win_errhandler_f_proxy(MPI_Win_errhandler_fn* fn, MPI_Win* win, int* errcode, ...)
{
    ((MPI_F77_Win_errhandler_fn*)fn)((MPI_Fint*)win, (MPI_Fint*)errcode);
}

//
// mpi_win_create_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_create_errhandler__ ( MPI_Win_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_create_errhandler_ ( MPI_Win_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_create_errhandler__ (v1, v2, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Win_errhandler_set_proxy( *v2, MPIR_Win_errhandler_f_proxy);
    }
}

//
// pmpi_win_create_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_create_errhandler__ ( MPI_Win_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_create_errhandler_ ( MPI_Win_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_create_errhandler__ (v1, v2, ierr);
    if (*ierr == MPI_SUCCESS) {
         MPIR_Win_errhandler_set_proxy( *v2, MPIR_Win_errhandler_f_proxy);
    }
}

//
// mpi_win_get_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_get_errhandler_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_get_errhandler__ (v1, v2, ierr);
}

//
// pmpi_win_get_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_get_errhandler_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_get_errhandler__ (v1, v2, ierr);
}

//
// mpi_win_set_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_set_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_win_set_errhandler__ (v1, v2, ierr);
}

//
// pmpi_win_set_errhandler
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_set_errhandler_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_win_set_errhandler__ (v1, v2, ierr);
}

//
// mpi_file_open
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_open__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL mpi_file_open_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    mpi_file_open__ (v1, v2, v3, v4, v5, ierr, d2);
}

//
// pmpi_file_open
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_open__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d2 ) ;

void FORT_CALL pmpi_file_open_ ( const MPI_Fint *v1, _In_count_(d2) const char *v2 FORT_MIXED_LEN(d2), const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr FORT_END_LEN(d2) )
{
    pmpi_file_open__ (v1, v2, v3, v4, v5, ierr, d2);
}

//
// mpi_file_close
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_close__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_close_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_file_close__ (v1, ierr);
}

//
// pmpi_file_close
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_close__ ( MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_close_ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_file_close__ (v1, ierr);
}

//
// mpi_file_delete
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_delete__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_file_delete_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_file_delete__ (v1, v2, ierr, d1);
}

//
// pmpi_file_delete
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_delete__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_file_delete_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), const MPI_Fint *v2, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_file_delete__ (v1, v2, ierr, d1);
}

//
// mpi_file_set_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_set_size__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_set_size_ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr )
{
    mpi_file_set_size__ (v1, v2, ierr);
}

//
// pmpi_file_set_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_set_size__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_set_size_ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr )
{
    pmpi_file_set_size__ (v1, v2, ierr);
}

//
// mpi_file_preallocate
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_preallocate__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_preallocate_ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr )
{
    mpi_file_preallocate__ (v1, v2, ierr);
}

//
// pmpi_file_preallocate
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_preallocate__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_preallocate_ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr )
{
    pmpi_file_preallocate__ (v1, v2, ierr);
}

//
// mpi_file_get_size
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_size__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_size_ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr )
{
    mpi_file_get_size__ (v1, v2, ierr);
}

//
// pmpi_file_get_size
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_size__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_size_ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr )
{
    pmpi_file_get_size__ (v1, v2, ierr);
}

//
// mpi_file_get_group
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_get_group__ (v1, v2, ierr);
}

//
// pmpi_file_get_group
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_group_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_get_group__ (v1, v2, ierr);
}

//
// mpi_file_get_amode
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_amode__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_amode_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_get_amode__ (v1, v2, ierr);
}

//
// pmpi_file_get_amode
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_amode__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_amode_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_get_amode__ (v1, v2, ierr);
}

//
// mpi_file_set_info
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_set_info__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_set_info_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_set_info__ (v1, v2, ierr);
}

//
// pmpi_file_set_info
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_set_info__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_set_info_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_set_info__ (v1, v2, ierr);
}

//
// mpi_file_get_info
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_info__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_info_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_get_info__ (v1, v2, ierr);
}

//
// pmpi_file_get_info
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_info__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_info_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_get_info__ (v1, v2, ierr);
}

//
// mpi_file_set_view
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_set_view__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, const MPI_Fint *v4, _In_count_(d5) const char *v5, const MPI_Fint *v6, MPI_Fint *ierr, MPI_Fint d5 ) ;

void FORT_CALL mpi_file_set_view_ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, const MPI_Fint *v4, _In_count_(d5) const char *v5 FORT_MIXED_LEN(d5), const MPI_Fint *v6, MPI_Fint *ierr FORT_END_LEN(d5) )
{
    mpi_file_set_view__ (v1, v2, v3, v4, v5, v6, ierr, d5);
}

//
// pmpi_file_set_view
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_set_view__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, const MPI_Fint *v4, _In_count_(d5) const char *v5, const MPI_Fint *v6, MPI_Fint *ierr, MPI_Fint d5 ) ;

void FORT_CALL pmpi_file_set_view_ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, const MPI_Fint *v4, _In_count_(d5) const char *v5 FORT_MIXED_LEN(d5), const MPI_Fint *v6, MPI_Fint *ierr FORT_END_LEN(d5) )
{
    pmpi_file_set_view__ (v1, v2, v3, v4, v5, v6, ierr, d5);
}

//
// mpi_file_get_view
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_view__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *v3, MPI_Fint *v4, _In_count_(d5) char *v5, MPI_Fint *ierr, MPI_Fint d5 ) ;

void FORT_CALL mpi_file_get_view_ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *v3, MPI_Fint *v4, _In_count_(d5) char *v5 FORT_MIXED_LEN(d5), MPI_Fint *ierr FORT_END_LEN(d5) )
{
    mpi_file_get_view__ (v1, v2, v3, v4, v5, ierr, d5);
}

//
// pmpi_file_get_view
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_view__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *v3, MPI_Fint *v4, _In_count_(d5) char *v5, MPI_Fint *ierr, MPI_Fint d5 ) ;

void FORT_CALL pmpi_file_get_view_ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *v3, MPI_Fint *v4, _In_count_(d5) char *v5 FORT_MIXED_LEN(d5), MPI_Fint *ierr FORT_END_LEN(d5) )
{
    pmpi_file_get_view__ (v1, v2, v3, v4, v5, ierr, d5);
}

//
// mpi_file_read_at
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_file_read_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_file_read_at
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_file_read_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_file_read_at_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_at_all__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_at_all_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_file_read_at_all__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_file_read_at_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_at_all__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_at_all_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_file_read_at_all__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_file_write_at
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_file_write_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_file_write_at
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_file_write_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_file_write_at_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_at_all__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_at_all_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_file_write_at_all__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_file_write_at_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_at_all__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_at_all_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_file_write_at_all__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_file_iread_at
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_iread_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_iread_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr )
{
    mpi_file_iread_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_file_iread_at
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_iread_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_iread_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr )
{
    pmpi_file_iread_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_file_iwrite_at
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_iwrite_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_iwrite_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr )
{
    mpi_file_iwrite_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_file_iwrite_at
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_iwrite_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_iwrite_at_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr )
{
    pmpi_file_iwrite_at__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_file_read
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_read__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_read
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_read__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_read_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_all__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_all_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_read_all__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_read_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_all__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_all_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_read_all__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_write
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_write__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_write
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_write__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_write_all
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_all__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_all_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_write_all__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_write_all
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_all__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_all_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_write_all__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_iread
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_iread__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_iread_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    mpi_file_iread__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_iread
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_iread__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_iread_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    pmpi_file_iread__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_iwrite
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_iwrite__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_iwrite_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    mpi_file_iwrite__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_iwrite
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_iwrite__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_iwrite_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    pmpi_file_iwrite__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_seek
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_seek__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_seek_ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_seek__ (v1, v2, v3, ierr);
}

//
// pmpi_file_seek
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_seek__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_seek_ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_seek__ (v1, v2, v3, ierr);
}

//
// mpi_file_get_position
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_position__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_position_ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr )
{
    mpi_file_get_position__ (v1, v2, ierr);
}

//
// pmpi_file_get_position
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_position__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_position_ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr )
{
    pmpi_file_get_position__ (v1, v2, ierr);
}

//
// mpi_file_get_byte_offset
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_byte_offset__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Offset *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_byte_offset_ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Offset *v3, MPI_Fint *ierr )
{
    mpi_file_get_byte_offset__ (v1, v2, v3, ierr);
}

//
// pmpi_file_get_byte_offset
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_byte_offset__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Offset *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_byte_offset_ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Offset *v3, MPI_Fint *ierr )
{
    pmpi_file_get_byte_offset__ (v1, v2, v3, ierr);
}

//
// mpi_file_read_shared
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_read_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_read_shared
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_read_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_write_shared
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_write_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_write_shared
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_write_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_iread_shared
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_iread_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_iread_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    mpi_file_iread_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_iread_shared
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_iread_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_iread_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    pmpi_file_iread_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_iwrite_shared
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_iwrite_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_iwrite_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    mpi_file_iwrite_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_iwrite_shared
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_iwrite_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_iwrite_shared_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    pmpi_file_iwrite_shared__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_read_ordered
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_ordered__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_ordered_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_read_ordered__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_read_ordered
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_ordered__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_ordered_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_read_ordered__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_write_ordered
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_ordered__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_ordered_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_write_ordered__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_write_ordered
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_ordered__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_ordered_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_write_ordered__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_seek_shared
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_seek_shared__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_seek_shared_ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_seek_shared__ (v1, v2, v3, ierr);
}

//
// pmpi_file_seek_shared
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_seek_shared__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_seek_shared_ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_seek_shared__ (v1, v2, v3, ierr);
}

//
// mpi_file_get_position_shared
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_position_shared__ ( const MPI_Fint *v1, MPI_Offset *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_position_shared_ ( const MPI_Fint *v1, MPI_Offset *v2, MPI_Fint *ierr )
{
    mpi_file_get_position_shared__ (v1, v2, ierr);
}

//
// pmpi_file_get_position_shared
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_position_shared__ ( const MPI_Fint *v1, MPI_Offset *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_position_shared_ ( const MPI_Fint *v1, MPI_Offset *v2, MPI_Fint *ierr )
{
    pmpi_file_get_position_shared__ (v1, v2, ierr);
}

//
// mpi_file_read_at_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_at_all_begin__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_at_all_begin_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_read_at_all_begin__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_read_at_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_at_all_begin__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_at_all_begin_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_read_at_all_begin__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_read_at_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_at_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_at_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_read_at_all_end__ (v1, v2, v3, ierr);
}

//
// pmpi_file_read_at_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_at_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_at_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_read_at_all_end__ (v1, v2, v3, ierr);
}

//
// mpi_file_write_at_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_at_all_begin__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_at_all_begin_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    mpi_file_write_at_all_begin__ (v1, v2, v3, v4, v5, ierr);
}

//
// pmpi_file_write_at_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_at_all_begin__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_at_all_begin_ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    pmpi_file_write_at_all_begin__ (v1, v2, v3, v4, v5, ierr);
}

//
// mpi_file_write_at_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_at_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_at_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_write_at_all_end__ (v1, v2, v3, ierr);
}

//
// pmpi_file_write_at_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_at_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_at_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_write_at_all_end__ (v1, v2, v3, ierr);
}

//
// mpi_file_read_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_all_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_all_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_file_read_all_begin__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_file_read_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_all_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_all_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_file_read_all_begin__ (v1, v2, v3, v4, ierr);
}

//
// mpi_file_read_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_read_all_end__ (v1, v2, v3, ierr);
}

//
// pmpi_file_read_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_read_all_end__ (v1, v2, v3, ierr);
}

//
// mpi_file_write_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_all_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_all_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_file_write_all_begin__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_file_write_all_begin
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_all_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_all_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_file_write_all_begin__ (v1, v2, v3, v4, ierr);
}

//
// mpi_file_write_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_write_all_end__ (v1, v2, v3, ierr);
}

//
// pmpi_file_write_all_end
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_all_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_write_all_end__ (v1, v2, v3, ierr);
}

//
// mpi_file_read_ordered_begin
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_ordered_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_ordered_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_file_read_ordered_begin__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_file_read_ordered_begin
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_ordered_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_ordered_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_file_read_ordered_begin__ (v1, v2, v3, v4, ierr);
}

//
// mpi_file_read_ordered_end
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_read_ordered_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_read_ordered_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_read_ordered_end__ (v1, v2, v3, ierr);
}

//
// pmpi_file_read_ordered_end
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_read_ordered_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_read_ordered_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_read_ordered_end__ (v1, v2, v3, ierr);
}

//
// mpi_file_write_ordered_begin
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_ordered_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_ordered_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    mpi_file_write_ordered_begin__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_file_write_ordered_begin
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_ordered_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_ordered_begin_ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    pmpi_file_write_ordered_begin__ (v1, v2, v3, v4, ierr);
}

//
// mpi_file_write_ordered_end
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_write_ordered_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_write_ordered_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_file_write_ordered_end__ (v1, v2, v3, ierr);
}

//
// pmpi_file_write_ordered_end
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_write_ordered_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_write_ordered_end_ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_file_write_ordered_end__ (v1, v2, v3, ierr);
}

//
// mpi_file_get_type_extent
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_type_extent__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_FAint * v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_type_extent_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_FAint * v3, MPI_Fint *ierr )
{
    mpi_file_get_type_extent__ (v1, v2, v3, ierr);
}

//
// pmpi_file_get_type_extent
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_type_extent__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_FAint * v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_type_extent_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_FAint * v3, MPI_Fint *ierr )
{
    pmpi_file_get_type_extent__ (v1, v2, v3, ierr);
}

/* This isn't a callable function */
//
// mpi_conversion_fn_null
//
FORT_IMPORT int MSMPI_FORT_CALL mpi_conversion_fn_null__ ( void*v1, MPI_Fint*v2, MPI_Fint*v3, void*v4, MPI_Offset*v5, MPI_Fint *v6, MPI_Fint*v7, MPI_Fint *ierr ) ;

int FORT_CALL mpi_conversion_fn_null_ ( void*v1, MPI_Fint*v2, MPI_Fint*v3, void*v4, MPI_Offset*v5, MPI_Fint *v6, MPI_Fint*v7, MPI_Fint *ierr )
{
    return mpi_conversion_fn_null__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// pmpi_conversion_fn_null
//
FORT_IMPORT int MSMPI_FORT_CALL pmpi_conversion_fn_null__ ( void*v1, MPI_Fint*v2, MPI_Fint*v3, void*v4, MPI_Offset*v5, MPI_Fint *v6, MPI_Fint*v7, MPI_Fint *ierr ) ;

int FORT_CALL pmpi_conversion_fn_null_ ( void*v1, MPI_Fint*v2, MPI_Fint*v3, void*v4, MPI_Offset*v5, MPI_Fint *v6, MPI_Fint*v7, MPI_Fint *ierr )
{
    return pmpi_conversion_fn_null__ (v1, v2, v3, v4, v5, v6, v7, ierr);
}

//
// mpi_register_datarep
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_register_datarep__ ( _In_count_(d1) const char *v1, _In_opt_ MPI_Datarep_conversion_function *v2, _In_opt_ MPI_Datarep_conversion_function*v3, _In_ MPI_Datarep_extent_function*v4, _In_opt_ void *v5, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL mpi_register_datarep_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), _In_opt_ MPI_Datarep_conversion_function *v2, _In_opt_ MPI_Datarep_conversion_function*v3, _In_ MPI_Datarep_extent_function*v4, _In_opt_ void *v5, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    mpi_register_datarep__ (v1, v2, v3, v4, v5, ierr, d1);
}

//
// pmpi_register_datarep
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_register_datarep__ ( _In_count_(d1) const char *v1, _In_opt_ MPI_Datarep_conversion_function *v2, _In_opt_ MPI_Datarep_conversion_function*v3, _In_ MPI_Datarep_extent_function*v4, _In_opt_ void *v5, MPI_Fint *ierr, MPI_Fint d1 ) ;

void FORT_CALL pmpi_register_datarep_ ( _In_count_(d1) const char *v1 FORT_MIXED_LEN(d1), _In_opt_ MPI_Datarep_conversion_function *v2, _In_opt_ MPI_Datarep_conversion_function*v3, _In_ MPI_Datarep_extent_function*v4, _In_opt_ void *v5, MPI_Fint *ierr FORT_END_LEN(d1) )
{
    pmpi_register_datarep__ (v1, v2, v3, v4, v5, ierr, d1);
}

//
// mpi_file_set_atomicity
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_set_atomicity__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_set_atomicity_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_set_atomicity__ (v1, v2, ierr);
}

//
// pmpi_file_set_atomicity
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_set_atomicity__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_set_atomicity_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_set_atomicity__ (v1, v2, ierr);
}

//
// mpi_file_get_atomicity
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_get_atomicity__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_get_atomicity_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_file_get_atomicity__ (v1, v2, ierr);
}

//
// pmpi_file_get_atomicity
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_get_atomicity__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_get_atomicity_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_file_get_atomicity__ (v1, v2, ierr);
}

//
// mpi_file_sync
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_file_sync__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_file_sync_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_file_sync__ (v1, ierr);
}

//
// pmpi_file_sync
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_file_sync__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_file_sync_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_file_sync__ (v1, ierr);
}

//
// mpi_pcontrol
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_pcontrol__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL mpi_pcontrol_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    mpi_pcontrol__ (v1, ierr);
}

//
// pmpi_pcontrol
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_pcontrol__ ( const MPI_Fint *v1, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_pcontrol_ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    pmpi_pcontrol__ (v1, ierr);
}

//
// mpi_address
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_address__ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_address_ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_address__ (v1, v2, ierr);
}

//
// pmpi_address
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_address__ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_address_ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_address__ (v1, v2, ierr);
}

//
// mpi_get_address
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_get_address__ ( void*v1, MPI_Aint*v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_get_address_ ( void*v1, MPI_Aint*v2, MPI_Fint *ierr )
{
    mpi_get_address__ (v1, v2, ierr);
}

//
// pmpi_get_address
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_get_address__ ( void*v1, MPI_Aint*v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_get_address_ ( void*v1, MPI_Aint*v2, MPI_Fint *ierr )
{
    pmpi_get_address__ (v1, v2, ierr);
}

//
// mpi_wtime
//
FORT_IMPORT double MSMPI_FORT_CALL mpi_wtime__ ( void ) ;

double FORT_CALL mpi_wtime_ ( void )
{
    return mpi_wtime__ ();
}

//
// pmpi_wtime
//
FORT_IMPORT double MSMPI_FORT_CALL pmpi_wtime__ ( void ) ;

double FORT_CALL pmpi_wtime_ ( void )
{
    return pmpi_wtime__ ();
}

//
// mpi_wtick
//
FORT_IMPORT double MSMPI_FORT_CALL mpi_wtick__ ( void ) ;

double FORT_CALL mpi_wtick_ ( void )
{
    return mpi_wtick__ ();
}

//
// pmpi_wtick
//
FORT_IMPORT double MSMPI_FORT_CALL pmpi_wtick__ ( void ) ;

double FORT_CALL pmpi_wtick_ ( void )
{
    return pmpi_wtick__ ();
}

/* The F77 attr copy function prototype and calling convention */
typedef void (FORT_CALL F77_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *,MPI_Fint *, MPI_Fint *, MPI_Fint *);

/* Helper proxy function to thunk the attr copy function call into F77 calling convention */
static
int
MPIAPI
MPIR_Comm_copy_attr_f77_proxy(
    MPI_Comm_copy_attr_function* user_function,
    MPI_Comm comm,
    int keyval,
    void* extra_state,
    void* value,
    void** new_value,
    int* flag
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)comm;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Fint fvalue = MPIU_PtrToInt(value);
    MPI_Fint* fextra  = (MPI_Fint*)extra_state;
    MPI_Fint fnew = 0;
    MPI_Fint fflag = 0;

    ((F77_CopyFunction*)user_function)( &fhandle, &fkeyval, fextra, &fvalue, &fnew, &fflag, &ierr );

    *flag = fflag;
    *new_value = MPIU_IntToPtr(fnew);
    return ierr;
}


/* The F77 attr delete function prototype and calling convention */
typedef void (FORT_CALL F77_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *);

/* Helper proxy function to thunk the attr delete function call into F77 calling convention */
static
int
MPIAPI
MPIR_Comm_delete_attr_f77_proxy(
    MPI_Comm_delete_attr_function* user_function,
    MPI_Comm comm,
    int keyval,
    void* value,
    void* extra_state
    )
{
    OACR_USE_PTR( value );
    MPI_Fint ierr = 0;
    MPI_Fint fhandle = (MPI_Fint)comm;
    MPI_Fint fkeyval = (MPI_Fint)keyval;
    MPI_Fint fvalue = MPIU_PtrToInt(value);
    MPI_Fint* fextra  = (MPI_Fint*)extra_state;

    ((F77_DeleteFunction*)user_function)( &fhandle, &fkeyval, &fvalue, fextra, &ierr );
    return ierr;
}

//
// mpi_keyval_create
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_keyval_create__ ( MPI_Copy_function v1, MPI_Delete_function v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_keyval_create_ ( MPI_Copy_function v1, MPI_Delete_function v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    mpi_keyval_create__ (v1, v2, v3, v4, ierr);
    if (!*ierr) {
        MPIR_Keyval_set_proxy( *v3, MPIR_Comm_copy_attr_f77_proxy,  MPIR_Comm_delete_attr_f77_proxy);
    }
}

//
// pmpi_keyval_create
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_keyval_create__ ( MPI_Copy_function v1, MPI_Delete_function v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_keyval_create_ ( MPI_Copy_function v1, MPI_Delete_function v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    pmpi_keyval_create__ (v1, v2, v3, v4, ierr);
    if (!*ierr) {
        MPIR_Keyval_set_proxy( *v3, MPIR_Comm_copy_attr_f77_proxy,  MPIR_Comm_delete_attr_f77_proxy);
    }
}

//
// mpi_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    mpi_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    pmpi_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    mpi_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    pmpi_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// mpi_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_null_copy_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_null_copy_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_null_copy_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_null_copy_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_comm_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    mpi_comm_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_comm_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    pmpi_comm_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_comm_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    mpi_comm_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_comm_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    pmpi_comm_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// mpi_comm_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_comm_null_copy_fn__ ( MPI_Fint *v1, MPI_Fint *v2, void *v3, void *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_comm_null_copy_fn_ ( MPI_Fint *v1, MPI_Fint *v2, void *v3, void *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_comm_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_comm_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_comm_null_copy_fn__ ( MPI_Fint *v1, MPI_Fint *v2, void *v3, void *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_comm_null_copy_fn_ ( MPI_Fint *v1, MPI_Fint *v2, void *v3, void *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_comm_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_win_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void **v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void **v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    mpi_win_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_win_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void **v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void **v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    pmpi_win_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_win_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    mpi_win_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_win_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    pmpi_win_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// mpi_win_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_win_null_copy_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_win_null_copy_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_win_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_win_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_win_null_copy_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_win_null_copy_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_win_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_type_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    mpi_type_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_type_dup_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_dup_fn__ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_dup_fn_ ( MPI_Fint v1, MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
    pmpi_type_dup_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_type_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    mpi_type_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// pmpi_type_null_delete_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_null_delete_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_null_delete_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
    pmpi_type_null_delete_fn__ (v1, v2, v3, v4, ierr);
}

//
// mpi_type_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_null_copy_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_null_copy_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    mpi_type_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// pmpi_type_null_copy_fn
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_null_copy_fn__ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_null_copy_fn_ ( MPI_Fint*v1, MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    pmpi_type_null_copy_fn__ (v1, v2, v3, v4, v5, v6, ierr);
}

//
// mpi_type_create_f90_real
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_f90_real__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_f90_real_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_type_create_f90_real__ (v1, v2, v3, ierr);
}

//
// pmpi_type_create_f90_real
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_f90_real__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_f90_real_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_type_create_f90_real__ (v1, v2, v3, ierr);
}

//
// mpi_type_create_f90_complex
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_f90_complex__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_f90_complex_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    mpi_type_create_f90_complex__ (v1, v2, v3, ierr);
}

//
// pmpi_type_create_f90_complex
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_f90_complex__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_f90_complex_ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    pmpi_type_create_f90_complex__ (v1, v2, v3, ierr);
}

//
// mpi_type_create_f90_integer
//
FORT_IMPORT void MSMPI_FORT_CALL mpi_type_create_f90_integer__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL mpi_type_create_f90_integer_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    mpi_type_create_f90_integer__ (v1, v2, ierr);
}

//
// pmpi_type_create_f90_integer
//
FORT_IMPORT void MSMPI_FORT_CALL pmpi_type_create_f90_integer__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr ) ;

void FORT_CALL pmpi_type_create_f90_integer_ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    pmpi_type_create_f90_integer__ (v1, v2, ierr);
}



#if defined(__cplusplus)
}
#endif

