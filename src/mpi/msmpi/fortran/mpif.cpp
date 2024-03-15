// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2015 by Microsoft Corporation
 *      See COPYRIGHT in top-level directory.
 *
 */
#include "mpiimpl.h"
#include "mpi_fortlogical.h"
#include "nmpi.h"
#include "mpifbind.h"

#if defined(__cplusplus)
extern "C" {
#endif

#pragma warning(disable:4100)

#define MPIR_Error(e, f) MPIR_Err_return_comm(NULL, f, e)

#define mpirinitc_ mpirinitc
#define mpirinitc2_ mpirinitc2

static void *MPIR_F_MPI_BOTTOM        = 0;
static void *MPIR_F_MPI_IN_PLACE      = 0;
static int  *MPI_F_ERRCODES_IGNORE    = 0;
static int  *MPIR_F_MPI_UNWEIGHTED    = 0;
static int  *MPIR_F_MPI_WEIGHTS_EMPTY = 0;
static void *MPI_F_ARGVS_NULL         = 0;


/*
    MPI_F_STATUS_IGNORE etc are declared within mpi.h (MPI-2 standard requires this)
    The variable are meant to be used from C

void *MPI_F_STATUS_IGNORE   = 0;
void *MPI_F_STATUSES_IGNORE = 0;
*/


void MSMPI_FORT_CALL mpirinitc_( void *a, void *b, void *c, void *d,
                                            void *e, void* f, void* g,
                                            void *h, MPI_Fint f1 )
{
    MPIR_F_MPI_BOTTOM        = a;
    MPIR_F_MPI_IN_PLACE      = b;
    MPI_F_STATUS_IGNORE      = (MPI_Fint *)c;
    MPI_F_STATUSES_IGNORE    = (MPI_Fint *)d;
    MPI_F_ERRCODES_IGNORE    = (int *)e;
    MPIR_F_MPI_UNWEIGHTED    = (int *)f;
    MPIR_F_MPI_WEIGHTS_EMPTY = (int *)g;

    MPI_F_ARGVS_NULL         = h;
}

/* Initialize the Fortran ARGV_NULL to a blank.  Using this routine
   avoids potential problems with string manipulation routines that
   exist in the Fortran runtime but not in the C runtime libraries */
void MSMPI_FORT_CALL mpirinitc2_( _Out_ char *a, MPI_Fint a1 )
{
    *a = ' ';
}


void MSMPI_FORT_CALL mpi_init__ ( MPI_Fint *ierr )
{
    *ierr = MPI_Init( 0, 0 );
}


void MSMPI_FORT_CALL mpi_init_thread__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Init_thread( 0, 0, *v1, v2 );
}


static void MPIAPI MPIR_Comm_errhandler_f_proxy(MPI_Comm_errhandler_fn* fn, MPI_Comm* comm, int* errcode, ...);


void MSMPI_FORT_CALL mpi_send__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = NMPI_Send( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_recv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{


    if (v7 == MPI_F_STATUS_IGNORE) { v7 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = NMPI_Recv( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Status *)v7 );
}


void MSMPI_FORT_CALL mpi_get_count__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Get_count( (MPI_Status *)(v1), (MPI_Datatype)(*v2), v3 );
}


void MSMPI_FORT_CALL mpi_bsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_Bsend( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_ssend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = NMPI_Ssend( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_rsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = NMPI_Rsend( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_buffer_attach__ ( void*v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Buffer_attach( v1, *v2 );
}


void MSMPI_FORT_CALL mpi_buffer_detach__ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    void *t1 = v1;
    *ierr = MPI_Buffer_detach( &t1, v2 );
}


void MSMPI_FORT_CALL mpi_isend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Isend( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_ibsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Ibsend( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_issend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Issend( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_irsend__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Irsend( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_irecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Irecv( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_wait__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{


    if (v2 == MPI_F_STATUS_IGNORE) { v2 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = NMPI_Wait( (MPI_Request *)(v1), (MPI_Status *)v2 );
}


void MSMPI_FORT_CALL mpi_test__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    int l2;


    if (v3 == MPI_F_STATUS_IGNORE) { v3 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = MPI_Test( (MPI_Request *)(v1), &l2, (MPI_Status *)v3 );
    *v2 = MPIR_TO_FLOG(l2);
}


void MSMPI_FORT_CALL mpi_request_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Request_free( (MPI_Request *)(v1) );
}


void MSMPI_FORT_CALL mpi_waitany__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    int l3;


    if (v4 == MPI_F_STATUS_IGNORE) { v4 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = NMPI_Waitany( *v1, (MPI_Request *)(v2),  &l3, (MPI_Status *)v4 );
    *v3 = (MPI_Fint)l3;
    if (l3 >= 0) *v3 = *v3 + 1;
}


void MSMPI_FORT_CALL mpi_testany__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    int l3;
    int l4;


    if (v5 == MPI_F_STATUS_IGNORE) { v5 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = MPI_Testany( *v1, (MPI_Request *)(v2),  &l3, &l4, (MPI_Status *)v5 );
    *v3 = (MPI_Fint)l3;
    if (l3 >= 0) *v3 = *v3 + 1;
    *v4 = MPIR_TO_FLOG(l4);
}


void MSMPI_FORT_CALL mpi_waitall__ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, _Out_cap_(*v1) MPI_Fint *v3, MPI_Fint *ierr )
{


    if (v3 == MPI_F_STATUSES_IGNORE) { v3 = (MPI_Fint *)MPI_STATUSES_IGNORE; }
    *ierr = NMPI_Waitall( *v1, (MPI_Request *)(v2), (MPI_Status *)v3 );
}


void MSMPI_FORT_CALL mpi_testall__ ( const MPI_Fint *v1, _Inout_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *ierr )
{
    int l3;


    if (v4 == MPI_F_STATUSES_IGNORE) { v4 = (MPI_Fint *)MPI_STATUSES_IGNORE; }
    *ierr = MPI_Testall( *v1, (MPI_Request *)(v2), &l3, (MPI_Status *)v4 );
    *v3 = MPIR_TO_FLOG(l3);
}


void MSMPI_FORT_CALL mpi_waitsome__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, _Out_cap_(*v1) MPI_Fint *v4, _Out_cap_(*v1) MPI_Fint *v5, MPI_Fint *ierr )
{


    if (v5 == MPI_F_STATUSES_IGNORE) { v5 = (MPI_Fint *)MPI_STATUSES_IGNORE; }
    *ierr = NMPI_Waitsome( *v1, (MPI_Request *)(v2), v3, v4, (MPI_Status *)v5 );

    {int li;
     for (li=0; li<*v3; li++) {
        if (v4[li] >= 0) v4[li] += 1;
     }
    }
}


void MSMPI_FORT_CALL mpi_testsome__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *v3, _Out_cap_(*v1) MPI_Fint *v4, _Out_cap_(*v1) MPI_Fint *v5, MPI_Fint *ierr )
{


    if (v5 == MPI_F_STATUSES_IGNORE) { v5 = (MPI_Fint *)MPI_STATUSES_IGNORE; }
    *ierr = MPI_Testsome( *v1, (MPI_Request *)(v2), v3, v4, (MPI_Status *)v5 );

    {int li;
     for (li=0; li<*v3; li++) {
        if (v4[li] >= 0) v4[li] += 1;
     }
    }
}


void MSMPI_FORT_CALL mpi_iprobe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    int l4;


    if (v5 == MPI_F_STATUS_IGNORE) { v5 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = MPI_Iprobe( *v1, *v2, (MPI_Comm)(*v3), &l4, (MPI_Status *)v5 );
    *v4 = MPIR_TO_FLOG(l4);
}


void MSMPI_FORT_CALL mpi_probe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{


    if (v4 == MPI_F_STATUS_IGNORE) { v4 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = NMPI_Probe( *v1, *v2, (MPI_Comm)(*v3), (MPI_Status *)v4 );
}


void MSMPI_FORT_CALL mpi_improbe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{


    if (v6 == MPI_F_STATUS_IGNORE) { v6 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = MPI_Improbe( *v1, *v2, (MPI_Comm)(*v3), v4, (MPI_Message *)(v5), (MPI_Status *)(v6) );
}


void MSMPI_FORT_CALL mpi_mprobe__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{


    if (v5 == MPI_F_STATUS_IGNORE) { v5 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = MPI_Mprobe( *v1, *v2, (MPI_Comm)(*v3), (MPI_Message *)(v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_mrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{


    if (v5 == MPI_F_STATUS_IGNORE) { v5 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = MPI_Mrecv( v1, *v2, (MPI_Datatype)(*v3), (MPI_Message *)(v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_imrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{


    *ierr = MPI_Imrecv( v1, *v2, (MPI_Datatype)(*v3), (MPI_Message *)(v4), (MPI_Request *)(v5) );
}


void MSMPI_FORT_CALL mpi_cancel__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Cancel( (MPI_Request *)(v1) );
}


void MSMPI_FORT_CALL mpi_test_cancelled__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    int l2;
    *ierr = MPI_Test_cancelled( (MPI_Status *)(v1), &l2 );
    *v2 = MPIR_TO_FLOG(l2);
}


void MSMPI_FORT_CALL mpi_send_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Send_init( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_bsend_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Bsend_init( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_ssend_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Ssend_init( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_rsend_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Rsend_init( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_recv_init__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Recv_init( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, (MPI_Comm)(*v6), (MPI_Request *)(v7) );
}


void MSMPI_FORT_CALL mpi_start__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Start( (MPI_Request *)(v1) );
}


void MSMPI_FORT_CALL mpi_startall__ ( const MPI_Fint *v1, _Out_cap_(*v1) MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Startall( *v1, (MPI_Request *)(v2) );
}


void MSMPI_FORT_CALL mpi_sendrecv__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, void*v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, MPI_Fint *v12, MPI_Fint *ierr )
{


    if (v12 == MPI_F_STATUS_IGNORE) { v12 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = NMPI_Sendrecv( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, v6, *v7, (MPI_Datatype)(*v8), *v9, *v10, (MPI_Comm)(*v11), (MPI_Status *)v12 );
}


void MSMPI_FORT_CALL mpi_sendrecv_replace__ ( void*v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{


    if (v9 == MPI_F_STATUS_IGNORE) { v9 = (MPI_Fint*)MPI_STATUS_IGNORE; }
    *ierr = NMPI_Sendrecv_replace( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, *v7, (MPI_Comm)(*v8), (MPI_Status *)v9 );
}


void MSMPI_FORT_CALL mpi_type_contiguous__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_contiguous( *v1, (MPI_Datatype)(*v2), (MPI_Datatype *)(v3) );
}


void MSMPI_FORT_CALL mpi_type_vector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_vector( *v1, *v2, *v3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_hvector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    MPI_Aint l3;
    l3 = (MPI_Aint)*v3;
    *ierr = MPI_Type_create_hvector( *v1, *v2, l3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_indexed__ ( const MPI_Fint *v1, _In_reads_(*v1) const MPI_Fint *v2, _In_reads_(*v1) const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_indexed( *v1, v2, v3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_hindexed__ ( const MPI_Fint *v1, _In_reads_(*v1) const MPI_Fint *v2, const MPI_Fint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    MPI_Aint *l3;

#ifdef HAVE_AINT_LARGER_THAN_FINT
    if (*v1 > 0) {
        int li;
        l3 = (MPI_Aint *)MPIU_Malloc( *v1 * sizeof(MPI_Aint) );
        if(l3 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup3;
        }
        for (li=0; li<*v1; li++)
            l3[li] = v3[li];
    }
    else l3 = 0;
#else
    l3 = const_cast<MPI_Fint *>(v3);
#endif
    *ierr = MPI_Type_create_hindexed( *v1, v2, l3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );

#ifdef HAVE_AINT_LARGER_THAN_FINT
    if (l3) { MPIU_Free(l3); }
fn_cleanup3:;
#endif
}


void MSMPI_FORT_CALL mpi_type_struct__ ( const MPI_Fint *v1, _In_reads_(*v1) const MPI_Fint *v2, const MPI_Fint * v3, _Out_cap_(*v1) MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    MPI_Aint *l3;

#ifdef HAVE_AINT_LARGER_THAN_FINT
    if (*v1 > 0) {
        int li;
        l3 = (MPI_Aint *)MPIU_Malloc( *v1 * sizeof(MPI_Aint) );
        if(l3 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup3;
        }
        for (li=0; li<*v1; li++)
            l3[li] = v3[li];
    }
    else l3 = 0;
#else
    l3 = const_cast<MPI_Fint *>(v3);
#endif
    *ierr = MPI_Type_create_struct( *v1, v2, l3, (MPI_Datatype *)(v4), (MPI_Datatype *)(v5) );

#ifdef HAVE_AINT_LARGER_THAN_FINT
    if (l3) { MPIU_Free(l3); }
fn_cleanup3:;
#endif
}


void MSMPI_FORT_CALL mpi_type_extent__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    MPI_Aint l2;
    MPI_Aint lb;
    *ierr = MPI_Type_get_extent( (MPI_Datatype)(*v1), &lb, &l2 );
    *v2 = (MPI_Fint)(l2);
}


void MSMPI_FORT_CALL mpi_type_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Type_size( (MPI_Datatype)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_type_size_x__ ( const MPI_Fint *v1, MPI_Count *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Type_size_x( (MPI_Datatype)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_type_lb__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    MPI_Aint l2;
    MPI_Aint extent;
    *ierr = MPI_Type_get_extent( (MPI_Datatype)(*v1), &l2, &extent );
    *v2 = (MPI_Fint)(l2);
}


void MSMPI_FORT_CALL mpi_type_ub__ ( const MPI_Fint *v1, MPI_Fint * v2, MPI_Fint *ierr )
{
    MPI_Aint l2;
    MPI_Aint extent;
    *ierr = MPI_Type_get_extent( (MPI_Datatype)(*v1), &l2, &extent );
    *v2 = (MPI_Fint)(l2 + extent);
}


void MSMPI_FORT_CALL mpi_type_commit__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Type_commit( (MPI_Datatype *)(v1) );
}


void MSMPI_FORT_CALL mpi_type_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Type_free( (MPI_Datatype *)(v1) );
}


void MSMPI_FORT_CALL mpi_get_elements__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Get_elements( (MPI_Status *)(v1), (MPI_Datatype)(*v2), v3 );
}


void MSMPI_FORT_CALL mpi_get_elements_x__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Count *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Get_elements_x( (MPI_Status *)(v1), (MPI_Datatype)(*v2), v3 );
}


void MSMPI_FORT_CALL mpi_pack__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v5) void *v4, const MPI_Fint *v5, MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Pack( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, v6, (MPI_Comm)(*v7) );
}


void MSMPI_FORT_CALL mpi_unpack__ ( _In_reads_bytes_(*v2) void *v1, const MPI_Fint *v2, MPI_Fint *v3, void*v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Unpack( v1, *v2, v3, v4, *v5, (MPI_Datatype)(*v6), (MPI_Comm)(*v7) );
}


void MSMPI_FORT_CALL mpi_pack_size__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Pack_size( *v1, (MPI_Datatype)(*v2), (MPI_Comm)(*v3), v4 );
}


void MSMPI_FORT_CALL mpi_barrier__ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = NMPI_Barrier( (MPI_Comm)(*v1) );
}


void MSMPI_FORT_CALL mpi_bcast__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = NMPI_Bcast( v1, *v2, (MPI_Datatype)(*v3), *v4, (MPI_Comm)(*v5) );
}


void MSMPI_FORT_CALL mpi_gather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Gather( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), *v7, (MPI_Comm)(*v8) );
}


void MSMPI_FORT_CALL mpi_gatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Gatherv( v1, *v2, (MPI_Datatype)(*v3), v4, v5, v6, (MPI_Datatype)(*v7), *v8, (MPI_Comm)(*v9) );
}


void MSMPI_FORT_CALL mpi_scatter__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Scatter( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), *v7, (MPI_Comm)(*v8) );
}


void MSMPI_FORT_CALL mpi_scatterv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v5 == MPIR_F_MPI_IN_PLACE )
    {
        v5 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Scatterv( v1, v2, v3, (MPI_Datatype)(*v4), v5, *v6, (MPI_Datatype)(*v7), *v8, (MPI_Comm)(*v9) );
}


void MSMPI_FORT_CALL mpi_allgather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Allgather( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), (MPI_Comm)(*v7) );
}


void MSMPI_FORT_CALL mpi_allgatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Allgatherv( v1, *v2, (MPI_Datatype)(*v3), v4, v5, v6, (MPI_Datatype)(*v7), (MPI_Comm)(*v8) );
}


void MSMPI_FORT_CALL mpi_alltoall__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Alltoall( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), (MPI_Comm)(*v7) );
}


void MSMPI_FORT_CALL mpi_alltoallv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v5 == MPIR_F_MPI_IN_PLACE )
    {
        v5 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Alltoallv( v1, v2, v3, (MPI_Datatype)(*v4), v5, v6, v7, (MPI_Datatype)(*v8), (MPI_Comm)(*v9) );
}


void MSMPI_FORT_CALL mpi_reduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Reduce( v1, v2, *v3, (MPI_Datatype)(*v4), (MPI_Op)(*v5), *v6, (MPI_Comm)(*v7) );
}


void MSMPI_FORT_CALL mpi_ireduce__( void *v1, void* v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Ireduce( v1, v2, *v3, (MPI_Datatype)(*v4), (MPI_Op)(*v5), *v6, (MPI_Comm)(*v7), (MPI_Request*)v8 );
}


void MSMPI_FORT_CALL mpi_reduce_local__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr)
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Reduce_local( v1, v2, *v3, (MPI_Datatype)(*v4), *v5);
}


void MSMPI_FORT_CALL mpi_iallgather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Iallgather( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), (MPI_Comm)(*v7), (MPI_Request*)v8 );
}


void MSMPI_FORT_CALL mpi_iallgatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Iallgatherv( v1, *v2, (MPI_Datatype)(*v3), v4, v5, v6, (MPI_Datatype)(*v7), (MPI_Comm)(*v8), (MPI_Request*)v9 );
}


void MSMPI_FORT_CALL mpi_iallreduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Iallreduce( v1, v2, *v3, (MPI_Datatype)(*v4), (MPI_Op)*v5, (MPI_Comm)(*v6), (MPI_Request*)v7 );
}


void MSMPI_FORT_CALL mpi_ialltoall__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Ialltoall( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), (MPI_Comm)(*v7), (MPI_Request*)v8 );
}


void MSMPI_FORT_CALL mpi_ialltoallv__(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr)
{
    if (v1 == MPIR_F_MPI_IN_PLACE)
    {
        v1 = MPI_IN_PLACE;
    }
    if (v5 == MPIR_F_MPI_IN_PLACE)
    {
        v5 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Ialltoallv(v1, v2, v3, (MPI_Datatype)(*v4), v5, v6, v7, (MPI_Datatype)(*v8), (MPI_Comm)(*v9), (MPI_Request*)v10);
}


void MSMPI_FORT_CALL mpi_ialltoallw__(const void*v1, const MPI_Fint v2[], const MPI_Fint v3[], const MPI_Fint v4[], void*v5, const MPI_Fint v6[], const MPI_Fint v7[], const MPI_Fint v8[], MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr)
{
    if (v1 == MPIR_F_MPI_IN_PLACE)
    {
        v1 = MPI_IN_PLACE;
    }
    if (v5 == MPIR_F_MPI_IN_PLACE)
    {
        v5 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Ialltoallw(v1, v2, v3, v4, v5, v6, v7, v8, (MPI_Comm)(*v9), (MPI_Request*)v10);
}


void MSMPI_FORT_CALL mpi_ibarrier__(const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr)
{
    *ierr = NMPI_Ibarrier( (MPI_Comm)(*v1), (MPI_Request *)v2 );
}


void MSMPI_FORT_CALL mpi_ibcast__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = NMPI_Ibcast( v1, *v2, (MPI_Datatype)(*v3), *v4, (MPI_Comm)(*v5), (MPI_Request *)v6 );
}


void MSMPI_FORT_CALL mpi_iexscan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    if (v1 == MPIR_F_MPI_IN_PLACE)
    {
        v1 = MPI_IN_PLACE;
    }
    if (v2 == MPIR_F_MPI_IN_PLACE)
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Iexscan( v1, v2, *v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6), (MPI_Request*)(v7) );
}


void MSMPI_FORT_CALL mpi_igather__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    if ( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Igather( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), *v7, (MPI_Comm)(*v8), (MPI_Request *)v9 );
}


void MSMPI_FORT_CALL mpi_igatherv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Igatherv( v1, *v2, (MPI_Datatype)(*v3), v4, v5, v6, (MPI_Datatype)(*v7), *v8, (MPI_Comm)(*v9), (MPI_Request *)v10 );
}


void MSMPI_FORT_CALL mpi_ireduce_scatter__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Ireduce_scatter( v1, v2, v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6), (MPI_Request*)v7 );
}


void MSMPI_FORT_CALL mpi_ireduce_scatter_block__(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr)
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Ireduce_scatter_block(v1, v2, *v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6), (MPI_Request*)v7 );
}


void MSMPI_FORT_CALL mpi_iscan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    if (v1 == MPIR_F_MPI_IN_PLACE)
    {
        v1 = MPI_IN_PLACE;
    }
    if (v2 == MPIR_F_MPI_IN_PLACE)
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Iscan( v1, v2, *v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6), (MPI_Request*)(v7) );
}


void MSMPI_FORT_CALL mpi_iscatter__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v4 == MPIR_F_MPI_IN_PLACE )
    {
        v4 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Iscatter( v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), *v7, (MPI_Comm)(*v8), (MPI_Request *)v9 );
}


void MSMPI_FORT_CALL mpi_iscatterv__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v5 == MPIR_F_MPI_IN_PLACE )
    {
        v5 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Iscatterv( v1, v2, v3, (MPI_Datatype)(*v4), v5, *v6, (MPI_Datatype)(*v7), *v8, (MPI_Comm)(*v9), (MPI_Request *)v10 );
}


/* The Fortran Op function prototype and calling convention */
typedef void (MSMPI_FORT_CALL MPI_F77_User_function)(const void *, void *, const MPI_Fint *, const MPI_Fint * );

/* Helper proxy function to thunk op function calls into Fortran calling convention */
static void MPIAPI MPIR_Op_f_proxy(MPI_User_function* uop, const void* invec, void* outvec, const int* len, const MPI_Datatype* dtype)
{
    ((MPI_F77_User_function*)uop)(invec, outvec, len, dtype);
}


void MSMPI_FORT_CALL mpi_op_create__ ( MPI_User_function *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Op_create( v1, *v2, v3 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Op_set_proxy( *v3, MPIR_Op_f_proxy);
    }
}


void MSMPI_FORT_CALL mpi_op_commutative__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Op_commutative( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_op_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Op_free( v1 );
}


void MSMPI_FORT_CALL mpi_allreduce__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Allreduce( v1, v2, *v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_reduce_scatter_block__(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr)
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Reduce_scatter_block(v1, v2, *v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6));
}


void MSMPI_FORT_CALL mpi_reduce_scatter__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Reduce_scatter( v1, v2, v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_scan__ ( void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }
    *ierr = NMPI_Scan( v1, v2, *v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_group_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Group_size( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_group_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Group_rank( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_group_translate_ranks__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_reads_(*v2) const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v2) MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Group_translate_ranks( *v1, *v2, v3, *v4, v5 );
}


void MSMPI_FORT_CALL mpi_group_compare__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Group_compare( *v1, *v2, v3 );
}


void MSMPI_FORT_CALL mpi_comm_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_group( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_group_union__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Group_union( *v1, *v2, v3 );
}


void MSMPI_FORT_CALL mpi_group_intersection__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Group_intersection( *v1, *v2, v3 );
}


void MSMPI_FORT_CALL mpi_group_difference__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Group_difference( *v1, *v2, v3 );
}


void MSMPI_FORT_CALL mpi_group_incl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Group_incl( *v1, *v2, v3, v4 );
}


void MSMPI_FORT_CALL mpi_group_excl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Group_excl( *v1, *v2, v3, v4 );
}


void MSMPI_FORT_CALL mpi_group_range_incl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Group_range_incl( *v1, *v2, (int (*)[3])(v3), v4 );
}


void MSMPI_FORT_CALL mpi_group_range_excl__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint v3[], MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Group_range_excl( *v1, *v2, (int (*)[3])(v3), v4 );
}


void MSMPI_FORT_CALL mpi_group_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Group_free( v1 );
}


void MSMPI_FORT_CALL mpi_comm_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_size( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_comm_rank__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_rank( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_comm_compare__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_compare( (MPI_Comm)(*v1), (MPI_Comm)(*v2), v3 );
}


void MSMPI_FORT_CALL mpi_comm_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_dup( (MPI_Comm)(*v1), (MPI_Comm *)(v2) );
}


void MSMPI_FORT_CALL mpi_comm_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_create( (MPI_Comm)(*v1), *v2, (MPI_Comm *)(v3) );
}


void MSMPI_FORT_CALL mpi_comm_split__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_split( (MPI_Comm)(*v1), *v2, *v3, (MPI_Comm *)(v4) );
}


void MSMPI_FORT_CALL mpi_comm_split_type__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_split_type( (MPI_Comm)(*v1), *v2, *v3, (MPI_Info)(*v4), (MPI_Comm *)(v5) );
}


void MSMPI_FORT_CALL mpi_comm_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_free( (MPI_Comm *)(v1) );
}


void MSMPI_FORT_CALL mpi_comm_test_inter__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    int l2;
    *ierr = MPI_Comm_test_inter( (MPI_Comm)(*v1), &l2 );
    *v2 = MPIR_TO_FLOG(l2);
}


void MSMPI_FORT_CALL mpi_comm_remote_size__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_remote_size( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_comm_remote_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_remote_group( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_intercomm_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3,const  MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_Intercomm_create( (MPI_Comm)(*v1), *v2, (MPI_Comm)(*v3), *v4, *v5, (MPI_Comm *)(v6) );
}


void MSMPI_FORT_CALL mpi_intercomm_merge__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    int l2;
    l2 = MPIR_FROM_FLOG(*v2);
    *ierr = MPI_Intercomm_merge( (MPI_Comm)(*v1), l2, (MPI_Comm *)(v3) );
}


void MSMPI_FORT_CALL mpi_keyval_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_free_keyval( v1 );
}


void MSMPI_FORT_CALL mpi_attr_put__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_set_attr( (MPI_Comm)(*v1), *v2, (void *)(MPI_Aint)((int)*(const int *)v3) );
}


void MSMPI_FORT_CALL mpi_attr_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    void* attrv3;
    int l4;
    *ierr = MPI_Comm_get_attr( (MPI_Comm)(*v1), *v2, &attrv3, &l4 );

    if ((int)*ierr || !l4) {
        *(MPI_Fint*)v3 = 0;
    }
    else {
        *(MPI_Fint*)v3 = (MPI_Fint)(MPI_Aint)attrv3;
    }
    *v4 = MPIR_TO_FLOG(l4);
}


void MSMPI_FORT_CALL mpi_attr_delete__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_delete_attr( (MPI_Comm)(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_topo_test__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Topo_test( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_cart_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    int *l4;
    int l5;

    {int li;
     l4 = (int *)MPIU_Malloc(*v2 * sizeof(int));
     if(l4 == NULL)
     {
         *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
         goto fn_cleanup4;
     }
     for (li=0; li<*v2; li++) {
        l4[li] = MPIR_FROM_FLOG(v4[li]);
     }
    }
    l5 = MPIR_FROM_FLOG(*v5);
    *ierr = MPI_Cart_create( (MPI_Comm)(*v1), *v2, v3, l4, l5, (MPI_Comm *)(v6) );
    MPIU_Free( l4 );
fn_cleanup4:;
}


void MSMPI_FORT_CALL mpi_dims_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Dims_create( *v1, *v2, v3 );
}


void MSMPI_FORT_CALL mpi_graph_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_Graph_create( (MPI_Comm)(*v1), *v2, v3, v4, *v5, (MPI_Comm *)(v6) );
}


void MSMPI_FORT_CALL mpi_graphdims_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Graphdims_get( (MPI_Comm)(*v1), v2, v3 );
}


void MSMPI_FORT_CALL mpi_graph_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v3) MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Graph_get( (MPI_Comm)(*v1), *v2, *v3, v4, v5 );
}


void MSMPI_FORT_CALL mpi_cartdim_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Cartdim_get( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_cart_get__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) MPI_Fint *v3, _In_count_(*v2) MPI_Fint *v4, _In_count_(*v2) MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Cart_get( (MPI_Comm)(*v1), *v2, v3, v4, v5 );

    {int li;
     for (li=0; li<*v2; li++) {
        v4[li] = MPIR_TO_FLOG(v4[li]);
     }
    }
}


void MSMPI_FORT_CALL mpi_cart_rank__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Cart_rank( (MPI_Comm)(*v1), v2, v3 );
}


void MSMPI_FORT_CALL mpi_cart_coords__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_count_(*v3) MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Cart_coords( (MPI_Comm)(*v1), *v2, *v3, v4 );
}


void MSMPI_FORT_CALL mpi_graph_neighbors_count__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Graph_neighbors_count( (MPI_Comm)(*v1), *v2, v3 );
}


void MSMPI_FORT_CALL mpi_graph_neighbors__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _Out_cap_(*v3) MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Graph_neighbors( (MPI_Comm)(*v1), *v2, *v3, v4 );
}


void MSMPI_FORT_CALL mpi_cart_shift__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Cart_shift( (MPI_Comm)(*v1), *v2, *v3, v4, v5 );
}


void MSMPI_FORT_CALL mpi_cart_sub__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Cart_sub( (MPI_Comm)(*v1), v2, (MPI_Comm *)(v3) );
}


void MSMPI_FORT_CALL mpi_cart_map__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_reads_(*v2) const MPI_Fint *v3, _In_reads_(*v2) const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Cart_map( (MPI_Comm)(*v1), *v2, v3, v4, v5 );
}


void MSMPI_FORT_CALL mpi_graph_map__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_reads_(*v2) const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Graph_map( (MPI_Comm)(*v1), *v2, v3, v4, v5 );
}


void MSMPI_FORT_CALL mpi_dist_graph_neighbors__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(*v2) MPI_Fint *v3, _Out_cap_(*v2) MPI_Fint *v4, const MPI_Fint *v5, _Out_cap_(*v5) MPI_Fint *v6, _Out_cap_(*v5) MPI_Fint *v7, MPI_Fint *ierr )
{
    if( v4 == MPIR_F_MPI_UNWEIGHTED )
    {
        v4 = MPI_UNWEIGHTED;
    }
    else if( v4 == MPIR_F_MPI_WEIGHTS_EMPTY )
    {
        v4 = MPI_WEIGHTS_EMPTY;
    }

    if( v7 == MPIR_F_MPI_UNWEIGHTED )
    {
        v7 = MPI_UNWEIGHTED;
    }
    else if( v7 == MPIR_F_MPI_WEIGHTS_EMPTY )
    {
        v7 = MPI_WEIGHTS_EMPTY;
    }

    *ierr = MPI_Dist_graph_neighbors( (MPI_Comm)(*v1), *v2, v3, v4, *v5, v6, v7 );
}


void MSMPI_FORT_CALL mpi_dist_graph_neighbors_count__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    int l4;
    *ierr = MPI_Dist_graph_neighbors_count( (MPI_Comm)(*v1), v2, v3, &l4 );
    if( *ierr == MPI_SUCCESS )
    {
        *v4 = MPIR_TO_FLOG(l4);
    }
}


void MSMPI_FORT_CALL mpi_dist_graph_create_adjacent__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) const MPI_Fint *v3, _In_count_(*v2) const MPI_Fint *v4, const MPI_Fint *v5, _In_count_(*v5) const MPI_Fint *v6, _In_count_(*v5) const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    if( v4 == MPIR_F_MPI_UNWEIGHTED )
    {
        v4 = MPI_UNWEIGHTED;
    }
    else if( v4 == MPIR_F_MPI_WEIGHTS_EMPTY )
    {
        v4 = MPI_WEIGHTS_EMPTY;
    }

    if( v7 == MPIR_F_MPI_UNWEIGHTED )
    {
        v7 = MPI_UNWEIGHTED;
    }
    else if( v7 == MPIR_F_MPI_WEIGHTS_EMPTY )
    {
        v7 = MPI_WEIGHTS_EMPTY;
    }

    *ierr = MPI_Dist_graph_create_adjacent( (MPI_Comm)(*v1), *v2, v3, v4, *v5, v6, v7, (MPI_Info)(*v8), *v9, (MPI_Comm *)(v10) );
}


void MSMPI_FORT_CALL mpi_dist_graph_create__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_count_(*v2) const MPI_Fint *v3, _In_count_(*v2) const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    if( v6 == MPIR_F_MPI_UNWEIGHTED )
    {
        v6 = MPI_UNWEIGHTED;
    }
    else if( v6 == MPIR_F_MPI_WEIGHTS_EMPTY )
    {
        v6 = MPI_WEIGHTS_EMPTY;
    }

    *ierr = MPI_Dist_graph_create( (MPI_Comm)(*v1), *v2, v3, v4, v5, v6, (MPI_Info)(*v7), *v8, (MPI_Comm *)(v9) );
}

void MSMPI_FORT_CALL mpi_get_processor_name__ ( _In_count_(d1) char *v1, MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;
    const size_t len = (d1 > MPI_MAX_PROCESSOR_NAME)? d1 : MPI_MAX_PROCESSOR_NAME;
    p1 = (char *)MPIU_Malloc( len + 1 );
    if (p1 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup1;
    }
    p1[0] = 0;
    *ierr = MPI_Get_processor_name( p1, v2 );
    if(*ierr == MPI_SUCCESS)
    {
        char *p = MPIU_Strncpy(v1, p1, d1);
        while ((p-v1) < d1) { *p++ = ' '; }
    }
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL msmpi_get_bsend_overhead__ ( MPI_Fint *size )
{
    *size = MSMPI_Get_bsend_overhead();
}


void MSMPI_FORT_CALL msmpi_get_version__ ( MPI_Fint *version )
{
    *version = MSMPI_Get_version();
}


void MSMPI_FORT_CALL mpi_get_version__ ( MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Get_version( v1, v2 );
}


void MSMPI_FORT_CALL mpi_get_library_version__ ( _In_count_(d1) char *v1, MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;
    const size_t len = (d1 > MPI_MAX_LIBRARY_VERSION_STRING)? d1 : MPI_MAX_LIBRARY_VERSION_STRING;
    p1 = static_cast<char *>(MPIU_Malloc( len + 1 ));
    if (p1 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        return;
    }
    p1[0] = 0;
    *ierr = MPI_Get_library_version( p1, v2 );
    if(*ierr == MPI_SUCCESS)
    {
        char *p = MPIU_Strncpy(v1, p1, d1);
        while ((p-v1) < d1) { *p++ = ' '; }
    }
    MPIU_Free( p1 );
}


void MSMPI_FORT_CALL mpi_errhandler_create__ ( MPI_Handler_function*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_create_errhandler( v1, v2 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Comm_errhandler_set_proxy( *v2, MPIR_Comm_errhandler_f_proxy);
    }
}


void MSMPI_FORT_CALL mpi_errhandler_set__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_set_errhandler( (MPI_Comm)(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_errhandler_get__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_get_errhandler( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_errhandler_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Errhandler_free( v1 );
}


void MSMPI_FORT_CALL mpi_error_string__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;
    const size_t len = (d2 > MPI_MAX_ERROR_STRING)? d2 : MPI_MAX_ERROR_STRING;
    p2 = (char *)MPIU_Malloc( len + 1 );
    if (p2 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup2;
    }
    p2[0] = 0;
    *ierr = MPI_Error_string( *v1, p2, v3 );

    char *p = MPIU_Strncpy(v2, p2, d2);
    while ((p-v2) < d2) { *p++ = ' '; }

    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_error_class__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Error_class( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_finalize__ ( MPI_Fint *ierr )
{
    *ierr = MPI_Finalize(  );
}


void MSMPI_FORT_CALL mpi_initialized__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    int l1;
    *ierr = MPI_Initialized( &l1 );
    *v1 = MPIR_TO_FLOG(l1);
}


#pragma warning(push)
#pragma warning(disable:4702)
void MSMPI_FORT_CALL mpi_abort__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Abort( (MPI_Comm)(*v1), *v2 );
}
#pragma warning(pop)


void MSMPI_FORT_CALL mpi_close_port__ ( _In_count_(d1) const char *v1, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {const char *p = v1 + d1 - 1;
     int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    *ierr = MPI_Close_port( p1 );
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_comm_accept__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {const char *p = v1 + d1 - 1;
     int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    *ierr = MPI_Comm_accept( p1, (MPI_Info)(*v2), *v3, (MPI_Comm)(*v4), (MPI_Comm *)(v5) );
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_comm_connect__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    *ierr = MPI_Comm_connect( p1, (MPI_Info)(*v2), *v3, (MPI_Comm)(*v4), (MPI_Comm *)(v5) );
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_comm_disconnect__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_disconnect( (MPI_Comm *)(v1) );
}


void MSMPI_FORT_CALL mpi_comm_get_parent__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_get_parent( (MPI_Comm *)(v1) );
}


void MSMPI_FORT_CALL mpi_comm_join__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_join( *v1, (MPI_Comm *)(v2) );
}


void MSMPI_FORT_CALL mpi_comm_spawn__ ( _In_count_(d1) const char *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, _Out_cap_(*v3) MPI_Fint v8[], MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d2 )
{
    char *p1;
    const char **p2;
    const char *ptmp2;
    const char *pcpy2;
    int  asize2=0;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }


    { int i;

      /* Compute the size of the array by looking for an all-blank line */
      pcpy2 = v2;
      for (asize2=1; ; asize2++) {
          const char *pt = pcpy2 + d2 - 1;
          while (*pt == ' ' && pt > pcpy2) pt--;
          if (*pt == ' ') break;
          pcpy2 += d2;
      }

      p2 = (const char **)MPIU_Malloc( asize2 * sizeof(char *) );
      if(p2 == NULL)
      {
          *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
          goto fn_cleanup2_1;
      }

      ptmp2 = (const char *)MPIU_Malloc( asize2 * (d2 + 1) );
      if(ptmp2 == NULL)
      {
          *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
          goto fn_cleanup2_2;
      }
      p2[0] = ptmp2;
      for (i=0; i<asize2-1; i++) {
          const char *p = v2 + i * d2;
          const char *pin;
          char *pdest;
          int j;

          pdest = const_cast<char *>(ptmp2) + i * (d2 + 1);
          p2[i] = pdest;
          /* Move to the end and work back */
          pin = p + d2 - 1;
          while (*pin == ' ' && pin > p) pin--;
          /* Copy and then null terminate */
          for (j=0; j<(pin-p)+1; j++) { pdest[j] = p[j]; }
          pdest[j] = 0;
          }
    /* Null terminate the array */
    p2[asize2-1] = 0;
    }

    if (v8 == MPI_F_ERRCODES_IGNORE) { v8 = MPI_ERRCODES_IGNORE; }
    *ierr = MPI_Comm_spawn( p1, (char **)p2, *v3, (MPI_Info)(*v4), *v5, (MPI_Comm)(*v6), (MPI_Comm *)(v7), (int *)v8 );
    MPIU_Free( (void *)ptmp2 );
fn_cleanup2_2:;
    MPIU_Free( p2 );
fn_cleanup2_1:;
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_comm_spawn_multiple__ ( const MPI_Fint *v1, _In_reads_(d2) const char *v2, _In_reads_(d3) const char *v3, _In_reads_(*v1) const MPI_Fint v4[], _In_reads_(*v1) const MPI_Info v5[], const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *v8, _Out_opt_ MPI_Fint v9[], MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d3 )
{
    char **p2;
    char *ptmp2;
    int  asize2=0;
    char ***p3=0;
    int k3=0;


    { int i;

      asize2 = *v1 + 1;

      p2 = (char **)MPIU_Malloc( asize2 * sizeof(char *) );
      if(p2 == NULL)
      {
          *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
          goto fn_cleanup2_1;
      }

      ptmp2 = (char *)MPIU_Malloc( asize2 * (d2 + 1) );
      if(ptmp2 == NULL)
      {
          *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
          goto fn_cleanup2_2;
      }
      p2[0] = ptmp2;
      for (i=0; i<asize2-1; i++) {
          const char *p = v2 + i * d2;
          const char *pin;
          char *pdest;
          int j;

          pdest = ptmp2 + i * (d2 + 1);
          p2[i] = pdest;
          /* Move to the end and work back */
          pin = p + d2 - 1;
          while (*pin == ' ' && pin > p) pin--;
          /* Copy and then null terminate */
          for (j=0; j<(pin-p)+1; j++) { pdest[j] = p[j]; }
          pdest[j] = 0;
          }
    /* Null terminate the array */
    p2[asize2-1] = 0;
    }

    /* Check for the special case of a the null args case. */
    if (v3 == MPI_F_ARGVS_NULL) { v3 = (const char *)MPI_ARGVS_NULL; }
    else {
        /* We must convert from the 2-dimensional Fortran array of
           fixed length strings to a C variable-sized array (really an
           array of pointers for each command of pointers to each
           argument, which is null terminated.*/


      /* Allocate the array of pointers for the commands */
      p3 = (char ***)MPIU_Malloc( *v1 * sizeof(char **) );
      if(p3 == NULL)
      {
          *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
          goto fn_cleanup3_1;
      }

      for (k3=0; k3<*v1; k3++) {
        /* For each command, find the number of command-line arguments.
           They are terminated by an empty entry. */
        /* Find the first entry in the Fortran array for this row */
        const char *p = v3 + k3 * d3;
        size_t arglen = 0;
        int argcnt=0, i;
        char **pargs, *pdata;
        for (argcnt=0; ; argcnt++) {
            const char *pin = p + d3 - 1; /* Move to the end of the
                                            current Fortran string */
            while (*pin == ' ' && pin > p) pin--; /* Move backwards until
                                                    we find a non-blank
                                                    (Fortran is blank padded)*/
            if (pin == p && *pin == ' ') {
                /* found the terminating empty arg */
                break;
            }
            /* Keep track of the amount of space needed */
            arglen += (pin - p) + 2;   /* add 1 for the null */
            /* Advance to the next entry in the array */
            p += (*v1) * d3;
        }

        /* argcnt is the number of provided arguments.
           Allocate the necessary elements and copy, null terminating copies */
        pargs = (char **)MPIU_Malloc( (argcnt+1)*sizeof(char *) );
        if(pargs == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup3_2;
        }

        pdata = (char *)MPIU_Malloc( arglen );
        if(pdata == NULL)
        {
            MPIU_Free(pargs);
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup3_2;
        }
        p3[k3] = pargs;
        pargs[argcnt] = 0;  /* Null terminate end */
        /* Copy each argument to consequtive locations in pdata,
           and set the corresponding pointer entry */
        p = v3 + k3 * d3;
        for (i=0; i<argcnt; i++) {
            int j;
            const char *pin;
            p3[k3][i] = pdata;
            /* Move to the end and work back */
            pin = p + d3 - 1;
            while (*pin == ' ' && pin > p) pin--;
            /* Copy and then null terminate */
            for (j=0; j<(pin-p)+1; j++) { *pdata++ = p[j]; }
            *pdata++ = 0;
            /* Advance to the next entry in the array */
            p += (*v1) * d3;
        }
        /* Set the terminator */
        p3[k3][i] = 0;
       }
    }

    if (v9 == MPI_F_ERRCODES_IGNORE) { v9 = MPI_ERRCODES_IGNORE; }
    *ierr = MPI_Comm_spawn_multiple( *v1, p2, p3, v4, v5, *v6, (MPI_Comm)(*v7), (MPI_Comm *)(v8), (int *)v9 );
fn_cleanup3_2:;
    if (p3 != NULL) {
        int i;
        for (i=0; i < k3; i++) {
            MPIU_Free( p3[i][0] );  /* Free space allocated to args */
            MPIU_Free( p3[i] );       /* Free space allocated to arg array */
        }
        /* Free the array of arrays */
        MPIU_Free( p3 );
    }
fn_cleanup3_1:;
    MPIU_Free( ptmp2 );
fn_cleanup2_2:;
    MPIU_Free( p2 );
fn_cleanup2_1:;
}


void MSMPI_FORT_CALL mpi_lookup_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 )
{
    char *p1;
    char *p3;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    p3 = (char *)MPIU_Malloc( d3 + 1 );
    if (p3 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup3;
    }
    p3[0] = 0;
    *ierr = MPI_Lookup_name( p1, (MPI_Info)(*v2), p3 );

    {
        char *p = MPIU_Strncpy(v3, p3, d3);
        while ((p-v3) < d3) { *p++ = ' '; }
    }
    MPIU_Free( p3 );
fn_cleanup3:;
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_open_port__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;
    p2 = (char *)MPIU_Malloc( d2 + 1 );
    if (p2 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup2;
    }
    p2[0] = 0;
    *ierr = MPI_Open_port( (MPI_Info)(*v1), p2 );

    {
        char *p = MPIU_Strncpy(v2, p2, d2);
        while ((p-v2) < d2) { *p++ = ' '; }
    }
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_publish_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 )
{
    char *p1;
    char *p3;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }

    {
        const char *p = v3 + d3 - 1;
        int  li;
        while (*p == ' ' && p > v3) p--;
        p++;
        p3 = (char *)MPIU_Malloc( p-v3 + 1 );
        if(p3 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup3;
        }
        for (li=0; li<(p-v3); li++) { p3[li] = v3[li]; }
        p3[li] = 0;
    }
    *ierr = MPI_Publish_name( p1, (MPI_Info)(*v2), p3 );
    MPIU_Free( p3 );
fn_cleanup3:;
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_unpublish_name__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d1, MPI_Fint d3 )
{
    char *p1;
    char *p3;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }

    {
        const char *p = v3 + d3 - 1;
        int  li;
        while (*p == ' ' && p > v3) p--;
        p++;
        p3 = (char *)MPIU_Malloc( p-v3 + 1 );
        if(p3 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup3;
        }
        for (li=0; li<(p-v3); li++) { p3[li] = v3[li]; }
        p3[li] = 0;
    }
    *ierr = MPI_Unpublish_name( p1, (MPI_Info)(*v2), p3 );
    MPIU_Free( p3 );
fn_cleanup3:;
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_accumulate__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5,const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *ierr )
{
    *ierr = MPI_Accumulate( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, (MPI_Datatype)(*v7), *v8, *v9 );
}


void MSMPI_FORT_CALL mpi_raccumulate__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const  MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    *ierr = MPI_Raccumulate( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, (MPI_Datatype)(*v7), *v8, *v9, (MPI_Request *)(v10) );
}


void MSMPI_FORT_CALL mpi_get__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    *ierr = MPI_Get( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, (MPI_Datatype)(*v7), *v8 );
}


void MSMPI_FORT_CALL mpi_rget__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    *ierr = MPI_Rget( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, (MPI_Datatype)(*v7), *v8, (MPI_Request *)(v9) );
}


void MSMPI_FORT_CALL mpi_put__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *ierr )
{
    *ierr = MPI_Put( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, (MPI_Datatype)(*v7), *v8 );
}


void MSMPI_FORT_CALL mpi_rput__ ( void *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Fint *v8, MPI_Fint *v9, MPI_Fint *ierr )
{
    *ierr = MPI_Rput( v1, *v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, (MPI_Datatype)(*v7), *v8,  (MPI_Request *)(v9) );
}


void MSMPI_FORT_CALL mpi_get_accumulate__(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *ierr)
{
    *ierr = MPI_Get_accumulate(v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), *v7, *v8, *v9, (MPI_Datatype)(*v10), *v11, *v12);
}


void MSMPI_FORT_CALL mpi_rget_accumulate__(void *v1, const MPI_Fint *v2, const MPI_Fint *v3, void *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, const MPI_Aint * v8, const  MPI_Fint *v9, const MPI_Fint *v10, const MPI_Fint *v11, const MPI_Fint *v12, MPI_Fint *v13, MPI_Fint *ierr)
{
    *ierr = MPI_Rget_accumulate(v1, *v2, (MPI_Datatype)(*v3), v4, *v5, (MPI_Datatype)(*v6), *v7, *v8, *v9, (MPI_Datatype)(*v10), *v11, *v12, (MPI_Request *)(v13));
}


void MSMPI_FORT_CALL mpi_fetch_and_op__(void *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Aint * v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr)
{
    *ierr = MPI_Fetch_and_op(v1, v2, (MPI_Datatype)(*v3), *v4, *v5, *v6, *v7);
}


void MSMPI_FORT_CALL mpi_compare_and_swap__(void *v1, void *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr)
{
    *ierr = MPI_Compare_and_swap(v1, v2, v3, (MPI_Datatype)(*v4), *v5, *v6,*v7);
}


void MSMPI_FORT_CALL mpi_win_complete__ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Win_complete( *v1 );
}


void MSMPI_FORT_CALL mpi_win_create__ ( void*v1, const MPI_Aint * v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_Win_create( v1, *v2, *v3, (MPI_Info)(*v4), (MPI_Comm)(*v5), v6 );
}

void MSMPI_FORT_CALL mpi_win_allocate__ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_Win_allocate( *v1, *v2, (MPI_Info)(*v3), (MPI_Comm)(*v4), v5, v6 );
}

void MSMPI_FORT_CALL mpi_win_allocate_shared__ ( const MPI_Aint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_Win_allocate_shared( *v1, *v2, (MPI_Info)(*v3), (MPI_Comm)(*v4), v5, v6 );
}

void MSMPI_FORT_CALL mpi_win_create_dynamic__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Win_create_dynamic( (MPI_Info)(*v1), (MPI_Comm)(*v2), v3 );
}

void MSMPI_FORT_CALL mpi_win_attach__ ( MPI_Fint *v1, void* v2, const MPI_Aint * v3, MPI_Fint *ierr )
{
    *ierr = MPI_Win_attach( *v1, v2, *v3 );
}

void MSMPI_FORT_CALL mpi_win_detach__ ( MPI_Fint *v1, void* v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_detach( *v1, v2 );
}

void MSMPI_FORT_CALL mpi_win_shared_query__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Aint *v3, MPI_Fint *v4, void *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Win_shared_query( *v1, *v2, v3, v4, v5);
}

void MSMPI_FORT_CALL mpi_win_fence__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_fence( *v1, *v2 );
}


void MSMPI_FORT_CALL mpi_win_flush__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_flush( *v1, *v2 );
}


void MSMPI_FORT_CALL mpi_win_flush_all__(const MPI_Fint *v1, MPI_Fint *ierr)
{
    *ierr = MPI_Win_flush_all(*v1);
}


void MSMPI_FORT_CALL mpi_win_flush_local__(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr)
{
    *ierr = MPI_Win_flush_local(*v1, *v2);
}


void MSMPI_FORT_CALL mpi_win_flush_local_all__(const MPI_Fint *v1, MPI_Fint *ierr)
{
    *ierr = MPI_Win_flush_local_all(*v1);
}


void MSMPI_FORT_CALL mpi_win_sync__(const MPI_Fint *v1, MPI_Fint *ierr)
{
    *ierr = MPI_Win_sync(*v1);
}


void MSMPI_FORT_CALL mpi_win_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Win_free( v1 );
}


void MSMPI_FORT_CALL mpi_win_get_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_get_group( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_win_lock__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Win_lock( *v1, *v2, *v3, *v4 );
}


void MSMPI_FORT_CALL mpi_win_lock_all__(const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr)
{
    *ierr = MPI_Win_lock_all(*v1, *v2);
}


void MSMPI_FORT_CALL mpi_win_post__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Win_post( *v1, *v2, *v3 );
}


void MSMPI_FORT_CALL mpi_win_start__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Win_start( *v1, *v2, *v3 );
}


void MSMPI_FORT_CALL mpi_win_test__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_test( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_win_unlock__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_unlock( *v1, *v2 );
}


void MSMPI_FORT_CALL mpi_win_unlock_all__(const MPI_Fint *v1, MPI_Fint *ierr)
{
    *ierr = MPI_Win_unlock_all(*v1);
}


void MSMPI_FORT_CALL mpi_win_wait__ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Win_wait( *v1 );
}


void MSMPI_FORT_CALL mpi_alltoallw__ ( const void*v1, const MPI_Fint v2[], const MPI_Fint v3[], const MPI_Fint v4[], void*v5, const MPI_Fint v6[], const MPI_Fint v7[], const MPI_Fint v8[], const MPI_Fint *v9, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v5 == MPIR_F_MPI_IN_PLACE )
    {
        v5 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Alltoallw( v1, v2, v3, v4, v5, v6, v7, v8, (MPI_Comm)(*v9) );
}


void MSMPI_FORT_CALL mpi_exscan__ ( void*v1, void*v2, const MPI_Fint *v3, const MPI_Fint *v4, const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *ierr )
{
    if( v1 == MPIR_F_MPI_IN_PLACE )
    {
        v1 = MPI_IN_PLACE;
    }
    if( v2 == MPIR_F_MPI_IN_PLACE )
    {
        v2 = MPI_IN_PLACE;
    }

    *ierr = NMPI_Exscan( v1, v2, *v3, (MPI_Datatype)(*v4), *v5, (MPI_Comm)(*v6) );
}


void MSMPI_FORT_CALL mpi_add_error_class__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Add_error_class( v1 );
}


void MSMPI_FORT_CALL mpi_add_error_code__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Add_error_code( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_add_error_string__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    *ierr = MPI_Add_error_string( *v1, p2 );
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_comm_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_call_errhandler( (MPI_Comm)(*v1), *v2 );
}


/* The F90 attr copy function prototype and calling convention */
typedef void (MSMPI_FORT_CALL F90_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *,MPI_Aint *, MPI_Fint *, MPI_Fint *);

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
typedef void (MSMPI_FORT_CALL F90_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *, MPI_Fint *);

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


void MSMPI_FORT_CALL mpi_comm_create_keyval__ ( MPI_Comm_copy_attr_function *v1, MPI_Comm_delete_attr_function *v2, MPI_Fint *v3, void *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_create_keyval( v1, v2, v3, v4 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Comm_copy_attr_f90_proxy, MPIR_Comm_delete_attr_f90_proxy );
    }
}


void MSMPI_FORT_CALL mpi_comm_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_delete_attr( (MPI_Comm)(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_comm_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_free_keyval( v1 );
}


void MSMPI_FORT_CALL mpi_comm_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    void *attrv3;
    int l4;
    *ierr = MPI_Comm_get_attr( (MPI_Comm)(*v1), *v2, &attrv3, &l4 );

    if ((int)*ierr || !l4) {
        *(MPI_Aint*)v3 = 0;
    }
    else {
        *(MPI_Aint*)v3 = (MPI_Aint)attrv3;
    }
    *v4 = MPIR_TO_FLOG(l4);
}


void MSMPI_FORT_CALL mpi_comm_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;
    p2 = (char *)MPIU_Malloc( d2 + 1 );
    if (p2 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup2;
    }
    p2[0] = 0;
    *ierr = MPI_Comm_get_name( (MPI_Comm)(*v1), p2, v3 );

    {
        char *p = MPIU_Strncpy(v2, p2, d2);
        while ((p-v2) < d2) { *p++ = ' '; }
    }
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_comm_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_set_attr( (MPI_Comm)(*v1), *v2, (void *)*(const MPI_Aint *)v3);
}


void MSMPI_FORT_CALL mpi_comm_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    *ierr = MPI_Comm_set_name( (MPI_Comm)(*v1), p2 );
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_file_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_call_errhandler( MPI_File_f2c(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_grequest_complete__ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Grequest_complete( *v1 );
}


/* The Fortran Grequest query function prototype and calling convention */
typedef void (MSMPI_FORT_CALL MPIR_Grequest_f77_query_function)(void*, MPI_Status*, MPI_Fint*);

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
typedef void (MSMPI_FORT_CALL MPIR_Grequest_f77_free_function)(void*, MPI_Fint*);

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
typedef void (MSMPI_FORT_CALL MPIR_Grequest_f77_cancel_function)(void*, int*, MPI_Fint*);

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


void MSMPI_FORT_CALL mpi_grequest_start__ ( MPI_Grequest_query_function*v1, MPI_Grequest_free_function*v2, MPI_Grequest_cancel_function*v3, void*v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Grequest_start( v1, v2, v3, v4, (MPI_Request *)(v5) );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Grequest_set_proxy( *v5, MPIR_Grequest_query_f_proxy, MPIR_Grequest_free_f_proxy, MPIR_Grequest_cancel_f_proxy);
    }
}


void MSMPI_FORT_CALL mpi_is_thread_main__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Is_thread_main( v1 );
}


void MSMPI_FORT_CALL mpi_query_thread__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Query_thread( v1 );
}


void MSMPI_FORT_CALL mpi_status_set_cancelled__ ( MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Status_set_cancelled( (MPI_Status *)(v1), *v2 );
}


void MSMPI_FORT_CALL mpi_status_set_elements__ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Status_set_elements( (MPI_Status *)(v1), (MPI_Datatype)(*v2), *v3 );
}


void MSMPI_FORT_CALL mpi_status_set_elements_x__ ( MPI_Fint *v1, const MPI_Fint *v2, const MPI_Count *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Status_set_elements_x( (MPI_Status *)(v1), (MPI_Datatype)(*v2), *v3 );
}


/* The F90 attr copy function prototype and calling convention */
typedef void (MSMPI_FORT_CALL F90_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *,MPI_Aint *, MPI_Fint *, MPI_Fint *);

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
typedef void (MSMPI_FORT_CALL F90_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *, MPI_Fint *);

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


void MSMPI_FORT_CALL mpi_type_create_keyval__ ( MPI_Type_copy_attr_function*v1, MPI_Type_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_keyval( v1, v2, v3, v4 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Type_copy_attr_f90_proxy, MPIR_Type_delete_attr_f90_proxy );
    }
}


void MSMPI_FORT_CALL mpi_type_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Type_delete_attr( (MPI_Datatype)(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_type_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Type_dup( (MPI_Datatype)(*v1), (MPI_Datatype *)(v2) );
}


void MSMPI_FORT_CALL mpi_type_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Type_free_keyval( v1 );
}


void MSMPI_FORT_CALL mpi_type_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    void *attrv3;
    int l4;
    *ierr = MPI_Type_get_attr( (MPI_Datatype)(*v1), *v2, &attrv3, &l4 );

    if ((int)*ierr || !l4) {
        *(MPI_Aint*)v3 = 0;
    }
    else {
        *(MPI_Aint*)v3 = (MPI_Aint)attrv3;
    }
    *v4 = MPIR_TO_FLOG(l4);
}


void MSMPI_FORT_CALL mpi_type_get_contents__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v2) MPI_Fint v5[], _Out_cap_(*v3) MPI_Aint * v6, _Out_cap_(*v4) MPI_Fint v7[], MPI_Fint *ierr )
{
    *ierr = MPI_Type_get_contents( (MPI_Datatype)(*v1), *v2, *v3, *v4, v5, v6, v7 );
}


void MSMPI_FORT_CALL mpi_type_get_envelope__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_get_envelope( (MPI_Datatype)(*v1), v2, v3, v4, v5 );
}


void MSMPI_FORT_CALL mpi_type_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;
    p2 = (char *)MPIU_Malloc( d2 + 1 );
    if (p2 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup2;
    }
    p2[0] = 0;
    *ierr = MPI_Type_get_name( (MPI_Datatype)(*v1), p2, v3 );

    {
        char *p = MPIU_Strncpy(v2, p2, d2);
        while ((p-v2) < d2) { *p++ = ' '; }
    }
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_type_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_set_attr( (MPI_Datatype)(*v1), *v2, (void *)*(const MPI_Aint *)v3);
}


void MSMPI_FORT_CALL mpi_type_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    *ierr = MPI_Type_set_name( (MPI_Datatype)(*v1), p2 );
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_type_match_size__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_match_size( *v1, *v2, (MPI_Datatype *)(v3) );
}


void MSMPI_FORT_CALL mpi_win_call_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_call_errhandler( *v1, *v2 );
}


/* The F90 attr copy function prototype and calling convention */
typedef void (MSMPI_FORT_CALL F90_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *,MPI_Aint *, MPI_Fint *, MPI_Fint *);

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
typedef void (MSMPI_FORT_CALL F90_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Aint *, MPI_Aint *, MPI_Fint *);

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


void MSMPI_FORT_CALL mpi_win_create_keyval__ ( MPI_Win_copy_attr_function*v1, MPI_Win_delete_attr_function*v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
    *ierr = MPI_Win_create_keyval( v1, v2, v3, v4 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Keyval_set_proxy( *v3, MPIR_Win_copy_attr_f90_proxy, MPIR_Win_delete_attr_f90_proxy );
    }
}


void MSMPI_FORT_CALL mpi_win_delete_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_delete_attr( *v1, *v2 );
}


void MSMPI_FORT_CALL mpi_win_free_keyval__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Win_free_keyval( v1 );
}


void MSMPI_FORT_CALL mpi_win_get_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void*v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    void *attrv3;
    int l4;
    *ierr = MPI_Win_get_attr( *v1, *v2, &attrv3, &l4 );

    if ((int)*ierr || !l4) {
        *(MPI_Aint*)v3 = 0;
    }
    else {
        *(MPI_Aint*)v3 = (MPI_Aint)attrv3;
    }
    *v4 = MPIR_TO_FLOG(l4);
}


void MSMPI_FORT_CALL mpi_win_get_name__ ( const MPI_Fint *v1, _Out_cap_(d2) char *v2, MPI_Fint *v3, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;
    p2 = (char *)MPIU_Malloc( d2 + 1 );
    if (p2 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup2;
    }
    p2[0] = 0;
    *ierr = MPI_Win_get_name( *v1, p2, v3 );

    {
        char *p = MPIU_Strncpy(v2, p2, d2);
        while ((p-v2) < d2) { *p++ = ' '; }
    }
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_win_set_attr__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Win_set_attr( *v1, *v2, (void *)*(const MPI_Aint *)v3);
}


void MSMPI_FORT_CALL mpi_win_set_name__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    *ierr = MPI_Win_set_name( *v1, p2 );
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_alloc_mem__ ( const MPI_Aint * v1, const MPI_Fint *v2, void*v3, MPI_Fint *ierr )
{
    *ierr = MPI_Alloc_mem( *v1, (MPI_Info)(*v2), v3 );
}


/* The Fortran Comm errhandler function prototype and calling convention */
/* Do not add ... as many Fotran compilers do not support it leading to a link error with stdcall */
typedef void (MSMPI_FORT_CALL MPI_F77_Comm_errhandler_fn)(MPI_Fint*, MPI_Fint*);

/* Helper proxy function to thunk comm errhandler function calls into Fortran calling convention */
static void MPIAPI MPIR_Comm_errhandler_f_proxy(MPI_Comm_errhandler_fn* fn, MPI_Comm* comm, int* errcode, ...)
{
    ((MPI_F77_Comm_errhandler_fn*)fn)((MPI_Fint*)comm, (MPI_Fint*)errcode);
}


void MSMPI_FORT_CALL mpi_comm_create_errhandler__ ( MPI_Comm_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_create_errhandler( v1, v2 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Comm_errhandler_set_proxy( *v2, MPIR_Comm_errhandler_f_proxy);
    }
}


void MSMPI_FORT_CALL mpi_comm_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_get_errhandler( (MPI_Comm)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_comm_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Comm_set_errhandler( (MPI_Comm)(*v1), *v2 );
}


/* The Fortran File errhandler function prototype and calling convention */
/* Do not add ... as many Fotran compilers do not support it leading to a link error with stdcall */
typedef void (MSMPI_FORT_CALL MPI_F77_File_errhandler_fn)(MPI_Fint*, MPI_Fint*);

/* Helper proxy function to thunk file errhandler function calls into Fortran calling convention */
static void MPIAPI MPIR_File_errhandler_f_proxy(MPI_File_errhandler_fn* fn, MPI_File* file, int* errcode, ...)
{
    ((MPI_F77_File_errhandler_fn*)fn)((MPI_Fint*)file, (MPI_Fint*)errcode);
}


void MSMPI_FORT_CALL mpi_file_create_errhandler__ ( MPI_File_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_create_errhandler( v1, v2 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_File_errhandler_set_proxy( *v2, MPIR_File_errhandler_f_proxy);
    }
}


void MSMPI_FORT_CALL mpi_file_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_errhandler( MPI_File_f2c(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_file_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_set_errhandler( MPI_File_f2c(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_finalized__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Finalized( v1 );
}


void MSMPI_FORT_CALL mpi_free_mem__ ( void*v1, MPI_Fint *ierr )
{
    *ierr = MPI_Free_mem( v1 );
}


void MSMPI_FORT_CALL mpi_info_create__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Info_create( (MPI_Info *)(v1) );
}


void MSMPI_FORT_CALL mpi_info_delete__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    *ierr = MPI_Info_delete( (MPI_Info)(*v1), p2 );
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_info_dup__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Info_dup( (MPI_Info)(*v1), (MPI_Info *)(v2) );
}


void MSMPI_FORT_CALL mpi_info_free__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Info_free( (MPI_Info *)(v1) );
}


void MSMPI_FORT_CALL mpi_info_get__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, _Out_cap_(d4) char *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d4 )
{
    char *p2;
    char *p4;
    int l5;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    p4 = (char *)MPIU_Malloc( d4 + 1 );
    if (p4 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup4;
    }
    p4[0] = 0;
    *ierr = MPI_Info_get( (MPI_Info)(*v1), p2, *v3, p4, &l5 );

    if(l5)
    {
        char *p = MPIU_Strncpy(v4, p4, d4);
        while ((p-v4) < d4) { *p++ = ' '; }
    }
    *v5 = MPIR_TO_FLOG(l5);
    MPIU_Free( p4 );
fn_cleanup4:;
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_info_get_nkeys__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Info_get_nkeys( (MPI_Info)(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_info_get_nthkey__ ( const MPI_Fint *v1, const MPI_Fint *v2, _Out_cap_(d3) char *v3, MPI_Fint *ierr, MPI_Fint d3 )
{
    char *p3;
    p3 = (char *)MPIU_Malloc( d3 + 1 );
    if (p3 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup3;
    }
    p3[0] = 0;
    *ierr = MPI_Info_get_nthkey( (MPI_Info)(*v1), *v2, p3 );

    {
        char *p = MPIU_Strncpy(v3, p3, d3);
        while ((p-v3) < d3) { *p++ = ' '; }
    }
    MPIU_Free( p3 );
fn_cleanup3:;
}


void MSMPI_FORT_CALL mpi_info_get_valuelen__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;
    int l4;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    *ierr = MPI_Info_get_valuelen( (MPI_Info)(*v1), p2, v3, &l4 );
    *v4 = MPIR_TO_FLOG(l4);
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_info_set__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, _In_count_(d3) const char *v3, MPI_Fint *ierr, MPI_Fint d2, MPI_Fint d3 )
{
    char *p2;
    char *p3;

    {
        const char *p = v2 + d2 - 1;
        const char *pin = v2;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        while (*pin == ' ' && pin < p) pin++;
        p2 = (char *)MPIU_Malloc( p-pin + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-pin); li++) { p2[li] = pin[li]; }
        p2[li] = 0;
    }

    {
        const char *p = v3 + d3 - 1;
        const char *pin = v3;
        int  li;
        while (*p == ' ' && p > v3) p--;
        p++;
        while (*pin == ' ' && pin < p) pin++;
        p3 = (char *)MPIU_Malloc( p-pin + 1 );
        if(p3 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup3;
        }
        for (li=0; li<(p-pin); li++) { p3[li] = pin[li]; }
        p3[li] = 0;
    }
    *ierr = MPI_Info_set( (MPI_Info)(*v1), p2, p3 );
    MPIU_Free( p3 );
fn_cleanup3:;
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_pack_external__ ( _In_count_(d1) const char *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, _Out_cap_(*v6) void *v5, const MPI_Aint * v6, MPI_Aint * v7, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    *ierr = MPI_Pack_external( p1, v2, *v3, (MPI_Datatype)(*v4), v5, *v6, v7 );
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_pack_external_size__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, const MPI_Fint *v3, MPI_Aint * v4, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    *ierr = MPI_Pack_external_size( p1, *v2, (MPI_Datatype)(*v3), v4 );
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_request_get_status__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Request_get_status( *v1, v2, (MPI_Status *)(v3) );
}


void MSMPI_FORT_CALL mpi_status_c2f__ ( MPI_Fint *v1, MPI_Fint*v2, MPI_Fint *ierr )
{
    *ierr = MPI_Status_c2f( (MPI_Status *)(v1), v2 );
}


void MSMPI_FORT_CALL mpi_status_f2c__ ( const MPI_Fint*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Status_f2c( v1, (MPI_Status *)(v2) );
}


void MSMPI_FORT_CALL mpi_type_create_darray__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Fint *v3, _In_reads_(*v3) const MPI_Fint v4[], _In_reads_(*v3) const MPI_Fint v5[], _In_reads_(*v3) const MPI_Fint v6[], _In_reads_(*v3) const MPI_Fint v7[], const MPI_Fint *v8, const MPI_Fint *v9, MPI_Fint *v10, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_darray( *v1, *v2, *v3, v4, v5, v6, v7, *v8, (MPI_Datatype)(*v9), (MPI_Datatype *)(v10) );
}


void MSMPI_FORT_CALL mpi_type_create_hindexed__ ( const MPI_Fint *v1, _In_reads_(*v1) const MPI_Fint v2[], _In_reads_(*v1) const MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_hindexed( *v1, v2, v3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_create_hindexed_block__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_reads_(*v1) const MPI_Aint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_hindexed_block( *v1, *v2, v3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_create_hvector__ ( const MPI_Fint *v1, const MPI_Fint *v2, const MPI_Aint * v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_hvector( *v1, *v2, *v3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_create_indexed_block__ ( const MPI_Fint *v1, const MPI_Fint *v2, _In_reads_(*v1) const MPI_Fint v3[], const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_indexed_block( *v1, *v2, v3, (MPI_Datatype)(*v4), (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_create_resized__ ( const MPI_Fint *v1, const MPI_Aint * v2, const MPI_Aint * v3, MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_resized( (MPI_Datatype)(*v1), *v2, *v3, (MPI_Datatype *)(v4) );
}


void MSMPI_FORT_CALL mpi_type_create_struct__ ( const MPI_Fint *v1, _In_count_(*v1) const MPI_Fint v2[], _In_count_(*v1) const MPI_Aint * v3, _In_count_(*v1) const MPI_Fint v4[], MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_struct( *v1, v2, v3, v4, (MPI_Datatype *)(v5) );
}


void MSMPI_FORT_CALL mpi_type_create_subarray__ ( const MPI_Fint *v1, _In_count_(*v1) const MPI_Fint v2[], _In_count_(*v1) const MPI_Fint v3[], _In_count_(*v1) const MPI_Fint v4[], const MPI_Fint *v5, const MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_subarray( *v1, v2, v3, v4, *v5, (MPI_Datatype)(*v6), (MPI_Datatype *)(v7) );
}


void MSMPI_FORT_CALL mpi_type_get_extent__ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_get_extent( (MPI_Datatype)(*v1), v2, v3 );
}


void MSMPI_FORT_CALL mpi_type_get_extent_x__ ( const MPI_Fint *v1, MPI_Count * v2, MPI_Count * v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_get_extent_x( (MPI_Datatype)(*v1), v2, v3 );
}


void MSMPI_FORT_CALL mpi_type_get_true_extent__ ( const MPI_Fint *v1, MPI_Aint * v2, MPI_Aint * v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_get_true_extent( (MPI_Datatype)(*v1), v2, v3 );
}


void MSMPI_FORT_CALL mpi_type_get_true_extent_x__ ( const MPI_Fint *v1, MPI_Count * v2, MPI_Count * v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_get_true_extent_x( (MPI_Datatype)(*v1), v2, v3 );
}


void MSMPI_FORT_CALL mpi_unpack_external__ ( _In_count_(d1) const char *v1, _In_bytecount_(*v3) void *v2, const MPI_Aint * v3, MPI_Aint * v4, void *v5, const MPI_Fint *v6, const MPI_Fint *v7, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    *ierr = MPI_Unpack_external( p1, v2, *v3, v4, v5, *v6, (MPI_Datatype)(*v7) );
    MPIU_Free( p1 );
fn_cleanup1:;
}


/* The Fortran Win errhandler function prototype and calling convention */
/* Do not add ... as many Fotran compilers do not support it leading to a link error with stdcall */
typedef void (MSMPI_FORT_CALL MPI_F77_Win_errhandler_fn)(MPI_Fint*, MPI_Fint*);

/* Helper proxy function to thunk win errhandler function calls into Fortran calling convention */
static void MPIAPI MPIR_Win_errhandler_f_proxy(MPI_Win_errhandler_fn* fn, MPI_Win* win, int* errcode, ...)
{
    ((MPI_F77_Win_errhandler_fn*)fn)((MPI_Fint*)win, (MPI_Fint*)errcode);
}


void MSMPI_FORT_CALL mpi_win_create_errhandler__ ( MPI_Win_errhandler_fn*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_create_errhandler( v1, v2 );

    if (*ierr == MPI_SUCCESS) {
         MPIR_Win_errhandler_set_proxy( *v2, MPIR_Win_errhandler_f_proxy);
    }
}


void MSMPI_FORT_CALL mpi_win_get_errhandler__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_get_errhandler( *v1, v2 );
}


void MSMPI_FORT_CALL mpi_win_set_errhandler__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Win_set_errhandler( *v1, *v2 );
}


void MSMPI_FORT_CALL mpi_file_open__ ( const MPI_Fint *v1, _In_count_(d2) const char *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr, MPI_Fint d2 )
{
    char *p2;
    MPI_File l5;

    {
        const char *p = v2 + d2 - 1;
        int  li;
        while (*p == ' ' && p > v2) p--;
        p++;
        p2 = (char *)MPIU_Malloc( p-v2 + 1 );
        if(p2 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup2;
        }
        for (li=0; li<(p-v2); li++) { p2[li] = v2[li]; }
        p2[li] = 0;
    }
    *ierr = MPI_File_open( (MPI_Comm)(*v1), p2, *v3, (MPI_Info)(*v4), &l5 );
    *v5 = MPI_File_c2f(l5);
    MPIU_Free( p2 );
fn_cleanup2:;
}


void MSMPI_FORT_CALL mpi_file_close__ ( MPI_Fint *v1, MPI_Fint *ierr )
{
    MPI_File l1 = MPI_File_f2c(*v1);
    *ierr = MPI_File_close( &l1 );
    *v1 = MPI_File_c2f(l1);
}


void MSMPI_FORT_CALL mpi_file_delete__ ( _In_count_(d1) const char *v1, const MPI_Fint *v2, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    *ierr = MPI_File_delete( p1, (MPI_Info)(*v2) );
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_file_set_size__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_set_size( MPI_File_f2c(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_file_preallocate__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_preallocate( MPI_File_f2c(*v1), *v2 );
}


void MSMPI_FORT_CALL mpi_file_get_size__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_size( MPI_File_f2c(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_file_get_group__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_group( MPI_File_f2c(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_file_get_amode__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_amode( MPI_File_f2c(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_file_set_info__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_set_info( MPI_File_f2c(*v1), (MPI_Info)(*v2) );
}


void MSMPI_FORT_CALL mpi_file_get_info__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_info( MPI_File_f2c(*v1), (MPI_Info *)(v2) );
}


void MSMPI_FORT_CALL mpi_file_set_view__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, const MPI_Fint *v4, _In_count_(d5) const char *v5, const MPI_Fint *v6, MPI_Fint *ierr, MPI_Fint d5 )
{
    char *p5;

    {
        const char *p = v5 + d5 - 1;
        int  li;
        while (*p == ' ' && p > v5) p--;
        p++;
        p5 = (char *)MPIU_Malloc( p-v5 + 1 );
        if(p5 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup5;
        }
        for (li=0; li<(p-v5); li++) { p5[li] = v5[li]; }
        p5[li] = 0;
    }
    *ierr = MPI_File_set_view( MPI_File_f2c(*v1), *v2, (MPI_Datatype)(*v3), (MPI_Datatype)(*v4), p5, (MPI_Info)(*v6) );
    MPIU_Free( p5 );
fn_cleanup5:;
}


void MSMPI_FORT_CALL mpi_file_get_view__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *v3, MPI_Fint *v4, _In_count_(d5) char *v5, MPI_Fint *ierr, MPI_Fint d5 )
{
    char *p5;
    p5 = (char *)MPIU_Malloc( d5 + 1 );
    if (p5 == NULL)
    {
        *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
        goto fn_cleanup5;
    }
    p5[0] = 0;
    *ierr = MPI_File_get_view( MPI_File_f2c(*v1), v2, (MPI_Datatype *)(v3), (MPI_Datatype *)(v4), p5 );

    {
        char *p = MPIU_Strncpy(v5, p5, d5);
        while ((p-v5) < d5) { *p++ = ' '; }
    }
    MPIU_Free( p5 );
fn_cleanup5:;
}


void MSMPI_FORT_CALL mpi_file_read_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_at( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5), (MPI_Status *)(v6) );
}


void MSMPI_FORT_CALL mpi_file_read_at_all__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_at_all( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5), (MPI_Status *)(v6) );
}


void MSMPI_FORT_CALL mpi_file_write_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_at( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5), (MPI_Status *)(v6) );
}


void MSMPI_FORT_CALL mpi_file_write_at_all__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_at_all( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5), (MPI_Status *)(v6) );
}


void MSMPI_FORT_CALL mpi_file_iread_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr )
{
    *ierr = MPI_File_iread_at( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5), v6 );
}


void MSMPI_FORT_CALL mpi_file_iwrite_at__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Request*v6, MPI_Fint *ierr )
{
    *ierr = MPI_File_iwrite_at( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5), v6 );
}


void MSMPI_FORT_CALL mpi_file_read__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_read( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_read_all__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_all( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_write__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_write( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_write_all__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_all( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_iread__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_iread( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), v5 );
}


void MSMPI_FORT_CALL mpi_file_iwrite__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_iwrite( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), v5 );
}


void MSMPI_FORT_CALL mpi_file_seek__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_seek( MPI_File_f2c(*v1), *v2, *v3 );
}


void MSMPI_FORT_CALL mpi_file_get_position__ ( const MPI_Fint *v1, MPI_Offset*v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_position( MPI_File_f2c(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_file_get_byte_offset__ ( const MPI_Fint *v1, const MPI_Offset *v2, MPI_Offset *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_byte_offset( MPI_File_f2c(*v1), *v2, v3 );
}


void MSMPI_FORT_CALL mpi_file_read_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_shared( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_write_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_shared( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_iread_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_iread_shared( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), v5 );
}


void MSMPI_FORT_CALL mpi_file_iwrite_shared__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Request*v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_iwrite_shared( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), v5 );
}


void MSMPI_FORT_CALL mpi_file_read_ordered__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_ordered( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_write_ordered__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_ordered( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4), (MPI_Status *)(v5) );
}


void MSMPI_FORT_CALL mpi_file_seek_shared__ ( const MPI_Fint *v1, const MPI_Offset *v2, const MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_seek_shared( MPI_File_f2c(*v1), *v2, *v3 );
}


void MSMPI_FORT_CALL mpi_file_get_position_shared__ ( const MPI_Fint *v1, MPI_Offset *v2, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_position_shared( MPI_File_f2c(*v1), v2 );
}


void MSMPI_FORT_CALL mpi_file_read_at_all_begin__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_at_all_begin( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5) );
}


void MSMPI_FORT_CALL mpi_file_read_at_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_at_all_end( MPI_File_f2c(*v1), v2, (MPI_Status *)(v3) );
}


void MSMPI_FORT_CALL mpi_file_write_at_all_begin__ ( const MPI_Fint *v1, const MPI_Offset *v2, void *v3, const MPI_Fint *v4, const MPI_Fint *v5, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_at_all_begin( MPI_File_f2c(*v1), *v2, v3, *v4, (MPI_Datatype)(*v5) );
}


void MSMPI_FORT_CALL mpi_file_write_at_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_at_all_end( MPI_File_f2c(*v1), v2, (MPI_Status *)(v3) );
}


void MSMPI_FORT_CALL mpi_file_read_all_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_all_begin( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4) );
}


void MSMPI_FORT_CALL mpi_file_read_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_all_end( MPI_File_f2c(*v1), v2, (MPI_Status *)(v3) );
}


void MSMPI_FORT_CALL mpi_file_write_all_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_all_begin( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4) );
}


void MSMPI_FORT_CALL mpi_file_write_all_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_all_end( MPI_File_f2c(*v1), v2, (MPI_Status *)(v3) );
}


void MSMPI_FORT_CALL mpi_file_read_ordered_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_ordered_begin( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4) );
}


void MSMPI_FORT_CALL mpi_file_read_ordered_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_read_ordered_end( MPI_File_f2c(*v1), v2, (MPI_Status *)(v3) );
}


void MSMPI_FORT_CALL mpi_file_write_ordered_begin__ ( const MPI_Fint *v1, void *v2, const MPI_Fint *v3, const MPI_Fint *v4, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_ordered_begin( MPI_File_f2c(*v1), v2, *v3, (MPI_Datatype)(*v4) );
}


void MSMPI_FORT_CALL mpi_file_write_ordered_end__ ( const MPI_Fint *v1, void *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_write_ordered_end( MPI_File_f2c(*v1), v2, (MPI_Status *)(v3) );
}


void MSMPI_FORT_CALL mpi_file_get_type_extent__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_FAint * v3, MPI_Fint *ierr )
{
    *ierr = MPI_File_get_type_extent( MPI_File_f2c(*v1), (MPI_Datatype)(*v2), v3 );
}


/* This isn't a callable function */
int MSMPI_FORT_CALL mpi_conversion_fn_null__ ( void*v1, MPI_Fint*v2, MPI_Fint*v3, void*v4, MPI_Offset*v5, MPI_Fint *v6, MPI_Fint*v7, MPI_Fint *ierr )
{
    return 0;
}


void MSMPI_FORT_CALL mpi_register_datarep__ ( _In_count_(d1) const char *v1, _In_opt_ MPI_Datarep_conversion_function *v2, _In_opt_ MPI_Datarep_conversion_function*v3, _In_ MPI_Datarep_extent_function*v4, _In_opt_ void *v5, MPI_Fint *ierr, MPI_Fint d1 )
{
    char *p1;

    {
        const char *p = v1 + d1 - 1;
        int  li;
        while (*p == ' ' && p > v1) p--;
        p++;
        p1 = (char *)MPIU_Malloc( p-v1 + 1 );
        if(p1 == NULL)
        {
            *ierr = MPIR_Error(MPI_ERR_NO_MEM, __FUNCTION__);
            goto fn_cleanup1;
        }
        for (li=0; li<(p-v1); li++) { p1[li] = v1[li]; }
        p1[li] = 0;
    }
    OACR_WARNING_DISABLE(25018, "Cast of functions with different calling conventions is benign.");
    OACR_WARNING_SUPPRESS(DIFFERENT_PARAM_TYPE_SIZE, "Parameter size can be safely ignored.");
    if (v2 == (MPI_Datarep_conversion_function *)mpi_conversion_fn_null__){
         v2 = MPI_CONVERSION_FN_NULL;
    }

    OACR_WARNING_SUPPRESS(DIFFERENT_PARAM_TYPE_SIZE, "Parameter size can be safely ignored.");
    if (v3 == (MPI_Datarep_conversion_function *)mpi_conversion_fn_null__){
         v3 = MPI_CONVERSION_FN_NULL;
    }
    OACR_WARNING_ENABLE(25018, "");
    *ierr = MPI_Register_datarep( p1, v2, v3, v4, v5 );
    MPIU_Free( p1 );
fn_cleanup1:;
}


void MSMPI_FORT_CALL mpi_file_set_atomicity__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *ierr )
{
    int l2;
    l2 = MPIR_FROM_FLOG(*v2);
    *ierr = MPI_File_set_atomicity( MPI_File_f2c(*v1), l2 );
}


void MSMPI_FORT_CALL mpi_file_get_atomicity__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    int l2;
    *ierr = MPI_File_get_atomicity( MPI_File_f2c(*v1), &l2 );
    *v2 = MPIR_TO_FLOG(l2);
}


void MSMPI_FORT_CALL mpi_file_sync__ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_File_sync( MPI_File_f2c(*v1) );
}


void MSMPI_FORT_CALL mpi_pcontrol__ ( const MPI_Fint *v1, MPI_Fint *ierr )
{
    *ierr = MPI_Pcontrol( (int)*v1 );
}


void MSMPI_FORT_CALL mpi_address__ ( void*v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    MPI_Aint a, b;
    *ierr = MPI_Get_address( v1, &a );


    b = a - (MPI_Aint) MPIR_F_MPI_BOTTOM;
    *v2 = (MPI_Fint)( b );
#ifdef HAVE_AINT_LARGER_THAN_FINT
    /* Check for truncation */
    if ((MPI_Aint)*v2 - b != 0) {
        *ierr = MPIR_Error(MPI_ERR_ARG, __FUNCTION__);
    }
#endif
}


void MSMPI_FORT_CALL mpi_get_address__ ( void*v1, MPI_Aint*v2, MPI_Fint *ierr )
{
    MPI_Aint a;
    *ierr = MPI_Get_address( v1, &a );


    a = a - (MPI_Aint) MPIR_F_MPI_BOTTOM;
    *v2 =  a;
}


double MSMPI_FORT_CALL mpi_wtime__ ( void )
{
    return MPI_Wtime();
}


double MSMPI_FORT_CALL mpi_wtick__ ( void )
{
    return MPI_Wtick();
}



/* The F77 attr copy function prototype and calling convention */
typedef void (MSMPI_FORT_CALL F77_CopyFunction) (MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *,MPI_Fint *, MPI_Fint *, MPI_Fint *);

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
typedef void (MSMPI_FORT_CALL F77_DeleteFunction) (MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *);

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


void MSMPI_FORT_CALL mpi_keyval_create__ ( MPI_Copy_function v1, MPI_Delete_function v2, MPI_Fint *v3, void*v4, MPI_Fint *ierr )
{
        *ierr = MPI_Comm_create_keyval( v1, v2, v3, v4 );
        if (!*ierr) {
            MPIR_Keyval_set_proxy( *v3, MPIR_Comm_copy_attr_f77_proxy,  MPIR_Comm_delete_attr_f77_proxy);
        }
}


void MSMPI_FORT_CALL mpi_dup_fn__ ( MPI_Fint v1, const MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
        *v5 = *v4;
        *v6 = MPIR_TO_FLOG(1);
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_null_delete_fn__ ( const MPI_Fint*v1, const MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_null_copy_fn__ ( const MPI_Fint*v1, const MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
        *v6 = MPIR_TO_FLOG(0);
}


void MSMPI_FORT_CALL mpi_comm_dup_fn__ ( MPI_Fint v1, const MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
        *v5 = *v4;
        *v6 = MPIR_TO_FLOG(1);
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_comm_null_delete_fn__ ( const MPI_Fint*v1, const MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_comm_null_copy_fn__ ( const MPI_Fint *v1, const MPI_Fint *v2, void *v3, void *v4, void *v5, MPI_Fint *v6, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
        *v6 = MPIR_TO_FLOG(0);
}


void MSMPI_FORT_CALL mpi_win_dup_fn__ ( MPI_Fint v1, const MPI_Fint*v2, void*v3, void**v4, void **v5, MPI_Fint*v6, MPI_Fint *ierr )
{
        *v5 = *v4;
        *v6 = MPIR_TO_FLOG(1);
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_win_null_delete_fn__ ( const MPI_Fint*v1, const MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_win_null_copy_fn__ ( const MPI_Fint*v1, const MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
        *v6 = MPIR_TO_FLOG(0);
}


void MSMPI_FORT_CALL mpi_type_dup_fn__ ( MPI_Fint v1, const MPI_Fint*v2, void*v3, void**v4, void**v5, MPI_Fint*v6, MPI_Fint *ierr )
{
        *v5 = *v4;
        *v6 = MPIR_TO_FLOG(1);
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_type_null_delete_fn__ ( const MPI_Fint*v1, const MPI_Fint*v2, void*v3, void*v4, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
}


void MSMPI_FORT_CALL mpi_type_null_copy_fn__ ( const MPI_Fint*v1, const MPI_Fint*v2, void*v3, void*v4, void*v5, MPI_Fint *v6, MPI_Fint *ierr )
{
        *ierr = MPI_SUCCESS;
        *v6 = MPIR_TO_FLOG(0);
}


void MSMPI_FORT_CALL mpi_type_create_f90_real__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_f90_real( *v1, *v2, (MPI_Datatype*)(v3) );
}


void MSMPI_FORT_CALL mpi_type_create_f90_complex__ ( const MPI_Fint *v1, const MPI_Fint *v2, MPI_Fint *v3, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_f90_complex( *v1, *v2, (MPI_Datatype*)(v3) );
}


void MSMPI_FORT_CALL mpi_type_create_f90_integer__ ( const MPI_Fint *v1, MPI_Fint *v2, MPI_Fint *ierr )
{
    *ierr = MPI_Type_create_f90_integer( *v1, (MPI_Datatype*)(v2) );
}

#if defined(__cplusplus)
}
#endif

