/// mist_exec.h
/// Global definitions for MiST Engine executable.

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

#ifndef MIST_EXEC_H_2018_INCLUDED
#define MIST_EXEC_H_2018_INCLUDED
    
///////////////////////////////////////////////////////////////////////////
struct CStringMap_;         // forward decl
struct CMistTemplateGroup_; // forward decl

// An aggregate for program settings.
struct SSettings
{
    // Nonzero if "simplified mode" of operation is requested by the user,
    // 0 otherwise.
    int is_simplified_mode;

    // Path to the file (in simplified mode) or directory (in regular mode)
    // containing the template(s). 
    // The path will be made absolute upon loading.
    char* tpl_path;
    
    // Path to the file containing the values for the parameters used in 
    // the template(s).
    // The path will be made absolute upon loading.
    char* val_path;
    
    // The main template group: the contents of the resulting document will be
    // generated from it. 
    struct CMistTemplateGroup_* main_tg;
    
    // The template group that defines the path to the output file 
    // (regular mode only). 
    struct CMistTemplateGroup_* path_tg;
    
    // A name-value map (multimap, actually) containing the values to be set
    // for the attributes in the templates.
    struct CStringMap_* values;
};

///////////////////////////////////////////////////////////////////////////

// Initialize the code generator: set global variables, process the arguments 
// specified in the command line, etc.
// Return nonzero if successful, 0 otherwise (particularly if there are too 
// few or too much arguments or an unknown option is specified).
// If '--help' or '--version' option is encountered,
// this function displays the requested information and the program exits
// with EXIT_SUCCESS status.
int
init(int argc, char* argv[], struct SSettings* psettings);

// Load template group (regular mode) or a single template (simplified mode)
// specified by the user.
// The function returns nonzero if successful, 0 otherwise.
// See the description of mist_tg_* functions in mist_base.h for details on how
// the templates are loaded.
//
// [NB] One thing to point out. In regular mode, if the name of the template directory 
// ends with "-t2c", this suffix is ignored when looking for the .cfg file but 
// not for the main template (the consequence of having part of the code base the same
// as for T2C system). 
// It is recommended to avoid "-t2c" suffix in the names of template directories.
int
load_templates(struct SSettings* psettings);

// Load parameter values listed in the input file into a string map.
// The function returns nonzero if successful, 0 otherwise.
int
load_param_values(struct SSettings* psettings);

// Generate the output document from the loaded template(s) and parameter
// values. The document is output to stdout in the simplified mode or to
// the required file in the regular mode.
// The function returns nonzero if successful, 0 otherwise.
int
generate_output(struct SSettings* psettings);

// Clean up the contents of a settings object: free memory, etc.
// This function must be called before exiting the program normally or with 
// an exit() call.
void 
cleanup_settings(struct SSettings* psettings);

// Like fprintf(stderr, format, ...) with the following exceptions:
// 1. A common prefix is output at the beginning. It usually looks like this:
//    "<app_name>: "
// 2. A newline is output at the end.
// [NB] Similar to fprintf, the variable argument list can be empty.
int 
print_error(const char* format, ...);
  
#endif //MIST_EXEC_H_2018_INCLUDED
