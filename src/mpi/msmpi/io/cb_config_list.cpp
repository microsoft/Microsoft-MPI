// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 2001 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */

/* cb_config_list.c
 *
 * The important, externally used functions from this file are:
 * ADIOI_cb_bcast_rank_map()
 * ADIOI_cb_gather_name_array()
 * ADIOI_cb_config_list_parse()
 * ADIOI_cb_copy_name_array()
 * ADIOI_cb_delete_name_array()
 *
 * Prototypes for these are in adio/include/adio_cb_config_list.h
 */

#include "precomp.h"
#include "adio.h"
#include "adio_cb_config_list.h"

/* token types */
#define AGG_WILDCARD 1
#define AGG_STRING   2
#define AGG_COMMA    3
#define AGG_COLON    4
#define AGG_ERROR   -1
#define AGG_EOS      0

/* a couple of globals keep things simple */
static int cb_config_list_keyval = MPI_KEYVAL_INVALID;
static char *yylval;
static const char *token_ptr;

/* internal stuff */
static int get_max_procs(int cb_nodes);


_Ret_range_(>=,0)
static int
match_procs(
    _In_opt_z_ PCSTR name,
    _In_range_(>=,0) int max_per_proc,
    _In_reads_(nr_procnames) PCSTR const procnames[],
    _Out_writes_(nr_procnames) bool used_procnames[],
    _In_ int nr_procnames,
    _Out_writes_(nr_ranks) int ranks[],
    _In_range_(>=,0) int nr_ranks,
    _Inout_ int *nr_ranks_allocated
    );


_Ret_range_(>=,0)
static int
match_this_proc(
    _In_ PCSTR name,
    _In_ int cur_proc,
    _In_range_(>=,0) int max_matches,
    _In_reads_(nr_procnames) PCSTR const procnames[],
    _Out_writes_(nr_procnames) bool used_procnames[],
    _In_range_(>=,0) int nr_procnames,
    _In_reads_(nr_ranks) int ranks[],
    _In_range_(>=,0) int nr_ranks,
    _In_range_(>=,0) int nr_ranks_allocated
    );


_Ret_range_(-1, nr_procnames-1)
static int
find_name(
    _In_ PCSTR name,
    _In_reads_(nr_procnames) PCSTR const procnames[],
    _In_reads_(nr_procnames) const bool used_procnames[],
    _In_range_(>=,0) int nr_procnames,
    _In_range_(>=, nr_procnames) int start_ind
    );


static int cb_config_list_lex(void);


/* ADIOI_cb_bcast_rank_map() - broadcast the rank array
 *
 * Parameters:
 * fd - MPI_File for which update is occurring.  cb_nodes and ranklist
 * parameters must be up-to-date on rank 0 of the fd->comm.
 *
 * should probably be a void fn.
 */
int ADIOI_cb_bcast_rank_map(MPI_File fd)
{
    int my_rank;
    char *value;

    NMPI_Bcast(&(fd->hints->cb_nodes), 1, MPI_INT, 0, fd->comm);
    if (fd->hints->cb_nodes > 0)
    {
        NMPI_Comm_rank(fd->comm, &my_rank);
        if (my_rank != 0)
        {
            fd->hints->ranklist = (int*)ADIOI_Malloc(fd->hints->cb_nodes*sizeof(int));
            if (fd->hints->ranklist == NULL)
            {
                /* NEED TO HANDLE ENOMEM */
            }
            MPIU_Assert(fd->hints->ranklist);
        }
        NMPI_Bcast(fd->hints->ranklist, fd->hints->cb_nodes, MPI_INT, 0,
                  fd->comm);
    }
    /* TEMPORARY -- REMOVE WHEN NO LONGER UPDATING INFO FOR
     * FS-INDEP. */
    value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));
    MPIU_Snprintf(value, MPI_MAX_INFO_VAL+1, "%d", fd->hints->cb_nodes);
    value[MPI_MAX_INFO_VAL] = 0;

    NMPI_Info_set(fd->info, const_cast<char*>("cb_nodes"), value);

    ADIOI_Free(value);

    return 0;
}


static int ADIO_cb_config_list_finalize( void * )
{
    if (cb_config_list_keyval != MPI_KEYVAL_INVALID)
    {
        NMPI_Comm_free_keyval( &cb_config_list_keyval );
    }

    return MPI_SUCCESS;
}


/* ADIOI_cb_gather_name_array() - gather a list of processor names from all processes
 *                          in a communicator and store them on rank 0.
 *
 * This is a collective call on the communicator(s) passed in.
 *
 * Obtains a rank-ordered list of processor names from the processes in
 * "dupcomm".
 *
 * Returns 0 on success, -1 on failure.
 *
 * NOTE: Needs some work to cleanly handle out of memory cases!
 */
int ADIOI_cb_gather_name_array(MPI_Comm comm,
                               MPI_Comm dupcomm,
                               ADIO_cb_name_array *arrayp)
{
    char my_procname[MPI_MAX_PROCESSOR_NAME], **procname = 0;
    int *procname_len = NULL, my_procname_len, *disp = NULL, i;
    int commsize, commrank, found;
    ADIO_cb_name_array array = NULL;
    int alloc_size;

    if (cb_config_list_keyval == MPI_KEYVAL_INVALID)
    {
        NMPI_Comm_create_keyval(
            ADIOI_cb_copy_name_array,
            ADIOI_cb_delete_name_array,
            &cb_config_list_keyval,
            NULL
            );
        MPIR_Add_finalize( ADIO_cb_config_list_finalize, NULL,
                           MPIR_FINALIZE_CALLBACK_PRIO-1);
    }
    else
    {
        NMPI_Comm_get_attr(comm, cb_config_list_keyval, (void *) &array, &found);
        if (found)
        {
            *arrayp = array;
            return 0;
        }
    }

    NMPI_Comm_size(dupcomm, &commsize);
    NMPI_Comm_rank(dupcomm, &commrank);

    NMPI_Get_processor_name(my_procname, &my_procname_len);

    /* allocate space for everything */
    array = (ADIO_cb_name_array) ADIOI_Malloc(sizeof(*array));
    if (array == NULL)
    {
        return -1;
    }
    array->refct = 2; /* we're going to associate this with two comms */

    if (commrank == 0)
    {
        /* process 0 keeps the real list */
        array->namect = commsize;

        array->names = (char **) ADIOI_Malloc(sizeof(char *) * commsize);
        if (array->names == NULL)
        {
            return -1;
        }
        procname = array->names; /* simpler to read */

        procname_len = (int *) ADIOI_Malloc(commsize * sizeof(int));
        if (procname_len == NULL)
        {
            return -1;
        }
    }
    else
    {
        /* everyone else just keeps an empty list as a placeholder */
        array->namect = 0;
        array->names = NULL;
    }
    /* gather lengths first */
    NMPI_Gather(&my_procname_len, 1, MPI_INT,
               procname_len, 1, MPI_INT, 0, dupcomm);

    if (commrank == 0)
    {
        alloc_size = 0;
        for (i=0; i < commsize; i++)
        {
            /* add one to the lengths because we need to count the
             * terminator, and we are going to use this list of lengths
             * again in the gatherv.
             */
            alloc_size += ++procname_len[i];
        }

        procname[0] = (char*)ADIOI_Malloc(alloc_size);
        if (procname[0] == NULL)
        {
            return -1;
        }

        for (i=1; i < commsize; i++)
        {
            procname[i] = procname[i-1] + procname_len[i-1];
        }

        /* create our list of displacements for the gatherv.  we're going
         * to do everything relative to the start of the region allocated
         * for procname[0]
         */
        disp = (int*)ADIOI_Malloc(commsize * sizeof(int));
        disp[0] = 0;
        for (i=1; i < commsize; i++)
        {
            disp[i] = (int) (procname[i] - procname[0]);
        }

    }

    /* now gather strings */
    if (commrank == 0)
    {
        NMPI_Gatherv(my_procname, my_procname_len + 1, MPI_CHAR,
                    procname[0], procname_len, disp, MPI_CHAR,
                    0, dupcomm);
    }
    else
    {
        /* if we didn't do this, we would need to allocate procname[]
         * on all processes...which seems a little silly.
         */
        NMPI_Gatherv(my_procname, my_procname_len + 1, MPI_CHAR,
                    NULL, NULL, NULL, MPI_CHAR, 0, dupcomm);
    }

    if (commrank == 0)
    {
        /* no longer need the displacements or lengths */
        ADIOI_Free(disp);
        ADIOI_Free(procname_len);
    }

    /* store the attribute; we want to store SOMETHING on all processes
     * so that they can all tell if we have gone through this procedure
     * or not for the given communicator.
     *
     * specifically we put it on both the original comm, so we can find
     * it next time an open is performed on this same comm, and on the
     * dupcomm, so we can use it in I/O operations.
     */
    NMPI_Comm_set_attr(comm, cb_config_list_keyval, array);
    NMPI_Comm_set_attr(dupcomm, cb_config_list_keyval, array);
    *arrayp = array;
    return 0;
}


/* ADIOI_cb_config_list_parse() - parse the cb_config_list and build the
 * ranklist
 *
 * Parameters:
 * (pretty self explanatory)
 *
 * Returns number of ranks allocated in parsing, -1 on error.
 */
_Success_( return != -1 )
int
ADIOI_cb_config_list_parse(
    _In_opt_z_ PCSTR config_list,
    _In_ const ADIO_cb_name_array array,
    _In_reads_(cb_nodes) int ranklist[],
    _In_ int cb_nodes
    )
{
    int token, max_procs, cur_rank = 0, nr_procnames;
    char *cur_procname;
    const char *cur_procname_p;
    char **procnames;
    bool *used_procnames;

    nr_procnames = array->namect;
    procnames = array->names;

    /* nothing big goes on the stack */
    /* we use info val here and for yylval because we know the string
     * cannot be any bigger than this.
     */
    cur_procname = (char*)ADIOI_Malloc((MPI_MAX_INFO_VAL+1) * sizeof(char));
    if (cur_procname == NULL)
    {
        return -1;
    }

    yylval = (char*)ADIOI_Malloc((MPI_MAX_INFO_VAL+1) * sizeof(char));
    if (yylval == NULL)
    {
        ADIOI_Free(cur_procname);
        return -1;
    }

    token_ptr = config_list;

    /* right away let's make sure cb_nodes isn't too big */
    if (cb_nodes > nr_procnames) cb_nodes = nr_procnames;

    /* used_procnames is used as a mask so that we don't have to destroy
     * our procnames array
     */
    used_procnames = static_cast<bool*>( ADIOI_Malloc(array->namect * sizeof(bool)) );
    if (used_procnames == NULL)
    {
        ADIOI_Free(cur_procname);
        ADIOI_Free(yylval);
        yylval = NULL;
        return -1;
    }
    ZeroMemory(used_procnames, array->namect * sizeof(bool));

    /* optimization for "*:*"; arguably this could be done before we
     * build the list of processor names...but that would make things
     * messy.
     */
    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        config_list,
                        -1,
                        "*:*",
                        -1 ) == CSTR_EQUAL )
    {
        cur_rank = cb_nodes;
        do
        {
            --cur_rank;
            ranklist[cur_rank] = cur_rank;
        } while (cur_rank > 0);
        ADIOI_Free(cur_procname);
        ADIOI_Free(yylval);
        yylval = NULL;
        ADIOI_Free(used_procnames);
        return cb_nodes;
    }

    while (cur_rank < cb_nodes)
    {
        token = cb_config_list_lex();

        if (token == AGG_EOS)
        {
            ADIOI_Free(cur_procname);
            ADIOI_Free(yylval);
            yylval = NULL;
            ADIOI_Free(used_procnames);
            return cur_rank;
        }

        if (token != AGG_WILDCARD && token != AGG_STRING)
        {
            /* maybe ignore and try to keep going? */
            ADIOI_Free(cur_procname);
            ADIOI_Free(yylval);
            yylval = NULL;
            ADIOI_Free(used_procnames);
            return cur_rank;
        }

        if (token == AGG_WILDCARD)
        {
            cur_procname_p = NULL;
        }
        else
        {
            /* AGG_STRING is the only remaining case */
            /* save procname (for now) */
            MPIU_Strncpy(cur_procname, yylval, MPI_MAX_INFO_VAL+1);
            cur_procname_p = cur_procname;
        }

        /* after we have saved the current procname, we can grab max_procs */
        max_procs = get_max_procs(cb_nodes);

        /* do the matching for this piece of the cb_config_list */
        match_procs(cur_procname_p, max_procs, procnames, used_procnames,
                    nr_procnames, ranklist, cb_nodes, &cur_rank);
    }
    ADIOI_Free(cur_procname);
    ADIOI_Free(yylval);
    yylval = NULL;
    ADIOI_Free(used_procnames);
    return cur_rank;
}

/* ADIOI_cb_copy_name_array() - attribute copy routine
 */
int ADIOI_cb_copy_name_array(MPI_Comm /*comm*/,
                       int /*keyval*/,
                       void* /*extra*/,
                       void *attr_in,
                       void *attr_out,
                       int *flag)
{
    ADIO_cb_name_array array;

    array = (ADIO_cb_name_array) attr_in;
    array->refct++;

    *(void**)attr_out = attr_in;
    *flag = 1; /* make a copy in the new communicator */

    return MPI_SUCCESS;
}

/* ADIOI_cb_delete_name_array() - attribute destructor
 */
int ADIOI_cb_delete_name_array(MPI_Comm /*comm*/,
                         int /*keyval*/,
                         void *attr_val,
                         void* /*extra*/)
{
    ADIO_cb_name_array array;

    array = (ADIO_cb_name_array) attr_val;
    array->refct--;

    if (array->refct <= 0)
    {
        /* time to free the structures (names, array of ptrs to names, struct)
         */
        if (array->namect)
        {
            /* Note that array->names[i], where i > 0,
             * are just pointers into the allocated region array->names[0]
             */
            ADIOI_Free(array->names[0]);
        }
        if (array->names != NULL) ADIOI_Free(array->names);
        ADIOI_Free(array);
    }

    return MPI_SUCCESS;
}

/* match_procs() - given a name (or NULL for wildcard) and a max. number
 *                 of aggregator processes (per processor name), this
 *                 matches in the procnames[] array and puts appropriate
 *                 ranks in the ranks array.
 *
 * Parameters:
 * name - processor name (or NULL for wildcard)
 * max_per_proc - maximum # of processes to use for aggregation from a
 *                single processor
 * procnames - array of processor names
 * nr_procnames - length of procnames array
 * ranks - array of process ranks
 * nr_ranks - length of process ranks array (also max. # of aggregators)
 * nr_ranks_allocated - # of array entries which have been filled in,
 *                      which is also the index to the first empty entry
 *
 * Returns number of matches.
 */
_Ret_range_(>=,0)
static int
match_procs(
    _In_opt_z_ PCSTR name,
    _In_range_(>=,0) int max_per_proc,
    _In_reads_(nr_procnames) PCSTR const procnames[],
    _Out_writes_(nr_procnames) bool used_procnames[],
    _In_ int nr_procnames,
    _Out_writes_(nr_ranks) int ranks[],
    _In_range_(>=,0) int nr_ranks,
    _Inout_ int *nr_ranks_allocated
    )
{
    int wildcard_proc, cur_proc, old_nr_allocated, ret;

    /* save this so we can report on progress */
    old_nr_allocated = *nr_ranks_allocated;

    if (name == NULL)
    {
        /* wildcard case */

        /* optimize for *:0 case */
        if (max_per_proc == 0)
        {
            /* loop through procnames and mark them all as used */
            for (cur_proc = 0; cur_proc < nr_procnames; cur_proc++)
            {
                used_procnames[cur_proc] = true;
            }
            return 0;
        }

        /* the plan here is to start at the beginning of the procnames
         * array looking for processor names to apply the wildcard to.
         *
         * we set wildcard_proc to 0 here but do the search inside the
         * while loop so that we aren't starting our search from the
         * beginning of the procnames array each time.
         */
        wildcard_proc = 0;

        while (nr_ranks - *nr_ranks_allocated > 0)
        {
            /* find a name */
            while ((wildcard_proc < nr_procnames) &&
                   (used_procnames[wildcard_proc] == true))
            {
                wildcard_proc++;
            }

            if (wildcard_proc >= nr_procnames)
            {
                /* we have used up the entire procnames list */
                return *nr_ranks_allocated - old_nr_allocated;
            }

            cur_proc = wildcard_proc;

            /* alloc max_per_proc from this host; cur_proc points to
             * the first one.  We want to save this name for use in
             * our while loop.
             */
            ranks[*nr_ranks_allocated] = cur_proc;
            *nr_ranks_allocated = *nr_ranks_allocated + 1;
            cur_proc++;

            /* so, to accomplish this we use the match_this_proc() to
             * alloc max_per_proc-1.  we increment cur_proc so that the
             * procnames[] entry doesn't get trashed.  then AFTER the call
             * we clean up the first instance of the name.
             */
            ret = match_this_proc(procnames[wildcard_proc],  cur_proc,
                                  max_per_proc-1, procnames, used_procnames,
                                  nr_procnames,
                                  ranks, nr_ranks, *nr_ranks_allocated);
            if (ret > 0) *nr_ranks_allocated = *nr_ranks_allocated + ret;

            /* clean up and point wildcard_proc to the next entry, since
             * we know that this one is NULL now.
             */
            used_procnames[wildcard_proc] = true;
            wildcard_proc++;
        }
    }
    else
    {
        /* specific host was specified; this one is easy */
        ret = match_this_proc(name, 0, max_per_proc, (const char**)procnames, used_procnames,
                              nr_procnames, ranks, nr_ranks,
                              *nr_ranks_allocated);
        if (ret > 0) *nr_ranks_allocated = *nr_ranks_allocated + ret;
    }
    return *nr_ranks_allocated - old_nr_allocated;
}

/* match_this_proc() - find each instance of processor name "name" in
 *                     the "procnames" array, starting with index "cur_proc"
 *                     and add the first "max_matches" into the "ranks"
 *                     array.  remove all instances of "name" from
 *                     "procnames".
 *
 * Parameters:
 * name - processor name to match
 * cur_proc - index into procnames[] at which to start matching
 * procnames - array of processor names
 * used_procnames - array of values indicating if a given procname has
 *                  been allocated or removed already
 * nr_procnames - length of procnames array
 * ranks - array of processor ranks
 * nr_ranks - length of ranks array
 * nr_ranks_allocated - number of ranks already filled in, or the next
 *                      entry to fill in (equivalent)
 *
 * Returns number of ranks filled in (allocated).
 */
_Ret_range_(>=,0)
static int
match_this_proc(
    _In_ PCSTR name,
    _In_ int cur_proc,
    _In_range_(>=,0) int max_matches,
    _In_reads_(nr_procnames) PCSTR const procnames[],
    _Out_writes_(nr_procnames) bool used_procnames[],
    _In_range_(>=,0) int nr_procnames,
    _In_reads_(nr_ranks) int ranks[],
    _In_range_(>=,0) int nr_ranks,
    _In_range_(>=,0) int nr_ranks_allocated
    )
{
    int ranks_remaining, nr_to_alloc, old_nr_allocated;

    old_nr_allocated = nr_ranks_allocated;

    /* calculate how many ranks we want to allocate */
    ranks_remaining = nr_ranks - nr_ranks_allocated;
    nr_to_alloc = (max_matches < ranks_remaining) ?
        max_matches : ranks_remaining;

    while (nr_to_alloc > 0)
    {
        cur_proc = find_name(name, procnames, used_procnames, nr_procnames,
                             cur_proc);
        if (cur_proc < 0)
        {
            /* didn't find it */
            return nr_ranks_allocated - old_nr_allocated;
        }

        /* need bounds checking on ranks */
        ranks[nr_ranks_allocated] = cur_proc;
        nr_ranks_allocated++;
        used_procnames[cur_proc] = true;

        cur_proc++;
        nr_to_alloc--;
    }

    /* take all other instances of this host out of the list */
    while (cur_proc >= 0)
    {
        cur_proc = find_name(name, procnames, used_procnames, nr_procnames,
                             cur_proc);
        if (cur_proc >= 0)
        {
            used_procnames[cur_proc] = true;
            cur_proc++;
        }
    }
    return nr_ranks_allocated - old_nr_allocated;
}


/* find_name() - finds the first entry in procnames[] which matches name,
 *               starting at index start_ind
 *
 * Returns an index [0..nr_procnames-1] on success, -1 if not found.
 */
_Ret_range_(-1, nr_procnames-1)
static int
find_name(
    _In_ PCSTR name,
    _In_reads_(nr_procnames) PCSTR const procnames[],
    _In_reads_(nr_procnames) const bool used_procnames[],
    _In_range_(>=,0) int nr_procnames,
    _In_range_(>=, nr_procnames) int start_ind
    )
{
    int i;

    for (i=start_ind; i < nr_procnames; i++)
    {
        if (!used_procnames[i] &&
            CompareStringA( LOCALE_INVARIANT,
                            0,
                            name,
                            -1,
                            procnames[i],
                            -1 ) == CSTR_EQUAL )
        {
            return i;
        }
    }

    return -1;
}

/* get_max_procs() - grab the maximum number of processes to use out of
 *                   the cb_config_list string
 *
 * Parameters:
 * cb_nodes - cb_nodes value.  this is returned when a "*" is encountered
 *            as the max_procs value.
 *
 * Returns # of processes, or -1 on error.
 */
static int get_max_procs(int cb_nodes)
{
    int token, max_procs = -1;
    char *errptr;

    token = cb_config_list_lex();

    switch(token)
    {
    case AGG_EOS:
    case AGG_COMMA:
        return 1;
    case AGG_COLON:
        token = cb_config_list_lex();

        if (token == AGG_WILDCARD)
        {
            max_procs = cb_nodes;
        }
        else if (token == AGG_STRING)
        {
            max_procs = strtol(yylval, &errptr, 10);
            if (*errptr != '\0')
            {
                /* some garbage value; default to 1 */
                max_procs = 1;
            }
        }
        else
        {
            return -1;
        }

        /* strip off next comma (if there is one) */
        token = cb_config_list_lex();
        if ((token == AGG_COMMA || token == AGG_EOS) && max_procs >= 0)
        {
            return max_procs;
        }
    }
    return -1;
}


/* cb_config_list_lex() - lexical analyzer for cb_config_list language
 *
 * Returns a token of types defined at top of this file.
 */
static int cb_config_list_lex(void)
{
    int slen;

    if (*token_ptr == '\0') return AGG_EOS;

    slen = (int)strcspn(token_ptr, ":,");

    if (*token_ptr == ':')
    {
        token_ptr++;
        return AGG_COLON;
    }
    if (*token_ptr == ',')
    {
        token_ptr++;
        return AGG_COMMA;
    }

    if (*token_ptr == '*')
    {
        /* make sure that we don't have characters after the '*' */
        if (slen == 1)
        {
            token_ptr++;
            return AGG_WILDCARD;
        }
        return AGG_ERROR;
    }

    /* last case: some kind of string.  for now we copy the string. */

    /* it would be a good idea to look at the string and make sure that
     * it doesn't have any illegal characters in it.  in particular we
     * should ensure that no one tries to use wildcards with strings
     * (e.g. "ccn*").
     */
    MPIU_Strncpy(yylval, token_ptr, slen+1);
    token_ptr += slen;
    return AGG_STRING;
}
