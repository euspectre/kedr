/// smap.c
/// String map - implementation.

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
#include "smap.h"

#include <string.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////
// Private structure definitions
///////////////////////////////////////////////////////////////////////////

struct CStringMap_
{
    CGrowingArray container;    /// Here the elements are actually contained
    int sorted;                 /// Nonzero if the array is already sorted, 0 otherwise
};

///////////////////////////////////////////////////////////////////////////
// Implementation: private methods
///////////////////////////////////////////////////////////////////////////

/// A destructor for TStringPair structures allocated on the heap
/// user_data argument is not used.
static void
string_pair_dtor(void* doomed, void* user_data)
{
    assert(doomed != NULL);

    TStringPair* sp = (TStringPair*)doomed;
    free(sp->key);
    free(sp->val);

    free(doomed);

    return;
}

/// A comparator function used for value lookup in the array of  (TStringPair*)
/// pointers (a string map).
/// Its arguments are actually (TStringPair**).
///
/// [NB] The function MUST NOT be used for sorting, 
/// use string_pair_compare_sort() there instead.
static int
string_pair_compare(const void* lhs, const void* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    const TStringPair** sleft  = (const TStringPair**)lhs;
    const TStringPair** sright = (const TStringPair**)rhs;

    assert(*sleft != NULL);
    assert(*sright != NULL);

    assert((*sleft)->key != NULL);
    assert((*sright)->key != NULL);
    
    // no need to compare strings if the corresponding pointers are equal
    if (sleft == sright || *sleft == *sright || 
        (*sleft)->key == (*sright)->key)
    {
        return 0;
    }
    
    return strcmp((*sleft)->key, (*sright)->key);
}

/// A comparator function used only for sorting the array of (TStringPair*) 
/// pointers (a string map).
/// Its arguments are actually (TStringPair**).
///
/// [NB] The function MUST NOT be used for lookup.
/// 
/// It is necessary that the order of the elements with equal keys 
/// remains the same (i.e. the sort must be stable). 
/// qsort() does not guarantee this, however.
///
/// To circumvent this, the suggestion from GNU C Library Manual 
/// (Section "9.3 Array Sort Function") is implemented here:
/// 
/// "If you want the effect of a stable sort, you can get this result by writing 
/// the comparison function so that, lacking other reason distinguish between two 
/// elements, it compares them by their addresses. Note that doing this may make 
/// the sorting algorithm less efficient, so do it only if necessary."
static int
string_pair_compare_sort(const void* lhs, const void* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    int res = string_pair_compare(lhs, rhs);

    if (res == 0)
    {
        if (lhs == rhs)
        {
            res = 0;
        }
        else
        {
            res = (lhs < rhs) ? -1 : 1;
        }
    }
    
    return res;
}

///////////////////////////////////////////////////////////////////////////
// Implementation: public methods
///////////////////////////////////////////////////////////////////////////
CStringMap*
smap_create()
{
    CStringMap* sm = (CStringMap*)malloc(sizeof(CStringMap));
    if (sm == NULL)
    {
        return NULL;
    }

    int res = grar_create(&(sm->container));
    if (res == 0)
    {
        free(sm);
        return NULL;
    }

    sm->sorted = 0; // unsorted by default
    return sm;
}

int
smap_add_element(CStringMap* smap, const char* key, const char* val)
{
    assert(smap != NULL);
    assert(key != NULL);
    assert(val != NULL);

    TStringPair* sp = (TStringPair*)malloc(sizeof(TStringPair));
    if (sp == NULL)
    {
        return 0;
    }

    sp->key = (char*)strdup(key);
    if (sp->key == NULL)
    {
        free(sp);
        return 0;
    }

    sp->val = (char*)strdup(val);
    if (sp->val == NULL)
    {
        free(sp->key);
        free(sp);
        return 0;
    }
    
    int res = grar_add_element(&(smap->container), (TGAElem)sp);
    if (res == 0)
    {
        string_pair_dtor(sp, NULL);
        return 0;
    }

    smap->sorted = 0; // the newly added element may have broken the order
    return res;
}

void
smap_destroy(CStringMap* smap)
{
    assert(smap != NULL);

    smap->sorted = 0;
    grar_destroy_with_elements(&(smap->container), string_pair_dtor, NULL);
    
    free(smap);

    return;
}

const char* 
smap_lookup(CStringMap* smap, char* skey)
{
    assert(smap != NULL);
    assert(skey != NULL);

    CGrowingArray* grar = &(smap->container);

    if (smap->sorted == 0)
    {
        // Sort the array first
        grar_sort(grar, string_pair_compare_sort);
        smap->sorted = 1;
    }

    // Lookup the key
    const TGAElem* base = grar_get_c_array(grar, TGAElem);
    
    TStringPair key_pair;
    key_pair.key = skey;
    key_pair.val = NULL;

    TStringPair* pkey_pair = &key_pair;

    const TGAElem* p = (const TGAElem*)bsearch(&pkey_pair, base, grar_get_size(grar),
        sizeof(TGAElem), string_pair_compare);

    if (p == NULL)
    {
        return NULL;
    }

    const TStringPair* sp = (const TStringPair*)*p;
    assert(sp != NULL);
    assert(sp->key != NULL);

    return sp->val;
}

void 
smap_clear(CStringMap* smap)
{
    assert(smap != NULL);

    size_t sz = grar_get_size(&(smap->container));
    TStringPair** base = grar_get_c_array(&(smap->container), TStringPair*);
    
    // free all the elements
    for (size_t i = 0; i < sz; ++i)
    {
        string_pair_dtor(base[i], NULL);
    }

    grar_clear(&(smap->container));

    smap->sorted = 0;

    return;
}

size_t
smap_get_size(const CStringMap* smap)
{
    assert(smap != NULL);
    return grar_get_size(&(smap->container));
}

TStringPair** 
smap_as_array(CStringMap* smap)
{
    assert(smap != NULL);
    return grar_get_c_array(&(smap->container), TStringPair*);
}

const char*
smap_check_duplicate_keys(CStringMap* smap)
{
    assert(smap != NULL);
    
    CGrowingArray* grar = &(smap->container);

    if (smap->sorted == 0)
    {
        // Sort the array first
        grar_sort(grar, string_pair_compare_sort);
        smap->sorted = 1;
    }

    // Now that the array is sorted, check the keys for duplicates
    const TStringPair** base = grar_get_c_array(grar, const TStringPair*);
    size_t num = grar_get_size(grar);
    if (num == 0)    
    {
        return NULL;
    }
    
    assert(base != NULL);
    assert(base[0] != NULL);
    assert(base[0]->key != NULL);
    assert(base[0]->val != NULL);
    
    const char* key = NULL;
    for (size_t i = 1; i < num; ++i)
    {
        assert(base[i] != NULL);
        assert(base[i]->key != NULL);
        assert(base[i]->val != NULL);
        
        if (strcmp(base[i]->key, base[i - 1]->key) == 0)
        {
            key = base[i]->key;
            break;
        }
    }
    
    return key;
}

int
smap_set_value(CStringMap* smap, const char* key, const char* val)
{
    assert(smap != NULL);
    assert(key != NULL);
    assert(val != NULL);
    
    CGrowingArray* grar = &(smap->container);

    if (smap->sorted == 0)
    {
        // Sort the array first
        grar_sort(grar, string_pair_compare_sort);
        smap->sorted = 1;
    }

    // Lookup the key
    TGAElem* base = grar_get_c_array(grar, TGAElem);
    
    TStringPair key_pair;
    key_pair.key = (char*)strdup(key);
    if (key_pair.key == NULL)
    {
        return 0;
    }
    key_pair.val = NULL;

    const TStringPair* pkey_pair = &key_pair;

    TGAElem* p = (TGAElem*)bsearch(&pkey_pair, base, grar_get_size(grar),
        sizeof(TGAElem), string_pair_compare);

    free(key_pair.key);

    int res = 1;
    if (p == NULL)
    {
        // There is no element with the specified key in the map,
        // add a new one.
        res = smap_add_element(smap, key, val);
    }
    else
    {
        // Found an element with the key given, now change its value
        TStringPair* sp = (TStringPair*)*p;
        
        assert(sp != NULL);
        assert(sp->key != NULL);
        assert(sp->val != NULL);
        assert(strcmp(sp->key, key) == 0);
        
        char* new_val = (char*)strdup(val);
        if (new_val == NULL)
        {
            return 0;
        }
        
        free(sp->val);
        sp->val = new_val;
    }
    
    return res;
}

int
smap_update(CStringMap* smap, CStringMap* upd)
{
    assert(smap != NULL);
    assert(upd != NULL);
    assert(smap != upd);

    // if the containers of the string maps are not sorted, sort them
    CGrowingArray* ga_old = &(smap->container);
    if (smap->sorted == 0)
    {
        grar_sort(ga_old, string_pair_compare_sort);
        smap->sorted = 1;
    }
    
    CGrowingArray* ga_upd = &(upd->container);
    if (upd->sorted == 0)
    {
        grar_sort(ga_upd, string_pair_compare_sort);
        upd->sorted = 1;
    }
    
    size_t i = 0;
    size_t k = 0;
    size_t old_size = grar_get_size(ga_old);
    size_t upd_size = grar_get_size(ga_upd);
    
    if (upd_size == 0)
    {
        // nothing to do
        return 1;
    }
    
    if (old_size == 0)
    {
        // 'smap' is empty - just move the elements from 'upd' there
        grar_swap(ga_old, ga_upd);
        return 1;
    }
    
    // a temporary array to store the results
    CGrowingArray ga_res;
    if (!grar_create(&ga_res))
    {
        return 0;
    }
    
    // reserve the space to avoid reallocations when the elements are added
    if (!grar_reserve(&ga_res, old_size + upd_size))
    {
        grar_destroy(&ga_res);
        return 0;
    }
    
    TStringPair** c_old = grar_get_c_array(ga_old, TStringPair*);
    TStringPair** c_upd = grar_get_c_array(ga_upd, TStringPair*);
    
    // while something remains to be processed in 'upd' array...
    while (k < upd_size)
    {
        assert(i >= old_size || (c_old != NULL && c_old[i] != NULL));
        assert(k >= upd_size || (c_upd != NULL && c_upd[k] != NULL));
        
        // if some elements from 'ga_old' come before the current element from 
        // 'ga_upd', move these elements to the resulting array
        while (i < old_size && string_pair_compare(&c_old[i], &c_upd[k]) < 0)
        {
            if (!grar_add_element(&ga_res, c_old[i]))
            {
                grar_destroy(&ga_res);
                return 0;
            }
            c_old[i] = NULL; // ownership has been transfered to 'ga_res'
            ++i;
        }
        
        // the elements from 'ga_old' that have the same key as the elements 
        // from 'ga_upd', must be replaced
        while (i < old_size && string_pair_compare(&c_old[i], &c_upd[k]) == 0)
        {
            string_pair_dtor(c_old[i], NULL);
            c_old[i] = NULL;
            ++i;
        }
        
        TStringPair* sp = c_upd[k];
        while (k < upd_size && string_pair_compare(&c_upd[k], &sp) == 0)
        {
            if (!grar_add_element(&ga_res, c_upd[k]))
            {
                grar_destroy(&ga_res);
                return 0;
            }
            c_upd[k] = NULL; // ownership has been transfered to 'ga_res'
            ++k;
        }
        
        // if any of the arrays is processed completely, move the remaining 
        // elements from the other one to 'ga_res'
        if (k == upd_size)
        {
            while (i < old_size)
            {
                if (!grar_add_element(&ga_res, c_old[i]))
                {
                    grar_destroy(&ga_res);
                    return 0;
                }
                c_old[i] = NULL; // ownership has been transfered to 'ga_res'
                ++i;
            }
        }
        else if (i == old_size)
        {
            while (k < upd_size)
            {
                if (!grar_add_element(&ga_res, c_upd[k]))
                {
                    grar_destroy(&ga_res);
                    return 0;
                }
                c_upd[k] = NULL; // ownership has been transfered to 'ga_res'
                ++k;
            }
        } // end if (k == upd_size)
    } // end while (k < upd_size)
    
    // all elements of 'ga_old' and 'ga_upd' are now NULLs
    grar_clear(ga_old);
    grar_clear(ga_upd);
    
    // put the results into 'smap' (the container remains sorted)
    grar_swap(ga_old, &ga_res);
    
    // the elements are now owned by 'ga_old' and, hence, by 'smap',
    // so we need to destroy only the array itself
    grar_destroy(&ga_res);
    return 1;
}

