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

#include <cassert>
#include <algorithm>
#include <sstream>

#include "Generator.h"

//<>
#include <iostream>
//<>

using namespace std;

///////////////////////////////////////////////////////////////////////
// Strings
const string begMarker = "<$";
const string endMarker = "$>";

// Error messages
const string errMistInitFailed = 
    "failed to initialize MiST Engine with the requested version";
const string errNoMainTemplate = 
    "the main template of the following group is missing: ";
const string errTGCreateFailed = 
    "failed to create template group";
const string errSetValuesFailed = 
    "failed to set values of the template parameters";
const string errGenFailed = 
    "failed to generate data by the template";
const string errMultivaluedMain = 
    "the main template is multi-valued, which is not allowed here";
    
///////////////////////////////////////////////////////////////////////
// Local typedefs
typedef std::vector< CMistNameValuePair > TMistParameters;

///////////////////////////////////////////////////////////////////////
// A comparator to be used when searching for CValue objects by name
class CNameEquals : public std::unary_function<std::string, bool>
{
public:
    CNameEquals(const std::string name)
        : targetName(name)
    {}
    
    bool
    operator ()(const CValue & v) const
    {
        return (v.name == targetName);
    }
    
private:
    const std::string targetName; // what to look for 

private:
    CNameEquals(); // do not allow to create without the target string
};

///////////////////////////////////////////////////////////////////////

// Find the main template in 'templates', i.e. the one with the same name as
// the template group has ('groupName'), and return its index. Throw if 
// not found because each template group must have a main template.
static size_t 
findMainTemplateIndex(const ValueList & templates, 
    const std::string & groupName);

// Create a template group - do not forget to destroy it (mist_tg_destroy)
// when it is no longer needed. The function will throw in case of failure.
static CMistTGroup*
createTemplateGroup(const ValueList & templates, 
    const std::string & groupName);

// Add CMistNameValuePair structure filled with pointers from 'name' and
// 'value' to 'where' array. Only the pointers are copied, the contents of 
// the name and values are not. This results in the requirement that the 
// life span of 'name' (and 'value') must coincide with or contain the 
// life span of 'where'.
static void
addParameter(TMistParameters & where, 
    const std::string & name, const std::string & value);

// This function is equivalent to calling addParameter() for 'where' for 
// each element from 'parameterList' in order.
static void
addParameterList(TMistParameters & where, const ValueList & parameterList);

// Generate the resulting string using the template group 'tGroup' and 
// the specified parameters.
static void
generateData(CMistTGroup & tGroup, const TMistParameters & parameters,
    std::string & result);

///////////////////////////////////////////////////////////////////////
CGenerator::CGenerator()
    : tgDocument(NULL), tgBlock(NULL)
{
    if (mist_engine_init(MIST_ENGINE_API_MAX_VERSION) != MIST_OK) {
        throw CGeneratorError(errMistInitFailed);
    }
}

CGenerator::~CGenerator()
{
    if (tgDocument != NULL) {
        mist_tg_destroy(tgDocument);
    }
    
    if (tgBlock != NULL) {
        mist_tg_destroy(tgBlock);
    }
}

void 
CGenerator::generateDocument(const std::vector<ValueList> & groups,
    const ValueList & documentTemplates,
    const ValueList & blockTemplates,
    std::string & document)
{
    assert(groups.size() >= 1);
    
    if (tgDocument != NULL) {
        mist_tg_destroy(tgDocument);
        tgDocument = NULL;
    }
    
    if (tgBlock != NULL) {
        mist_tg_destroy(tgBlock);
        tgBlock = NULL;
    }
    
    tgDocument = createTemplateGroup(documentTemplates, documentGroupName);
    tgBlock = createTemplateGroup(blockTemplates, blockGroupName);
    assert(tgDocument != NULL);
    assert(tgBlock != NULL);
    
    TMistParameters documentParameters;
    TMistParameters blockParameters;
    
    // Set the parameters of the document (globals)
    addParameterList(documentParameters, groups.at(0));
    
    // We need to keep the values of the blocks available until 
    // the document is evaluated.
    vector<string> blocks(groups.size() - 1);
    
    if (groups.size() > 1) {
        // Start with the second element (if any). 
        for (size_t i = 0; i < blocks.size(); ++i) {
            blockParameters.clear(); 
            addParameterList(blockParameters, groups.at(i + 1));
            
            // It is necessary to add all the parameter values to the 
            // document too to create simple list-like structures from them:
            // for example, to create a list of section names or the like.
            addParameterList(documentParameters, groups.at(i + 1));
            
            generateData(*tgBlock, blockParameters, blocks.at(i));
            addParameter(documentParameters, blockGroupName, blocks.at(i));
        }
    }
    
    generateData(*tgDocument, documentParameters, document);
    return;
}

static size_t 
findMainTemplateIndex(const ValueList & templates, 
    const std::string & groupName)
{
    ValueList::const_iterator pos = find_if(
        templates.begin(), 
        templates.end(), 
        CNameEquals(groupName));
    
    if (pos == templates.end()) {
        throw CGenerator::CGeneratorError(errNoMainTemplate + "\"" 
            + groupName + "\"");
    }
    return (pos - templates.begin());
}

static CMistTGroup*
createTemplateGroup(const ValueList & templates, 
    const std::string & groupName)
{
    size_t mainIndex = findMainTemplateIndex(templates, groupName);
    
    CMistNameValuePair* source = new CMistNameValuePair[templates.size()];
    for (size_t i = 0; i < templates.size(); ++i) {
        source[i].name = templates.at(i).name.c_str();
        source[i].val  = templates.at(i).value.c_str();
    }
    
    size_t badElement = (size_t)-1;
    char* whatFailed = NULL;
    
    CMistTGroup* tg = NULL;
    EMistErrorCode ec = mist_tg_create(
        &tg,
        source,
        templates.size(),
        mainIndex,
        begMarker.c_str(),
        endMarker.c_str(),
        &badElement,
        &whatFailed
    );    
    delete [] source;
    
    if (ec != MIST_OK) {
        ostringstream err;
        err << errTGCreateFailed << " \"" << groupName << "\"";
        
        if (badElement != (size_t)(-1)) {
            err << " (template: \"" << templates.at(badElement).name 
                << "\")";
        }
        
        if (whatFailed != NULL) {
            err << ": " << whatFailed;
        } else {
            err << ": unspecified error";
        }
        
        free(whatFailed);
        throw (CGenerator::CGeneratorError(err.str()));
    }
    return tg;
}

static void
addParameter(TMistParameters & where,
    const std::string & name, const std::string & value)
{
    assert(! name.empty());
    
    CMistNameValuePair t;
    t.name = name.c_str();
    t.val  = value.c_str();
    
    where.push_back(t);
    return;
}

static void
addParameterList(TMistParameters & where, const ValueList & parameterList)
{
    ValueList::const_iterator i = parameterList.begin();
    for ( ; i != parameterList.end(); ++i) {
        addParameter(where, i->name, i->value);
    }
    return;
}

static void
generateData(CMistTGroup & tGroup, const TMistParameters & parameters,
    std::string& result)
{
    if (mist_tg_clear_values(&tGroup) != MIST_OK 
        || mist_tg_set_values(&tGroup, 
                &parameters[0], parameters.size()) != MIST_OK)
    {
        throw CGenerator::CGeneratorError(errSetValuesFailed);
    }

    const char** presult;
    size_t nvals;
    
    if (mist_tg_evaluate(&tGroup, &presult, &nvals) != MIST_OK) {
        throw CGenerator::CGeneratorError(errGenFailed);
    }
    assert(nvals != 0);
    
    if (nvals > 1) {
        throw CGenerator::CGeneratorError(errMultivaluedMain);
    }
    
    string s = presult[0];
    result.swap(s);
    return;
}
