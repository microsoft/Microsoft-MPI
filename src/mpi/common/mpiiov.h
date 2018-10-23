// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef MPIIOV_H_INCLUDED
#define MPIIOV_H_INCLUDED

/* IOVs */
/* The basic channel interface uses IOVs */
typedef WSABUF MPID_IOV;
typedef char iovsendbuf_t;
typedef char iovrecvbuf_t;

/* FIXME: How is IOV_LIMIT chosen? */
#define MPID_IOV_LIMIT   16

static inline unsigned iov_size(_In_reads_(n_iov) const MPID_IOV* iov, int n_iov)
{
    unsigned total = 0;
    while(n_iov--)
    {
        total += iov->len;
        iov++;
    }

    return total;
}

#endif
