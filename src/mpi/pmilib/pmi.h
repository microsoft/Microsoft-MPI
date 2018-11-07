// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef PMI_H_INCLUDED
#define PMI_H_INCLUDED

/* prototypes for the PMI interface in MPICH2 */

/* PMI Group functions */

/*@
PMI_Init - initialize the Process Manager Interface

Output Parameter:
. spawned - spawned flag

Return values:
+ MPI_SUCCESS - initialization completed successfully
- mpi error code - initialization failed

Notes:
Initialize PMI for this process group. The value of spawned indicates whether
this process was created by 'PMI_Spawn_multiple'.  'spawned' will be 1 if
this process group has a parent and 0 if it does not.

@*/
int
PMI_Init(
    _Out_                     int*   spawned,
    _Outptr_result_maybenull_ char** parentPortName
    );

/*@
PMI_Finalize - finalize the Process Manager Interface

Return values:
+ MPI_SUCCESS - finalization completed successfully
- mpi error code - finalization failed

Notes:
 Finalize PMI for this process group.

@*/
int PMI_Finalize( void );

/*@
PMI_Get_size - obtain the size of the process group

Return values:
. the size of the process group to which the local process belongs.

@*/
int PMI_Get_size( void );

/*@
PMI_Get_rank - obtain the rank of the local process in the process group

Return values:
. the rank of the local process in its process group.

@*/
int PMI_Get_rank( void );

/*@
PMI_Get_universe_size - obtain the universe size

Return values:
. the size of the universe (or unknown)


@*/
int PMI_Get_universe_size( void );

/*@
PMI_Get_appnum - obtain the application number

Return values:
. the appnum

@*/
int PMI_Get_appnum( void );

/*@
PMI_Publish_name - publish a name

Input parameters:
. service_name - string representing the service being published
. port - string representing the port on which to contact the service

Return values:
+ MPI_SUCCESS - port for service successfully published
- mpi error code - unable to publish service


@*/
int PMI_Publish_name( const char service_name[], const char port[] );

/*@
PMI_Unpublish_name - unpublish a name

Input parameters:
. service_name - string representing the service being unpublished

Return values:
+ MPI_SUCCESS - port for service successfully published
- mpi error code - unable to unpublish service


@*/
int PMI_Unpublish_name( const char service_name[] );

/*@
PMI_Lookup_name - lookup a service by name

Input parameters:
. service_name - string representing the service being published

Output parameters:
. port - string representing the port on which to contact the service

Return values:
+ MPI_SUCCESS - port for service successfully obtained
- mpi error code - unable to lookup service


@*/
int PMI_Lookup_name( _In_z_ const char service_name[], _Out_writes_(MPI_MAX_PORT_NAME) char port[] );

/*@
PMI_KVS_Get_id - obtain the id of the process group

Notes:
This function returns a guid that uniquely identifies the process group
that the local process belongs to.

@*/
const GUID&
PMI_KVS_Get_id();


/*@
PMI_Barrier - barrier across the process group

Return values:
+ MPI_SUCCESS - barrier successfully finished
- mpi error code - barrier failed

Notes:
This function is a collective call across all processes in the process group
the local process belongs to.  It will not return until all the processes
have called 'PMI_Barrier()'.

@*/
int PMI_Barrier( void );

/*@
PMI_Get_node_ids - get the array that maps ranks to node id.
@*/
UINT16* PMI_Get_node_ids();


/*@
PMI_Get_rank_affinities - returns the HWAFFINITY array that contains affinities set for each rank at launch.
Array will contain entries for all global ranks, but only ranks on this machine will have valid entries, remote ones will have 0.
@*/
void* PMI_Get_rank_affinities();

/*@
PMI_Abort - abort the process group associated with this process

Input Parameters:
+ intern    - indicate if aborted by mpi or the application
. exit_code - exit code to be returned by this process
- error_msg - error message to be printed

Return values:
. none - this function should not return
@*/
DECLSPEC_NORETURN void PMI_Abort(BOOL intern, int exit_code, const char error_msg[]);


/*@
PMI_KVS_Put - put a key/value pair in a keyval space

Input Parameters:
+ kvs - keyval space id
. key - key
- value - value

Return values:
+ MPI_SUCCESS - keyval pair successfully put in keyval space
- mpi error code - put failed

Notes:
This function puts the key/value pair in the specified keyval space.  The
value is not visible to other processes until 'PMI_KVS_Commit()' is called.
The function may complete locally.  After 'PMI_KVS_Commit()' is called, the
value may be retrieved by calling 'PMI_KVS_Get()'.  All keys put to a keyval
space must be unique to the keyval space.  You may not put more than once
with the same key.

@*/
_Success_( return == MPI_SUCCESS )
int
PMI_KVS_Put(
    _In_   const GUID& kvs,
    _In_z_ const char key[],
    _In_z_ const char value[]
    );


/*@
PMI_KVS_PublishBC - Publish the business card of this process

Input Parameters:
+ kvs - keyval space id
. rank - rank of the process
- value - business card string

Return values:
+ MPI_SUCCESS - business card for the process is stored successfully
- mpi error code - publishing failed

Notes:
The function publishes the business card of the process.
@*/
_Success_( return == MPI_SUCCESS )
int
PMI_KVS_PublishBC(
    _In_   const GUID& kvs,
    _In_   UINT16 rank,
    _In_   UINT16 nprocs,
    _In_z_ const char value[]
    );


/*@
PMI_KVS_Commit - commit all previous puts to the keyval space

Input Parameters:
. kvs - keyval space id

Return values:
+ MPI_SUCCESS - commit succeeded
- mpi error code - commit failed

Notes:
This function commits all previous puts since the last 'PMI_KVS_Commit()' into
the specified keyval space. It is a process local operation.

@*/
_Success_( return == MPI_SUCCESS )
int
PMI_KVS_Commit(
    const GUID& kvs
    );

/*@
PMI_KVS_Get - get a key/value pair from a keyval space

Input Parameters:
+ kvs - keyval space id
. key - key
- length - length of value character array

Output Parameters:
. value - value

Return values:
+ MPI_SUCCESS - get succeeded
- mpi error code - get failed

Notes:
This function gets the value of the specified key in the keyval space.

@*/
_Success_( return == MPI_SUCCESS )
int
PMI_KVS_Get(
    _In_   const GUID& kvs,
    _In_z_ const char key[],
    _Out_writes_(length) char value[],
    _In_ size_t length
    );


/*@
PMI_KVS_RetrieveBC - Retrieve the business card of another process

Input Parameters:
+ kvs - keyval space id
. rank - rank
- length - length of value character array

Output Parameters:
. value - the business card of the specified rank

Return values:
+ MPI_SUCCESS - successfully retrieve the business card
- mpi error code - operation failed

Notes:
The function retrieves the business card of the process.
@*/
_Success_( return == MPI_SUCCESS )
int
PMI_KVS_RetrieveBC(
    _In_   const GUID& kvs,
    _In_   UINT16 rank,
    _Out_writes_(length) char value[],
    _In_ size_t length
    );


/* PMI Process Creation functions */

/*S
PMI_keyval_t - keyval structure used by PMI_Spawn_mulitiple

Fields:
+ key - name of the key
- val - value of the key

S*/
typedef struct PMI_keyval_t
{
    char * key;
    char * val;

} PMI_keyval_t;

/*@
PMI_Spawn_multiple - spawn a new set of processes

Input Parameters:
+ count - count of commands
. cmds - array of command strings
. argvs - array of argv arrays for each command string
. maxprocs - array of maximum processes to spawn for each command string
. info_keyval_sizes - array giving the number of elements in each of the  'info_keyval_vectors'
. info_keyval_vectors - array of keyval vector arrays
. preput_keyval_size - Number of elements in 'preput_keyval_vector'
- preput_keyval_vector - array of keyvals to be pre-put in the spawned keyval space

Output Parameter:
. errors - array of errors for each command

Return values:
+ MPI_SUCCESS - spawn successful
- mpi error code - spawn failed

Notes:
This function spawns a set of processes into a new process group.  The 'count'
field refers to the size of the array parameters - 'cmd', 'argvs', 'maxprocs',
'info_keyval_sizes' and 'info_keyval_vectors'.  The 'preput_keyval_size' refers
to the size of the 'preput_keyval_vector' array.  The 'preput_keyval_vector'
contains keyval pairs that will be put in the keyval space of the newly
created process group before the processes are started.  The 'maxprocs' array
specifies the desired number of processes to create for each 'cmd' string.
The actual number of processes may be less than the numbers specified in
maxprocs.  The acceptable number of processes spawned may be controlled by
``soft'' keyvals in the info arrays.  The ``soft'' option is specified by
mpiexec in the MPI-2 standard.  Environment variables may be passed to the
spawned processes through PMI implementation specific 'info_keyval' parameters.
@*/
_Success_(return == MPI_SUCCESS)
int
PMI_Spawn_multiple(
    _In_range_(>= , 0)   int       count,
    _In_reads_(count)    wchar_t** cmds,
    _In_reads_(count)    wchar_t** argvs,
    _In_count_(count)    const int* maxprocs,
    _In_count_(count)    int* cinfo_keyval_sizes,
    _In_count_(count)    PMI_keyval_t** info_keyval_vectors,
    _In_z_               char*     parentPortName,
    _Out_                int*      errors
    );

#endif
