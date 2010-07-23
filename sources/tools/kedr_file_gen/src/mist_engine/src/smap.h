/// smap.h
/// Definitions for simple string map manipulation facilities.

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

#ifndef SMAP_H_1333_INCLUDED
#define SMAP_H_1333_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////
/// A string map is an array of pairs <key, value>, where key and value are
/// strings (char* pointers).
typedef struct TStringPair_ 
{
    char* key;
    char* val;
} TStringPair;

/// CStringMap_ structure definition is private, the fields of this structure
/// should not be accessed directly.
struct CStringMap_; 
typedef struct CStringMap_ CStringMap;

/////////////////////////////////////////////////////////////////////////////
//  Methods 
/////////////////////////////////////////////////////////////////////////////

/// Creates an empty string map on the heap and returns a pointer to it.
/// NULL is returned if the function is unable to allocate memory for the
/// map.
CStringMap*
smap_create();

/// Appends an element (TStringPair) to the string map. A 'key' field of this 
/// element will contain a copy of 'key' argument, 'value' - a copy of 'val'
/// argument respectively.
/// Returns nonzero if successful, 0 otherwise (usually - out of memory).
/// The function does not check if an element with the same key already exists
/// in this map.
/// The function either succeeds or has no effect.
int
smap_add_element(CStringMap* smap, const char* key, const char* val);

/// Clears the string map (makes it empty). All the elements it contains
/// will be freed.
void 
smap_clear(CStringMap* smap);

/// Destroys the string map (the strings contained in it will be freed by 
/// this function as well)
void
smap_destroy(CStringMap* smap);

/// Returns the value string corresponding to key 'skey' in the string map.
/// If there is no element with specified key in the string map, NULL will
/// be returned.
/// If there are several elements with identical keys, it is unspecified
/// which of the corresponding values will be returned.
/// The elements of the map may be reshuffled in the process.
/// The returned string is owned by the map and will be freed automatically
/// when the map is destroyed.
const char* 
smap_lookup(CStringMap* smap, char* skey);

/// Returns the number of (key, value) pairs contained in the string map
size_t
smap_get_size(const CStringMap* smap);

/// Returns the contents of the string map as a C-array of pointers to the
/// (key, value) pairs (see TStringPair). The returned array is owned by the 
/// string map and should not be modified or freed.
/// This function (as well as smap_get_size()) may be helpful if one wants to 
/// traverse the string map.
TStringPair** 
smap_as_array(CStringMap* smap);

/// Set the value of an element with the specified key to a copy of 'val'. 
/// If there is already an element with the specified key, its value will be free'd
/// and a copy of 'val' will be set as its new value.
/// If there is no element with this key, the function is equivalent to 
/// smap_add_element(smap, key, val)
/// 
/// If there are two or more elements with key 'key', it is unspecified which one
/// will be updated. Generally you should avoid calling this function in such 
/// situations. Use smap_check_duplicate_keys() to check for duplicate keys.
///
/// The function returns nonzero if successful, 0 otherwise (typically, if there is 
/// not enough memory).
/// The function either succeeds or has no effect.
/// 
/// If adding a new element, this function might be slower than smap_add_element().
/// If performance is critical for this part of the application, consider using
/// smap_add_element() instead.
int
smap_set_value(CStringMap* smap, const char* key, const char* val);

/// Check if there are elements with the same keys in the map.
/// If there are such elements, the function returns the key of one of them
/// (it is unspecified, which one). Otherwise the function returns NULL.
/// The returned string is owned by the map, so it mustn't be modified or free'd
/// be the caller.
/// The function may rearrange the elements of the map.
const char*
smap_check_duplicate_keys(CStringMap* smap);

/// Update the set of elements in 'smap' with those provided in 'upd'.
/// For each element in 'upd', if there is no element with the same key in 
/// 'smap', it will be added to 'smap'. Otherwise, all the elements with
/// this key will be destroyed in 'smap' and the elements with this key from
/// 'upd' will be added to 'smap'.
/// The elements of 'smap' not present in 'upd' will be left unchanged.
///
/// Note that 'upd' will be emptied in the process. The strings it contained 
/// will now be owned by 'smap'. The function will not destroy 'upd', just 
/// empty it.
///
/// Empty string maps can be passed as 'smap' or 'upd' (or both) to this 
/// function.
/// 
/// Passing the same pointer for the both arguments may result in undefined 
/// behaviour.
///
/// The function returns nonzero if successful, 0 otherwise (typically, if there is 
/// not enough memory).
/// If the function fails, 'smap' and 'upd' may become unusable. It is only 
/// safe to call smap_destroy() for them in this case.
int
smap_update(CStringMap* smap, CStringMap* upd);

#ifdef __cplusplus
}
#endif

#endif // SMAP_H_1333_INCLUDED
