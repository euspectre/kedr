/// mist_file_utils.c 
/// Implementation of Minimal String Template engine (MiST) API: 
/// string utility functions.

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <assert.h>

#include "mist_string_utils.h"

// Initial size of a string buffer
#define MIST_INIT_BUFFER_SIZE 16

// An array indicating which characters are allowed in the names of the entities.
// Such characters are marked with '1'.
static char ch_allowed[128] = {
/*  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */  
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x00-0x0F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10-0x1F */
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, /* 0x20-0x2F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, /* 0x30-0x3F */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40-0x4F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, /* 0x50-0x5F */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60-0x6F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 0x70-0x7F */
};

///////////////////////////////////////////////////////////////////////////
// Implementation: "public methods"
///////////////////////////////////////////////////////////////////////////
CMistString*
mist_string_create_from_range(const char* beg, const char* end)
{
    assert(beg != NULL);
    assert(end != NULL);
    assert(beg <= end);
    
    CMistString* ms = (CMistString*)malloc(sizeof(CMistString));
    
    if (ms == NULL)
    {
        return NULL;
    }

    size_t len = end - beg;

    // length of init + 1, rounded up to a multiple of MIST_INIT_BUFFER_SIZE
    // [NB] To round up a nonnegative number x to a multiple of N, use
    // (x + N - 1) & ~(N - 1)
    ms->bsize = (len + MIST_INIT_BUFFER_SIZE) & ~(MIST_INIT_BUFFER_SIZE - 1);

    ms->buf = (char*)malloc(ms->bsize);
    if (ms->buf == NULL)
    {
        free(ms);
        return NULL;
    }

    ms->str = ms->buf;

    char* pos = ms->str;
    if (len > 0)
    {
        strncpy(pos, beg, len);
        pos += len;
    }
    *pos = '\0';

    return ms;
}

CMistString*
mist_string_create(const char* init)
{
    if (init == NULL)
    {
        init = "";
    }
    
    return mist_string_create_from_range(init, init + strlen(init));
}

void
mist_string_destroy(CMistString* ms)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);

    free(ms->buf);
    free(ms);
}

EMistErrorCode
mist_string_reserve(CMistString* ms, size_t size)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);
    assert((ms->str >= ms->buf) && (ms->str < ms->buf + ms->bsize));
    
    size_t new_size = size + 1;

    if (new_size > ms->bsize)
    {
        size_t offs = ms->str - ms->buf;
        char* p = (char*)realloc(ms->buf, new_size);

        if (p == NULL)
        {
            return MIST_OUT_OF_MEMORY;
        }

        ms->bsize = new_size;
        ms->buf = p;
        ms->str = ms->buf + offs;
    }

    return MIST_OK;
}

void
mist_string_reset(CMistString* ms)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);
    assert((ms->str >= ms->buf) && (ms->str < ms->buf + ms->bsize));

    ms->str = ms->buf;
    *(ms->str) = 0;

    return;
}

EMistErrorCode
mist_string_set(CMistString* ms, const char* src)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);
    assert((ms->str >= ms->buf) && (ms->str < ms->buf + ms->bsize));

    assert(src != NULL);

    size_t new_size = strlen(src) + 1;

    EMistErrorCode ec = mist_string_reserve(ms, new_size);
    if (ec != MIST_OK)
    {
        return ec;
    }

    ms->str = strcpy(ms->buf, src);
    
    return MIST_OK;
}

EMistErrorCode 
mist_string_append(CMistString* ms_to, const CMistString* ms_what)
{
    assert(ms_to != NULL);
    assert(ms_to->buf != NULL);
    assert((ms_to->str >= ms_to->buf) && 
           (ms_to->str < ms_to->buf + ms_to->bsize));

    assert(ms_what != NULL);
    assert(ms_what->buf != NULL);
    assert((ms_what->str >= ms_what->buf) && 
           (ms_what->str < ms_what->buf + ms_what->bsize));

    EMistErrorCode ec = mist_string_reserve(ms_to, 
        (ms_to->str - ms_to->buf) + strlen(ms_to->str) + strlen(ms_what->str) + 1);

    if (ec != MIST_OK)
    {
        return ec;
    }

    strcat(ms_to->str, ms_what->str);

    return MIST_OK;
}

EMistErrorCode 
mist_string_append_range(CMistString* ms_to, const char* beg, const char* end)
{
    assert(ms_to != NULL);
    assert(ms_to->buf != NULL);
    assert((ms_to->str >= ms_to->buf) && 
           (ms_to->str < ms_to->buf + ms_to->bsize));
    
    assert(beg != NULL);
    assert(end != NULL);
    assert(beg <= end);
    
    size_t len = end - beg;
    
    EMistErrorCode ec = mist_string_reserve(ms_to, 
        (ms_to->str - ms_to->buf) + strlen(ms_to->str) + len + 1);

    if (ec != MIST_OK)
    {
        return ec;
    }
    
    if (len > 0)
    {
        strncat(ms_to->str, beg, len);
    }

    return MIST_OK;
    
}

CMistString* 
mist_string_sum(const CMistString* ms_left, const CMistString* ms_right)
{
    assert(ms_left != NULL);
    assert(ms_left->buf != NULL);
    assert((ms_left->str >= ms_left->buf) && 
        (ms_left->str < ms_left->buf + ms_left->bsize));

    assert(ms_right != NULL);
    assert(ms_right->buf != NULL);
    assert((ms_right->str >= ms_right->buf) && 
        (ms_right->str < ms_right->buf + ms_right->bsize));

    CMistString* result = mist_string_create(NULL);
    if (result == NULL)
    {
        return NULL;
    }

    EMistErrorCode ec = mist_string_reserve(result,
        strlen(ms_left->str) + strlen(ms_right->str) + 1);

    if (ec != MIST_OK)
    {
        mist_string_destroy(result);
        return NULL;
    }
    
    ec = mist_string_set(result, ms_left->str);
    if (ec != MIST_OK)
    {
        mist_string_destroy(result);
        return NULL;
    }

    ec = mist_string_append(result, ms_right);
    if (ec != MIST_OK)
    {
        mist_string_destroy(result);
        return NULL;
    }

    return result;
}

void 
mist_string_trim(CMistString* ms)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);
    assert((ms->str >= ms->buf) && (ms->str < ms->buf + ms->bsize));

    static const char seps[] = " \t\n\r";
    static size_t sz_seps = sizeof(seps) / sizeof(seps[0]) - 1;

    size_t len = strlen(ms->str);
    if (len == 0)
    { // empty string, nothing to do
        return;
    }
    
    const char* end = ms->str + len;
    const char* beg_new = mist_find_in_range_first_not_of(ms->str, end, seps, sz_seps);
    if (beg_new == end)
    { // the string consists of whitespace chars only
        ms->str[0] = 0;
    }
    else
    {
        const char* end_new = mist_find_in_range_last_not_of(ms->str, end, seps, sz_seps);
        if (end_new != end)
        {
            ++end_new;
            assert(end_new < ms->buf + ms->bsize);
            assert(end_new > beg_new);

            ms->str[end_new - ms->str] = '\0';
        }

        ms->str = &(ms->str[beg_new - ms->str]);
    }

    return;
}

void
mist_string_swap(CMistString* lhs, CMistString* rhs)
{
    assert(lhs != NULL);
    assert(lhs->buf != NULL);
    assert((lhs->str >= lhs->buf) && (lhs->str < lhs->buf + lhs->bsize));
    
    assert(rhs != NULL);
    assert(rhs->buf != NULL);
    assert((rhs->str >= rhs->buf) && (rhs->str < rhs->buf + rhs->bsize));
    
    char* tstr;
    tstr = lhs->buf;
    lhs->buf = rhs->buf;
    rhs->buf = tstr;
    
    tstr = lhs->str;
    lhs->str = rhs->str;
    rhs->str = tstr;
    
    size_t tsz;
    tsz = lhs->bsize;
    lhs->bsize = rhs->bsize;
    rhs->bsize = tsz;
    
    return;
}

EMistErrorCode
mist_string_replace(CMistString* ms, const char* what, const char* with)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);
    assert((ms->str >= ms->buf) && (ms->str < ms->buf + ms->bsize));
    
    assert(what != NULL);
    assert(what[0] != '\0');
    assert(with != NULL);
    
    size_t len = strlen(ms->str);
    size_t what_len = strlen(what);
    size_t with_len = strlen(with);
    
    // count the occurencies of 'what' substring
    size_t c = 0;
    char* pos = strstr(ms->str, what);
    while (pos != NULL)
    {
        assert((pos >= ms->str) && (pos < ms->buf + ms->bsize));
        ++c;
        pos = strstr(pos + what_len, what);
    }
    
    if (c == 0) // no occurence of 'what' - nothing to do
    {
        return MIST_OK;
    }
    
    CMistString* ts = mist_string_create(NULL);
    if (ts == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    EMistErrorCode ec = mist_string_reserve(ts, len - c * what_len + c * with_len);
    if (ec != MIST_OK)
    {
        mist_string_destroy(ts);
        return ec;
    }
    
    pos = strstr(ms->str, what);
    while (pos != NULL)
    {
        size_t offs = pos - ms->str;
        if (offs != 0)
        {
            strncpy(ts->str, ms->str, offs);
            ts->str += offs;
        }
        
        if (with_len != 0)
        {
            strncpy(ts->str, with, with_len);
            ts->str += with_len;
        }
        
        ms->str = pos + what_len;
        pos = strstr(ms->str, what);
    }
    
    // copy the remainder of the string (if present)
    len = strlen(ms->str);
    if (len != 0)
    {
        strncpy(ts->str, ms->str, len);
        ts->str += len;
    }
    
    assert((ts->str >= ts->buf) && (ts->str < ts->buf + ts->bsize));
    
    *(ts->str) = '\0';
    ts->str = ts->buf;
    
    mist_string_swap(ms, ts);
    mist_string_destroy(ts);
    return MIST_OK;
}
EMistErrorCode
mist_string_unescape(CMistString* ms)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);
    assert((ms->str >= ms->buf) && (ms->str < ms->buf + ms->bsize));
    
    static const char ch_what[] = "tnr";
    static const char ch_to[]   = "\t\n\r";
    static const size_t nchars = sizeof(ch_what) / sizeof(ch_what[0]);
    assert(sizeof(ch_what) == sizeof(ch_to));
    
    size_t len = strlen(ms->str);
    if (len < 2)
    {
        // nothing to do
        return MIST_OK;
    }
    
    CMistString* res = mist_string_create("");
    if (res == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    EMistErrorCode ec;
    ec = mist_string_reserve(res, strlen(ms->str));
    if (ec != MIST_OK)
    {
        return ec;
    }
    
    const char* beg = ms->str;
    const char* end = beg + len;
    const char* pos = beg;
    size_t dist;
    
    while (pos < end)
    {
        if (*pos == '\\')
        {
            dist = pos - beg;
            if (dist > 0)
            {
                strncpy(res->str, beg, dist);
                res->str += dist;
            }
            
            ++pos;
            if (pos == end)
            {
                *(res->str) = '\\';
                ++(res->str);
                beg = pos;
                break;
            }
            
            if (*pos == '\\')
            {
                *(res->str) = '\\';
                ++(res->str);
                ++pos;
                beg = pos;
            }
            else
            {
                // check if the next character is special...
                size_t i;
                for (i = 0; i < nchars; ++i)
                {
                    if (*pos == ch_what[i])
                    {
                        break;
                    }
                }
                
                // ...if so, unescape it
                if (i < nchars)
                {
                    *(res->str) = ch_to[i];
                    ++(res->str);
                    ++pos;
                    beg = pos;
                }
                else // unknown escape sequence, leave it unchanged
                {
                    *(res->str) = '\\';
                    ++(res->str);
                    beg = pos;
                }
            }
        }
        else
        {
            ++pos;
        }
    }
    
    // copy the last segment
    dist = end - beg;
    if (dist > 0)
    {
        strncpy(res->str, beg, dist);
        res->str += dist;
    }
    *(res->str) = '\0';
    
    res->str = res->buf; // set res->str to the beginning of the string
    
    mist_string_swap(ms, res);
    mist_string_destroy(res);
    return ec;
}

char*
mist_string_detach(CMistString* ms)
{
    assert(ms != NULL);
    assert(ms->buf != NULL);
    assert((ms->str >= ms->buf) && (ms->str < ms->buf + ms->bsize));
    
    if (ms->str > ms->buf)
    {
        // if the string was not null-terminated, it is not our fault
        memmove(ms->buf, ms->str, ms->bsize - (ms->str - ms->buf));
    }
    
    char* buf = ms->buf;
    
    free(ms);
    return buf;
}

///////////////////////////////////////////////////////////////////////////
// find_in_range_XXXX() functions

const char*
mist_find_in_range_first_of(const char* beg, const char* end, 
    const char* syms, size_t nsyms)
{
    assert(beg != NULL);
    assert(end != NULL);
    assert(beg <= end);
    assert(syms != NULL);
        
    const char* pos;
    for (pos = beg; pos < end; ++pos)
    {
        size_t j;
        for (j = 0; j < nsyms; ++j)
        {
            if (*pos == syms[j])
            {
                break;
            }
        }

        if (j < nsyms)
        {
            break;
        }
    }

    return pos;
}

const char*
mist_find_in_range_first_not_of(const char* beg, const char* end, 
    const char* syms, size_t nsyms)
{
    assert(beg != NULL);
    assert(end != NULL);
    assert(beg <= end);
    assert(syms != NULL);
    
    const char* pos;
    for (pos = beg; pos < end; ++pos)
    {
        size_t j;
        for (j = 0; j < nsyms; ++j)
        {
            if (*pos == syms[j])
            {
                break;
            }
        }

        if (j == nsyms)
        {
            break;
        }
    }

    return pos;
}

const char*
mist_find_in_range_last_not_of(const char* beg, const char* end, 
    const char* syms, size_t nsyms)
{
    assert(beg != NULL);
    assert(end != NULL);
    assert(beg <= end);
    assert(syms != NULL);

    const char* pos = end;
    for (--pos; pos >= beg; --pos)
    {
        size_t j;
        for (j = 0; j < nsyms; ++j)
        {
            if (*pos == syms[j])
            {
                break;
            }
        }

        if (j == nsyms)
        {
            break;
        }
    }

    return ((pos >= beg) ? pos : end);
}

///////////////////////////////////////////////////////////////////////////
// find_XXXX() functions

const char*
mist_find_first_of(const char* str, const char* syms, size_t nsyms)
{
    assert(str != NULL);
    assert(syms != NULL);

    size_t len = strlen(str);
    const char* end = str + len;
    const char* pos = mist_find_in_range_first_of(str, end, syms, nsyms);
    
    assert(pos >= str && pos <= end);

    return ((pos != end) ? pos : NULL);
}

const char*
mist_find_first_not_of(const char* str, const char* syms, size_t nsyms)
{
    assert(str != NULL);
    assert(syms != NULL);
    
    size_t len = strlen(str);
    const char* end = str + len;
    const char* pos = mist_find_in_range_first_not_of(str, str + len, syms, nsyms);
    
    assert(pos >= str && pos <= end);

    return ((pos != end) ? pos : NULL);
}

const char*
mist_find_last_not_of(const char* str, const char* syms, size_t nsyms)
{
    assert(str != NULL);
    assert(syms != NULL);
    
    size_t len = strlen(str);
    const char* end = str + len;
    const char* pos = mist_find_in_range_last_not_of(str, str + len, syms, nsyms);
    
    assert(pos >= str && pos <= end);

    return ((pos != end) ? pos : NULL);
}

///////////////////////////////////////////////////////////////////////////
// Other functions

int
mist_name_is_bad(const char* name)
{
    assert(name != NULL);
    
    if (name[0] == '\0' || 
        name[0] == '.'  || name[0] == ' ')
    {
        return 1;
    }
    
    size_t len = strlen(name);
    for (size_t i = 0; i < len; ++i)    
    {
        // This test should work for both signed and unsigned char types.
        if (name[i] <= 0x1F || name[i] >= 0x7F ||
            ch_allowed[(int)(name[i])] == 0)
        {
            return 1;
        }
    }
    
    return 0;
}

unsigned int
mist_line_num_for_ptr(const char* src, const char* ptr)
{
    assert(src != NULL);
    assert(ptr != NULL);
    assert((ptr >= src) && (ptr <= src + strlen(src)));

    unsigned int cnt = 1;

    for (const char* p = src; p < ptr; ++p)
    {
        if (*p == '\n') ++cnt;
    }

    return cnt;
}

void
mist_format_parse_error(char** error_descr, const char* fmt, unsigned int ln)
{
    assert(fmt != NULL);

    if (error_descr == NULL)
    {
        // nothing to do
        return;
    }
    
    if (ln > 0)
    {
        *error_descr = (char*)malloc(strlen(fmt) + MAX_NUM_DIGITS + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, fmt, ln);
        }
    }
    else // ln == 0
    {
        *error_descr = (char*)strdup(fmt);
    }

    return;
}

char*
mist_get_substring(const char* beg, const char* end)
{
    assert(beg != NULL);
    assert(end != NULL);
    assert(beg <= end);
    
    size_t len = end - beg;
    char* str = (char*)malloc(len + 1);
    if (str == NULL)
    {
        return NULL;
    }
    
    strncpy(str, beg, len);
    str[len] = '\0';
    
    return str;
}

int
mist_is_char_escaped(const char* pos, const char* src)
{
    assert(pos != NULL);
    assert(src != NULL);
    assert(pos >= src);
    
    if (pos == src)
    {
        return 0;
    }
    
    // count preceding slashes
    size_t cnt = 0;
    for (--pos; pos >= src; --pos)
    {
        if (*pos == '\\')
        {
            ++cnt;
        }
        else
        {
            break;
        }
    }
    
    return (cnt & 0x1); 
}

void
mist_chars_to_spaces(char* beg, char* end, const char* syms, size_t nsyms)
{
    assert(beg != NULL);
    assert(end != NULL);
    assert(beg <= end);
    
    assert(syms != NULL);
    assert(*syms != '\0');

    if (beg == end)
    {
        // nothing to do
        return;
    }
    
    const char* pos = mist_find_in_range_first_of(beg, end, syms, nsyms);
    while (pos != end)
    {
        beg[pos - beg] = ' ';
        pos = mist_find_in_range_first_of(pos + 1, end, syms, nsyms);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////
