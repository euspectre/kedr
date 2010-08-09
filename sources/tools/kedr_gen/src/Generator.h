#ifndef GENERATOR_H_1900_INCLUDED
#define GENERATOR_H_1900_INCLUDED

#include <stdexcept>
#include <string>

#include <mist_engine.h>

#include "Common.h"

// Objects of this class are responsible for generating the output document
// based on the templates and the values of the attributes.
class CGenerator
{
public:
    // This class represents the exceptions thrown if the generator fails.
    class CGeneratorError : public std::runtime_error 
    {
    public:
        CGeneratorError(const std::string& msg = "") 
            : std::runtime_error(msg) {};
    };
    
public:
    CGenerator();
    ~CGenerator();

    // Generate the output document and store it in 'document'.
    // The function may throw exceptions (including but not limited to
    // CGeneratorError).
    void 
    generateDocument(const std::vector<ValueList> & groups,
        const ValueList & documentTemplates,
        const ValueList & blockTemplates,
        std::string & document);
    
private:
    CMistTGroup* tgDocument;
    CMistTGroup* tgBlock;
};

#endif // GENERATOR_H_1900_INCLUDED
