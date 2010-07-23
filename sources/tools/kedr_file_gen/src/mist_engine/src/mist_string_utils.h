/// mist_file_utils.h
/// Minimal String Template engine (MiST):
/// definitions of various string utility functions.

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

#ifndef MIST_STRING_UTILS_H_2227_INCLUDED
#define MIST_STRING_UTILS_H_2227_INCLUDED

#include "mist_errors.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Max number of digits to allocate memory for 
/// (enough even for an unsigned 64-bit integer).
#define MAX_NUM_DIGITS 22 

///////////////////////////////////////////////////////////////////////////
/// CMistString - a simple string structure.
/// Its methods encapsulate memory (re)allocation required for concatenation,
/// copying, etc.
/// This can also be useful for tokenizing as well as for other sequential 
/// processing of a string.
/// 
/// Unless specifically stated, if NULL pointer is passed to any of 
/// CMistString functions, the behaviour of the function is undefined.
/// If a pointer to uninitialized CMistString structure us passed to a 
/// function except mist_string_create(), the behaviour of the function
/// is also undefined.
///////////////////////////////////////////////////////////////////////////

/// CMistString structure.
/// The 'buf' field always contains the address of the memory buffer, 
/// containing the string and should not be changed explicitly (same for 'bsize').
/// 'str' points somewhere inside the buffer but not necessarily to its first 
/// byte (invariant: buf <= str && str < buf + bsize). 
/// 
/// Only 'str' field of this structure can be changed explicitly,
/// for example in the expressions like ms.str = trim_whitespace(ms.str).
/// Make sure it always points inside the buffer.
/// 
/// Note that if you save the value of 'str', it may be invalidated by the 
/// subsequent calls of CMistString methods.
typedef struct CMistString_
{
    /// A pointer to the string (points somewhere inside the buffer).
    char*  str; 

    /// A buffer containing the string and may be some other data.
    char*  buf; 

    /// Size of the buffer (in bytes).
    size_t bsize; 
} CMistString;

/// Create a new CMistString in the dynamic memory, initialize with
/// a copy of string 'init'. If init == NULL, the function initializes
/// the CMistString with an empty string (as if 'init' pointed to "").
/// Return a pointer to the newly created CMistString or NULL in case
/// of error (out of memory).
CMistString*
mist_string_create(const char* init);

/// Like mist_string_create() except that the string is initialized with
/// a copy of the string defined by [beg, end) range. If beg == end, the
/// function will initialize CMistString with an empty string.
CMistString*
mist_string_create_from_range(const char* beg, const char* end);

/// Destroy the CMistString: free the buffer and then free memory occupied
/// by the structure itself.
void
mist_string_destroy(CMistString* ms);

/// If 'size' + 1 is greater than the current size of the buffer in 'ms', 
/// the function enlarges (reallocates) the buffer to hold at least 'size' + 1
/// characters and sets 'ms.str' to point to the corresponding position in the 
/// new buffer (same offset as in the old buffer).
/// If there is not enough memory to expand 
/// the buffer, the function returns MIST_OUT_OF_MEMORY and leaves the contents 
/// of the CMistString unchanged.
EMistErrorCode
mist_string_reserve(CMistString* ms, size_t size);

/// Reset the string: sets 'str' to point to the beginning of the buffer.
/// and set "" as the string's value.
/// The buffer is not freed or reallocated by the function.
void
mist_string_reset(CMistString* ms);

/// Set the string's value to 'src', expanding the buffer if necessary.
/// Return MIST_OK if successful. If there is not enough memory to expand 
/// the buffer, the function returns MIST_OUT_OF_MEMORY and leaves the contents 
/// of the CMistString unchanged.
EMistErrorCode
mist_string_set(CMistString* ms, const char* src);

/// Append a copy of the string contained in 'ms_what' to the string in 
/// 'ms_to' (pointed to ms_what->str and ms_to->str, respectively). 
/// The string buffer in 'ms_to' will be expanded as necessary. 
/// The function returns MIST_OK if successful. 
/// If there is not enough memory to expand the buffer, the function returns 
/// MIST_OUT_OF_MEMORY and leaves the contents of 'ms_to' unchanged.
EMistErrorCode 
mist_string_append(CMistString* ms_to, const CMistString* ms_what);

/// Like mist_string_append except that it appends a copy of the string
/// defined by [beg, end) range.
EMistErrorCode 
mist_string_append_range(CMistString* ms_to, const char* beg, const char* end);

/// Create a new string that contains the result of concatenation of 
/// the strings controlled by 'ms_left' and 'ms_right' (in that order).
/// The function returns a pointer to the new string or NULL if there is not
/// enough memory to complete the operation.
CMistString* 
mist_string_sum(const CMistString* ms_left, const CMistString* ms_right);

/// Remove leading and trailing whitespace characters (' ', '\t', '\n', '\r')
/// from the string controlled by 'ms' (accessed via ms->str).
/// This function does not copy the string or reallocate memory, it modifies
/// the string in place.
void 
mist_string_trim(CMistString* ms);

/// Swap the contents of two CMistString structures.
void
mist_string_swap(CMistString* lhs, CMistString* rhs);

/// Replace each occurrence of 'what' substring in 'ms' with 'with' substring.
/// 'what' mustn't be an empty string but 'with' can be.
/// The memory occupied by the string in 'ms' will be expanded if necessary.
/// The function returns MIST_OUT_OF_MEMORY if there is not enough memory
/// to perform the operation.
EMistErrorCode
mist_string_replace(CMistString* ms, const char* what, const char* with);

/// Replace each occurence of '\\'+'t', '\\'+'n', '\\'+'r' and '\\'+'\\' sequences in 'ms'
/// with their unescaped representations ('\t', '\n', '\r' and '\\').
EMistErrorCode
mist_string_unescape(CMistString* ms);

/// Detach the C-string contained in the CMistString structure.
/// The function returns the pointer to the buffer containing the string.
/// The string is copied to the beginning of the buffer if it is not there
/// already. CMistString structure is then destroyed. The caller should free
/// the returned string (using plain free()) when it is no longer needed.
char*
mist_string_detach(CMistString* ms);

///////////////////////////////////////////////////////////////////////////
// Symbol finders (find_XXX_of)
///////////////////////////////////////////////////////////////////////////

/// Look through string 'str' for the FIRST occurence of any character FROM 
/// 'syms' string. 
/// 'syms' must not be empty.
/// 'nsyms' is the number of the first characters of 'syms' to consider.
/// Return a pointer to this character in 'str' if found, NULL otherwise.
const char*
mist_find_first_of(const char* str, const char* syms, size_t nsyms);

/// Look through string 'str' for the FIRST occurence of any character that is
/// NOT PRESENT in 'syms' string. 
/// 'syms' must not be empty.
/// 'nsyms' is the number of the first characters of 'syms' to consider.
/// Return a pointer to this character in 'str' if found, NULL otherwise.
const char*
mist_find_first_not_of(const char* str, const char* syms, size_t nsyms);

/// Look through string 'str' for the LAST occurence of any character that is
/// NOT PRESENT in 'syms' string. 
/// 'syms' must not be empty.
/// 'nsyms' is the number of the first characters of 'syms' to consider.
/// Return a pointer to this character in 'str' if found, NULL otherwise.
const char*
mist_find_last_not_of(const char* str, const char* syms, size_t nsyms);

///////////////////////////////////////////////////////////////////////////
/// If a function takes 'beg' and 'end' pointers (usually const char*), it 
/// operates on a string denoted by them. 'beg' points to the beginning of 
/// the string, 'end' - at the first character past the part to be processed.
/// If the whole string is to be processed, 'end' points to the terminating
/// null character.
/// If beg == end, the string is considered empty. Unless specifically stated,
/// this string is allowed to be processed by the function.
/// If 'beg' and / or 'end' are NULL or if they do not point to the same string
/// (end < beg || end > beg + strlen(beg)), 
/// the behaviour of the function is undefined.
///////////////////////////////////////////////////////////////////////////

/// Look through [beg, end) range for the FIRST occurence of any character FROM 
/// 'syms' string. 
/// 'syms' must not be empty.
/// 'nsyms' is the number of the first characters of 'syms' to consider.
/// Return a pointer to the character if found, 'end' otherwise.
const char*
mist_find_in_range_first_of(const char* beg, const char* end, 
    const char* syms, size_t nsyms);

/// Look through [beg, end) range for the FIRST occurence of any character that is
/// NOT PRESENT in 'syms' string. 
/// 'syms' must not be empty.
/// 'nsyms' is the number of the first characters of 'syms' to consider.
/// Return a pointer to the character if found, 'end' otherwise.
const char*
mist_find_in_range_first_not_of(const char* beg, const char* end, 
    const char* syms, size_t nsyms);

/// Look through [beg, end) range for the LAST occurence of any character that is
/// NOT PRESENT in 'syms' string. 
/// 'syms' must not be empty.
/// 'nsyms' is the number of the first characters of 'syms' to consider.
/// Return a pointer to the character if found, 'end' otherwise.
const char*
mist_find_in_range_last_not_of(const char* beg, const char* end, 
    const char* syms, size_t nsyms);

///////////////////////////////////////////////////////////////////////////
// Other string manipulation functions
///////////////////////////////////////////////////////////////////////////

/// Return a copy of the substring specified by [beg, end) or NULL if there is
/// not enough memory to perform this operation.
char*
mist_get_substring(const char* beg, const char* end);

/// Return nonzero if the specified name of an entity (file, directory, template,
/// placeholder, etc.) contains characters that are not allowed, begins with 
/// a space or a dot. Otherwise 0 is returned.
/// Only the following characters are allowed in the name:
/// - latin letters ('A'..'Z', 'a'..'z'), codes 0x41-0x5A, 0x61-0x7A;
/// - digits ('0'..'9'), codes 0x30-0x39;
/// - space (' '), code 0x20;
/// - hyphen ('-'), code 0x2D;
/// - dot ('.'), code 0x2E;
/// - underscore ('_'), code 0x5F.

int
mist_name_is_bad(const char* name);

/// If 'error_descr' is not NULL, the function allocates appropriate
/// amount of memory for *error_descr string and outputs the description of
/// the error there using sprintf(). 'fmt' is a format string for sprintf(),
/// If 'ln' is not zero, it is considered a line number in the data being
/// parsed where the error was detected. 'fmt' must have an appropriate 
/// format specifier for it ("%u" or the like). 
/// If 'ln' is 0, it is ignored and 'fmt' is considered an ordinary string
/// without any format specifiers and output as it is.
/// If 'error_descr' is NULL, the function does nothing.
void
mist_format_parse_error(char** error_descr, const char* fmt, unsigned int ln);

/// Returns the line number in string 'src' of the character 'ptr' points to
/// (number of '\n' characters  before the character plus 1)
/// The caller must ensure that 'ptr' points to a character of string 'str'
/// rather than somewhere else.
/// 'src' and 'ptr' mustn't be NULL.
unsigned int 
mist_line_num_for_ptr(const char* src, const char* ptr);

/// Return nonzero if the character of the string 'src' pointed to by 'pos'
/// is escaped, 0 otherwise. A character is considered escaped if there is a
/// slash (\) before it and, in turn, this slash is not escaped.
/// If 'pos' does not point anywhere inside 'src' string, the behaviour of the 
/// function is undefined.
int
mist_is_char_escaped(const char* pos, const char* src);

/// Traverse [beg, end) range and replace each character listed in 'syms' with
/// space (' ').
/// 'nsyms' is the number of the first characters of 'syms' to consider.
/// This function can be used to remove some special characters from a string.
void
mist_chars_to_spaces(char* beg, char* end, const char* syms, size_t nsyms);

#ifdef __cplusplus
}
#endif

#endif // MIST_STRING_UTILS_H_2227_INCLUDED
