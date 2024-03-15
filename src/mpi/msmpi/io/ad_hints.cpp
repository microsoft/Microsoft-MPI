// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */

#include "precomp.h"
#include "adio.h"
#include "adio_extern.h"



int ADIOI_GEN_SetInfo(MPI_File fd, MPI_Info users_info)
{
/* if fd->info is null, create a new info object.
   Initialize fd->info to default values.
   Initialize fd->hints to default values.
   Examine the info object passed by the user. If it contains values that
   ROMIO understands, override the default. */

    MPI_Info info;
    char *value;
    int flag, intval, tmp_val, nprocs=0, nprocs_is_valid = 0;
    size_t len;

    if (fd->info == MPI_INFO_NULL) NMPI_Info_create(&(fd->info));
    info = fd->info;

    /* Note that fd->hints is allocated at file open time; thus it is
     * not necessary to allocate it, or check for allocation, here.
     */

    value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));

    MPIU_Assert(value);

    /* initialize info and hints to default values if they haven't been
     * previously initialized
     */
    if (!fd->hints->initialized)
    {
        /* buffer size for collective I/O */
        NMPI_Info_set(info, const_cast<char *>("cb_buffer_size"), const_cast<char *>(ADIOI_CB_BUFFER_SIZE_DFLT));
        fd->hints->cb_buffer_size = atoi(ADIOI_CB_BUFFER_SIZE_DFLT);

        /* default is to let romio automatically decide when to use
         * collective buffering
         */
        NMPI_Info_set(info, const_cast<char *>("romio_cb_read"), const_cast<char *>("automatic"));
        fd->hints->cb_read = ADIOI_HINT_AUTO;
        NMPI_Info_set(info, const_cast<char *>("romio_cb_write"), const_cast<char *>("automatic"));
        fd->hints->cb_write = ADIOI_HINT_AUTO;

        fd->hints->cb_config_list = NULL;

        /* number of processes that perform I/O in collective I/O */
        NMPI_Comm_size(fd->comm, &nprocs);
        nprocs_is_valid = 1;
        MPIU_Snprintf(value, MPI_MAX_INFO_VAL+1, "%d", nprocs);
        value[MPI_MAX_INFO_VAL] = 0;
        NMPI_Info_set(info, const_cast<char *>("cb_nodes"), value);
        fd->hints->cb_nodes = nprocs;

        /* hint indicating that no indep. I/O will be performed on this file */
        NMPI_Info_set(info, const_cast<char *>("romio_no_indep_rw"), const_cast<char *>("false"));
        fd->hints->no_indep_rw = 0;
         /* deferred_open derived from no_indep_rw and cb_{read,write} */
        fd->hints->deferred_open = 0;

        /* buffer size for data sieving in independent reads */
        NMPI_Info_set(info, const_cast<char *>("ind_rd_buffer_size"), const_cast<char *>(ADIOI_IND_RD_BUFFER_SIZE_DFLT));
        fd->hints->ind_rd_buffer_size = atoi(ADIOI_IND_RD_BUFFER_SIZE_DFLT);

        /* buffer size for data sieving in independent writes */
        NMPI_Info_set(info, const_cast<char *>("ind_wr_buffer_size"), const_cast<char *>(ADIOI_IND_WR_BUFFER_SIZE_DFLT));
        fd->hints->ind_wr_buffer_size = atoi(ADIOI_IND_WR_BUFFER_SIZE_DFLT);

        /* default is to let romio automatically decide when to use data
         * sieving
         */
        NMPI_Info_set(info, const_cast<char *>("romio_ds_read"), const_cast<char *>("automatic"));
        fd->hints->ds_read = ADIOI_HINT_AUTO;
        NMPI_Info_set(info, const_cast<char *>("romio_ds_write"), const_cast<char *>("automatic"));
        fd->hints->ds_write = ADIOI_HINT_AUTO;

        fd->hints->initialized = 1;
    }

    /* add in user's info if supplied */
    if (users_info != MPI_INFO_NULL)
    {
        NMPI_Info_get(users_info, const_cast<char *>("cb_buffer_size"), MPI_MAX_INFO_VAL,
                     value, &flag);
        if (flag && ((intval=atoi(value)) > 0))
        {
            tmp_val = intval;

            NMPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
            if (tmp_val != intval)
                return MPIO_ERR_CREATE_CODE_INFO_NOT_SAME(const_cast<char *>("cb_buffer_size"));

            NMPI_Info_set(info, const_cast<char *>("cb_buffer_size"), value);
            fd->hints->cb_buffer_size = intval;

        }

        /* new hints for enabling/disabling coll. buffering on
         * reads/writes
         */
        NMPI_Info_get(users_info, const_cast<char *>("romio_cb_read"), MPI_MAX_INFO_VAL, value, &flag);
        if (flag)
        {
            if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "enable",
                                -1 )  == CSTR_EQUAL ||
                CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "ENABLE",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_cb_read"), value);
                fd->hints->cb_read = ADIOI_HINT_ENABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "disable",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "DISABLE",
                                -1 ) == CSTR_EQUAL )
            {
                    /* romio_cb_read overrides no_indep_rw */
                NMPI_Info_set(info, const_cast<char *>("romio_cb_read"), value);
                NMPI_Info_set(info, const_cast<char *>("romio_no_indep_rw"), const_cast<char *>("false"));
                fd->hints->cb_read = ADIOI_HINT_DISABLE;
                fd->hints->no_indep_rw = ADIOI_HINT_DISABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "automatic",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "AUTOMATIC",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_cb_read"), value);
                fd->hints->cb_read = ADIOI_HINT_AUTO;
            }

            tmp_val = fd->hints->cb_read;

            NMPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
            if (tmp_val != fd->hints->cb_read)
                return MPIO_ERR_CREATE_CODE_INFO_NOT_SAME(const_cast<char *>("romio_cb_read"));
        }

        NMPI_Info_get(users_info, const_cast<char *>("romio_cb_write"), MPI_MAX_INFO_VAL, value,
                     &flag);
        if (flag)
        {
            if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "enable",
                                -1 )  == CSTR_EQUAL ||
                CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "ENABLE",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_cb_write"), value);
                fd->hints->cb_write = ADIOI_HINT_ENABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "disable",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "DISABLE",
                                -1 ) == CSTR_EQUAL )
            {
                /* romio_cb_write overrides no_indep_rw, too */
                NMPI_Info_set(info, const_cast<char *>("romio_cb_write"), value);
                NMPI_Info_set(info, const_cast<char *>("romio_no_indep_rw"), const_cast<char *>("false"));
                fd->hints->cb_write = ADIOI_HINT_DISABLE;
                fd->hints->no_indep_rw = ADIOI_HINT_DISABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "automatic",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "AUTOMATIC",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_cb_write"), value);
                fd->hints->cb_write = ADIOI_HINT_AUTO;
            }

            tmp_val = fd->hints->cb_write;

            NMPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
            if (tmp_val != fd->hints->cb_write)
                return MPIO_ERR_CREATE_CODE_INFO_NOT_SAME(const_cast<char *>("romio_cb_write"));
        }

        /* new hint for specifying no indep. read/write will be performed */
        NMPI_Info_get(users_info, const_cast<char *>("romio_no_indep_rw"), MPI_MAX_INFO_VAL, value,
                     &flag);
        if (flag)
        {
            if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "true",
                                -1 )  == CSTR_EQUAL ||
                CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "TRUE",
                                -1 ) == CSTR_EQUAL )
            {
                    /* if 'no_indep_rw' set, also hint that we will do
                     * collective buffering: if we aren't doing independent io,
                     * then we have to do collective  */
                NMPI_Info_set(info, const_cast<char *>("romio_no_indep_rw"), value);
                NMPI_Info_set(info, const_cast<char *>("romio_cb_write"), const_cast<char *>("enable"));
                NMPI_Info_set(info, const_cast<char *>("romio_cb_read"), const_cast<char *>("enable"));
                fd->hints->no_indep_rw = 1;
                fd->hints->cb_read = 1;
                fd->hints->cb_write = 1;
                tmp_val = 1;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "false",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "FALSE",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_no_indep_rw"), value);
                fd->hints->no_indep_rw = 0;
                tmp_val = 0;
            }
            else
            {
                /* default is above */
                tmp_val = 0;
            }

            NMPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
            if (tmp_val != fd->hints->no_indep_rw)
                return MPIO_ERR_CREATE_CODE_INFO_NOT_SAME(const_cast<char *>("romio_no_indep_rw"));
        }

        /* new hints for enabling/disabling data sieving on
         * reads/writes
         */
        NMPI_Info_get(users_info, const_cast<char *>("romio_ds_read"), MPI_MAX_INFO_VAL, value,
                     &flag);
        if (flag)
        {
            if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "enable",
                                -1 )  == CSTR_EQUAL ||
                CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "ENABLE",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_ds_read"), value);
                fd->hints->ds_read = ADIOI_HINT_ENABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "disable",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "DISABLE",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_ds_read"), value);
                fd->hints->ds_read = ADIOI_HINT_DISABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "automatic",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "AUTOMATIC",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_ds_read"), value);
                fd->hints->ds_read = ADIOI_HINT_AUTO;
            }
            /* otherwise ignore */
        }
        NMPI_Info_get(users_info, const_cast<char *>("romio_ds_write"), MPI_MAX_INFO_VAL, value,
                     &flag);
        if (flag)
        {
            if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "enable",
                                -1 )  == CSTR_EQUAL ||
                CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "ENABLE",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_ds_write"), value);
                fd->hints->ds_write = ADIOI_HINT_ENABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "disable",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "DISABLE",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_ds_write"), value);
                fd->hints->ds_write = ADIOI_HINT_DISABLE;
            }
            else if( CompareStringA( LOCALE_INVARIANT,
                               0,
                               value,
                               -1,
                               "automatic",
                                -1 )  == CSTR_EQUAL ||
                     CompareStringA( LOCALE_INVARIANT,
                                0,
                                value,
                                -1,
                                "AUTOMATIC",
                                -1 ) == CSTR_EQUAL )
            {
                NMPI_Info_set(info, const_cast<char *>("romio_ds_write"), value);
                fd->hints->ds_write = ADIOI_HINT_AUTO;
            }
            /* otherwise ignore */
        }

        NMPI_Info_get(users_info, const_cast<char *>("cb_nodes"), MPI_MAX_INFO_VAL,
                     value, &flag);
        if (flag && ((intval=atoi(value)) > 0))
        {
            tmp_val = intval;

            NMPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
            if (tmp_val != intval)
                return MPIO_ERR_CREATE_CODE_INFO_NOT_SAME(const_cast<char *>("cb_nodes"));

            if (!nprocs_is_valid)
            {
                /* if hints were already initialized, we might not
                 * have already gotten this?
                 */
                NMPI_Comm_size(fd->comm, &nprocs);
                nprocs_is_valid = 1;
            }
            if (intval <= nprocs)
            {
                NMPI_Info_set(info, const_cast<char *>("cb_nodes"), value);
                fd->hints->cb_nodes = intval;
            }
        }

        NMPI_Info_get(users_info, const_cast<char *>("ind_wr_buffer_size"), MPI_MAX_INFO_VAL,
                     value, &flag);
        if (flag && ((intval = atoi(value)) > 0))
        {
            NMPI_Info_set(info, const_cast<char *>("ind_wr_buffer_size"), value);
            fd->hints->ind_wr_buffer_size = intval;
        }

        NMPI_Info_get(users_info, const_cast<char *>("ind_rd_buffer_size"), MPI_MAX_INFO_VAL,
                     value, &flag);
        if (flag && ((intval = atoi(value)) > 0))
        {
            NMPI_Info_set(info, const_cast<char *>("ind_rd_buffer_size"), value);
            fd->hints->ind_rd_buffer_size = intval;
        }

        NMPI_Info_get(users_info, const_cast<char *>("cb_config_list"), MPI_MAX_INFO_VAL,
                     value, &flag);
        if (flag)
        {
            if (fd->hints->cb_config_list == NULL)
            {
                /* only set cb_config_list if it isn't already set.
                 * Note that since we set it below, this ensures that
                 * the cb_config_list hint will be set at file open time
                 * either by the user or to the default
                 */
                NMPI_Info_set(info, const_cast<char *>("cb_config_list"), value);
                len = ( MPIU_Strlen( value, MPI_MAX_INFO_VAL + 1 ) + 1 ) * sizeof(char);
                fd->hints->cb_config_list = (char*)ADIOI_Malloc(len);
                if (fd->hints->cb_config_list == NULL)
                {
                    /* NEED TO HANDLE ENOMEM */
                }
                MPIU_Assert(fd->hints->cb_config_list != NULL);
                MPIU_Strncpy(fd->hints->cb_config_list, value, len);
            }
            /* if it has been set already, we ignore it the second time.
             * otherwise we would get an error if someone used the same
             * info value with a cb_config_list value in it in a couple
             * of calls, which would be irritating. */
        }
    }

    /* handle cb_config_list default value here; avoids an extra
     * free/alloc and insures it is always set
     */
    if (fd->hints->cb_config_list == NULL)
    {
        NMPI_Info_set(info, const_cast<char *>("cb_config_list"), const_cast<char *>(ADIOI_CB_CONFIG_LIST_DFLT));
        len = _countof(ADIOI_CB_CONFIG_LIST_DFLT)  * sizeof(char);
        fd->hints->cb_config_list = (char*)ADIOI_Malloc(len);
        if (fd->hints->cb_config_list == NULL)
        {
            /* NEED TO HANDLE ENOMEM */
        }
        MPIU_Assert(fd->hints->cb_config_list);
        MPIU_Strncpy(fd->hints->cb_config_list, ADIOI_CB_CONFIG_LIST_DFLT, len);
    }
    /* deferred_open won't be set by callers, but if the user doesn't
     * explicitly disable collecitve buffering (two-phase) and does hint that
     * io w/o independent io is going on, we'll set this internal hint as a
     * convenience */
    if ( ( (fd->hints->cb_read != ADIOI_HINT_DISABLE) \
                            && (fd->hints->cb_write != ADIOI_HINT_DISABLE)\
                            && fd->hints->no_indep_rw ) )
    {
            fd->hints->deferred_open = 1;
    }
    else
    {
            /* setting romio_no_indep_rw enable and romio_cb_{read,write}
             * disable at the same time doesn't make sense. honor
             * romio_cb_{read,write} and force the no_indep_rw hint to
             * 'disable' */
            NMPI_Info_set(info, const_cast<char *>("romio_no_indep_rw"), const_cast<char *>("false"));
            fd->hints->no_indep_rw = 0;
            fd->hints->deferred_open = 0;
    }

    ADIOI_Free(value);
    return MPI_SUCCESS;
}
