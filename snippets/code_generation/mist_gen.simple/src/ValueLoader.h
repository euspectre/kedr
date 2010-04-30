#ifndef VALUELOADER_H_1121_INCLUDED
#define VALUELOADER_H_1121_INCLUDED

#include <stdexcept>
#include <string>
#include <vector>

#include "Common.h"

// CValueLoader - loads the data file and extracts parameters
class CValueLoader
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
    typedef std::vector<ValueList> ValueGroups;
    
public:
    CValueLoader();
    
    // Load data from the file specified. 
    // Throws CValueLoader::CLoadingError with appropriate message if 
    // not successful.
    // 
    // The file should have the following format:
    //      global definitions
    //
    //      [group]
    //      definitions1
    //
    //      [group]
    //      definitions2
    //      ...
    // Here, global definitions, definitions1 and the like have the same 
    // syntax as in the value files used by MiST Engine (or .cfg used 
    // by T2C).
    //
    // "[group]" is case sensitive, so are the names and of the parameters.
    void loadValues(const std::string & filePath);
    
    // Accessor for the structure containing the data loaded.
    const ValueGroups &
    getValueGroups() const
    {
        return valueGroups;
    }
    
private:
    // Groups of values.
    // The first group (#0) contains the global values, each of 
    // the remaining groups corresponds to a single entity to be generated.
    ValueGroups valueGroups; 
    
private: // implementation-related stuff
    // Reads the stream and loads the next value group to 'valueGroup'.
    // The function stops if the end of stream or "[group]" is encountered,
    // whatever comes first.
    // The function returns true if there are more value groups to be 
    // loaded after it returns, false otherwise, i.e. if there are 
    // no groups left.
    // 
    // 'lineNumber' will be incremented each time a new line is read from
    // the stream (useful for error reporting).
    static bool 
    loadValueGroup(std::istream & inputStream, ValueList & valueGroup,
        int& lineNumber);
    
    // Combines possibly split lines.
    // If 'line' ends with a single backslash ('\') character, the function 
    // replaces it with a space (' '), reads another line from 'inputStream'
    // and so on until 'inputStream' is depleted or a line without backslash
    // at the end is encountered.
    // That is, the function combines the "logical lines" broken with '\'
    // into a single "physical line".
    // The whitespace characters after '\' are ignored.
    // Note that if the string ends with two consecutive backslash 
    // characters, they are not treated as a line break, but rather as 
    // an "escaped" backslash character, that is, the last one will be 
    // removed but the line will not be considered broken.
    // The function also trims trailing whitespace characters from 
    // the resulting line.
    // 
    // On entry, 'line' must contain at least one character.
    static void 
    combineSplitLine(std::string & line, std::istream & inputStream,
        int& lineNumber);
    
    // Load a multi-line value from the stream. The line ending with "=>>"
    // must have been already read.
    static void 
    loadMultiLineValue(std::istream & inputStream, std::string & value,
        int& lineNumber);
};

#endif // VALUELOADER_H_1121_INCLUDED
