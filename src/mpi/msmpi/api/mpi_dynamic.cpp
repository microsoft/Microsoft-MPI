// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "namepub.h"


/*@
   MPI_Open_port - Establish an address that can be used to establish
   connections between groups of MPI processes

 Input Parameter:
. info - implementation-specific information on how to establish a
   port for 'MPI_Comm_accept' (handle)

 Output Parameter:
. port_name - newly established port (string)

Notes:
MPI copies a system-supplied port name into 'port_name'. 'port_name' identifies
the newly opened port and can be used by a client to contact the server.
The maximum size string that may be supplied by the system is
'MPI_MAX_PORT_NAME'.

 Reserved Info Key Values:
+ ip_port - Value contains IP port number at which to establish a port.
- ip_address - Value contains IP address at which to establish a port.
 If the address is not a valid IP address of the host on which the
 'MPI_OPEN_PORT' call is made, the results are undefined.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Open_port(
    _In_ MPI_Info info,
    _Out_writes_z_(MPI_MAX_PORT_NAME) char* port_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Open_port(info);

    MPID_Info *info_ptr;
    int mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( port_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "port_name" );
        goto fn_fail;
    }

    mpi_errno = MPID_Open_port(info_ptr, port_name);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Open_port(port_name);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_open_port %I %p",
            info,
            port_name
            )
        );
    TraceError(MPI_Open_port, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Close_port - close port

   Input Parameter:
.  port_name - a port name (string)

.N NotThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Close_port(
    _In_z_ const char* port_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Close_port(port_name);

    int mpi_errno;
    if( port_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "port_name" );
        goto fn_fail;
    }

    mpi_errno = MPID_Close_port(port_name);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Close_port();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_close_port %s",
            port_name
            )
        );
    TraceError(MPI_Close_port, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Publish_name - Publish a service name for use with MPI_Comm_connect

 Input Parameters:
+ service_name - a service name to associate with the port (string)
. info - implementation-specific information (handle)
- port_name - a port name (string)

Notes:
The maximum size string that may be supplied for 'port_name' is
'MPI_MAX_PORT_NAME'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Publish_name(
    _In_z_ const char* service_name,
    _In_ MPI_Info info,
    _In_z_ const char* port_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Publish_name(service_name, info, port_name);

    MPID_Info *info_ptr;
    int mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( service_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "service_name" );
        goto fn_fail;
    }

    if( port_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "port_name" );
        goto fn_fail;
    }

#   ifdef HAVE_NAMEPUB_SERVICE
    {
        if (!MPIR_Namepub)
        {
            mpi_errno = MPID_NS_Create( info_ptr, &MPIR_Namepub );
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            if(MPIR_Namepub != NULL)
            {
                MPIR_Add_finalize( (int (*)(void*))MPID_NS_Free, &MPIR_Namepub, 9 );
            }
        }

        mpi_errno = MPID_NS_Publish( MPIR_Namepub, info_ptr,
                                     (const char *)service_name,
                                     (const char *)port_name );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

    }
#   else
    {
        /* No name publishing service available */
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**nonamepub");
        goto fn_fail;
    }
#   endif

    TraceLeave_MPI_Publish_name();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_publish_name %s %I %s",
            service_name,
            info,
            port_name
            )
        );
    TraceError(MPI_Publish_name, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Unpublish_name - Unpublish a service name published with
   MPI_Publish_name

 Input Parameters:
+ service_name - a service name (string)
. info - implementation-specific information (handle)
- port_name - a port name (string)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INFO
.N MPI_ERR_ARG
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Unpublish_name(
    _In_z_ const char* service_name,
    _In_ MPI_Info info,
    _In_z_ const char* port_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Unpublish_name(service_name, info, port_name);

    MPID_Info *info_ptr;
    int mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( service_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "service_name" );
        goto fn_fail;
    }

    if( port_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "port_name" );
        goto fn_fail;
    }

#   ifdef HAVE_NAMEPUB_SERVICE
    {
        /* The standard leaves explicitly undefined what happens if the code
           attempts to unpublish a name that is not published.  In this case,
           MPI_Unpublish_name could be called before a name service structure
           is allocated. */
        if (!MPIR_Namepub)
        {
            mpi_errno = MPID_NS_Create( info_ptr, &MPIR_Namepub );
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            if(MPIR_Namepub != NULL)
            {
                MPIR_Add_finalize( (int (*)(void*))MPID_NS_Free, &MPIR_Namepub, 9 );
            }
        }

        mpi_errno = MPID_NS_Unpublish( MPIR_Namepub, info_ptr,
                                       (const char *)service_name );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

    }
#   else
    {
        /* No name publishing service available */
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**nonamepub");
        goto fn_fail;
    }
#   endif

    TraceLeave_MPI_Unpublish_name();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_unpublish_name %s %I %s",
            service_name,
            info,
            port_name
            )
        );
    TraceError(MPI_Unpublish_name, mpi_errno);
    goto fn_exit;
}


/* One of these routines needs to define the global handle.  Since
   Most routines will use lookup (if they use any of the name publishing
   interface at all), we place this in MPI_Lookup_name.
*/
MPID_NS_t *MPIR_Namepub = 0;



/*@
   MPI_Lookup_name - Lookup a port given a service name

   Input Parameters:
+ service_name - a service name (string)
- info - implementation-specific information (handle)


   Output Parameter:
.  port_name - a port name (string)

Notes:
If the 'service_name' is found, MPI copies the associated value into
'port_name'.  The maximum size string that may be supplied by the system is
'MPI_MAX_PORT_NAME'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Lookup_name(
    _In_z_ const char* service_name,
    _In_ MPI_Info info,
    _Out_writes_z_(MPI_MAX_PORT_NAME) char* port_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Lookup_name(service_name, info);

    MPID_Info *info_ptr;
    int mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( service_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "service_name" );
        goto fn_fail;
    }
    if( port_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "port_name" );
        goto fn_fail;
    }

#   ifdef HAVE_NAMEPUB_SERVICE
    {
        if (!MPIR_Namepub)
        {
            mpi_errno = MPID_NS_Create( info_ptr, &MPIR_Namepub );
            ON_ERROR_FAIL(mpi_errno);

            if(MPIR_Namepub != NULL)
            {
                MPIR_Add_finalize( (int (*)(void*))MPID_NS_Free, &MPIR_Namepub, 9 );
            }
        }

        mpi_errno = MPID_NS_Lookup( MPIR_Namepub, info_ptr,
                                    (const char *)service_name, port_name );
        if(ERROR_GET_CLASS(mpi_errno) != MPI_ERR_NAME)
        {
            ON_ERROR_FAIL(mpi_errno);
        }
    }
#   else
    {
        /* No name publishing service available */
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**nonamepub");
        goto fn_fail;
    }
#   endif

    TraceLeave_MPI_Lookup_name(port_name);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_lookup_name %s %I %p",
            service_name,
            info,
            port_name
            )
        );
    TraceError(MPI_Lookup_name, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_accept - Accept a request to form a new intercommunicator

 Input Parameters:
+ port_name - port name (string, used only on root)
. info - implementation-dependent information (handle, used only on root)
. root - rank in comm of root node (integer)
- IN - comm intracommunicator over which call is collective (handle)

 Output Parameter:
. newcomm - intercommunicator with client as remote group (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INFO
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_accept(
    _In_z_ const char* port_name,
    _In_ MPI_Info info,
    _In_range_(>=, 0) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Comm* newcomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_accept(port_name, info, root, comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newcomm" );
        goto fn_fail;
    }

    MPID_Comm *newcomm_ptr;
    mpi_errno = MPID_Comm_accept(port_name, info_ptr, root, comm_ptr,
                                 &newcomm_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newcomm = newcomm_ptr->handle;

    TraceLeave_MPI_Comm_accept(*newcomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_accept %s %I %d %C %p",
            port_name,
            info,
            root,
            comm,
            newcomm
            )
        );
    TraceError(MPI_Comm_accept, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_connect - Make a request to form a new intercommunicator

 Input Parameters:
+ port_name - network address (string, used only on root)
. info - implementation-dependent information (handle, used only on root)
. root - rank in comm of root node (integer)
- comm - intracommunicator over which call is collective (handle)

 Output Parameter:
. newcomm - intercommunicator with server as remote group (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_INFO
.N MPI_ERR_PORT
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_connect(
    _In_z_ const char* port_name,
    _In_ MPI_Info info,
    _In_range_(>=, 0) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Comm* newcomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_connect(port_name, info, root, comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Info *info_ptr;
    mpi_errno = MpiaInfoValidateHandleOrNull( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newcomm" );
        goto fn_fail;
    }

    MPID_Comm *newcomm_ptr;
    mpi_errno = MPID_Comm_connect(port_name, info_ptr, root, comm_ptr,
                                  &newcomm_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *newcomm = newcomm_ptr->handle;

    /* ... end of body of routine ... */
    TraceLeave_MPI_Comm_connect(*newcomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_connect %s %I %d %C %p",
            port_name,
            info,
            root,
            comm,
            newcomm
            )
        );
    TraceLeave_MPI_Comm_connect(mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_disconnect - Disconnect from a communicator

   Input Parameter:
.  comm - communicator (handle)

Notes:
This routine waits for all pending communication to complete, then frees the
communicator and sets 'comm' to 'MPI_COMM_NULL'.  It may not be called
with 'MPI_COMM_WORLD' or 'MPI_COMM_SELF'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS

.seealso MPI_Comm_connect, MPI_Comm_join
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_disconnect(
    _In_ MPI_Comm* comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_disconnect(*comm);

    MPID_Comm *comm_ptr;
    int mpi_errno;
    if( comm == nullptr )
    {
        comm_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateHandle( *comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Comm_disconnect(comm_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *comm = MPI_COMM_NULL;

    TraceLeave_MPI_Comm_disconnect();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_disconnect %C",
            *comm
            )
        );
    TraceError(MPI_Comm_disconnect, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_join - Create a communicator by joining two processes connected by
     a socket.

   Input Parameter:
. fd - socket file descriptor

   Output Parameter:
. intercomm - new intercommunicator (handle)

 Notes:
  The socket must be quiescent before 'MPI_COMM_JOIN' is called and after
  'MPI_COMM_JOIN' returns. More specifically, on entry to 'MPI_COMM_JOIN', a
  read on the socket will not read any data that was written to the socket
  before the remote process called 'MPI_COMM_JOIN'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_join(
    _In_ int fd,
    _Out_ MPI_Comm* intercomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_join(fd);

    int mpi_errno;
    if( intercomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "intercomm" );
        goto fn_fail;
    }

    mpi_errno = MPID_Comm_join( fd, intercomm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Comm_join(*intercomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_join %d %p",
            fd,
            intercomm
            )
        );
    TraceError(MPI_Comm_join,
        mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_spawn - Spawn up to maxprocs instances of a single MPI application

   Input Parameters:
+ command - name of program to be spawned (string, significant only at root)
. argv - arguments to command (array of strings, significant only at root)
. maxprocs - maximum number of processes to start (integer, significant only
  at root)
. info - a set of key-value pairs telling the runtime system where and how
   to start the processes (handle, significant only at root)
. root - rank of process in which previous arguments are examined (integer)
- comm - intracommunicator containing group of spawning processes (handle)

   Output Parameters:
+ intercomm - intercommunicator between original group and the
   newly spawned group (handle)
- array_of_errcodes - one code per process (array of integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
.N MPI_ERR_INFO
.N MPI_ERR_SPAWN
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_spawn(
    _In_z_ const char* command,
    _In_ char* argv[],
    _In_range_(>=, 0) int maxprocs,
    _In_ MPI_Info info,
    _In_range_(>=, 0) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Comm* intercomm,
    _Out_writes_(maxprocs) int array_of_errcodes[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_spawn(command, argv, maxprocs, info, root, comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        comm_ptr = nullptr;
        goto fn_fail;
    }
    if (comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**commnotintra");
        goto fn_fail;
    }

    MPID_Info *info_ptr = nullptr;

    if (comm_ptr->rank == root)
    {
        mpi_errno = MpiaInfoValidateHandleOrNull(info, &info_ptr);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }

        if (maxprocs < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**argneg %s %d", "maxprocs", maxprocs);
            goto fn_fail;
        }
        else if (maxprocs == 0)
        {
            (*intercomm) = MPI_COMM_NULL;
            mpi_errno = MPI_SUCCESS;
            goto fn_exit;
        }
    }

    mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( intercomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "intercomm" );
        goto fn_fail;
    }

    /* TODO: add error check to see if this collective function is
       being called from multiple threads. */
    MPID_Comm *intercomm_ptr;
    mpi_errno = MPID_Comm_spawn_multiple(
        1, 
        const_cast<char**>(&command),
        const_cast<char***>(&argv),
        const_cast<int*>(&maxprocs),
        &info_ptr,
        root,
        const_cast<MPID_Comm *>(comm_ptr),
        &intercomm_ptr,
        array_of_errcodes
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *intercomm = intercomm_ptr->handle;

    TraceLeave_MPI_Comm_spawn(*intercomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_spawn %s %p %d %I %d %C %p %p",
            command,
            argv,
            maxprocs,
            info,
            root,
            comm,
            intercomm,
            array_of_errcodes
            )
        );
    TraceError(MPI_Comm_spawn, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_spawn_multiple - short description

   Input Parameters:
+ count - number of commands (positive integer, significant to MPI only at
  root
. array_of_commands - programs to be executed (array of strings, significant
  only at root)
. array_of_argv - arguments for commands (array of array of strings,
  significant only at root)
. array_of_maxprocs - maximum number of processes to start for each command
 (array of integer, significant only at root)
. array_of_info - info objects telling the runtime system where and how to
  start processes (array of handles, significant only at root)
. root - rank of process in which previous arguments are examined (integer)
- comm - intracommunicator containing group of spawning processes (handle)

  Output Parameters:
+ intercomm - intercommunicator between original group and newly spawned group
  (handle)
- array_of_errcodes - one error code per process (array of integer)

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
.N MPI_ERR_INFO
.N MPI_ERR_SPAWN
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_spawn_multiple(
    _In_range_(>, 0) int count,
    _In_reads_z_(count) char* array_of_commands[],
    _In_reads_z_(count) char** array_of_argv[],
    _In_reads_(count) const int array_of_maxprocs[],
    _In_reads_(count) const MPI_Info array_of_info[],
    _In_range_(>=, 0) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Comm* intercomm,
    _Out_ int array_of_errcodes[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_spawn_multiple(count, TraceArrayLength(count), (const void**)array_of_commands, SENTINEL_SAFE_COUNT(array_of_argv,TraceArrayLength(count)), (const void**)array_of_argv, TraceArrayLength(count), array_of_maxprocs, TraceArrayLength(count), (const unsigned int*)array_of_info, root, comm);

    StackGuardArray<MPID_Info*> array_of_info_ptrs;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    if (comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**commnotintra");
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    if( intercomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "intercomm" );
        goto fn_fail;
    }

    if (comm_ptr->rank == root)
    {
        if (count <= 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**argnonpos %s %d", "count", count);;
            goto fn_fail;
        }

        array_of_info_ptrs = new MPID_Info*[count];
        if( array_of_info_ptrs == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        for (int i = 0; i < count; i++)
        {
            mpi_errno = MpiaInfoValidateHandleOrNull(
                array_of_info[i],
                &array_of_info_ptrs[i]
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }

        bool nonZero = false;
        for (int i = 0; i < count; i++)
        {
            if (array_of_maxprocs[i] < 0)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**argneg %s %d", "maxprocs", array_of_maxprocs[i]);
                goto fn_fail;
            }
            else if (array_of_maxprocs[i] > 0)
            {
                nonZero = true;
            }
        }
        if (nonZero == false)
        {
            (*intercomm) = MPI_COMM_NULL;
            mpi_errno = MPI_SUCCESS;
            goto fn_exit;
        }
    }

    /* TODO: add error check to see if this collective function is
       being called from multiple threads. */
    MPID_Comm *intercomm_ptr;
    mpi_errno = MPID_Comm_spawn_multiple(
        count,
        array_of_commands,
        array_of_argv,
        const_cast<int*>(array_of_maxprocs),
        array_of_info_ptrs,
        root,
        comm_ptr,
        &intercomm_ptr,
        array_of_errcodes
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *intercomm = intercomm_ptr->handle;

    TraceLeave_MPI_Comm_spawn_multiple(*intercomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_spawn_multiple %d %p %p %p %p %d %C %p %p",
            count,
            array_of_commands,
            array_of_argv,
            array_of_maxprocs,
            array_of_info,
            root,
            comm,
            intercomm,
            array_of_errcodes
            )
        );
    TraceError(MPI_Comm_spawn_multiple, mpi_errno);
    goto fn_exit;
}
