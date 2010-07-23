// main.c
// Provides main() function for MiST Engine executable.

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
#include <assert.h>

#include "mist_exec.h"

///////////////////////////////////////////////////////////////////////////
// Globals
///////////////////////////////////////////////////////////////////////////
extern struct SSettings settings; // defined in mist_exec.c

///////////////////////////////////////////////////////////////////////////  
// Main
///////////////////////////////////////////////////////////////////////////
int
main(int argc, char* argv[])
{
    if (!init(argc, argv, &settings))
    {
        cleanup_settings(&settings);
        return EXIT_FAILURE; 
    }
    
    if (!load_templates(&settings))
    {
        cleanup_settings(&settings);
        return EXIT_FAILURE; 
    }
    
    if (!load_param_values(&settings))
    {
        cleanup_settings(&settings);
        return EXIT_FAILURE; 
    }
    
    if (!generate_output(&settings))
    {
        cleanup_settings(&settings);
        return EXIT_FAILURE;
    }
   
    cleanup_settings(&settings);
    return EXIT_SUCCESS;
}
