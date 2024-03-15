// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef MPICHTIMER_H
#define MPICHTIMER_H
/*
 * This include file provide the definitions that are necessary to use the
 * timer calls, including the definition of the time stamp type and 
 * any inlined timer calls.
 *
 * The include file timerconf.h (created by autoheader from configure.in)
 * is needed only to build the function versions of the timers.
 */

/* Define a time stamp */
typedef LARGE_INTEGER MPID_Time_t;

/* 
 * Prototypes.  These are defined here so that inlined timer calls can
 * use them, as well as any profiling and timing code that is built into
 * MPICH
 */
/*@
  MPID_Wtime - Return a time stamp
  
  Output Parameter:
. timeval - A pointer to an 'MPID_Wtime_t' variable.

  Notes:
  This routine returns an `opaque` time value.  This difference between two
  time values returned by 'MPID_Wtime' can be converted into an elapsed time
  in seconds with the routine 'MPID_Wtime_diff'.

  This routine is defined this way to simplify its implementation as a macro.
  For example, the for Intel x86 and gcc, 
.vb
#define MPID_Wtime(timeval) \
     __asm__ __volatile__("rdtsc" : "=A" (*timeval))
.ve

  For some purposes, it is important
  that the timer calls change the timing of the code as little as possible.
  This form of a timer routine provides for a very fast timer that has
  minimal impact on the rest of the code.  

  From a semantic standpoint, this format emphasizes that any particular
  timer value has no meaning; only the difference between two values is 
  meaningful.

  Module:
  Timer

  Question:
  MPI-2 allows 'MPI_Wtime' to be a macro.  We should make that easy; this
  version does not accomplish that.
  @*/
void MPID_Wtime( MPID_Time_t * timeval);

/*@
  MPID_Wtime_todouble - Converts an MPID timestamp to a double 

  Input Parameter:
. timeval - 'MPID_Time_t' time stamp

  Output Parameter:
. seconds - Time in seconds from an arbitrary (but fixed) time in the past

  Notes:
  This routine may be used to change a timestamp into a number of seconds,
  suitable for 'MPI_Wtime'.  

  @*/
void MPID_Wtime_todouble( const MPID_Time_t *timeval, double *seconds );

/*@
  MPID_Wtick - Provide the resolution of the 'MPID_Wtime' timer

  Return value:
  Resolution of the timer in seconds.  In many cases, this is the 
  time between ticks of the clock that 'MPID_Wtime' returns.  In other
  words, the minimum significant difference that can be computed by 
  'MPID_Wtime_diff'.

  Note that in some cases, the resolution may be estimated.  No application
  should expect either the same estimate in different runs or the same
  value on different processes.

  Module:
  Timer
  @*/
double MPID_Wtick( void );

/*@
  MPID_Wtime_init - Initialize the timer

  Note:
  This routine should perform any steps needed to initialize the timer.
  In addition, it should set the value of the attribute 'MPI_WTIME_IS_GLOBAL'
  if the timer is known to be the same for all processes in 'MPI_COMM_WORLD'
  (the value is zero by default).

  If any operations need to be performed when the MPI program calls 
  'MPI_Finalize' this routine should register a handler with 'MPI_Finalize'
  (see the MPICH Design Document).
  
  Module:
  Timer

  @*/
void MPID_Wtime_init(void);

/* Inlined timers.  Note that any definition of one of the functions
   prototyped above in terms of a macro will simply cause the compiler
   to use the macro instead of the function definition.

   Currently, all except the Windows performance counter timers
   define MPID_Wtime_init as null; by default, the value of
   MPI_WTIME_IS_GLOBAL is false.
 */

#define MPID_Wtime(var) \
    QueryPerformanceCounter(var)


#endif
