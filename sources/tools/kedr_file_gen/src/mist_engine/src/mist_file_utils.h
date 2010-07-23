/// mist_file_utils.h
/// Minimal String Template engine (MiST):
/// definitions of various file utility functions.

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

#ifndef MIST_FILE_UTILS_H_2208_INCLUDED
#define MIST_FILE_UTILS_H_2208_INCLUDED

#include <stdio.h>

#include "grar.h"
#include "smap.h"
#include "mist_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Loads the configuration data from the specified file into a string map
/// in which parameters' names are the keys and their values - values, 
/// respectively.
/// This function does not clear the string map before adding new elements.
/// Returns MIST_OK if successful, an error code otherwise (see EMistErrorCode
/// enum).
///
/// A configuration file has the following format. The file may contain blank
/// lines (1), comments (2), and parameter definitions (3), (4).
/// Only spaces and tabs are considered whitespace characters here.
///
/// (1) Blank lines (those containing only whitespace charaters) are ignored.
/// (2) A comment is a line, the first non-whitespace character in which is '#'
/// Comments are also ignored.
/// (3) A "single-line" parameter definition has the following form:
///     <name> = <value>
/// Whitespace characters surrounding <name> and <value> are ignored.
/// <value> can be empty. It can also contain '=' characters. Example:
///     CFLAGS = -std=c99 -O2
/// Here, parameter's name is "CFLAGS" and its value is "-std=c99 -O2"
/// '\' can be used to split the long <value> strings into several "logical"
/// lines ("logical" because all of them will be combined in a single line by 
/// the function). Whitespace will be trimmed from each logical line; single
/// space characters will be inserted inbetween when combining.
/// For example, the above definition of "CFLAGS" can be written equivalently
/// as follows (ignore apostrophes - they are here to avoid compiler warnings
/// concerning "multiline comments"):
///     CFLAGS = \                                                        '
///         -std=c99 -O2
/// or
///     CFLAGS = -std=c99 \                                               '
///         -O2
/// [NB] You can not put '\' between <name> and =. The following example 
/// is NOT allowed:
///     CFLAGS \                                                          '
///         = -std=c99 -O2
/// The function will report a syntax error in this case.
/// [NB] The additional "logical" lines that begin with '#' or contain only
/// whitespace characters are considered to be the part of <value> and hence
/// are not ignored. For example, the definition
///     CFLAGS = -std=c99 \                                               '
///        \                                                              '
///     # foo 
/// is interpreted as
///     CFLAGS = -std=c99 # foo
///
/// (4) A "multiline" parameter definition has the following form:
///     <name> =>> 
///         <line1>
///         <line2>
///         ...
///         <lineN>
///     <<
/// The value of the parameters will be the lines between "<name> =>>" and 
/// "<<" (excluding the newline characters before <line1> and after <lineN>). 
/// These lines are taken "as is": no whitespace trimming, no comment/blank
/// recognition, no special meaning of ending '\' like in (3) above, etc. 
/// The resulting value will contain newline characters between 
/// <line1>...<lineN>
/// [NB] Non-whitespace characters are not allowed after "=>>" at the same 
/// line. 
/// [NB] If and only if a line contains just "<<" and, optionally, whitespace,
/// it is considered an end marker for the value of this parameter. That is,
/// it is allowed to specify multiline parameters like this:
///     CFLAGS =>>
///         -O2
///         -D"_FOO=<<"
///         -g
///     <<
/// "<<" in -D"_FOO=<<" is not considered an end marker for this value 
/// because apart from "<<", there are non-whitespace characters on the line.
/// 
/// [NB] If there is more than one parameter with the same value in the file,
/// all of them will be present in the resulting string map. However, it is
/// unspecified the value of which one will be returned during lookup.
/// Use smap_as_array() to traverse all the elements in the map.
/// 
/// If an error occurs (except out of memory), the function may return its
/// description in '*err' (if 'err' is not NULL). '*err' should be freed 
/// by the caller when no longer needed.
EMistErrorCode
mist_load_config_file(const char* path, CStringMap* sm, char** err);

/// A convenience function that loads configuration data from the following file
/// into the string map 'sm':
///     <base_dir>/<name>.cfg
/// The function uses mist_load_config_file() internally.
/// The path must be absolute and must not specify root directory. 
/// Only Unix-style separators ('/') must be used in the path.
///
/// Possible return values are the same as for mist_load_config_file().
EMistErrorCode
mist_load_config_file_for_name(const char* base_dir, const char* name,
    CStringMap* sm, char** err);

/// A convenience function that loads configuration data from a file located
/// in the specified directory into the string map 'sm'. 
/// The function is equivalent to mist_load_config_file_for_name() with
/// 'name' being the same as the last component of 'base_dir' but without "-t2c" 
/// suffix if it is present in the latter.
///
/// Example 1. base_dir is "/home/tester/suite-t2c", the path to the file is
///  "/home/tester/suite-t2c/suite.cfg"
/// Example 2. base_dir is "/home/tester/another_suite", the path to the file is
///  "/home/tester/another_suite/another_suite.cfg"
///
/// Possible return values are the same as for mist_load_config_file().
EMistErrorCode
mist_load_config_file_from_dir(const char* base_dir, CStringMap* sm, char** err);

///////////////////////////////////////////////////////////////////////////
// Path-related functions
///////////////////////////////////////////////////////////////////////////

/// Replace all occurences of '\' char ('\\') in 'str' with '/'.
/// Can be useful to make separators consistent in the filesystem paths.
/// This function modifies the string in place rather than operates on its copy.
/// The function returns 'str'.
char* 
mist_path_to_unix_slashes(char* str);

/// Concatenate two paths ('path_left' and 'path_right') and return the new path.
/// '/' is used as a separator. If 'path_left' is empty, no separator will be used.
/// If 'path_right' is an absolute path, a copy of 'path_right' is returned.
///
/// Examples:
/// mist_path_sum("aaa/bbb/ccc", "dd/ee")  => "aaa/bbb/ccc/dd/ee"
/// mist_path_sum("aaa/bbb/ccc", "/dd/ee") => "/dd/ee"
/// mist_path_sum("aaa/bbb/ccc/", "dd/ee") => "aaa/bbb/ccc/dd/ee"
/// mist_path_sum("aaa/bbb/ccc///", "dd/ee") => "aaa/bbb/ccc//dd/ee"
/// mist_path_sum("", "dd/ee") => "dd/ee"
/// mist_path_sum("aaa/bbb/ccc", "") => "aaa/bbb/ccc/"
/// 
/// The returned pointer must be freed when no londer needed.
/// If there is not enough memory to complete the operation, NULL is returned.
char* 
mist_path_sum(const char* path_left, const char* path_right);

/// Construct the absolute filesystem path corresponding to 'path'. 
/// The function returns a pointer to a new string containing the absolute path.
/// The returned pointer must be freed when no longer needed.
/// The resulting path will not contain "." and "..".
/// If the path specifies a directory, the resulting path will not have a slash
/// at the end.
/// All separators in the resulting path will be Unix-style ('/'). 
/// The input path may use '/' or '\' as separators (may be even both types in 
/// the same path).
/// The function does not check whether the path exists in the filesystem.
/// The function returns NULL in case of any error (out of memory, etc.).
char* 
mist_path_absolute(const char* path);

/// Returns nonzero if the specified path is absolute, 0 if it is relative.
/// [NB] An empty path ("") is considered relative too.
/// The way to check this may be quite primitive and not fool-proof so far but,
/// for now, it is enough.
int
mist_path_is_absolute(const char* path);

/// Return a copy of the last component of the path. It is assumed that '/' 
/// (i.e. Unix-style separator) is used as a separator in the path.
/// Do not call this function for NULL, empty string, or root directory ("/")
/// 
/// The function allocates extra 4 bytes for the resulting string 
/// for the caller may want to append ".cfg" extension to this name 
/// The returned pointer must be freed when no longer needed.
/// 
/// Examples:
/// mist_path_get_last("/home/tester") => "tester"
/// mist_path_get_last("/home/tester/suite-t2c/") => "suite-t2c"
/// mist_path_get_last("workspace") => "workspace"
/// mist_path_get_last("workspace/dir") => "dir"
/// 
/// [NB] This function is usually called for the paths returned by 
/// mist_path_absolute().
char*
mist_path_get_last(const char* path);

/// Return an absolute path to the directory containing the file specified by
/// 'path' (the latter may be relative or absolute). 
/// 'path' must not have a separator (slash) at the end.
/// All separators in the resulting path will be Unix-style. The resulting path 
/// will not have a slash at the end.
/// The function does not check whether the path exists in the filesystem.
/// The function returns NULL in case of any error (out of memory, etc.).
/// The returned pointer must be freed when no longer needed.
char* 
mist_path_get_containing_dir(const char* path);

///////////////////////////////////////////////////////////////////////////
// Getting information about files and directories
///////////////////////////////////////////////////////////////////////////

/// Computes the size (in bytes) of the file associated with the stream 'fs'.
/// If the stream is invalid or the size cannot be obtained for some other reason,
/// the return value is undefined. However, if it is -1L, it definitely indicates 
/// that an error has occured.
long 
mist_file_get_size(FILE* fd);

/// Return nonzero if the specified file exists, 0 otherwise
int
mist_file_exists(const char* path);

/// Allocate a buffer of appropriate size and read the contents of the file to 
/// the buffer ('\0' is appended at the end). The file must exist.
/// '*buf' should be freed by the caller when no longer needed.
EMistErrorCode 
mist_file_read_all(const char* path, char** buf);

/// Return nonzero if the specified directory exists, 0 otherwise
int
mist_dir_exists(const char* path);

/// Return nonzero if the specified subdirectory exists in 'dir', 0 otherwise
/// 'dir' itself must exist.
int
mist_subdir_exists(const char* dir, const char* subdir);

/// Return nonzero if the specified directory is a root directory ('/' on Unix
/// '\' or '<letter>:[\]' on Windows), 0 otherwise
int 
mist_dir_is_root(const char* path);

/// Return nonzero if the name of the directory begins with '.' or is "CVS".
/// Such special directories usually are not to be processed by the code generator.
/// Separators in the path must be Unix-style.
int 
mist_dir_is_special(const char* path);

/// Create directory at the specified path. 
/// This is a wrapper around mkdir(). It is necessary because the latter has
/// different arguments in POSIX-compliant systems and in MinGW.
/// In case of a POSIX-compliant system, permissions for the newly created 
/// directory will be set to 777.
/// [NB] All components of the path except the last one must exist.
/// Unlike mkdir, the function returns nonzero if successful, 0 otherwise.
/// If the specified directory already exists, the function returns 0 (failure).
int 
mist_create_directory(const char* path);

/// Create directory at the specified path. If some other components of the 
/// path do not exist, create them too (like 'mkdir -p' command on Unix).
/// The function uses mist_create_directory() to create the components of 
/// the path.
/// The function returns nonzero if successful, 0 otherwise.
/// Note that if the function does not succeed, it still may have created some
/// of the directories that are components of the path.
/// If the specified directory already exists, the function returns 0 (failure).
int 
mist_create_path(const char* path);

/// Create directory for the specified file.
/// The function returns nonzero if successful, 0 otherwise.
/// If the directory already exists, the function returns nonzero (success).
int
mist_create_path_for_file(const char* file_path);

///////////////////////////////////////////////////////////////////////////
// Other functions
///////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // MIST_FILE_UTILS_H_2208_INCLUDED
