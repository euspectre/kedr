// Implementation of common and utility stuff

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

#include <sstream>
#include <cassert>

#include "Common.h"

using namespace std;

///////////////////////////////////////////////////////////////////////
// Strings
const string lineString = "line";

// Whitespace characters
extern const string whitespaceList = " \t\n\r\v\a\b\f";

// Names of the "document" and "block" groups
extern const string documentGroupName = "document";
extern const string blockGroupName = "block";
///////////////////////////////////////////////////////////////////////
void 
trimString(std::string& s)
{
    if (s.length() == 0) {
        return;
    }
    
    string::size_type beg = s.find_first_not_of(whitespaceList);
    if (beg == string::npos) {
        // the string consists entirely of whitespace characters
        s.clear();
        return;
    }
    
    string::size_type end = s.find_last_not_of(whitespaceList);
    string t(s, beg, end - beg + 1);
    s.swap(t);
    
    return;
}

std::string
formatErrorMessage(int lineNumber, const std::string & text)
{
    ostringstream err;
    err << lineString << " " << lineNumber << ": " << text;
    return err.str();
}


