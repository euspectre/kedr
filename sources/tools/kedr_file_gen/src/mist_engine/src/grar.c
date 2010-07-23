/// grar.c
/// Growing arrays - implementation.

/////////////////////////////////////////////////////////////////////////////
// Copyright 2009-2010 
// Institute for System Programming of the Russian Academy of Sciences (ISPRAS)
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//    http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License. 
/////////////////////////////////////////////////////////////////////////////

#include "grar.h"

#include <string.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////
// Implementation: private methods
///////////////////////////////////////////////////////////////////////////

/// A comparator function used to sort the arrays of strings and search for
/// the particular strings in these arrays.
/// Its arguments are actually (const char**), i.e. the pointers to pointers 
/// to strings.
static int
grar_string_compare(const void* lhs, const void* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    const char** sleft  = (const char**)lhs;
    const char** sright = (const char**)rhs;

    assert(*sleft != NULL);
    assert(*sright != NULL);
    
    if (sleft == sright || *sleft == *sright)
    {
        return 0;
    }

    return strcmp(*sleft, *sright);
}

///////////////////////////////////////////////////////////////////////////
// Implementation: public methods
///////////////////////////////////////////////////////////////////////////
int 
grar_create(CGrowingArray* grar)
{
    assert(grar != NULL);
    
    TGAElem* data = (TGAElem*)malloc(GRAR_DEFAULT_CAPACITY * sizeof(TGAElem));
    if (data == NULL)
    {
        return 0;
    }
    
    // memory allocation is done now
    grar->capacity = GRAR_DEFAULT_CAPACITY;
    grar->size = 0;
    grar->data = data;
    
    return 1;
}

int 
grar_reserve(CGrowingArray* grar, size_t new_min_capacity)
{
    assert(grar != NULL);
        
    if (grar->capacity >= new_min_capacity)
    {
        // nothing to do
        return 1;
    }
    
    size_t cap = grar->capacity;
    do
    {
         cap *= GRAR_CAP_INC_FACTOR;
    }
    while (cap < new_min_capacity);
    
    TGAElem* data = (TGAElem*)realloc(grar->data, cap * sizeof(TGAElem));
    if (data == NULL)
    {
        return 0;
    }
    
    grar->capacity = cap;
    grar->data = data;
    
    return 1;
}

int
grar_add_element(CGrowingArray* grar, TGAElem elem)
{
    assert(grar != NULL);
    
    int res = grar_reserve(grar, grar->size + 1);
    if (!res)
    {
        return 0;
    }
    
    grar->data[grar->size] = elem;
    ++grar->size;
    
    return res;
}

void
grar_destroy(CGrowingArray* grar)
{
    assert(grar != NULL);

    free(grar->data);
    
    grar->data = NULL;
    grar->size = 0;
    grar->capacity = 0; // 0 because no memory is allocated now
    
    return;
}

void
grar_destroy_with_elements(CGrowingArray* grar, TDestructorFunc dtor, void* user_data)
{
    assert(grar != NULL);
    
    if (dtor == NULL)
    {
        // no custom destructor specified, just free memory
        for (size_t i = 0; i < grar->size; ++i)
        {
            free(grar->data[i]);
        }
    }
    else //(dtor != NULL)
    {
        // call the specified destructor function for each element
        for (size_t i = 0; i < grar->size; ++i)
        {
            dtor(grar->data[i], user_data);
        }
    }
    
    grar_destroy(grar);
    
    return;
}

int
grar_append_array(CGrowingArray* grar, const CGrowingArray* src)
{
    assert(grar != NULL);
    assert(src != NULL);
    
    // minimum required capacity
    size_t req_cap = grar->size + src->size;
    
    int res = grar_reserve(grar, req_cap); 
    
    if (res == 0)
    {
        return 0;
    }
    
    memcpy((void*)(grar->data + grar->size), src->data, src->size * sizeof(TGAElem));
    
    grar->size += src->size;
    
    return 1;
}

void
grar_clear(CGrowingArray* grar)
{
    assert(grar != NULL);
    assert(grar->data != NULL);

    grar->size = 0;

    return;
}

void
grar_sort(CGrowingArray* grar, TCompareFunc cf)
{
    assert(grar != NULL);
    assert(cf != NULL);

    qsort(grar_get_c_array(grar, TGAElem), grar_get_size(grar),
        sizeof(TGAElem), cf);

    return;
}

int
grar_find(const CGrowingArray* grar, const void* key, TCompareFunc cf)
{
    assert(grar != NULL);
    assert(cf != NULL);
    assert(key != NULL);

    const TGAElem* base = grar_get_c_array(grar, TGAElem);

    const TGAElem* p = (const TGAElem*)bsearch(key, base, grar_get_size(grar),
        sizeof(TGAElem), cf);
    
    if (p == NULL)
    {
        return -1;
    }

    int index = (int)(p - base);
    assert(index >= 0 && index < (int)grar_get_size(grar));

    return index;
}

void
grar_swap(CGrowingArray* lhs, CGrowingArray* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);
    
    TGAElem* data = lhs->data;
    lhs->data = rhs->data;
    rhs->data = data;
    
    size_t s = lhs->size;
    lhs->size = rhs->size;
    rhs->size = s;
    
    s = lhs->capacity;
    lhs->capacity = rhs->capacity;
    rhs->capacity = s;
    
    return;
}

void
grar_string_sort(CGrowingArray* grar)
{
    grar_sort(grar, grar_string_compare);
    return;
}

int 
grar_string_find(const CGrowingArray* grar, const char* skey)
{
    return grar_find(grar, &skey, grar_string_compare);
}

size_t
grar_string_total_length(CGrowingArray* grar)
{
    assert(grar != NULL);

    size_t len = 0;
    size_t nstr = grar_get_size(grar);

    for (size_t i = 0; i < nstr; ++i)
    {
        const char* elem = grar_get_element(grar, const char*, i);
        assert(elem != NULL);
        len += strlen(elem);
    }

    return len;
}
