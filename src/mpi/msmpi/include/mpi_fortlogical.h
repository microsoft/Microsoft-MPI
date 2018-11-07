// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef MPI_FORTLOGICAL_H_INCLUDED
#define MPI_FORTLOGICAL_H_INCLUDED

/* Fortran logical values */
#define MPIR_F_TRUE  1
#define MPIR_F_FALSE 0

/* 
   Note on true and false.  This code is only an approximation.
   Some systems define either true or false, and allow some or ALL other
   patterns for the other.  This is just like C, where 0 is false and 
   anything not zero is true.  Modify this test as necessary for your
   system.

   We check against FALSE instead of TRUE because many (perhaps all at this
   point) Fortran compilers use 0 for false and some non-zero value for
   true.  By using this test, it is possible to use the same Fortran
   interface library for multiple compilers that differ only in the 
   value used for Fortran .TRUE. .
 */
#define MPIR_FROM_FLOG(a) ( (a) == MPIR_F_FALSE ? 0 : 1 )
#define MPIR_TO_FLOG(a) ((a) ? MPIR_F_TRUE : MPIR_F_FALSE)


//
// operators to support logical operations on MPI_Fint (MPI_LOGICAL)
//
class FLogical
{
    MPI_Fint _val;
private:
    FLogical( MPI_Fint val ) : _val( val ){}

public:
    FLogical operator||( const FLogical& rhs ) const
    {
        if( rhs._val != MPIR_F_FALSE || _val != MPIR_F_FALSE )
        {
            return FLogical( MPIR_F_TRUE );
        }
        return FLogical( MPIR_F_FALSE );
    }

    FLogical operator&&( const FLogical& rhs ) const
    {
        if( rhs._val == MPIR_F_FALSE || _val == MPIR_F_FALSE )
        {
            return FLogical( MPIR_F_FALSE );
        }
        return FLogical( MPIR_F_TRUE );
    }

    FLogical operator!() const
    {
        if( _val == MPIR_F_FALSE )
        {
            return FLogical( MPIR_F_TRUE );
        }
        return FLogical( MPIR_F_FALSE );
    }
};

#endif /* MPI_FORTLOGICAL_H_INCLUDED */
