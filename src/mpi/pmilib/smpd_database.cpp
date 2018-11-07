// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"
#include "rpc.h"


typedef struct smpd_key_value_t
{
    struct smpd_key_value_t* next;
    char szKey[SMPD_MAX_DBS_KEY_LEN];
    char szValue[SMPD_MAX_DBS_VALUE_LEN];

} smpd_key_value_t;


typedef struct smpd_kvs_t
{
    struct smpd_kvs_t* next;
    GUID id;
    char** ppBizCards;
    UINT16 size;
    smpd_key_value_t* data;
    smpd_key_value_t* iterator;

} smpd_kvs_t;


static smpd_kvs_t* g_kvs_db = nullptr;


bool
smpd_dbs_init()
{
    return smpd_dbs_create( 0, &smpd_process.domain );
}


static void smpd_dbs_free_kvs(smpd_kvs_t* kvs)
{
    while (kvs->data)
    {
        smpd_key_value_t* data = kvs->data;
        kvs->data = data->next;
        delete data;
    }

    if( kvs->ppBizCards != nullptr )
    {
        for( unsigned i = 0; i < kvs->size; ++i )
        {
            delete[] kvs->ppBizCards[i];
        }
        delete[] kvs->ppBizCards;
    }

    delete kvs;
}


void smpd_dbs_finalize()
{
    smpd_kvs_t* kvs = g_kvs_db;
    while(kvs)
    {
        smpd_kvs_t* next = kvs->next;
        smpd_dbs_free_kvs(kvs);
        kvs = next;
    }

    g_kvs_db = nullptr;
}


static inline
smpd_kvs_t** smpd_dbs_find_kvs(
    const GUID& id
    )
{
    smpd_kvs_t** pkvs = &g_kvs_db;
    for( ; (*pkvs) != nullptr; pkvs = &(*pkvs)->next)
    {
        if( IsEqualGUID( (*pkvs)->id, id ) )
        {
            return pkvs;
        }
    }

    return pkvs;
}


_Success_( return == true )
bool
smpd_dbs_create(
    _In_    UINT16  size,
    _Inout_ GUID*   id
    )
{
    smpd_kvs_t* kvs = new smpd_kvs_t;
    if( kvs == nullptr )
    {
        return false;
    }

    kvs->next = nullptr;
    kvs->data = nullptr;
    kvs->iterator = nullptr;
    kvs->size = size;

    if( size != 0 )
    {
        kvs->ppBizCards = new char*[size];
        if( kvs->ppBizCards == nullptr )
        {
            delete kvs;
            return false;
        }
        ZeroMemory( kvs->ppBizCards, sizeof(char*) * size );
    }
    else
    {
        kvs->ppBizCards = nullptr;
    }

    if (*id != GUID_NULL)
    {
        kvs->id = *id;
        smpd_kvs_t** pkvs = smpd_dbs_find_kvs(kvs->id);
        ASSERT((*pkvs) == nullptr);
        *pkvs = kvs;
        return true;
    }
    else
    {
        for (;;)
        {
            DWORD rc = UuidCreate(&kvs->id);
            if (rc != RPC_S_OK && rc != RPC_S_UUID_LOCAL_ONLY)
            {
                if (size != 0)
                {
                    delete[] kvs->ppBizCards;
                }
                delete kvs;
                return false;
            }

            smpd_kvs_t** pkvs = smpd_dbs_find_kvs(kvs->id);
            if ((*pkvs) != nullptr)
            {
                continue;
            }

            //
            // add the new kvs to the dbs and return its name
            //
            *pkvs = kvs;
            *id = kvs->id;

            return true;
        }
    }

}


_Ret_maybenull_
static inline
smpd_key_value_t*
smpd_dbs_find_key(
    _In_ const smpd_kvs_t* kvs,
    const char* key
    )
{
    for(smpd_key_value_t* data = kvs->data; data != nullptr; data = data->next)
    {
        if( CompareStringA( LOCALE_INVARIANT,
                            0,
                            data->szKey,
                            -1,
                            key,
                            -1 ) == CSTR_EQUAL )
        {
            return data;
        }
    }

    return nullptr;
}


_Success_(return == true)
bool
smpd_dbs_get(
    _In_ const GUID& id,
    _In_ PCSTR key,
    _Out_writes_z_(val_size) char* value,
    _In_ size_t val_size
    )
{
    smpd_kvs_t* kvs = *smpd_dbs_find_kvs( id );
    if( kvs == nullptr )
    {
        return false;
    }

    const smpd_key_value_t* data = smpd_dbs_find_key(kvs, key);
    if( data == nullptr )
    {
        return false;
    }

    smpd_strncpy(value, val_size, data->szValue);
    return true;
}


_Success_(return == true)
bool
smpd_dbs_bcget(
    _In_ const GUID& id,
    _In_ UINT16 rank,
    _Out_writes_z_(val_size) char* value,
    _In_ size_t val_size
    )
{
    smpd_kvs_t* kvs = *smpd_dbs_find_kvs( id );
    if( kvs == nullptr )
    {
        return false;
    }

    ASSERT(kvs->ppBizCards != nullptr && kvs->ppBizCards[rank] != nullptr);
    smpd_strncpy( value, val_size, kvs->ppBizCards[rank] );
    return true;
}


bool
smpd_dbs_put(
    _In_ const GUID& id,
    _In_ PCSTR key,
    _In_ PCSTR value
    )
{
    smpd_kvs_t* kvs = *smpd_dbs_find_kvs( id );
    if( kvs == nullptr )
    {
        return false;
    }

    smpd_key_value_t* data = smpd_dbs_find_key(kvs, key);
    if( data != nullptr )
    {
        MPIU_Strcpy( data->szValue, _countof(data->szValue), value );
        return true;
    }

    data = new smpd_key_value_t;
    if(data == nullptr)
    {
        return false;
    }

    data->next = kvs->data;
    MPIU_Strcpy( data->szKey, _countof(data->szKey), key );
    MPIU_Strcpy( data->szValue, _countof(data->szValue), value );
    kvs->data = data;
    return true;
}


bool
smpd_dbs_bcput(
    _In_ const GUID& id,
    _In_ UINT16 rank,
    _In_ PCSTR value
    )
{
    smpd_kvs_t* kvs = *smpd_dbs_find_kvs( id );
    if( kvs == nullptr )
    {
        return false;
    }

    ASSERT(kvs->ppBizCards != nullptr && kvs->ppBizCards[rank] == nullptr);

    kvs->ppBizCards[rank] = new char[SMPD_MAX_DBS_VALUE_LEN];
    if( kvs->ppBizCards[rank] == nullptr )
    {
        return false;
    }

    smpd_strncpy( kvs->ppBizCards[rank], SMPD_MAX_DBS_VALUE_LEN, value );
    return true;
}


bool
smpd_dbs_destroy(
    _In_ const GUID& id
    )
{
    smpd_kvs_t** pkvs = smpd_dbs_find_kvs( id );
    if(*pkvs == nullptr)
    {
        return false;
    }

    smpd_kvs_t* next = (*pkvs)->next;
    smpd_dbs_free_kvs(*pkvs);
    *pkvs = next;
    return true;
}
