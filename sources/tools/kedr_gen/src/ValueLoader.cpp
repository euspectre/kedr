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

#include <iterator>
#include <fstream>
#include <cassert>

#include "ValueLoader.h"

using namespace std;

///////////////////////////////////////////////////////////////////////
// Strings
const string groupMarker = "[group]";
const string begMarker = "=>>";
const string endMarker = "<<";

// Errors
const string errOpenFailed = "unable to open file ";
const string errReadFailed = "unable to read file ";
const string errEqExpected = "\'=\' is missing";
const string errNameExpected = "name is missing";
const string errOnlyWhitespaceAllowed = 
    "only whitespace characters are allowed after ";
const string errEndMarkerExpected = "\"<<\" is missing";

///////////////////////////////////////////////////////////////////////
CValueLoader::CValueLoader()
{}

void 
CValueLoader::loadValues(const std::string& filePath)
{
    assert(valueGroups.size() == 0);
    vector<ValueList> groups(1); // create one element - for the globals
    
    ifstream inputFile(filePath.c_str());
    if (!inputFile) {
        throw CLoadingError(errOpenFailed + filePath);
    }
    
    int lineNumber = 0;
    while (loadValueGroup(inputFile, groups.back(), lineNumber)) {
        groups.resize(groups.size() + 1);
    }
    
    valueGroups.swap(groups);
    return;
}

bool 
CValueLoader::loadValueGroup(std::istream& inputStream, 
    ValueList& valueGroup, int& lineNumber)
{
    assert(valueGroup.empty());
        
    string line;
    while (getline(inputStream, line)) {
        ++lineNumber;
        trimString(line);

        // If the line is blank or is a comment, skip it.
        if (line.empty() || line.at(0) == '#') {
            continue;
        }
        
        if (line == groupMarker) {
            // reached the end of the current group, nothing more to do
            return true;
        }
        
        if (line.length() > groupMarker.length() 
            && line.compare(0, groupMarker.length(), groupMarker) == 0)
        {
            string err = formatErrorMessage(lineNumber, 
                    errOnlyWhitespaceAllowed + "\"" + groupMarker + "\"");
            throw CLoadingError(err);
        }
        
        combineSplitLine(line, inputStream, lineNumber);
        
        // Extract the name of the parameter.
        CValue v;
        string::size_type posEq = line.find('=');
        
        if (posEq == string::npos) {
            string err = formatErrorMessage(lineNumber, errEqExpected);
            throw CLoadingError(err);
            
        } else if (posEq == 0) {
            string err = formatErrorMessage(lineNumber, errNameExpected);
            throw CLoadingError(err);
        }
        
        string::size_type pos = line.find_last_not_of(whitespaceList, 
            posEq - 1);
        v.name = line.substr(0, pos + 1);

        if (line.compare(posEq, begMarker.length(), begMarker) == 0) {
            // Possibly a multi-line value
            if (line.length() - posEq != begMarker.length()) {
                string err = formatErrorMessage(lineNumber, 
                    errOnlyWhitespaceAllowed + "\"" + begMarker + "\"");
                throw CLoadingError(err);
            }
            
            loadMultiLineValue(inputStream, v.value, lineNumber);
            
        } else {
            // A single-line value
            pos = line.find_first_not_of(whitespaceList, posEq + 1);
            if (pos != string::npos) {
                v.value = line.substr(pos);
            }
        }
        
        // Now both the name and the value must be prepared.
        valueGroup.push_back(v);
    }
    
    return false;
}

void 
CValueLoader::combineSplitLine(std::string& str, std::istream& inputStream,
    int& lineNumber)
{
    assert(str.length() >= 1);
    string::size_type length = str.length();
    
    while (str.at(length - 1) == '\\') {
        str.erase(--length);
        if (length != 0 && str.at(length - 1) == '\\') {
           // "escaped" backslash, nothing more to do: no line break
           return;
        }
        
        // remove trailing whitespace chars
        string::size_type pos = str.find_last_not_of(whitespaceList);
        if (pos == string::npos) {
            // the string consists entirely of whitespace characters
            str.clear();
        } else {
            str.erase(pos + 1);
        }
        
        string line;
        if (! getline(inputStream, line)) {
            return;
        }
        ++lineNumber;
        
        trimString(line);
        if (line.empty()) {
            return;
        }
        
        if (! str.empty()) {
            str = str + " " + line;
        } else {
            str = line;
        }
        length = str.length();
    }
    
    return;
}

void 
CValueLoader::loadMultiLineValue(std::istream& inputStream, 
    std::string& value, int& lineNumber)
{
    assert(value.empty());
    
    do {
        string line;
        if (! getline(inputStream, line)) {
            string err = formatErrorMessage(lineNumber, 
                errEndMarkerExpected);
            throw CLoadingError(err);
        }
        ++lineNumber;
        
        string trimmed = line;
        trimString(trimmed);
        if (trimmed == endMarker) {
            break;
        }
        
        if (value.empty()) {
            value = line;
        } else {
            value = value + "\n" + line;
        }
    }
    while (true);
    
    return;
}
