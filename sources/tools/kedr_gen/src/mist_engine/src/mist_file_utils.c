/// mist_file_utils.c 
/// Implementation of Minimal String Template engine (MiST) API: 
/// file manipulation utilities.

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

#include "mist_string_utils.h"
#include "mist_file_utils.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <assert.h>

/// Default size (in bytes) of a buffer containing a path in the file system
#define MIST_PATH_BUFFER_SIZE ((PATH_MAX < 2048) ? 2048 : PATH_MAX)

/// Types of values in the configuration file (user by mist_load_* functions)
typedef enum EMistLineType_
{ 
    MLT_SINGLE,     /// single-line value definition
    MLT_MULTI,      /// multiline value definition
    MLT_COMMENT,    /// comment
    MLT_BLANK,      /// blank line
    MLT_NOEQ,       /// error: no '=' character
    MLT_BAD_MULTI   /// error: invalid leading line of a multiline value
} EMistLineType;

///////////////////////////////////////////////////////////////////////////
// Private variables
///////////////////////////////////////////////////////////////////////////
static char path_buf[MIST_PATH_BUFFER_SIZE];

/// Default extension of the configuration files
static const char cfg_ext[] = ".cfg";
static const size_t sz_cfg_ext = sizeof(cfg_ext) / sizeof(cfg_ext[0]) - 1;

// Whitespace chars
static const char wspace[] = " \t\n\r";
static const size_t sz_wspace = sizeof(wspace) / sizeof(wspace[0]) - 1;

// Markers of multiline values
static const char ml_beg[] = "=>>";
static const size_t sz_ml_beg = sizeof(ml_beg) / sizeof(ml_beg[0]) - 1;

static const char ml_end[] = "<<";
static const size_t sz_ml_end = sizeof(ml_end) / sizeof(ml_end[0]) - 1;

// Format strings for various errors
static const char* errNoMemory = 
    "there is not enough memory to complete the operation";
static const char* errFileOpenFailed = 
    "failed to open file, please check if it exists and has appropriate permissions set";
static const char* errFileReadFailed = 
    "an error occured while trying to read data from the file";
static const char* errUnexpectedEOF = 
    "unexpected end of the file found";
static const char* errEqExpected = 
    "line %u: expected '='";
static const char* errBadCharsAfterMLBegin = 
    "line %u: only whitespace characters are allowed after \"=>>\" at the same line";
static const char* errEmptyName = 
    "line %u: name of a parameter is missing";

///////////////////////////////////////////////////////////////////////////
// Declarations: "private methods"
///////////////////////////////////////////////////////////////////////////

/// Determine the type of the string read from a config file: whether it 
/// defines a single-line value, multiline value, is a comment or a blank line
/// or if it is invalid.
/// The string will be trimmed by the function. 
/// If the string contains a name of a parameter, a copy of the name will be
/// returned in '*pname'. 'ms->str' will then point to the first non-whitespace 
/// character after '=' for a single-line value, 'ms' will be reset if a 
/// multiline value is encountered. 
/// The caller should free '*pname' when it is no longer needed. 
/// If the string does not contain a parameter definition, '*pname' will be 
/// set to NULL.
static EMistLineType
mist_cfg_get_line_type(CMistString* ms, char** pname);

/// Load a multiline value from the file and store it in 'sm'. 
/// The name of the parameter is specified by 'name'.
/// 'fs' is a file to read from.
/// 'bsize' is the size of a buffer enough to read the whole file (in bytes).
/// 'val' will be used as an accumulator for the lines read.
/// Current line number '*line_no' will be incremented as necessary.
/// In case of error, its description may be returned in '*err' if 'err' is 
/// not NULL.
static EMistErrorCode
mist_cfg_get_multiline_value(const char* name,
    FILE* fs, 
    long bsize,
    CMistString* val,
    unsigned int* line_no,
    CStringMap* sm,
    char** err);

/// Load a single-line value from the file and store it in 'sm'. 
/// The name of the parameter is specified by 'name'.
/// 'fs' is a file to read from.
/// 'bsize' is the size of a buffer enough to read the whole file (in bytes).
/// On entry, 'val' contains the part of the value read so far.
/// Current line number '*line_no' will be incremented as necessary.
/// In case of error, its description may be returned in '*err' if 'err' is 
/// not NULL.
static EMistErrorCode
mist_cfg_get_singleline_value(const char* name,
    FILE* fs, 
    long bsize,
    CMistString* val,
    unsigned int* line_no,
    CStringMap* sm,
    char** err);

///////////////////////////////////////////////////////////////////////////
// Implementation: "private methods"
///////////////////////////////////////////////////////////////////////////
static EMistLineType
mist_cfg_get_line_type(CMistString* ms, char** pname)
{
    assert(ms != NULL);
    assert(pname != NULL);
    
    *pname = NULL;
    mist_string_trim(ms);
        
    if (ms->str[0] == '#')
    {
        return MLT_COMMENT;
    }
    
    if (ms->str[0] == '\0')
    {
        return MLT_BLANK;
    }
    
    static const char syms[] = " \t=";
    static const size_t sz_syms = sizeof(syms) / sizeof(syms[0]) - 1;
    
    const char* end = ms->str + strlen(ms->str);
    const char* name_end = mist_find_in_range_first_of(ms->str, end, syms, sz_syms);
    const char* pos = mist_find_in_range_first_not_of(name_end, end, wspace, sz_wspace);
    
    if (*pos != '=')
    {
        return MLT_NOEQ;
    }
    
    if (!strncmp(pos, ml_beg, sz_ml_beg))
    {
        // found begin marker of a multiline value
        pos = mist_find_in_range_first_not_of(pos + sz_ml_beg, end, wspace, sz_wspace);
        if (pos != end)
        {
            return MLT_BAD_MULTI;
        }
        
        *pname = mist_get_substring(ms->str, name_end);
        // the caller must check it for NULL
        
        mist_string_reset(ms);
        return MLT_MULTI;
    }
    else
    {
        *pname = mist_get_substring(ms->str, name_end);
        // the caller must check it for NULL
        
        // get 'ms' to the beginning of the value
        pos = mist_find_in_range_first_not_of(pos + 1, end, wspace, sz_wspace);
        ms->str += (pos - ms->str);
        
        return MLT_SINGLE;
    }
}

static EMistErrorCode
mist_cfg_get_multiline_value(const char* name,
    FILE* fs, 
    long bsize,
    CMistString* val,
    unsigned int* line_no,
    CStringMap* sm,
    char** err)
{
    assert(name != NULL);
    assert(*name != '\0');
    assert(fs != NULL);
    assert(bsize >= 1);
    assert(val != NULL);
    assert(line_no != NULL);
    assert(*line_no >= 1);
    assert(sm != NULL);
    
    // 'val' provides a buffer large enough to read the file
    char* beg = val->str;
    size_t len = 0;
    const char* pos_beg = NULL;
    const char* pos_end = NULL;
    
    do
    {
        char* p = fgets(beg, (int)bsize, fs);
        if (p != NULL)
        {
            ++(*line_no);
            
            len = strlen(beg);
            pos_beg = mist_find_in_range_first_not_of(beg, beg + len, wspace, sz_wspace);
            pos_end = mist_find_in_range_last_not_of(beg, beg + len, wspace, sz_wspace);
            
            if ((size_t)(pos_end - pos_beg) + 1 == sz_ml_end && 
                !strncmp(pos_beg, ml_end, sz_ml_end))
            {
                // end of value
                *beg = '\0';
                break;
            }
            
            bsize -= (long)len;
            beg += len;
        }
        else // failed to read a line
        {
            if (ferror(fs))
            {
                mist_format_parse_error(err, errFileReadFailed, 0);
                return MIST_READ_FILE_FAILED;
            }
            mist_format_parse_error(err, errUnexpectedEOF, 0);
            return MIST_SYNTAX_ERROR;
        }
    }
    while (1);
    
    mist_string_trim(val);
    if (!smap_add_element(sm, name, val->str))
    {
        mist_format_parse_error(err, errNoMemory, 0);
        return MIST_OUT_OF_MEMORY;
    }
    return MIST_OK;
}

static EMistErrorCode
mist_cfg_get_singleline_value(const char* name,
    FILE* fs, 
    long bsize,
    CMistString* val,
    unsigned int* line_no,
    CStringMap* sm,
    char** err)
{
    assert(name != NULL);
    assert(*name != '\0');
    assert(fs != NULL);
    assert(bsize >= 1);
    assert(val != NULL);
    assert(line_no != NULL);
    assert(*line_no >= 1);
    assert(sm != NULL);
    
    size_t len = strlen(val->str);
    if (len == 0)
    {
        if (!smap_add_element(sm, name, val->str))
        {
            mist_format_parse_error(err, errNoMemory, 0);
            return MIST_OUT_OF_MEMORY;
        }
        return MIST_OK;
    }
    
    
    // at least this much space we have left
    bsize -= (long)((val->str - val->buf) + len); 
    assert(bsize >= 1);
    
    char* beg = val->str;
    char* end = beg + (len - 1); // len > 0 if we got here
    
    while (*end == '\\')
    {
        const char* pos = mist_find_in_range_last_not_of(beg, end, wspace, sz_wspace);
        beg += (size_t)(pos - (const char*)beg);
        if (beg != end)
        {
            ++beg;
        }
        *beg = ' '; // the merged parts are separated by single spaces
        ++beg;
        
        char* p = fgets(beg, (int)bsize, fs);
        if (p != NULL)
        {
            ++(*line_no);
            
            CMistString* ts = mist_string_create(beg);
            if (ts == NULL)
            {
                mist_format_parse_error(err, errNoMemory, 0);
                return MIST_OUT_OF_MEMORY;
            }
            mist_string_trim(ts);
            len = strlen(ts->str);
            
            if (len > 0)
            {
                strncpy(beg, ts->str, len + 1);
                mist_string_destroy(ts);
            }
            else
            {
                // only whitespace have been read
                --beg;
                *beg = '\0';
                mist_string_destroy(ts);
                break;
            }
            
            bsize -= (long)len;
            if (bsize < 1)
            {
                // file size increased unexpectedly - unlikely, but still
                mist_format_parse_error(err, errFileReadFailed, 0);
                return MIST_READ_FILE_FAILED;
            }
            
            end = beg + (len - 1); // len >= 1 here
        }
        else // failed to read a line
        {
            if (ferror(fs))
            {
                mist_format_parse_error(err, errFileReadFailed, 0);
                return MIST_READ_FILE_FAILED;
            }
            mist_format_parse_error(err, errUnexpectedEOF, 0);
            return MIST_SYNTAX_ERROR;
        }
    }
    
    if (!smap_add_element(sm, name, val->str))
    {
        mist_format_parse_error(err, errNoMemory, 0);
        return MIST_OUT_OF_MEMORY;
    }
    return MIST_OK;
}

///////////////////////////////////////////////////////////////////////////
// Implementation: "public methods"
///////////////////////////////////////////////////////////////////////////
long
mist_file_get_size(FILE* fd)
{
    long sz = -1L;
    long old_pos;
    int res;

    old_pos = ftell(fd);
    if (old_pos == -1L)
    {
        return -1L;
    }

    res = fseek(fd, 0, SEEK_END);
    if (res == 0)
    {
        sz = ftell(fd);
        res = fseek(fd, old_pos, SEEK_SET);
    }

    if (res != 0)
    {
        return -1L;
    }

    return sz;
}

EMistErrorCode
mist_load_config_file(const char* path, CStringMap* sm, char** err)
{
    assert(path != NULL);
    assert(sm != NULL);
    
    if (err != NULL)
    {
        *err = NULL;
    }
    
    unsigned int line_no = 0;
    
    FILE* fs = fopen(path, "r");
    if (fs == NULL)
    {
        mist_format_parse_error(err, errFileOpenFailed, 0);
        return MIST_OPEN_FILE_FAILED;
    }

    long size = mist_file_get_size(fs);
    if (size == -1L)
    {
        // in fact, the file seems to be opened but not in the way we need
        fclose(fs);
        mist_format_parse_error(err, errFileOpenFailed, 0);
        return MIST_OPEN_FILE_FAILED;
    }
    size += 2; // for '\0', etc.

    // Read the file line by line into a CMistString structure
    CMistString* ms = mist_string_create(NULL);
    if (ms == NULL)
    {
        fclose(fs);
        mist_format_parse_error(err, errNoMemory, 0);
        return MIST_OUT_OF_MEMORY;
    }

    EMistErrorCode ec = mist_string_reserve(ms, size);
    if (ec != MIST_OK)
    {
        mist_string_destroy(ms);
        fclose(fs);
        mist_format_parse_error(err, errNoMemory, 0);
        return ec;
    }

    do 
    {
        mist_string_reset(ms);
        char* p = fgets(ms->str, (int)size, fs);

        if (p != NULL)
        {
            ++line_no; 
            char* name = NULL;
            
            EMistLineType mlt = mist_cfg_get_line_type(ms, &name);
            switch (mlt)
            {
            case MLT_SINGLE:
                if (name == NULL)
                {
                    mist_string_destroy(ms);
                    fclose(fs);
                    mist_format_parse_error(err, errNoMemory, 0);
                    return MIST_OUT_OF_MEMORY;
                }
                
                if (*name == '\0')
                {
                    mist_string_destroy(ms);
                    fclose(fs);
                    mist_format_parse_error(err, errEmptyName, line_no);
                    return MIST_SYNTAX_ERROR;
                }
                
                ec = mist_cfg_get_singleline_value(name, fs, size, 
                    ms, &line_no, sm, err);
                free(name);
                
                if (ec != MIST_OK)
                {
                    mist_string_destroy(ms);
                    fclose(fs);
                    return ec;
                }
                break;
                
            case MLT_MULTI:
                if (name == NULL)
                {
                    mist_string_destroy(ms);
                    fclose(fs);
                    mist_format_parse_error(err, errNoMemory, 0);
                    return MIST_OUT_OF_MEMORY;
                }
                
                if (*name == '\0')
                {
                    mist_string_destroy(ms);
                    fclose(fs);
                    mist_format_parse_error(err, errEmptyName, line_no);
                    return MIST_SYNTAX_ERROR;
                }
                
                ec = mist_cfg_get_multiline_value(name, fs, size, 
                    ms, &line_no, sm, err);
                free(name);
                
                if (ec != MIST_OK)
                {
                    mist_string_destroy(ms);
                    fclose(fs);
                    return ec;
                }
                break;
                
            case MLT_NOEQ:
                mist_string_destroy(ms);
                fclose(fs);
                mist_format_parse_error(err, errEqExpected, line_no);
                return MIST_SYNTAX_ERROR;
                
            case MLT_BAD_MULTI:
                mist_string_destroy(ms);
                fclose(fs);
                mist_format_parse_error(err, errBadCharsAfterMLBegin, line_no);
                return MIST_SYNTAX_ERROR;
                
            default: // ignore comments and blank lines
                break;
            }
        }

        if (ferror(fs))
        {
            mist_string_destroy(ms);
            fclose(fs);
            mist_format_parse_error(err, errFileReadFailed, 0);
            return MIST_READ_FILE_FAILED;
        }

        if (feof(fs))
        {
            break;
        }
    } 
    while (1);

    mist_string_destroy(ms);
    fclose(fs);
    return MIST_OK;
}

EMistErrorCode
mist_load_config_file_for_name(const char* base_dir, const char* name,
    CStringMap* sm, char** err)
{
    assert(base_dir != NULL);
    assert(*base_dir != '\0');
    assert(name != NULL);
    assert(*name != '\0');
    
    size_t base_dir_len = strlen(base_dir);
    size_t name_len = strlen(name);
    
    char* path = (char*)malloc(base_dir_len + name_len + sz_cfg_ext + 2);
    // +1 for a '\' inbetween, +1 more for '\0' at the end
    if (path == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    strncpy(path, base_dir, base_dir_len + 1);
    mist_path_to_unix_slashes(path);
    
    char* last_part = path + (base_dir_len - 1);
    if (*last_part != '/')
    {
        ++last_part;
        *last_part = '/';
    }
    ++last_part;
    
    strncpy(last_part, name, name_len + 1);
    last_part += name_len;
    
    // +1 to ensure '\0' is at the end
    strncpy(last_part, cfg_ext, sz_cfg_ext + 1);
    
    EMistErrorCode res = mist_load_config_file(path, sm, err);
    free(path);
    return res;
}

EMistErrorCode
mist_load_config_file_from_dir(const char* base_dir, CStringMap* sm, char** err)
{
    assert(base_dir != NULL);
    assert(*base_dir != '\0');
    assert(strchr(base_dir, '\\') == NULL);
    
    static const char suffix[] = "-t2c";
    static const size_t suf_len = sizeof(suffix) / sizeof(suffix[0]) - 1;
    
    EMistErrorCode res = MIST_OK;

    // Construct the name of the config file first
    char* tname = mist_path_get_last(base_dir);
    if (tname == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    size_t len = strlen(tname);
    int idx = (int)len - (int)suf_len;
    if (len > suf_len && 
        !strncmp(&tname[idx], suffix, suf_len))
    {
        tname[idx] = '\0';  // cut off the suffix if present
    }

    res = mist_load_config_file_for_name(base_dir, tname, sm, err);
    free(tname);
    return res;
}

int
mist_dir_exists(const char* path)
{
    assert(path != NULL);
    assert(path[0] != '\0');
        
    char* cwd = getcwd(&path_buf[0], MIST_PATH_BUFFER_SIZE);
    
    if (cwd == NULL)    // some error occured
    {
        return 0;
    }
    
    if (chdir(path) == 0)
    {
        // successfully changed directory, now change it back
        if (chdir(cwd) != 0)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        // the directory does not exist or some error has occured
        return 0;
    }
}

int
mist_subdir_exists(const char* dir, const char* subdir)
{
    assert(dir != NULL);
    assert(dir[0] != '\0');
    assert(subdir != NULL);
    assert(subdir[0] != '\0');
    
    assert(mist_dir_exists(dir));
        
    char* cwd = getcwd(&path_buf[0], MIST_PATH_BUFFER_SIZE);
    
    if (cwd == NULL)    // some error occured
    {
        return 0;
    }
    
    int res = 1;
    if (chdir(dir) == 0)
    {
        // successfully changed directory to 'dir', now go to 'subdir'
        if (chdir(subdir) != 0)
        {
            res = 0;
        }
        
        // go back now 
        if (chdir(cwd) != 0)
        {
            res = 0;
        }
    }
    else
    {
        // the directory does not exist or some error has occured
        res = 0;
    }
    
    return res;
}

int
mist_file_exists(const char* path)
{
    assert(path != NULL);
    assert(*path != '\0');
    
    FILE* fd = fopen(path, "r");
    if (fd == NULL)
    {
        return 0;
    }
    
    fclose(fd);
    return 1;
}

int 
mist_dir_is_root(const char* path)
{
    assert(path != NULL);
    assert(*path != '\0');
    
    if (!strcmp(path, "/") || !strcmp(path, "\\"))
    {
        return 1;
    }
    
    if (path[0] != '\0' && path[1] == ':' && 
        mist_find_first_not_of(&path[2], "/\\", 2) == NULL)
    {
        return 1;
    }
    
    return 0;
}

char* 
mist_path_to_unix_slashes(char* str)
{
    assert(str != NULL);
    
    size_t len = strlen(str);
    size_t i;
    
    for (i = 0; i < len; ++i)
    {
        if (str[i] == '\\')
        {
            str[i] = '/';
        }
    }
    
    return str;
}

char* 
mist_path_sum(const char* path_left, const char* path_right)
{
    static const char sep = '/';    // default separator
    
    assert(path_left != NULL);
    assert(path_right != NULL);
    
    if (mist_path_is_absolute(path_right))
    {
        return (char*)strdup(path_right);
    }
    
    size_t len = strlen(path_left);
    
    char* res = (char*)malloc(len + strlen(path_right) + 2);
    if (res == NULL)
    {
        return NULL;
    }
    
    strcpy(res, path_left);
    if (len != 0)
    {
        if (res[len - 1] == sep)
        {
            --len;
        }
        
        res[len] = sep;
        ++len;
    }
    strcpy(&res[len], ((path_right[0] == sep) ? &path_right[1] : &path_right[0]));
    
    return res;
}

int
mist_path_is_absolute(const char* path)
{
    assert(path != NULL);
    
    // If the path contains a colon (':'), it is also considered absolute.
    return (path[0] == '/'  || 
            path[0] == '\\' || 
            path[0] == '~'  || 
            strchr(path, ':') != NULL);
}

char* 
mist_path_absolute(const char* path)
{
    assert(path != NULL);
    assert(*path != '\0');
    
    char* raw = NULL;
    
    // Determine if the path is absolute, prepend the path to the current 
    // directory if it is not.
    
    if (!mist_path_is_absolute(path))
    {
        char* cwd = getcwd(&path_buf[0], MIST_PATH_BUFFER_SIZE);
        if (cwd == NULL)    // some error occured
        {
            return NULL;
        }
        
        raw = mist_path_sum(cwd, path);
    }
    else
    {
        // 'path' already specifies an absolute path
        raw = (char*)strdup(path);
    }
    
    if (raw == NULL)
    {
        return NULL;
    }
    
    // An absolute path mustn't begin with '.' or ':'
    assert(raw[0] != '.');
    assert(raw[0] != ':');
    
    char* res = (char*)malloc(strlen(raw) + 1);
    if (res == NULL)
    {
        free(raw);
        return NULL;
    }
    char* cur = res;
    
    // Convert separators.
    mist_path_to_unix_slashes(raw);
    
    // Split the path into components
    CGrowingArray comp; // An array of pointers to the components of the path
    size_t len;
    
    char* pos = strchr(raw, '/');
    if (pos == NULL)
    {
        strcpy(res, raw);
        free(raw);
        return res;
    }
    else
    {
        if (!grar_create(&comp))
        {
            free(raw);
            free(res);
            return NULL;
        }
        
        len = pos - raw;
        strncpy(res, raw, len);
        cur += len;
    }
    
    while (*pos == '/') 
    {
        *pos = '\0';
        ++pos;
    }
    
    while (*pos != '\0')
    {
        if (!grar_add_element(&comp, pos))
        {
            grar_destroy(&comp);
            free(raw);
            free(res);
            return NULL;
        }
        
        pos = strchr(pos, '/');
        if (pos == NULL)
        {
            break;
        }
        
        while (*pos == '/') 
        {
            *pos = '\0';
            ++pos;
        }
    }
        
    // Get rid of "." and ".." in the path.
    char** component = grar_get_c_array(&comp, char*);
    
    size_t i; 
    for (i = 0; i < grar_get_size(&comp); ++i)
    {
        if (!strcmp(component[i], "."))
        {
            component[i] = NULL;
        }
        else if (!strcmp(component[i], ".."))
        {
            component[i] = NULL;
            
			if (i > 0)
			{
		        size_t j = i;
		        for (; j != 0; --j)
		        {
		            if (component[j - 1] != NULL)
		            {
		                component[j - 1] = NULL;
		                break;
		            }
		        } // end for j
			}
        }
    } // end for i
    
    // Construct the resulting path.
    for (i = 0; i < grar_get_size(&comp); ++i)
    {
        if (component[i] != NULL)
        {
            *cur = '/';
            ++cur;
            
            len = strlen(component[i]);
            strncpy(cur, component[i], len);
            cur += len;
        }
    } // end for i
    
    if (res == cur) // if root directory is the result...
    {
        *cur = '/';
        ++cur;
    }
    
    // add terminating null byte
    *cur = '\0';

    grar_destroy(&comp);
    free(raw);
    return res;
}

char*
mist_path_get_last(const char* path)
{
    assert(path != NULL);
    assert(*path != '\0');
    
    const char* end = &path[strlen(path) - 1];
    while (*end == '/')
    {
        --end;
    }
    
    assert(end >= path);
    
    const char* beg = end;
    while (beg >= path && *beg != '/')
    {
        --beg;
    }
    
    if (beg < path) 
    {
        beg = path;
    }
    
    if (*beg == '/')
    {
        ++beg;
    }
    
    size_t len = end - beg + 1;
    char* res = (char*)malloc(len + 1 + sz_cfg_ext);
    if (res == NULL)
    {
        return NULL;
    }
    
    res = strncpy(res, beg, len);
    res[len] = '\0';
    
    return res;
}

int 
mist_create_directory(const char* path)
{
    assert(path != NULL);
    assert(path[0] != '\0');
    
    int res;
#ifdef MKDIR_ONE_ARG
    res = mkdir(path);
#else
    res = mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO); // permissions: 777
#endif
    return (res == 0);
}

int 
mist_create_path(const char* path)
{
    assert(path != NULL);
    assert(path[0] != '\0');
    
    // Get absolute path first
    char* full_path = mist_path_absolute(path);
    if (full_path == NULL)
    {
        return 0;
    }
    // Among all other things, 'full_path' now uses only '/' as separators,
    // no more that one in succession each time.
    
    int created_something = 0;
    int is_error = 0;
    
    char* pos = full_path;
    if (pos[0] == '/')
    {
        ++pos;
    }
    assert(pos[0] != '/');
    
    while (pos != NULL && !is_error)
    {
        pos = strchr(pos + 1, '/');
        if (pos != NULL)
        {
            // temporarily replace '/' with '\0'
            *pos = '\0';
        }
        
        if (!mist_dir_exists(full_path))
        {
            if (mist_create_directory(full_path) == 0)
            {
                is_error = 1;
            }
            else
            {
                created_something = 1;
            }
        }
        
        if (pos != NULL)
        {
            // restore '/'
            *pos = '/';
        }
    }
   
    free(full_path);
    return (is_error == 0 && created_something != 0);
}

int 
mist_dir_is_special(const char* path)
{
    assert(path != NULL);
    assert(path[0] != '\0');
    
    const char* pos = strrchr(path, '/');
    if (pos == NULL)
    {
        pos = path;
    }
    else
    {
        pos = pos + 1;
    }
    
    if (pos[0] == '.' || strcmp(pos, "CVS") == 0)
    {
        return 1;
    }
    
    return 0;
}


EMistErrorCode 
mist_file_read_all(const char* path, char** buf)
{
    assert(path != NULL);
    assert(buf != NULL);
    assert(mist_file_exists(path));
    
    *buf = NULL;
    
    FILE* fd = fopen(path, "r");
    if (fd == NULL)
    {
        return MIST_OPEN_FILE_FAILED;
    }
    
    long sz = mist_file_get_size(fd);
    if (sz == -1L)
    {
        // there is something wrong with the file 
        fclose(fd);
        return MIST_READ_FILE_FAILED;
    }
    
    *buf = (char*)malloc(sz + 1);
    if (*buf == NULL)
    {
        fclose(fd);
        return MIST_OUT_OF_MEMORY;
    }
    
    size_t nread = fread(*buf, 1, (size_t)sz, fd);
    if (ferror(fd))
    {
        // an error occured while reading the file
        free(*buf);
        *buf = NULL;
        
        fclose(fd);
        return MIST_READ_FILE_FAILED;
    }
    (*buf)[nread] = '\0';
    
    fclose(fd);
    return MIST_OK;
}

int
mist_create_path_for_file(const char* file_path)
{
    assert(file_path != NULL);
 
    char* path = mist_path_absolute(file_path);
    if (path == NULL)
    {
        return 0;
    }
    
    if (mist_dir_is_root(path)) // file_path is "/", which is bad
    {
        free(path);
        return 0;
    }
    
    char* pos = strrchr(path, '/');
    assert(pos != NULL);
    
    *pos = '\0'; // cut off the name of the file
    
    if (!mist_dir_exists(path))
    {
        if (!mist_create_path(path))
        {
            free(path);
            return 0;
        }
    }
    
    free(path);
    return 1;
}

char* 
mist_path_get_containing_dir(const char* path)
{
    char* dpath = mist_path_absolute(path);
    if (dpath == NULL)
    {
        return NULL;
    }
    
    // after mist_path_absolute() all slashes are Unix-style and
    // the components of the path are separated by single separator chars
    char* last_slash = strrchr(dpath, '/');
    assert(last_slash != NULL);
    assert(*(last_slash + 1) != '\0'); //must not be at the end
    
    // cut off the last component of the path (including the slash)
    *last_slash = '\0';
    
    return dpath;
}

///////////////////////////////////////////////////////////////////////////
