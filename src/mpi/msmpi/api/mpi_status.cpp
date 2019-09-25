// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
  MPI_Get_count - Gets the number of "top level" elements

Input Parameters:
+ status - return status of receive operation (Status)
- datatype - datatype of each receive buffer element (handle)

Output Parameter:
. count - number of received elements (integer)
Notes:
If the size of the datatype is zero, this routine will return a count of
zero.  If the amount of data in 'status' is not an exact multiple of the
size of 'datatype' (so that 'count' would not be integral), a 'count' of
'MPI_UNDEFINED' is returned instead.

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
@*/
EXTERN_C
_Pre_satisfies_(status != MPI_STATUS_IGNORE)
MPI_METHOD
MPI_Get_count(
    _In_ const MPI_Status* status,
    _In_ MPI_Datatype datatype,
    _mpi_out_(count, MPI_UNDEFINED) int* count
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Get_count(status, datatype);

    int mpi_errno;

    TypeHandle hType;

    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    if( count == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "count" );
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateCommitted( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Check for correct number of bytes.
    //
    MPI_Count size = hType.GetSize();
    MPI_Count status_count = MPIR_Status_get_count(status);

    if (size != 0)
    {
        if( ( status_count % size ) != 0 || ( status_count / size ) > INT_MAX )
        {
            *count = MPI_UNDEFINED;
        }
        else
        {
            *count = static_cast<int>( status_count / size );
        }
    }
    else if (status_count > 0)
    {
        //
        // Case where datatype size is 0 and count is > 0 should
        // never occur.
        //
        *count = MPI_UNDEFINED;
    }
    else
    {
        //
        // This is ambiguous. However, discussions on MPI Forum
        // reached a consensus that this is the correct return value.
        //
        *count = 0;
    }

    TraceLeave_MPI_Get_count(*count, status_count);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_get_count %p %D %p",
            status,
            datatype,
            count
            )
        );
    TraceError(MPI_Get_count, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Status_set_cancelled - Sets the cancelled state associated with a
   Status object

Input Parameters:
+  status - status to associate cancel flag with (Status)
-  flag - if true indicates request was cancelled (logical)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Status_set_cancelled(
    _In_ MPI_Status* status,
    _In_range_(0,1) int flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Status_set_cancelled(sizeof(*status), status, flag);

    int mpi_errno = MPI_SUCCESS;

    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    MPIR_Status_set_cancelled( status, ( flag ? TRUE : FALSE ) );

    TraceLeave_MPI_Status_set_cancelled();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_status_set_cancelled %p %d",
            status,
            flag
            )
        );
    TraceError(MPI_Status_set_cancelled, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Status_set_elements - Set the number of elements in a status

   Input Parameters:
+ status - status to associate count with (Status)
. datatype - datatype associated with count (handle)
- count - number of elements to associate with status (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Status_set_elements(
    _In_ MPI_Status* status,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, 0) int count
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Status_set_elements(status, datatype, count);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateCommitted( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", count );
        goto fn_fail;
    }

    if( status == nullptr || status == MPI_STATUS_IGNORE )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    MPI_Count count_x = count * hType.GetSize();
    MPIR_Status_set_count( status, count_x );

    TraceLeave_MPI_Status_set_elements( count_x );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_status_set_elements %p %D %d",
            status,
            datatype,
            count
            )
        );
    TraceError(MPI_Status_set_elements, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Status_set_elements_x - Set the number of elements in a status

   Input Parameters:
+ status - status to associate count with (Status)
. datatype - datatype associated with count (handle)
- count - number of elements to associate with status (MPI_Count)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_TYPE
@*/
EXTERN_C
MPI_METHOD
MPI_Status_set_elements_x(
    _In_ MPI_Status *status,
    _In_ MPI_Datatype datatype,
    _In_range_(>=, 0) MPI_Count count
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Status_set_elements_x( status, datatype, count );

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateCommitted( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", count );
        goto fn_fail;
    }

    if( status == nullptr || status == MPI_STATUS_IGNORE )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    count *= hType.GetSize();
    MPIR_Status_set_count( status, count );

    TraceLeave_MPI_Status_set_elements_x( count );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_status_set_elements_x %p %D %l",
            status,
            datatype,
            count
            )
        );
    TraceError( MPI_Status_set_elements_x, mpi_errno );
    goto fn_exit;
}


/* Not quite correct, but much closer for MPI2 */
/* TODO: still needs to handle partial datatypes and situations where the mpi
 * implementation fills status with something other than bytes (globus2 might
 * do this) */
int MPIR_Status_set_bytes(MPI_Status *status, MPI_Datatype /*datatype*/,
                          int nbytes)
{
    /* it's ok that ROMIO stores number-of-bytes in status, not
     * count-of-copies, as long as MPI_GET_COUNT knows what to do */
    if (status != MPI_STATUS_IGNORE)
    {
        return NMPI_Status_set_elements(status, MPI_BYTE, nbytes);
    }
    return MPI_SUCCESS;
}


EXTERN_C
MPI_METHOD
MPI_Status_c2f(
    _In_ const MPI_Status* c_status,
    _Out_ MPI_Fint* f_status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();

    int mpi_errno = MPI_SUCCESS;

    /* This code assumes that the ints are the same size */
    if (c_status == MPI_STATUS_IGNORE)
    {
        /* The call is erroneous (see 4.12.5 in MPI-2) */
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**notcstatignore" );
        goto fn_fail;
    }

    if (c_status == MPI_STATUSES_IGNORE)
    {
        /* The call is erroneous (see 4.12.5 in MPI-2) */
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**notcstatignore" );
        goto fn_fail;
    }

    *(MPI_Status *)f_status = *c_status;

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm( 0, FCNAME,  mpi_errno );
    goto fn_exit;
}


EXTERN_C
MPI_METHOD
MPI_Status_f2c(
    _In_ const MPI_Fint* f_status,
    _Out_ MPI_Status* c_status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();

    int mpi_errno = MPI_SUCCESS;
    /* This code assumes that the ints are the same size */

    if (f_status == MPI_F_STATUS_IGNORE)
    {
        /* The call is erroneous (see 4.12.5 in MPI-2) */
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**notfstatignore" );
        goto fn_fail;
    }
    *c_status = *(MPI_Status *) f_status;

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm( 0, FCNAME,  mpi_errno );
    goto fn_exit;
}


/*@
   MPI_Get_elements - get_elements

   Arguments:
+  MPI_Status *status - status
.  MPI_Datatype datatype - datatype
-  int *elements - elements

   Notes:

 If the size of the datatype is zero and the amount of data returned as
 determined by 'status' is also zero, this routine will return a count of
 zero.  This is consistent with a clarification made by the MPI Forum.

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Get_elements(
    _In_ const MPI_Status* status,
    _In_ MPI_Datatype datatype,
    _mpi_out_(count, MPI_UNDEFINED) int* count
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Get_elements(status, datatype);

    TypeHandle hType;

    int mpi_errno;

    /* Validate parameters, especially handles needing to be converted */
    if( datatype == MPI_LB || datatype == MPI_UB )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**dtype");
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateCommitted( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    if( count == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "count" );
        goto fn_fail;
    }

    MPI_Count byte_count = MPIR_Status_get_count( status );
    MPI_Count count_x = MPIR_Type_get_elements(
        &byte_count,
        -1,
        hType.GetMpiHandle()
        );

    if( byte_count != 0 || count_x > INT_MAX )
    {
        *count = MPI_UNDEFINED;
    }
    else
    {
        *count = static_cast<int>( count_x );
    }

    TraceLeave_MPI_Get_elements( *count, byte_count );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_get_elements %p %D %p",
            status,
            datatype,
            count
            )
        );
    TraceError(MPI_Get_elements, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Get_elements_x - get the number of basic elements received

   Arguments:
+  status - status of receive operation
.  datatype - datatype used by receive operation
-  count - number of received basic elements

   Notes:
 If the size of the datatype is zero and the amount of data returned as
 determined by 'status' is also zero, this routine will return a count of
 zero. This is consistent with a clarification made by the MPI Forum.

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Get_elements_x(
    _In_ const MPI_Status *status,
    _In_ MPI_Datatype datatype,
    _mpi_out_(count, MPI_UNDEFINED) MPI_Count *count
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Get_elements_x( status, datatype );

    TypeHandle hType;
    int mpi_errno;

    //
    // Validate parameters, especially handles needing to be converted.
    //
    if( datatype == MPI_LB || datatype == MPI_UB )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**dtype");
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateCommitted( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    if( count == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "count" );
        goto fn_fail;
    }

    MPI_Count byte_count = MPIR_Status_get_count( status );
    *count = MPIR_Type_get_elements( &byte_count, -1, hType.GetMpiHandle() );

    if( byte_count != 0 )
    {
        *count = MPI_UNDEFINED;
    }

    TraceLeave_MPI_Get_elements_x( *count, byte_count );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_get_elements_x %p %D %p",
            status,
            datatype,
            count
            )
        );
    TraceError( MPI_Get_elements_x, mpi_errno );
    goto fn_exit;
}


/*@
  MPI_Test_cancelled - Tests to see if a request was cancelled

Input Parameter:
. status - status object (Status)

Output Parameter:
. flag - true if the request was cancelled, false otherwise (logical)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
_Pre_satisfies_(status != MPI_STATUS_IGNORE)
MPI_METHOD
MPI_Test_cancelled(
    _In_ const MPI_Status* status,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Test_cancelled(sizeof(*status),(void*)status);

    int mpi_errno = MPI_SUCCESS;

    /* Validate parameters if error checking is enabled */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }
    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    *flag = MPIR_Status_get_cancelled( status );

    TraceLeave_MPI_Test_cancelled(*flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_test_cancelled %p %p",
            status,
            flag
            )
        );
    TraceError(MPI_Test_cancelled, mpi_errno);
    goto fn_exit;
}
