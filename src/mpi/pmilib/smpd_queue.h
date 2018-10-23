// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef SMPD_QUEUE_H
#define SMPD_QUEUE_H

template<class T> struct smpd_queue
{
public:
    T* m_head;
    T* m_tail;

public:
    smpd_queue() : m_head(0) {}

    T* head() const { return m_head; }
    void enqueue(T* node);
    T* dequeue();
};

template<class T>
inline void smpd_queue<T>::enqueue(T* node)
{
    node->next = NULL;
    if (m_head == NULL)
    {
        m_head = node;
    }
    else
    {
        m_tail->next = node;
    }
    m_tail = node;
}

template<class T>
inline T* smpd_queue<T>::dequeue()
{
    T* node = m_head;
    m_head = m_head->next;
    return node;
}

#endif /* SMPD_QUEUE_H */
