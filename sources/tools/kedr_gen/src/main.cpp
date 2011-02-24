/* ========================================================================
 * Copyright (C) 2010-2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <iostream>
#include <iomanip>
#include <cstddef>
#include <cstdlib>

#include "ValueLoader.h"
#include "TemplateLoader.h"
#include "Generator.h"

using namespace std;

///////////////////////////////////////////////////////////////////////
// Common data
const string appName = "kedr_gen";

///////////////////////////////////////////////////////////////////////
// Output information about the usage of the tool
static void
usage();

///////////////////////////////////////////////////////////////////////
int 
main(int argc, char* argv[])
{
    if (argc < 3) {
        usage();
        return EXIT_SUCCESS;
    }
    string dataFile = argv[2];
    string templatePath = argv[1];
    
    string document;
    
    try {
        // Load data (values of the parameters)
        CValueLoader valueLoader;
        valueLoader.loadValues(dataFile);
        
        // Load the templates
        CTemplateLoader templateLoader;
        templateLoader.loadValues(templatePath);
        
        // Generate the resulting document
        CGenerator generator;
        generator.generateDocument(valueLoader.getValueGroups(),
            templateLoader.getDocumentGroup(),
            templateLoader.getBlockGroup(),
            document);
    } 
    catch (bad_alloc& e) {
        cerr << "Error: not enough memory" << endl;
        return EXIT_FAILURE;
    }
    catch (CValueLoader::CLoadingError& e) {
        cerr << "Failed to load " << dataFile << ": " << e.what() << endl;
        return EXIT_FAILURE;
    }
    catch (CTemplateLoader::CLoadingError& e) {
        cerr << "Failed to load templates from " << templatePath 
             << ": " << e.what() << endl;
        return EXIT_FAILURE;
    }
    catch (runtime_error& e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    
    // Output the result
    cout << document.c_str();
    return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////
static void 
usage()
{
    cout << "Usage: " << appName << " "
         << "<template directory> " 
         << "<data file>" << endl;
    return;
}
