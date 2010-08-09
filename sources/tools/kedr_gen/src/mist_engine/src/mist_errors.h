/// mist_errors.h
/// Contains definitions of error codes used by some MiST functions.

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

#ifndef MIST_ERRORS_H_2349_INCLUDED
#define MIST_ERRORS_H_2349_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/// Possible error codes
typedef enum EMistErrorCode_
{
    /// This value is returned by some MiST functions in case of success.
    MIST_OK = -1,

    /// Error: out of memory
    MIST_OUT_OF_MEMORY = 0,

    /// Error: unable to open file
    MIST_OPEN_FILE_FAILED = 1,

    /// Error: unable to read from file
    MIST_READ_FILE_FAILED = 2,

    /// Error: unable to write to file
    MIST_WRITE_FILE_FAILED = 3,
    
    /// Error: unable to obtain the contents of the directory
    MIST_READ_DIR_FAILED = 4,
    
    /// Error: syntax error in the data being parsed
    MIST_SYNTAX_ERROR = 5,
    
    /// Error: invalid name of an entity (template, file, etc.)
    MIST_BAD_NAME = 6,
    
    /// Error: parameter is specified more than once in a .cfg file.
    MIST_DUP_PARAM = 7,
    
    /// Error: a required parameter is missing from a .cfg file.
    MIST_MISSING_PARAM = 8,
    
    /// Error: no template files found in the directory
    MIST_NO_TPL_FILES = 9,
    
    /// Error: failed to load template
    MIST_FAILED_TO_LOAD_TEMPLATE = 10,
    
    /// Error: the top-level template is multivalued after evaluation
    MIST_MAIN_TPL_MULTIVALUED = 11,
    
    /// Error: failed to create directory to contain the output file
    MIST_CREATE_DIR_FAILED = 12,
        
    /// Error: unspecified error
    MIST_UNSPECIFIED_ERROR = 13,
        
    /// (MiST library only) 
    /// Error: the requested version of the library is not supported.
    MIST_VERSION_NOT_SUPPORTED = 14,
        
    /// (MiST library only) 
    /// Error: the MiST library has not been initialized yet.
    MIST_LIBRARY_NOT_INITIALIZED = 15
} EMistErrorCode;

#ifdef __cplusplus
}
#endif

#endif // MIST_ERRORS_H_2349_INCLUDED
