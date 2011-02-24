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

#include <unistd.h>
#include <dirent.h>
#include <limits.h>

#include <iterator>
#include <fstream>

//<>
#include <iostream>
//<>

#include <cassert>

#include "TemplateLoader.h"

using namespace std;

///////////////////////////////////////////////////////////////////////
// Strings

// Extension that the names of the templates file have.
const string extension = ".tpl";

// Error messages
const string errNoDir = 
    "directory does not exist or cannot be accessed: ";
const string errNoCwd = 
    "failed to obtain the path to the current directory";
const string errNoGroupDir = 
    "not found template group: ";
const string errReadFailed = "unable to read file ";

///////////////////////////////////////////////////////////////////////
// Constants
const size_t pathBufferSize = ((PATH_MAX < 2048) ? 2048 : PATH_MAX);

///////////////////////////////////////////////////////////////////////
CTemplateLoader::CTemplateLoader()
{}

void 
CTemplateLoader::loadValues(const std::string& templatePath)
{
    static char currentDirectory[pathBufferSize];
    
    // Save the path to the current directory for us to return there later.
    char* cwd = getcwd(&currentDirectory[0], pathBufferSize);
    if (cwd == NULL) {
        throw CLoadingError(errNoCwd);
    }
    
    // Go to 'templatePath' directory and operate from there.
    if (chdir(templatePath.c_str()) != 0) {
        throw CLoadingError(errNoDir + templatePath);
    }
    
    // Store the data in locals for now, update the appropriate fields of 
    // the object right before return 
    // ("Either succeed or have no effect" rule).
    RawTemplates tempDocumentGroup;
    RawTemplates tempBlockGroup;
    
    loadTemplateGroup(documentGroupName, tempDocumentGroup);
    loadTemplateGroup(blockGroupName, tempBlockGroup);
    
    // Return to the previously saved directory. 
    if (chdir(cwd) != 0) { 
        throw CLoadingError(errNoDir + string(cwd));
    }
    
    documentGroup.swap(tempDocumentGroup);
    blockGroup.swap(tempBlockGroup);
    return;
}

void 
CTemplateLoader::loadTemplateGroup(const std::string& name, 
    RawTemplates& templates)
{
    assert(templates.empty());
    
    // Traverse the directory and find all files with .tpl extension.
    DIR* d = opendir(name.c_str());
    if (d == NULL) {
        throw CLoadingError(errNoGroupDir + "\"" + name + "\"");
    }
    
    struct dirent* entry = readdir(d);
    while (entry != NULL)
    {
        string fileName = entry->d_name;
        if (fileName.length() > extension.length()
            && 0 == fileName.compare(fileName.length() - extension.length(),
                    extension.length(), extension)) 
        {
            try {
                CValue v;
                loadTemplateFile(name, fileName, v);
                templates.push_back(v);
            }
            catch (...) {
                closedir(d);
                throw;
            }
        }
        entry = readdir(d);
    } // end while
    
    closedir(d);
    return;
}

void 
CTemplateLoader::loadTemplateFile(const std::string& groupName, 
    const std::string& fileName, CValue& rawTemplate)
{
    assert(! groupName.empty());
    assert(fileName.length() > extension.length());
    assert(0 == fileName.compare(fileName.length() - extension.length(),
                extension.length(), extension));
    
    // Cut off the extension
    string name = fileName.substr(0, 
        fileName.length() - extension.length());
    
    // Read the whole template file at once
    string path = groupName + "/" + fileName;
    
    // No need to pass 'ios_base::binary' to handle newlines properly.
    // We operate on text files anyway and the output routines will convert
    // the newlines as appropriate.
    ifstream in(path.c_str()/*, ios_base::binary*/);
    if (! in) {
        throw CLoadingError(errReadFailed + path);
    }
    
    string fileData((istreambuf_iterator<char>(in)), 
        istreambuf_iterator<char>()); 
    
    rawTemplate.name.swap(name);
    rawTemplate.value.swap(fileData);
    return;
}
