// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2005 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <windows.h>
#include "mpi.h"
#include "mpistr.h"
#include "mpiutil.h"

/* MPIR_dll_name is defined in dbg_init.cpp; it must be part of the target image,
   not the debugger interface */

/* mpi_interface.h defines the interface to the debugger.  This interface
   is the same for any MPI implementation, for a given debugger
   (a more precise name might be mpi_tv_interface.h) */
#include "mpi_interface.h"

/* mpich2_dll_defs.h defines the structures for a particular MPI
   implementation (MPICH2 in this case) */
#include "mpich2_dll_defs.h"

/* ------------------------------------------------------------------------ */
/* Local variables for this package */

static const mqs_basic_callbacks *mqs_basic_entrypoints = 0;
static int host_is_big_endian = -1;

/* ------------------------------------------------------------------------ */
/* Error values. */
enum
{
    err_silent_failure = mqs_first_user_code,
    err_no_current_communicator,
    err_bad_request,
    err_no_store,
    err_all_communicators,
    err_group_corrupt,
    err_failed_qhdr,
    err_unexpected,
    err_posted,
    err_failed_queue,
    err_first,

};

/* Internal functions used only by routines in this package */
static void mqs_free_communicator_list( struct communicator_t* comm );
static int communicators_changed( mqs_process* proc );
static int rebuild_communicator_list( mqs_process* proc );


/* ------------------------------------------------------------------------ */
/*
 * Many of the services used by this file are performed by calling
 * functions executed by the debugger.  In other words, these are routines
 * that the debugger must export to this package.  To make it easy to
 * identify these functions as well as to make their use simple,
 * we use macros that start with dbgr_xxx (for debugger).  These
 * function pointers are set early in the initialization phase.
 *
 * Note: to avoid any changes to the mpi_interface.h file, the fields in
 * the structures that contain the function pointers have not been
 * renamed dbgr_xxx and continue to use their original mqs_ prefix.
 * Using the dbgr_ prefix for the debugger-provided callbacks was done to
 * make it more obvious whether the debugger or the MPI interface DLL is
 * responsible for providing the function.
 */
#define dbgr_malloc           (mqs_basic_entrypoints->mqs_malloc_fp)
#define dbgr_free             (mqs_basic_entrypoints->mqs_free_fp)
#define dbgr_prints           (mqs_basic_entrypoints->mqs_eprints_fp)
#define dbgr_put_image_info   (mqs_basic_entrypoints->mqs_put_image_info_fp)
#define dbgr_get_image_info   (mqs_basic_entrypoints->mqs_get_image_info_fp)
#define dbgr_put_process_info (mqs_basic_entrypoints->mqs_put_process_info_fp)
#define dbgr_get_process_info (mqs_basic_entrypoints->mqs_get_process_info_fp)


/* These macros *RELY* on the function already having set up the conventional
 * local variables i_info or p_info.
 */
#define dbgr_find_type        (i_info->image_callbacks->mqs_find_type_fp)
#define dbgr_field_offset     (i_info->image_callbacks->mqs_field_offset_fp)
#define dbgr_get_type_sizes   (i_info->image_callbacks->mqs_get_type_sizes_fp)
#define dbgr_find_function    (i_info->image_callbacks->mqs_find_function_fp)
#define dbgr_find_symbol      (i_info->image_callbacks->mqs_find_symbol_fp)

#define dbgr_get_image        (p_info->process_callbacks->mqs_get_image_fp)
#define dbgr_get_global_rank  (p_info->process_callbacks->mqs_get_global_rank_fp)
#define dbgr_fetch_data       (p_info->process_callbacks->mqs_fetch_data_fp)
#define dbgr_target_to_host   (p_info->process_callbacks->mqs_target_to_host_fp)

/* Routines to access data within the process */
static mqs_taddr_t
fetch_pointer(
    mqs_process* proc,
    mqs_taddr_t addr,
    mpich_process_info* p_info
    )
{
    int asize = p_info->sizes.pointer_size;
    mqs_taddr_t res = 0;

    dbgr_fetch_data( proc, addr, asize, &res );
    return res;
}


static int
fetch_int(
    mqs_process* proc,
    mqs_taddr_t addr,
    mpich_process_info* p_info
    )
{
    int isize = p_info->sizes.int_size;
    int res = 0;

    dbgr_fetch_data( proc, addr, isize, &res );
    return res;
}

/* ------------------------------------------------------------------------ */
/* Startup calls
   These three routines are the first ones invoked by the debugger; they
   are used to ensure that the debug interface library is a known version.
*/
int
mqs_version_compatibility( void )
{
    return MQS_INTERFACE_COMPATIBILITY;
}


char*
mqs_version_string( void )
{
    return "MS-MPI message queue support v1.0";
}


/* Allow the debugger to discover the size of an address type */
int
mqs_dll_taddr_width( void )
{
    return sizeof(mqs_taddr_t);
}

/* ------------------------------------------------------------------------ */
/* Initialization

   The function mqs_setup_basic_callbacks is used by the debugger to
   inform the routines in this file of the addresses of functions that
   it may call in the debugger.

   The function mqs_setup_image creates the image structure (local to this
   file) and tell the debugger about it

   The function mqs_image_has_queues initializes the image structure.
   Much of the information that is saved in the image structure is information
   about the relative offset to data within an MPICH2 data structure.
   These offsets allow the debugger to retrieve information about the
   MPICH2 structures.  The debugger routine dbgr_find_type is used to
   find information on an named type, and dbgr_field_offset is used
   to get the offset of a named field within a type.

   The function mqs_setup_process(process, callbacks) creates a private
   process information structure and stores a pointer to it in process
   (using dbgr_put_process_info).  The use of a routine to store this
   value rather than passing an address to the process structure is
   done to give the debugger control over any operation that might store
   into the debuggers memory (instead, we'll use put_xxx_info).

   The function mqs_process_has_queues ??
*/
void
mqs_setup_basic_callbacks(
    const mqs_basic_callbacks* cb
    )
{
    mqs_basic_entrypoints = cb;
}

/*
  Allocate and setup the basic image data structure.  Also
  save the callbacks provided by the debugger; these will be used
  to access information about the image.  This memory may be recovered
  with mqs_destroy_image_info.
*/
int
mqs_setup_image(
    mqs_image* image,
    const mqs_image_callbacks* icb
    )
{
    mpich_image_info* i_info =
        static_cast<mpich_image_info*>( dbgr_malloc( sizeof(mpich_image_info) ) );

    if( i_info == nullptr)
    {
        return err_no_store;
    }

    ZeroMemory( i_info, sizeof(mpich_image_info) );
    i_info->image_callbacks = icb;

    /* Tell the debugger to associate i_info with image */
    dbgr_put_image_info( image, reinterpret_cast<mqs_image_info*>( i_info ) );

    return mqs_ok;
}

/*
 * Setup information needed to access the queues.  If successful, return
 * mqs_ok.  If not, return an error code.  Also set the message pointer
 * with an explanatory message if there is a problem; otherwise, set it
 * to nullptr.
 *
 * This routine is where much of the information specific to an MPI
 * implementation is used.  In particular, the names of the structures
 * internal to an implementation and their fields are used here.
 *
 * FIXME: some of this information is specific to particular devices.
 * For example, the message queues are defined by the device.  How do
 * we export this information?  Should the queue code itself be responsible
 * for this( either by calling a routine in the image, using
 * dbgr_find_function( ?) or by having the queue implementation provide a
 * separate file that can be included here to get the necessary information.
 */
_Success_( return == mqs_ok )
int
mqs_image_has_queues(
    _In_ mqs_image* image,
    _Outptr_result_z_ const char** message
    )
{
    mpich_image_info* i_info =
        reinterpret_cast<mpich_image_info*>( dbgr_get_image_info( image ) );

    /* Default failure message ! */
    *message =
        "No message queue display is possible.\n"
        "This is probably a MS-MPI version or configuration problem.";

    /* Find the various global variables and structure definitions
       that describe the communicator and message queue structures for
       the MPICH2 implementation */

    /* First, the communicator information.  This is in two parts:
       MPIR_All_Communicators - a structure containing the head of the
       list of all active communicators.  The type is MPIR_Comm_list.
       The communicators themselves are of type MPID_Comm.
    */
    mqs_type* commlist_type = dbgr_find_type( image, "MPIR_Comm_list",
                                        mqs_lang_c );
    if( commlist_type != nullptr )
    {
        i_info->sequence_number_offs =
            dbgr_field_offset( commlist_type, "sequence_number" );
        i_info->comm_head_offs = dbgr_field_offset( commlist_type, "head" );
    }

    mqs_type* comm_type = dbgr_find_type( image, "MPID_Comm", mqs_lang_c );
    if( comm_type != nullptr )
    {
        i_info->comm_handle_offs = dbgr_field_offset( comm_type, "handle" );
        i_info->comm_name_offs = dbgr_field_offset( comm_type, "name" );
        i_info->comm_next_offs = dbgr_field_offset( comm_type, "comm_next" );
        i_info->comm_rsize_offs = dbgr_field_offset( comm_type, "remote_size" );
        i_info->comm_rank_offs = dbgr_field_offset( comm_type, "rank" );
        i_info->comm_context_id_offs = dbgr_field_offset( comm_type, "context_id" );
        i_info->comm_recvcontext_id_offs = dbgr_field_offset(
            comm_type,
            "recvcontext_id" );
    }

    /* Now the receive queues.  The receive queues contain MPID_Request
       objects, and the various fields are within types in that object.
       To simplify the eventual access, we compute all offsets relative to the
       request.  This means diving into the types that make of the
       request definition */
    mqs_type* req_type = dbgr_find_type( image, "MPID_Request", mqs_lang_c );
    if( req_type != nullptr )
    {
        int dev_offs;
        dev_offs = dbgr_field_offset( req_type, "dev" );
        i_info->req_handle_offs = dbgr_field_offset( req_type, "handle" );
        i_info->req_status_offs = dbgr_field_offset( req_type, "status" );
        i_info->req_cc_offs     = dbgr_field_offset( req_type, "cc" );

        if( dev_offs >= 0 )
        {
            mqs_type* dreq_type = dbgr_find_type(
                image,
                "MPIDI_Request_t",
                mqs_lang_c );
            i_info->req_dev_offs = dev_offs;

            if( dreq_type != nullptr )
            {
                int loff, match_offs;

                loff = dbgr_field_offset( dreq_type, "next" );
                i_info->req_next_offs = dev_offs + loff;
                loff = dbgr_field_offset( dreq_type, "user_buf" );
                i_info->req_user_buf_offs = dev_offs + loff;
                loff = dbgr_field_offset( dreq_type, "user_count" );
                i_info->req_user_count_offs = dev_offs + loff;
                loff = dbgr_field_offset( dreq_type, "datatype" );
                i_info->req_datatype_offs = dev_offs + loff;
                loff = dbgr_field_offset( dreq_type, "sender_req_id" );
                i_info->req_sender_req_offs = dev_offs + loff;

                match_offs = dbgr_field_offset( dreq_type, "match" );

                if( match_offs >= 0 )
                {
                    mqs_type* match_type = dbgr_find_type(
                        image,
                        "MPIDI_Message_match",
                        mqs_lang_c );

                    if( match_type != nullptr )
                    {
                        int moff;
                        moff = dbgr_field_offset( match_type, "tag" );
                        i_info->req_tag_offs = dev_offs + match_offs + moff;

                        moff = dbgr_field_offset( match_type, "rank" );
                        i_info->req_rank_offs = dev_offs + match_offs + moff;

                        moff = dbgr_field_offset( match_type, "context_id" );
                        i_info->req_context_id_offs = dev_offs + match_offs + moff;
                    }
                }
            }
        }
    }


    /* Send queues use a separate system */

    mqs_type* sreq_type = dbgr_find_type( image, "MPIR_Sendq", mqs_lang_c );
    if( sreq_type )
    {
        i_info->sendq_next_offs = dbgr_field_offset( sreq_type, "next" );
        i_info->sendq_tag_offs  = dbgr_field_offset( sreq_type, "tag" );
        i_info->sendq_rank_offs  = dbgr_field_offset( sreq_type, "rank" );
        i_info->sendq_context_id_offs  = dbgr_field_offset( sreq_type, "context_id" );
        i_info->sendq_req_offs = dbgr_field_offset( sreq_type, "sreq" );
    }

    return mqs_ok;
}


/* mqs_setup_process initializes the process structure.
 * The memory allocated by this routine( and routines that modify this
 * structure) is freed with mqs_destroy_process_info
 */
int
mqs_setup_process(
    mqs_process* process,
    const mqs_process_callbacks* pcb
    )
{
    /* Extract the addresses of the global variables we need and save
       them away */
    mpich_process_info* p_info =
        static_cast<mpich_process_info*>( dbgr_malloc( sizeof(mpich_process_info) ) );

    if( p_info == nullptr )
    {
        return err_no_store;
    }

    mqs_image*        image;
    mpich_image_info* i_info;

    p_info->process_callbacks = pcb;

    /* Now we can get the rest of the info ! */
    image  = dbgr_get_image( process );
    i_info = reinterpret_cast<mpich_image_info*>(
        dbgr_get_image_info( image ) );

    /* Library starts at zero, so this ensures we go look to start with */
    p_info->communicator_sequence = -1;

    /* We have no communicators yet */
    p_info->communicator_list     = nullptr;

    /* Ask the debugger to initialize the structure that contains
       the sizes of basic items( short, int, long, long long, and
       void *) */
    dbgr_get_type_sizes( process, &p_info->sizes );

    /* Tell the debugger to associate p_info with process */
    dbgr_put_process_info( process, reinterpret_cast<mqs_process_info*>( p_info ) );

    return mqs_ok;
}


_Success_( return == mqs_ok )
int
mqs_process_has_queues(
    _In_ mqs_process* proc,
    _Outptr_result_maybenull_ const char** msg
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );
    mqs_image* image           = dbgr_get_image( proc );
    mpich_image_info* i_info =
        reinterpret_cast<mpich_image_info*>( dbgr_get_image_info( image ) );

    /* Don't bother with a pop up here, it's unlikely to be helpful */
    *msg = 0;

    /* Check first for the communicator list */
    if( dbgr_find_symbol( image,
                          "MPIR_All_communicators",
                          &p_info->commlist_base ) != mqs_ok )
    {
        return err_all_communicators;
    }

    /* Check for the receive and send queues */
    if( dbgr_find_symbol( image,
                          "recvq_posted",
                          &p_info->posted_base ) != mqs_ok )
    {
        return err_posted;
    }

    if( dbgr_find_symbol( image,
                          "recvq_unexpected",
                          &p_info->unexpected_base ) != mqs_ok )
    {
        return err_unexpected;
    }

    /* Send queues are optional */
    if( dbgr_find_symbol( image,
                          "MPIR_Sendq_head",
                          &p_info->sendq_base) == mqs_ok )
    {
        p_info->has_sendq = 1;
    }
    else
    {
        p_info->has_sendq = 0;
    }

    return mqs_ok;
}


/* This routine is called by the debugger to map an error code into a
   printable string */
char*
mqs_dll_error_string(
    int errcode
    )
{
    switch( errcode )
    {
    case err_silent_failure:
        return "";
    case err_no_current_communicator:
        return "No current communicator in the communicator iterator";
    case err_bad_request:
        return "Attempting to setup to iterate over an unknown queue of operations";
    case err_no_store:
        return "Unable to allocate store";
    case err_group_corrupt:
        return "Could not read a communicator's group from the process( probably a store corruption)";
    case err_unexpected:
        return "Failed to find symbol recvq_unexpected";
    case err_posted:
        return "Failed to find symbol recvq_posted";
    }
    return "Unknown error code";
}


/* ------------------------------------------------------------------------ */
/* Queue Display
 *
 */

/* Communicator list.
 *
 * To avoid problems that might be caused by having the list of communicators
 * change in the process that is being debugged, the communicator access
 * routines make an internal copy of the communicator list.
 *
 */
/* Internal structure we hold for each communicator */
typedef struct communicator_t
{
    struct communicator_t*  next;
    group_t*                group;                /* Translations */
    GUID                    context_id;           /* To catch changes */
    GUID                    recvcontext_id;       /* May also be needed for
                                                     matching */
    int                     present;
    mqs_communicator        comm_info;            /* Info needed at the higher level */
    int                     handle;
} communicator_t;


/* update_communicator_list makes a copy of the list of currently active
 * communicators and stores it in the mqs_process structure.
 */
int
mqs_update_communicator_list(
    mqs_process* proc
    )
{
    if( communicators_changed( proc ) )
    {
        return rebuild_communicator_list( proc );
    }

    return mqs_ok;
}


/* These three routines( setup_communicator_iterator, get_communicator,
 * and next_communicator) provide a way to access each communicator in the
 * list that is initialized by update_communicator_list.
 */
int
mqs_setup_communicator_iterator(
    mqs_process* proc
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );

    /* Start at the front of the list again */
    p_info->current_communicator = p_info->communicator_list;

    /* Reset the operation iterator too */
    p_info->next_msg = 0;

    return p_info->current_communicator == nullptr ? mqs_end_of_list : mqs_ok;
}


int
mqs_get_communicator(
    mqs_process* proc,
    mqs_communicator* comm
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );

    if( p_info->current_communicator != nullptr )
    {
        *comm = p_info->current_communicator->comm_info;
        return mqs_ok;
    }

    return err_no_current_communicator;
}


int
mqs_next_communicator(
    mqs_process* proc
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );

    p_info->current_communicator = p_info->current_communicator->next;

    return( p_info->current_communicator != nullptr ) ? mqs_ok : mqs_end_of_list;
}


/* Get the next entry in the current receive queue( posted or unexpected ) */
static int
fetch_receive(
    mqs_process* proc,
    mpich_process_info* p_info,
    mqs_pending_operation* op,
    int is_posted
    )
{
    mqs_image* image           = dbgr_get_image( proc );
    mpich_image_info* i_info =
        reinterpret_cast<mpich_image_info*>( dbgr_get_image_info( image ) );
    communicator_t* comm       = p_info->current_communicator;
    GUID comm_context          = comm->recvcontext_id;
    mqs_taddr_t base           = fetch_pointer( proc, p_info->next_msg, p_info );

    while( base != 0 )
    {
        //
        // TODO: We should consider exposing MPID_Request to the mqd
        // module so that we can do a read for MPID_Request instead of
        // reading all its fields separately
        //
        GUID msg_context;
        dbgr_fetch_data( proc,
                         base + i_info->req_context_id_offs,
                         sizeof(msg_context),
                         &msg_context );

        if( msg_context == comm_context )
        {
            int req_handle = fetch_int( proc, base + i_info->req_handle_offs, p_info );
            int tag = fetch_int( proc, base + i_info->req_tag_offs, p_info );
            int rank = fetch_int( proc, base + i_info->req_rank_offs, p_info );
            int is_complete = fetch_int( proc, base + i_info->req_cc_offs, p_info );

            int user_count = fetch_int(
                proc,
                base + i_info->req_user_count_offs,
                p_info );

            op->desired_tag = tag;
            op->desired_local_rank = rank;

            //
            // TODO: convert comm's rank into world rank
            //
            op->desired_global_rank = -1;
            op->desired_length = user_count;
            op->tag_wild = (op->desired_tag == MPI_ANY_TAG);

            //
            // Max size of a guid string
            //
            char guid_str[37];
            GuidToStr( comm_context, guid_str, _countof(guid_str) );

            char commStr[_countof(op->extra_text[0])] = {0};
            char commCtxStr[_countof(op->extra_text[1])] = {0};
            char typeStr[_countof(op->extra_text[0])] = {0};
            if( is_posted )
            {
                op->buffer = fetch_pointer(
                    proc,
                    base + i_info->req_user_buf_offs,
                    p_info );
                int datatype = fetch_int(
                    proc,
                    base + i_info->req_datatype_offs,
                    p_info );

                MPIU_Snprintf( commStr,
                               _countof(commStr),
                               "Comm: 0x%08x",
                               comm->handle );
                MPIU_Snprintf( commCtxStr,
                                  _countof(commCtxStr),
                                  "Ctx: %s",
                                  guid_str );
                MPIU_Snprintf( typeStr,
                                  _countof(typeStr),
                                  "Type: 0x%08x",
                                  datatype );
            }
            else
            {
                int sender_req = fetch_int(
                    proc,
                    base + i_info->req_sender_req_offs,
                    p_info );
                MPIU_Snprintf( commStr,
                               _countof(commStr),
                               "Ctx: %s",
                               guid_str );
                MPIU_Snprintf( typeStr,
                               _countof(typeStr),
                               "Sender Req Id: 0x%08x",
                               sender_req );
            }


            /* We don't know the rest of these */
            op->system_buffer = 0;
            op->actual_local_rank = rank;
            op->actual_global_rank = -1;
            op->actual_tag = tag;
            op->actual_length = -1;

            unsigned i = 0;
            MPIU_Snprintf(
                op->extra_text[i],
                _countof(op->extra_text[i]),
                "Handle: 0x%08x %s",
                req_handle,
                commStr );
            i++;

            if(commCtxStr[0] != '\0')
            {
                MPIU_Snprintf(
                    op->extra_text[i],
                    _countof(op->extra_text[i]),
                    "%s",
                    commCtxStr );
                i++;
            }

            MPIU_Snprintf(
                op->extra_text[i],
                _countof(op->extra_text[i]),
                "%s",
                typeStr );
            i++;
            if( i < 5 )
            {
                op->extra_text[i][0] = 0;
            }
            op->status =( is_complete != 0 ) ? mqs_st_pending : mqs_st_complete;

            //
            // Move to the next message in the queue
            //
            p_info->next_msg = base + i_info->req_next_offs;
            return mqs_ok;
        }
        else
        {
            base = fetch_pointer( proc, base + i_info->req_next_offs, p_info );
        }
    }

    p_info->next_msg = 0;
    return mqs_end_of_list;
}


/* Get the next entry in the send queue, if there is one.  The assumption is
   that the MPI implementation is quiescent while these queue probes are
   taking place, so we can simply keep track of the location of the "next"
   entry.( in the next_msg field) */
static int
fetch_send(
    mqs_process* proc,
    mpich_process_info* p_info,
    mqs_pending_operation *op
    )
{
    mqs_image*  image        = dbgr_get_image( proc );
    mpich_image_info* i_info =
        reinterpret_cast<mpich_image_info*>( dbgr_get_image_info( image ) );
    communicator_t* comm     = p_info->current_communicator;
    GUID wanted_context       = comm->context_id;
    mqs_taddr_t base         = fetch_pointer( proc, p_info->next_msg, p_info );

    if( !p_info->has_sendq )
    {
        return mqs_no_information;
    }

    /* Say what operation it is. We can only see non blocking send operations
     * in MPICH. Other MPI systems may be able to show more here.
     */

    MPIU_Strncpy( op->extra_text[0],
                  "Non-blocking send",
                  _countof(op->extra_text[0]) );
    op->extra_text[1][0] = '\0';

    GUID actual_context;
    while( base != 0 )
    {
        /* Check this entry to see if the context matches */
        dbgr_fetch_data(
            proc,
            base + i_info->sendq_context_id_offs,
            sizeof(actual_context),
            &actual_context);

        if( actual_context == wanted_context)
        {
            int target = fetch_int(
                proc,
                base + i_info->sendq_rank_offs,
                p_info );

            int tag    = fetch_int(
                proc,
                base + i_info->sendq_tag_offs,
                p_info );

            mqs_taddr_t sreq = fetch_pointer(
                proc,
                base + i_info->sendq_req_offs,
                p_info );

            int is_complete = fetch_int(
                proc,
                sreq + i_info->req_cc_offs,
                p_info );

            mqs_taddr_t data = fetch_pointer(
                proc,
                sreq + i_info->req_user_buf_offs,
                p_info );

            int length = fetch_int(
                proc,
                sreq + i_info->req_user_count_offs,
                p_info );

            op->status = (is_complete != 0) ? mqs_st_pending : mqs_st_complete;
            op->actual_local_rank = op->desired_local_rank = target;
            op->actual_global_rank = op->desired_global_rank = -1;
            op->tag_wild       = 0;
            op->actual_tag     = op->desired_tag = tag;
            op->desired_length = op->actual_length = length;
            op->system_buffer  = 0;
            op->buffer         = data;

            //
            // Move to the next messagein the queue
            //
            p_info->next_msg = base + i_info->sendq_next_offs;
            return mqs_ok;
        }
        else
        {
            base = fetch_pointer( proc, base + i_info->sendq_next_offs, p_info );
        }
    }
    return mqs_end_of_list;
}


int
mqs_setup_operation_iterator(
    mqs_process* proc,
    int op
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );

    p_info->what = static_cast<mqs_op_class>( op );

    switch( op )
    {
    case mqs_pending_sends:
        if( !p_info->has_sendq )
        {
            return mqs_no_information;
        }
        else
        {
            p_info->next_msg = p_info->sendq_base;
            return mqs_ok;
        }

    case mqs_pending_receives:
        p_info->next_msg = p_info->posted_base;
        return mqs_ok;

    case mqs_unexpected_messages:
        p_info->next_msg = p_info->unexpected_base;
        return mqs_ok;

    default:
        return err_bad_request;
    }
}


/* Fetch the next operation on the current communicator, from the
   selected queue. Since MPICH2 does not( normally) use separate queues
   for each communicator, we must compare the queue items with the
   current communicator.
*/
int
mqs_next_operation(
    mqs_process* proc,
    mqs_pending_operation* op
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );

    switch( p_info->what )
    {
    case mqs_pending_receives:
        return fetch_receive( proc,p_info,op, true );
    case mqs_unexpected_messages:
        return fetch_receive( proc,p_info,op, false );
    case mqs_pending_sends:
        return fetch_send( proc,p_info,op );
    default:
        return err_bad_request;
    }
}


/* ------------------------------------------------------------------------ */
/* Clean up routines
 * These routines free any memory allocated when the process or image
 * structures were allocated.
 */
void
mqs_destroy_process_info(
    mqs_process_info* mp_info
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( mp_info );

    /* Need to handle the communicators and groups too */
    mqs_free_communicator_list( p_info->communicator_list );

    dbgr_free( p_info );
}


void
mqs_destroy_image_info(
    mqs_image_info* info
    )
{
    dbgr_free( info );
}


/* ------------------------------------------------------------------------ */
/* Communicator */
static group_t*
find_or_create_group(
    mqs_process* /*proc*/,
    mqs_tword_t /*np*/,
    mqs_taddr_t /*table*/
    )
{
    return nullptr;
}


static void
group_decref(
    group_t* group
    )
{
    if( --(group->ref_count) == 0 )
    {
        dbgr_free( group->local_to_global );
        dbgr_free( group );
    }
}


static int
communicators_changed(
    mqs_process* proc
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );
    mqs_image * image          = dbgr_get_image( proc );
    mpich_image_info* i_info =
        reinterpret_cast<mpich_image_info*>( dbgr_get_image_info( image ) );
    int new_seq = fetch_int( proc,
                             p_info->commlist_base + i_info->sequence_number_offs,
                             p_info );
    int  res =( new_seq != p_info->communicator_sequence );

    /* Save the sequence number for next time */
    p_info->communicator_sequence = new_seq;

    return res;
}


/***********************************************************************
 * Find a matching communicator on our list. We check the recv context
 * as well as the address since the communicator structures may be
 * being re-allocated from a free list, in which case the same
 * address will be re-used a lot, which could confuse us.
 */
static communicator_t*
find_communicator(
    mpich_process_info* p_info,
    mqs_taddr_t comm_base,
    GUID& recv_ctx
    )
{
    communicator_t* comm = p_info->communicator_list;

    for( ;comm != nullptr; comm = comm->next )
    {
        if( comm->comm_info.unique_id == comm_base &&
            comm->recvcontext_id == recv_ctx)
        {
            return comm;
        }
    }

    return nullptr;
}


static int
rebuild_communicator_list(
    mqs_process* proc
    )
{
    mpich_process_info* p_info =
        reinterpret_cast<mpich_process_info*>( dbgr_get_process_info( proc ) );
    mqs_image * image          = dbgr_get_image( proc );
    mpich_image_info* i_info =
        reinterpret_cast<mpich_image_info*>( dbgr_get_image_info( image ) );
    mqs_taddr_t comm_base = fetch_pointer(
        proc,
        p_info->commlist_base + i_info->comm_head_offs,
        p_info );

    /* Iterate over the list in the process comparing with the list
     * we already have saved. This is n**2, because we search for each
     * communicator on the existing list. I don't think it matters, though
     * because there aren't that many communicators to worry about, and
     * we only ever do this if something changed.
     */
    while( comm_base )
    {
        GUID recv_ctx;
        GUID send_ctx;

        dbgr_fetch_data( proc,
                         comm_base + i_info->comm_recvcontext_id_offs,
                         sizeof(recv_ctx),
                         &recv_ctx );

        dbgr_fetch_data( proc,
                         comm_base + i_info->comm_context_id_offs,
                         sizeof(send_ctx),
                         &send_ctx );

        communicator_t* old = find_communicator( p_info, comm_base, recv_ctx );

        //
        // We ignore reading the name of the communicator because
        // the MQD interface does not have any efficient way of
        // reading a string from the target memory. Once the handle
        // introspection interface is standardized we can use it
        // to handle the communicator nams
        //
        if( old != nullptr )
        {
            old->present = 1;           /* We do want this communicator */
        }
        else
        {
            mqs_taddr_t group_base = fetch_pointer(
                proc,
                comm_base + i_info->lrank_to_grank_offs,
                p_info );
            int np     = fetch_int( proc, comm_base + i_info->comm_rsize_offs,p_info );
            group_t *g = find_or_create_group( proc, np, group_base );
            communicator_t* nc;

            nc = static_cast<communicator_t*>(
                dbgr_malloc( sizeof(communicator_t) ) );

            /* Save the results */
            nc->next = p_info->communicator_list;
            p_info->communicator_list = nc;
            nc->present = 1;
            nc->group   = g;
            nc->context_id = send_ctx;
            nc->recvcontext_id = recv_ctx;

            nc->comm_info.unique_id = comm_base;
            nc->comm_info.size      = np;
            nc->comm_info.local_rank = fetch_int(
                proc,
                comm_base + i_info->comm_rank_offs,
                p_info );
            nc->handle = fetch_int(
                proc,
                comm_base + i_info->comm_handle_offs,
                p_info );

            const char* name;

            if( nc->handle == MPI_COMM_WORLD )
            {
                name = "MPI_COMM_WORLD";
            }
            else if( nc->handle == MPI_COMM_SELF )
            {
                name = "MPI_COMM_SELF";
            }
            else
            {
                name = "Name Unknown";
            }

            MPIU_Strncpy( nc->comm_info.name, name, _countof(nc->comm_info.name) );
        }

        /* Step to the next communicator on the list */
        comm_base = fetch_pointer( proc, comm_base + i_info->comm_next_offs, p_info );
    }

    unsigned int commcount = 0;

    /* Now iterate over the list tidying up any communicators which
     * no longer exist, and cleaning the flags on any which do.
     */
    for( communicator_t** commp = &p_info->communicator_list;
         *commp != nullptr;
         commp = &(*commp)->next )
    {
        communicator_t* comm = *commp;

        if( comm->present )
        {
            comm->present = 0;
            commcount++;
        }
        else
        {
            /* It needs to be deleted */
            *commp = comm->next;                        /* Remove from the list */
            group_decref( comm->group );         /* Group is no longer referenced from here */
            dbgr_free( comm );
        }
    }

    return mqs_ok;
}


/* Internal routine to free the communicator list */
static void
mqs_free_communicator_list(
    communicator_t* comm
    )
{
    while( comm != nullptr )
    {
        communicator_t* next = comm->next;

        /* Release the group data structures */
        dbgr_free( comm );

        comm = next;
    }
}
