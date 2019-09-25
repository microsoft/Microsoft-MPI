C     -*- Mode: Fortran; -*-
C
C     Copyright (c) Microsoft Corporation. All rights reserved.
C     Licensed under the MIT License.
C
C     (C) 2001 by Argonne National Laboratory.
C     (C) Microsoft Corporation
C     See COPYRIGHT in top-level directory.
C
       SUBROUTINE MPIRINITF( )

C
C      The common block must be declared the same here as in
C       mpif.h and mpi.f90
C      This information is exported by MSMPI.dll and imported by fortran apps.
C
       INTEGER MPI_BOTTOM, MPI_IN_PLACE
       INTEGER MPI_STATUS_SIZE
       PARAMETER (MPI_STATUS_SIZE=5)
       INTEGER MPI_STATUS_IGNORE(MPI_STATUS_SIZE)
       INTEGER MPI_STATUSES_IGNORE(MPI_STATUS_SIZE,1)
       INTEGER MPI_ERRCODES_IGNORE(1)
       INTEGER MPI_UNWEIGHTED, MPI_WEIGHTS_EMPTY
       CHARACTER*1 MPI_ARGVS_NULL(1,1)
       CHARACTER*1 MPI_ARGV_NULL(1)

       COMMON /MPIPRIV1/ MPI_BOTTOM, MPI_IN_PLACE, MPI_STATUS_IGNORE

       COMMON /MPIPRIV2/ MPI_STATUSES_IGNORE, MPI_ERRCODES_IGNORE
       SAVE /MPIPRIV1/,/MPIPRIV2/
       
       COMMON /MPIFCMB5/ MPI_UNWEIGHTED
       COMMON /MPIFCMB9/ MPI_WEIGHTS_EMPTY
       SAVE /MPIFCMB5/,/MPIFCMB9/

       COMMON /MPIPRIVC/ MPI_ARGVS_NULL, MPI_ARGV_NULL
       SAVE   /MPIPRIVC/

       CALL MPIRINITC( MPI_BOTTOM, MPI_IN_PLACE, MPI_STATUS_IGNORE,
     $     MPI_STATUSES_IGNORE, MPI_ERRCODES_IGNORE,
     $     MPI_UNWEIGHTED, MPI_WEIGHTS_EMPTY,
     $     MPI_ARGVS_NULL )
       CALL MPIRINITC2( MPI_ARGV_NULL )
       END
