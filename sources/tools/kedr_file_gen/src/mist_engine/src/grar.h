/// grar.h
/// Definitions for growing array manipulation facilities.

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

#ifndef GRAR_H_2219_INCLUDED
#define GRAR_H_2219_INCLUDED

/// Structures and functions for primitive dynamic arrays of void* pointers
/// that can only grow.

/// It is guaranteed that the elements are stored in a contiguous memory block.
/// Pointers to the elements (void**, actually) may be invalidated as the elements 
/// are added: memory reallocation may occur.

/// DO NOT use the fields of these data structures directly, call accessor
/// functions instead.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/// default array's capacity
#define GRAR_DEFAULT_CAPACITY 16

/// default capacity increase factor
#define GRAR_CAP_INC_FACTOR 2

typedef void* TGAElem;
typedef struct CGrowingArray_
{
    /// Here the data is stored. This pointer may change as the elements are added.
    TGAElem* data;  
    
    /// Current number of elements (size).
    size_t size;
    
    /// Maximum number of elements available without reallocation (capacity)
    size_t capacity;
} CGrowingArray;

/////////////////////////////////////////////////////////////////////////////
//  Methods 
/////////////////////////////////////////////////////////////////////////////

/// Unless specifically stated, if a function takes pointer as an argument 
/// and NULL is passed to it as the value of this argument, the behaviour of the 
/// function is undefined.

/// Creates a new array (initializes the structure)
/// On creation size is 0, capacity is GRAR_DEFAULT_CAPACITY,
/// appropriate amount of memory is allocated.
/// Returns nonzero if successful, 0 if it is unable to allocate memory for the
/// array.
/// If the operation fails, the contents of the argument remain unchanged.
int 
grar_create(CGrowingArray* grar);

/// Destroys the array (releases the memory it occupies) but does not free memory
/// the elements point to.
/// After this operation the user may initialize the array again with grar_create.
void
grar_destroy(CGrowingArray* grar);

/// If current capacity is less than the one specified as an argument,
/// increase the capacity (and reallocate the elements), so as new capacity is 
/// greater than or equal to the specified value.
/// Returns nonzero if no error has occured (even if no capacity increase was 
/// necessary), 0 otherwise.
int 
grar_reserve(CGrowingArray* grar, size_t new_min_capacity);

/// Type of destructor functions for the elements. These functions take the element
/// to be destroyed as the 1st parameter and a pointer to some additional data 
/// the caller wants to pass (user_data) as the second one. 
/// The destructor function is called by grar_destroy_with_elements function 
/// for each element of the array. grar_destroy_with_elements passes its user_data
/// argument as the second argument of the destructor function.
typedef void (*TDestructorFunc)(TGAElem, void*);

/// Destroys the array with its elements: if dtor is not NULL, the function calls
/// specified destructor function (dtor) for each element. user_data is passed to
/// the destructor function as the second parameter.
/// If dtor is NULL, the function just calls free() for each element.
/// After destruction of the elements the function calls grar_destroy() to destroy 
/// the array itself.
void
grar_destroy_with_elements(CGrowingArray* grar, TDestructorFunc dtor, void* user_data);

/// Returns current size of the array (number of elements in it).
#define grar_get_size(grar_) ((grar_)->size)

/// Appends the specified element to the array, expanding the latter if necessary.
/// Returns 0 in case of failure, nonzero otherwise.
/// The function either succeeds or has no effect.
int
grar_add_element(CGrowingArray* grar, TGAElem elem);

/// Appends a copy of 'src' array to the 'grar' array.
/// Both arrays passed to this function must be initialized.
/// Returns 0 in case of failure, nonzero otherwise.
int
grar_append_array(CGrowingArray* grar, const CGrowingArray* src);

/// Returns the element with the specified index cast to the specified type 
/// (usually some kind of pointer). If the index is out of range, the behaviour
/// is undefined.
#define grar_get_element(grar_, type_, index_) ((type_)((grar_)->data[(index_)]))

/// Returns a pointer to the contents of the array, cast to appropriate type (type_*).
/// This allows manipulating the array as if it was an ordinary C array.
/// The returned pointer may become invalid if new elements are added to the array
/// after calling grar_get_c_array().
#define grar_get_c_array(grar_, type_) ((type_*)((grar_)->data))

/// Clears the array: sets its size to 0 to make it look empty but preserves
/// capacity. The function does not perform memory reallocation, so the pointer
/// to the beginning of the array remains the same. It also does not call  
/// free() for the pointers contained in the array. 
void
grar_clear(CGrowingArray* grar);

/// Type of the comparator functions to be used for sorting growing arrays
/// and searching for values in them (qsort-style function).
/// Both arguments are actually of type (const TGAElem*). 
/// For example, if the array contains pointers to strings, the arguments 
/// of the comparator should be cast to (const char**) inside the latter.
typedef int (*TCompareFunc)(const void*, const void*);

/// A wrapper around qsort() used to sort the contents of a growing array
/// in ascending order using the specified comparator.
void
grar_sort(CGrowingArray* grar, TCompareFunc cf);

/// This function looks for a specified element (key) in the array.
/// The comparator function is used like in bsearch() function.
/// If an appropriate element is found, the function returns its index
/// (unlike bsearch() that returns a pointer to the found element).
/// -1 is returned if the element is not found.
/// If there are two or more elements 'equivalent' to key (in terms of the
/// specified comparator)in the array, it is unspecified, the index of which
/// is returned.
/// The grar should be sorted in ascending order using the same comparator
/// before calling this function.
int
grar_find(const CGrowingArray* grar, const void* key, TCompareFunc cf);

/// Swap the contents of two CGrowingArray structures.
void
grar_swap(CGrowingArray* lhs, CGrowingArray* rhs);

///////////////////////////////////////////////////////////////////////////
/// Functions for the arrays of strings (i.e., of char* pointers).
/// If the arrays passed to these functions as arguments contain something
/// other than char* pointers, the behaviour of the functions is undefined.

/// Sorts an array in a lexicographical order. 
/// Note that strcmp() is used to compare the strings, so do not rely on this
/// function to properly arrange the strings containing characters above 0x7F.
/// Anyway, if the array is being sorted only for later searching through
/// with grar_string_find(), this function is still enough even in case of 
/// such strings.
void
grar_string_sort(CGrowingArray* grar);

/// Searches for a string ('skey') in a sorted array of strings, returns its
/// index if found, -1 otherwise.  
/// If there are two or more strings equal to 'skey' in the array, it is 
/// unspecified, the index of which is returned.
/// The array should be sorted using grar_string_sort() before calling this
/// function.
/// Note that this function receives (const char*) rather than (const char**)
/// like it would be for grar_find() function.
int 
grar_string_find(const CGrowingArray* grar, const char* skey);

/// Computes total length of the strings, the pointers to which are stored in 
/// the array. For example, if you want to concatenate all the strings
/// from array 'ar', you need (grar_string_total_length(ar) + 1) bytes of memory
/// for the output string.
/// If the array contains something other than (char*) pointers, the behaviour
/// is undefined.
/// If the array is empty, the function returns 0 as it should.
size_t
grar_string_total_length(CGrowingArray* grar);


#ifdef __cplusplus
}
#endif

#endif // GRAR_H_2219_INCLUDED
