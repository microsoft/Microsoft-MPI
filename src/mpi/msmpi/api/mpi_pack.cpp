// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
    MPI_Pack - Packs a datatype into contiguous memory

   Arguments:
+  void *inbuf - input buffer
.  int incount - input count
.  MPI_Datatype datatype - datatype
.  void *outbuf - output buffer
.  int outcount - output count
.  int *position - position
-  MPI_Comm comm - communicator

  Notes (from the specifications):

  The input value of position is the first location in the output buffer to be
  used for packing.  position is incremented by the size of the packed message,
  and the output value of position is the first location in the output buffer
  following the locations occupied by the packed message.  The comm argument is
  the communicator that will be subsequently used for sending the packed
  message.


.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Pack(
    _In_opt_ const void* inbuf,
    _In_range_(>=, 0) int incount,
    _In_ MPI_Datatype datatype,
    _mpi_writes_bytes_(outsize) void* outbuf,
    _In_range_(>=, 0) int outsize,
    _mpi_position_(outsize) int* position,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Pack(inbuf, incount, datatype, outbuf, outsize, *position, comm);

    MPI_Aint first, last;
    TypeHandle hType;
    MPID_Segment *segp;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: inbuf could be null (MPI_BOTTOM) */
    mpi_errno = MpiaDatatypeValidate(
        inbuf,
        incount,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( outsize < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**argneg %s %d", "outsize", outsize);
        goto fn_fail;
    }

    if (incount > 0)
    {
        if( outbuf == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "outbuf" );
            goto fn_fail;
        }
    }

    if( position == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "position" );
        goto fn_fail;
    }

    if (*position < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**argposneg %d", *position);
        goto fn_fail;
    }

    /* Verify that there is space in the buffer to pack the type */
    MPI_Count cbInput = hType.GetSize() * incount;
    MPI_Count cbOutput = outsize - *position;
    if( cbInput > cbOutput )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_ARG,
            "**argpackbuf %l %l",
            cbInput,
            cbOutput
            );
        goto fn_fail;
    }

    if (incount == 0)
    {
        goto fn_exit;
    }

    /* TODO: CHECK RETURN VALUES?? */
    /* TODO: SHOULD THIS ALL BE IN A MPID_PACK??? */
    segp = MPID_Segment_alloc();
    if( segp == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    mpi_errno = MPID_Segment_init(inbuf, incount, hType.GetMpiHandle(), segp, 0);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    /* NOTE: the use of buffer values and positions in MPI_Pack and in
     * MPID_Segment_pack are quite different.  See code or docs or something.
     */
    first = 0;
    last  = SEGMENT_IGNORE_LAST;

    MPID_Segment_pack(segp,
                      first,
                      &last,
                      (void *) ((char *) outbuf + *position));

    if( static_cast<__int64>( last ) + static_cast<__int64>( *position ) > INT_MAX )
    {
        //
        // Not safe to truncate the value of last and return it in *position.
        // The standard should be updated to make the position parameter an MPI_Aint*.
        // Signal an error has taken place for now.
        //
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_SIZE, "**packtruncate");
        goto fn_fail;
    }

    *position += static_cast<int>( last );

    MPID_Segment_free(segp);

  fn_exit:
    TraceLeave_MPI_Pack(*position);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_pack %p %d %D %p %d %p %C",
            inbuf,
            incount,
            datatype,
            outbuf,
            outsize,
            position,
            comm
            )
        );
    TraceError(MPI_Pack, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Pack_external - Packs a datatype into contiguous memory, using the
     external32 format

   Input Parameters:
+ datarep - data representation (string)
. inbuf - input buffer start (choice)
. incount - number of input data items (integer)
. datatype - datatype of each input data item (handle)
- outsize - output buffer size, in bytes (integer)

   Output Parameter:
. outbuf - output buffer start (choice)

   Input/Output Parameter:
. position - current position in buffer, in bytes (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
@*/
EXTERN_C
MPI_METHOD
MPI_Pack_external(
    _In_z_ const char* datarep,
    _In_opt_ const void* inbuf,
    _In_range_(>=, 0) int incount,
    _In_ MPI_Datatype datatype,
    _mpi_writes_bytes_(outsize) void* outbuf,
    _In_range_(>=, 0) MPI_Aint outsize,
    _mpi_position_(outsize) MPI_Aint* position
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Pack_external(datarep, inbuf, incount, datatype, outbuf, outsize, *position);

    MPI_Aint first, last;
    TypeHandle hType;

    MPID_Segment *segp;

    int mpi_errno = MpiaDatatypeValidate( inbuf, incount, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        datarep,
                        -1,
                        "external32",
                        -1 ) != CSTR_EQUAL )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_DATAREP,
            "**datarepunsupported %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    if( outsize < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", outsize );;
        goto fn_fail;
    }
    /* NOTE: inbuf could be null (MPI_BOTTOM) */
    if (incount > 0)
    {
        if( outbuf == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "outbuf" );
            goto fn_fail;
        }
    }
    if( position == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "position" );
        goto fn_fail;
    }

    if (incount == 0)
    {
        goto fn_exit;
    }

    segp = MPID_Segment_alloc();
    if( segp == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    mpi_errno = MPID_Segment_init(inbuf, incount, datatype, segp, 1);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: the use of buffer values and positions in MPI_Pack_external and
     * in MPID_Segment_pack_external are quite different.  See code or docs
     * or something.
     */
    first = 0;
    last  = SEGMENT_IGNORE_LAST;

    MPID_Segment_pack_external32(segp,
                                 first,
                                 &last,
                                 (void *)((char *) outbuf + *position));

    *position += last;

    MPID_Segment_free(segp);

  fn_exit:
    TraceLeave_MPI_Pack_external(*position);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_pack_external %s %p %d %D %p %d %p",
            datarep,
            inbuf,
            incount,
            datatype,
            outbuf,
            outsize,
            position
            )
        );
    TraceError(MPI_Pack_external, mpi_errno);
    goto fn_exit1;
}


/*@
  MPI_Pack_external_size - Returns the upper bound on the amount of
  space needed to pack a message using MPI_Pack_external.

   Input Parameters:
+ datarep - data representation (string)
. incount - number of input data items (integer)
- datatype - datatype of each input data item (handle)

   Output Parameters:
. size - output buffer size, in bytes (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Pack_external_size(
    _In_z_ const char* datarep,
    _In_range_(>=, 0) int incount,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Aint* size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Pack_external_size(datarep, incount, datatype);

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidateCommitted( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        datarep,
                        -1,
                        "external32",
                        -1 ) != CSTR_EQUAL )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_DATAREP,
            "**datarepunsupported %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    if( incount < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", incount );
        goto fn_fail;
    }

    *size = incount * MPID_Datatype_size_external32(hType.Get(), datatype);

    TraceLeave_MPI_Pack_external_size(*size);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_pack_external_size %s %d %D %p",
            datarep,
            incount,
            datatype,
            size
            )
        );
    TraceError(MPI_Pack_external_size, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Pack_size - Returns the upper bound on the amount of space needed to
                    pack a message

Input Parameters:
+ incount - count argument to packing call (integer)
. datatype - datatype argument to packing call (handle)
- comm - communicator argument to packing call (handle)

Output Parameter:
. size - upper bound on size of packed message, in bytes (integer)

Notes:
The MPI standard document describes this in terms of 'MPI_Pack', but it
applies to both 'MPI_Pack' and 'MPI_Unpack'.  That is, the value 'size' is
the maximum that is needed by either 'MPI_Pack' or 'MPI_Unpack'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TYPE
.N MPI_ERR_ARG

@*/
EXTERN_C
MPI_METHOD
MPI_Pack_size(
    _In_range_(>=, 0) int incount,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Comm comm,
    _mpi_out_(size, MPI_UNDEFINED) int *size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Pack_size(incount, datatype, comm);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateCommitted( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( incount < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", incount );;
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "size" );
        goto fn_fail;
    }

    MPI_Count packsize = incount * hType.GetSize();
    *size = ( packsize > INT_MAX ) ? MPI_UNDEFINED : static_cast<int>( packsize );

    TraceLeave_MPI_Pack_size(*size);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_pack_size %d %D %C %p",
            incount,
            datatype,
            comm,
            size
            )
        );
    TraceError(MPI_Pack_size, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Unpack - Unpack a buffer according to a datatype into contiguous memory

Input Parameters:
+ inbuf - input buffer start (choice)
. insize - size of input buffer, in bytes (integer)
. position - current position in bytes (integer)
. outcount - number of items to be unpacked (integer)
. datatype - datatype of each output data item (handle)
- comm - communicator for packed message (handle)

Output Parameter:
. outbuf - output buffer start (choice)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_ARG

.seealso: MPI_Pack, MPI_Pack_size
@*/
EXTERN_C
MPI_METHOD
MPI_Unpack(
    _mpi_reads_bytes_(insize) const void* inbuf,
    _In_range_(>=, 0) int insize,
    _mpi_position_(insize) int* position,
    _When_(insize > 0, _Out_opt_) void* outbuf,
    _In_range_(>=, 0) int outcount,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Unpack(inbuf, insize, *position, outbuf, outcount, datatype, comm);

    MPI_Aint first, last;
    MPID_Segment *segp;

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate( outbuf, outcount, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( position == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "position" );
        goto fn_fail;
    }

    if( insize < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", insize );;
        goto fn_fail;
    }

    if (insize == 0)
    {
        goto fn_exit;
    }

    if( inbuf == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "inbuf" );
        goto fn_fail;
    }

    segp = MPID_Segment_alloc();
    if( segp == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    MPID_Segment_init(outbuf, outcount, datatype, segp, 0);

    /* NOTE: the use of buffer values and positions in MPI_Unpack and in
     * MPID_Segment_unpack are quite different.  See code or docs or something.
     */
    first = 0;
    last  = SEGMENT_IGNORE_LAST;

    MPID_Segment_unpack(segp,
                        first,
                        &last,
                        reinterpret_cast<const void *>(
                            static_cast<const char *>( inbuf ) + *position
                            )
                        );

    if( static_cast<__int64>( last ) + static_cast<__int64>( *position ) > INT_MAX )
    {
        //
        // Not safe to truncate the value of last and return it in *position.
        // The standard should be updated to make the position parameter an MPI_Aint*.
        // Signal an error has taken place for now.
        //
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_SIZE, "**unpacktruncate");
        goto fn_fail;
    }

    *position += static_cast<int>( last );

    MPID_Segment_free(segp);

  fn_exit:
    TraceLeave_MPI_Unpack(*position);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_unpack %p %d %p %p %d %D %C",
            inbuf,
            insize,
            position,
            outbuf,
            outcount,
            datatype,
            comm
            )
        );
    TraceError(MPI_Unpack, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Unpack_external - Unpack a buffer (packed with MPI_Pack_external)
   according to a datatype into contiguous memory

   Input Parameters:
+ datarep - data representation (string)
. inbuf - input buffer start (choice)
. insize - input buffer size, in bytes (integer)
. outcount - number of output data items (integer)
. datatype - datatype of output data item (handle)

   Input/Output Parameter:
. position - current position in buffer, in bytes (integer)

   Output Parameter:
. outbuf - output buffer start (choice)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Unpack_external(
    _In_z_ const char* datarep,
    _In_reads_bytes_opt_(insize) const void* inbuf,
    _In_range_(>=, 0) MPI_Aint insize,
    _mpi_position_(insize) MPI_Aint* position,
    _When_(insize > 0, _Out_opt_) void* outbuf,
    _In_range_(>=, 0) int outcount,
    _In_ MPI_Datatype datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Unpack_external(datarep, inbuf, insize, *position, outbuf, outcount, datatype);

    MPI_Aint first, last;
    MPID_Segment *segp;

    TypeHandle hType;
    int mpi_errno = MpiaDatatypeValidate( outbuf, outcount, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        datarep,
                        -1,
                        "external32",
                        -1 ) != CSTR_EQUAL )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_DATAREP,
            "**datarepunsupported %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    if( position == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "position" );
        goto fn_fail;
    }

    if( insize < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", insize );;
        goto fn_fail;
    }

    if (insize == 0)
    {
        goto fn_exit;
    }

    if( inbuf == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "inbuf" );
        goto fn_fail;
    }

    segp = MPID_Segment_alloc();
    if( segp == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    mpi_errno = MPID_Segment_init(outbuf, outcount, datatype, segp, 1);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: buffer values and positions in MPI_Unpack_external are used very
     * differently from use in MPID_Segment_unpack_external...
     */
    first = 0;
    last  = SEGMENT_IGNORE_LAST;

    MPID_Segment_unpack_external32(
        segp,
        first,
        &last,
        static_cast<UINT8*>( const_cast<void*>(inbuf) ) + *position
        );

    *position += last;

    MPID_Segment_free(segp);

  fn_exit:
    TraceLeave_MPI_Unpack_external(*position);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_unpack_external %s %p %d %p %p %d %D",
            datarep,
            inbuf,
            insize,
            position,
            outbuf,
            outcount,
            datatype
            )
        );
    TraceError(MPI_Unpack_external, mpi_errno);
    goto fn_exit1;
}
