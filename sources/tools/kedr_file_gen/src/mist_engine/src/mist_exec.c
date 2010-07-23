/// mist_exec.c
/// MiST Engine executable.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <assert.h>

#include <limits.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#include "about.h"
#include "mist_base.h"
#include "mist_file_utils.h"
#include "mist_string_utils.h"
#include "mist_exec.h"

///////////////////////////////////////////////////////////////////////////  
// Constants
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// Structure definitions
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// Global  variables
///////////////////////////////////////////////////////////////////////////
struct SSettings settings = {
    0,    // is_simplified_mode
    NULL, // tpl_path
    NULL, // val_path
    NULL, // main_tg
    NULL, // path_tg
    NULL  // values
};

///////////////////////////////////////////////////////////////////////////
// Private variables
///////////////////////////////////////////////////////////////////////////

// Format strings for various errors
static const char* errAbsTplPathFail = 
    "unable to obtain absolute path to the template file / directory";
static const char* errAbsValPathFail = 
    "unable to obtain absolute path to the file with values";
static const char* errBadArgCount = 
    "wrong number of arguments; execute " MIST_SHORT_NAME " --help for usage summary";
static const char* errDirNotFound = 
    "directory \"%s\" does not exist or cannot be accessed";
static const char* errFileNotFound = 
    "file \"%s\" does not exist or cannot be accessed";
static const char* errTplDirRoot = 
    "root directory should not be used as a template directory";
static const char* errReadTplFailed = 
    "failed to read template data from \"%s\": %s";
static const char* errNoMemory = 
    "not enough memory";
static const char* errFileNotAccessible = 
    "the file does not exist or cannot be accessed";
static const char* errFailedToLoadTpl = 
    "failed to load the template(s): %s";
static const char* errUnspecifiedError = 
    "unspecified error";
static const char* errReadFileFailed = 
    "an error occured while reading from the file";
static const char* errSyntaxError = 
    "syntax error";
static const char* errFailedToLoadValues = 
    "failed to load parameter values from \"%s\": %s";
static const char* errGenOutPathFailed = 
    "failed to generate path to the output file: %s";
static const char* errGenResultFailed = 
    "failed to generate the resulting document: %s";
static const char* errMultivaluedResult = 
    "the result is multi-valued, perhaps \"join\"-clause is missing somewhere in the template";

///////////////////////////////////////////////////////////////////////////  
// Declarations of "private" functions
///////////////////////////////////////////////////////////////////////////

/// Process the arguments specified in the command line.
/// See the description of init() function for details.
static int
load_args(int argc, char* argv[], struct SSettings* psettings);

/// Show usage information.
static void
show_usage();

/// Show version information.
static void
show_version();

// Return a string describing the error corresponding to the specified 
// error code. 
// Only a few of error codes are currently supported, "unspecified error"
// is returned for the remaining ones.
// MIST_OK should not be passed in.
static const char*
error_code_to_string(EMistErrorCode ec);

///////////////////////////////////////////////////////////////////////////  
// Implementation
///////////////////////////////////////////////////////////////////////////

static void
show_usage()
{
    printf("%s\n%s\n", MIST_LONG_NAME, MIST_USAGE);
    return;
}

static void
show_version()
{
    printf("%s version %s\n", MIST_SHORT_NAME, MIST_VERSION);
    return;
}

static const char*
error_code_to_string(EMistErrorCode ec)
{
    assert(ec != MIST_OK);
    const char* ret = NULL;
    
    switch (ec)
    {
    case MIST_OUT_OF_MEMORY:
        ret = errNoMemory;
        break;
    
    case MIST_OPEN_FILE_FAILED:
        ret = errFileNotAccessible;
        break;
        
    case MIST_READ_FILE_FAILED:
        ret = errReadFileFailed;
        break;
    
    case MIST_SYNTAX_ERROR:
        ret = errSyntaxError;
        break;
    
    default:
        ret = errUnspecifiedError;
        break;
    }
    
    return ret;
}

static int
load_args(int argc, char* argv[], struct SSettings* psettings)
{
    int c;
    int ret = 1;

    assert(psettings != NULL);

    struct option long_options[] =
    {
        // these options do not set flags
        {"help",    no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"simplified-mode", no_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };
    
    while (1)
    {
        int option_index = 0;
        c = getopt_long(argc, argv, "s", long_options, &option_index);
        
        if (c == -1) // all options have been processed
        {
            break;
        }
        
        switch (c)
        {
        case 0:
            // If a flag was set, nothing more to do. If not...
            if (long_options[option_index].flag == 0)
            {
                assert(0); // should not get here
            }
            break;
            
        case 'h':
            show_usage();
            
            cleanup_settings(psettings);
            exit(EXIT_SUCCESS);
            break;
            
        case 'v':
            show_version();
            
            cleanup_settings(psettings);
            exit(EXIT_SUCCESS);
            break;
            
        case 's':
            psettings->is_simplified_mode = 1;
            break;
            
        case '?':
            // getopt_long should have already printed an error message.
            ret = 0;
            break;

        default:
            assert(0); // should not get here
            break;
        }
    }
    
    if (ret != 0)
    {
        if (argc != optind + 2)
        {
            // exactly 2 arguments should be still left unprocessed
            print_error(errBadArgCount); 
            ret = 0;
        }
        else 
        {
            // The first non-option argument should be the path to the template file
            // or directory, the second one should be the path of an input file with
            // values.
            psettings->tpl_path = mist_path_absolute(argv[optind]);
            if (psettings->tpl_path == NULL)
            {
                print_error(errAbsTplPathFail);
                cleanup_settings(psettings);
                exit(EXIT_FAILURE);
            }
            
            ++optind; // to the next argument
            
            psettings->val_path = mist_path_absolute(argv[optind]);
            if (psettings->val_path == NULL)
            {
                print_error(errAbsValPathFail);
                cleanup_settings(psettings);
                exit(EXIT_FAILURE);
            }
        }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////  
// Implementation of "public" functions
///////////////////////////////////////////////////////////////////////////

int 
print_error(const char* format, ...)
{
    assert(format != NULL);
    
    int res = -1;
    va_list ap;
    
    // output the prefix
    res = fprintf(stderr, "%s: ", MIST_SHORT_NAME);
    if (res < 0)
    {
        return res;
    }

    va_start(ap, format);
    res = vfprintf(stderr, format, ap);
    va_end(ap);
    
    if (res < 0)
    {
        return res;
    }
    
    res = fprintf(stderr, "\n");
    return res;
}

int
init(int argc, char* argv[], struct SSettings* psettings)
{
    assert(psettings != NULL);
    
    if (load_args(argc, argv, psettings) == 0)
    {
        return 0;
    }
    
    // Check if the imput files and directories exist
    if (psettings->is_simplified_mode)
    {
        if (!mist_file_exists(psettings->tpl_path))
        {
            print_error(errFileNotFound, psettings->tpl_path);
            return 0;
        }
    }
    else
    {
        if (!mist_dir_exists(psettings->tpl_path))
        {
            print_error(errDirNotFound, psettings->tpl_path);
            return 0;
        }
        if (mist_dir_is_root(psettings->tpl_path))
        {
            print_error(errTplDirRoot);
            return 0;
        }
    }

    if (!mist_file_exists(psettings->val_path))
    {
        print_error(errFileNotFound, psettings->val_path);
        return 0;
    }
    return 1;
}

void 
cleanup_settings(struct SSettings* psettings)
{
    assert(psettings != NULL);
    
    if (psettings->values != NULL)
    {
        smap_destroy(psettings->values);
    }
    
    if (psettings->main_tg != NULL)
    {
        mist_tg_destroy_impl(psettings->main_tg);
    }
    
    if (psettings->path_tg != NULL)
    {
        mist_tg_destroy_impl(psettings->path_tg);
    }
    
    free(psettings->tpl_path);
    free(psettings->val_path);
    
    return;
}

int
load_templates(struct SSettings* psettings)
{
    assert(psettings != NULL);
    assert(psettings->tpl_path != NULL);
    assert(psettings->main_tg == NULL);
    assert(psettings->path_tg == NULL);
    
    EMistErrorCode ec;
    if (psettings->is_simplified_mode)
    {
        static char tpl_name[] = "main";
        char* buf = NULL;
        char* err = NULL;
        
        ec = mist_file_read_all(psettings->tpl_path, &buf);
        if (ec != MIST_OK)
        {
            print_error(errReadTplFailed, psettings->tpl_path,
                (ec == MIST_OUT_OF_MEMORY ? errNoMemory : errFileNotAccessible));
            return 0;
        }
        
        psettings->main_tg = mist_tg_create_single_impl(&tpl_name[0], buf, "<$", "$>", &err);
        free(buf);
        
        if (psettings->main_tg == NULL)
        {
            print_error(errFailedToLoadTpl, (err != NULL ? err : errUnspecifiedError));
            free(err);
            return 0;
        }
    }
    else // regular mode
    {
        char* err = NULL;
        ec = mist_tg_load_from_dir_impl(psettings->tpl_path, 
            &(psettings->main_tg), &(psettings->path_tg), &err);
        
        if (ec != MIST_OK)
        {
            print_error(errFailedToLoadTpl, (err != NULL ? err : errUnspecifiedError));
            free(err);
            return 0;
        }
    }
    return 1;
}

int
load_param_values(struct SSettings* psettings)
{
    assert(psettings != NULL);
    assert(psettings->val_path != NULL);
    assert(psettings->values == NULL);
    
    psettings->values = smap_create();
    if (psettings->values == NULL)
    {
        print_error(errNoMemory);
        return 0;
    }
    
    char* err = NULL;
    EMistErrorCode ec = mist_load_config_file(psettings->val_path, 
        psettings->values, &err);
    
    if (ec != MIST_OK)
    {
        // psettings->values may contain some data now. It is OK, the data
        // will be destroyed when cleanup_settings() is called.
        print_error(errFailedToLoadValues, psettings->val_path,
            (err != NULL ? err : error_code_to_string(ec)));
        free(err);
        return 0;
    }
    return 1;
}

int
generate_output(struct SSettings* psettings)
{
    assert(psettings != NULL);
    assert(psettings->main_tg != NULL);
    assert(psettings->is_simplified_mode || psettings->path_tg != NULL);
    assert(psettings->values != NULL);
    
    char* err = NULL;
    EMistErrorCode ec = MIST_OK;
    
    if (psettings->is_simplified_mode)
    {
        assert(psettings->path_tg == NULL);
        ec = mist_tg_set_values_impl(psettings->main_tg, psettings->values);
        if (ec != MIST_OK)
        {
            print_error(errGenResultFailed, error_code_to_string(ec));
            return 0;
        }
        
        // generate the contents of the resulting document
        CGrowingArray* vals = mist_tg_evaluate_impl(psettings->main_tg);
        if (vals == NULL)
        {
            print_error(errGenResultFailed, errNoMemory);
            return 0;
        }
        assert(grar_get_size(vals) > 0);
        
        // there should be a single resulting value
        if (grar_get_size(vals) != 1)
        {
            print_error(errGenResultFailed, errMultivaluedResult);
            return 0;
        }
        
        // output the result to stdout
        const char* contents = grar_get_element(vals, const char*, 0);
        assert(contents != NULL);
        printf("%s", contents);
    }
    else // regular mode
    {
        // create path to the output file
        const char* path = mist_tg_generate_path_string_impl(psettings->path_tg,
            psettings->values, &err);
        if (path == NULL)
        {
            print_error(errGenOutPathFailed, (err != NULL ? err : errUnspecifiedError));
            free(err);
            return 0;
        }
        assert(err == NULL);
        
        // generate the output file
        ec = mist_tg_generate_file_impl(psettings->main_tg, path, psettings->values, &err);
        if (ec != MIST_OK)
        {
            print_error(errGenResultFailed, (err != NULL ? err : error_code_to_string(ec)));
            free(err);
            return 0;
        }
    }

    return 1;
}

///////////////////////////////////////////////////////////////////////////
