#ifndef TEMPLATELOADER_H_1850_INCLUDED
#define TEMPLATELOADER_H_1850_INCLUDED

#include <stdexcept>
#include <string>
#include <vector>

#include "Common.h"

// Objects of this class are responsible for loading MiST templates from
// files. The templates are not parsed, just copied into memory as they are.
// The goal is to get the pairs (template name, template contents) and store
// them for later use by the document generator.
class CTemplateLoader
{
public:
    // This class represents the exceptions thrown if loading fails.
    class CLoadingError : public std::runtime_error 
    {
    public:
        CLoadingError(const std::string& msg = "") 
            : std::runtime_error(msg) {};
    };
    
public: 
    // A group of templates in a raw format - pairs (name, contents).
    typedef ValueList RawTemplates;
    
public:
    CTemplateLoader();
    
    // Load the templates from the appropriate subdirectories of 
    // 'templatePath'. Currently, there should always be two of them:
    // - document - a template group to generate the output document 
    //      as a whole (compare with 'test_group' group in T2C);
    // - block - a template group for a basic block, a sequence of which
    //      constitutes the body of the resulting document (compare with 
    //      'test_case' group in T2C).
    // Each of both subdirectories must contain at least the main template,
    // document.tpl and block.tpl, respectively and any number of auxiliary
    // templates (.tpl files). 
    // 
    // .cfg files are not required to be present in these two directories 
    // because the settings these files define do not matter for this 
    // example: the template loader does not use the template of the path to
    // the output file.
    // 
    // In addition, "<$" and "$>" are assumed to be the placeholders in the 
    // templates.
    // 
    // All .tpl files in each of these directories will be loaded into 
    // memory. The function will not check if these files are syntactically
    // valid templates. 
    // 
    // The function may throw CLoadingError exception if the loading fails 
    // for some reason (a directory does not exist, etc.).
    void loadValues(const std::string& templatePath);
    
    // Accessor methods.
    const RawTemplates &
    getDocumentGroup() const
    {
        return documentGroup;
    }
    
    const RawTemplates &
    getBlockGroup() const
    {
        return blockGroup;
    }

private:
    // Here group of templates "document" will be stored.
    RawTemplates documentGroup;
    
    // Here group of templates "block" will be stored.
    RawTemplates blockGroup;

private:
    // implementation-related stuff
    
    // Load a group of templates from all the .tpl files contained in 'name'
    // subdirectory of the current working directory.
    // The results are stored in 'templates'.
    // The function throws CLoadingError in case of failure and may throw
    // any other exception as well.
    static void 
    loadTemplateGroup(const std::string& name, RawTemplates& templates);
    
    // Load the template from the file <groupName>/<fileName> into 
    // 'rawTemplate'.
    // The function throws CLoadingError in case of failure and may throw
    // any other exception as well.
    static void 
    loadTemplateFile(const std::string& groupName, 
        const std::string& fileName, CValue& rawTemplate);
};

#endif // TEMPLATELOADER_H_1850_INCLUDED
