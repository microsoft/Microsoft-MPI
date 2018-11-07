// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */


#ifndef ADIO_PROTO
#define ADIO_PROTO

/*--------------------------------------------------------------------------*/
/* GENERIC file system interface                                            */
/*--------------------------------------------------------------------------*/

int
ADIOI_GEN_Delete(
    const wchar_t* filename
    );

int
ADIOI_GEN_ReadStrided(
    MPI_File fd,
    _Out_ void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

int
ADIOI_GEN_IreadStrided(
    MPI_File fd,
    void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Request* request
    );

int
ADIOI_GEN_IwriteStrided(
    MPI_File fd,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Request* request
    );

int
ADIOI_GEN_ReadStrided_naive(
    MPI_File fd,
    void* buf,
    int count,
    MPI_Datatype buftype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

int
ADIOI_GEN_WriteStrided(
    MPI_File fd,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

int
ADIOI_GEN_WriteStrided_naive(
    MPI_File fd,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

int
ADIOI_GEN_ReadStridedColl(
    MPI_File fd,
    void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

int
ADIOI_GEN_WriteStridedColl(
    MPI_File fd,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

void
ADIOI_GEN_SeekIndividual(
    MPI_File fd,
    MPI_Offset offset
    );

int
ADIOI_GEN_SetInfo(
    MPI_File fd,
    MPI_Info users_info
    );

int
ADIOI_GEN_Prealloc(
    MPI_File fd,
    MPI_Offset size
    );


/*--------------------------------------------------------------------------*/
/* NTFS specific interface                                                  */
/*--------------------------------------------------------------------------*/

int
ADIOI_NTFS_Open(
    MPI_File fd
    );

void
ADIOI_NTFS_Close(
    MPI_File fd
    );

int
ADIOI_NTFS_ReadContig(
    MPI_File fd,
    void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

int
ADIOI_NTFS_WriteContig(
    MPI_File fd,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Status* status
    );

int
ADIOI_NTFS_IwriteContig(
    MPI_File fd,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Request* request
    );   

int
ADIOI_NTFS_IreadContig(
    MPI_File fd,
    void* buf,
    int count, 
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Request* request
    );

int
ADIOI_NTFS_Fcntl(
    MPI_File fd,
    int flag,
    ADIO_Fcntl_t* fcntl_struct
    );

int
ADIOI_NTFS_Flush(
    MPI_File fd
    );

int
ADIOI_NTFS_Resize(
    MPI_File fd,
    MPI_Offset size
    );


/*--------------------------------------------------------------------------*/
/* ADIO to NTFS/GEN interface mapping                                       */
/*--------------------------------------------------------------------------*/

#define ADIO_FS_Open            ADIOI_NTFS_Open
#define ADIO_ReadContig         ADIOI_NTFS_ReadContig
#define ADIO_WriteContig        ADIOI_NTFS_WriteContig
#define ADIO_ReadStridedColl    ADIOI_GEN_ReadStridedColl
#define ADIO_WriteStridedColl   ADIOI_GEN_WriteStridedColl
#define ADIO_SeekIndividual     ADIOI_GEN_SeekIndividual
#define ADIO_Fcntl              ADIOI_NTFS_Fcntl
#define ADIO_SetInfo            ADIOI_GEN_SetInfo
#define ADIO_ReadStrided        ADIOI_GEN_ReadStrided
#define ADIO_WriteStrided       ADIOI_GEN_WriteStrided
#define ADIO_FS_Close           ADIOI_NTFS_Close
#define ADIO_IreadContig        ADIOI_NTFS_IreadContig
#define ADIO_IwriteContig       ADIOI_NTFS_IwriteContig
#define ADIO_IreadStrided       ADIOI_GEN_IreadStrided
#define ADIO_IwriteStrided      ADIOI_GEN_IwriteStrided
#define ADIO_Flush              ADIOI_NTFS_Flush
#define ADIO_Resize             ADIOI_NTFS_Resize
#define ADIO_FS_Delete          ADIOI_GEN_Delete


#endif
