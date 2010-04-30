#ifndef COMMON_H_1854_INCLUDED
#define COMMON_H_1854_INCLUDED

#include <string>
#include <vector>

// Common declarations

///////////////////////////////////////////////////////////////////////
// This structure represents a (name, value) pair.
struct CValue
{
    std::string name;
    std::string value;
};

///////////////////////////////////////////////////////////////////////
// Whitespace characters (defined in Common.cpp)
extern const std::string whitespaceList;

// Names of the "document" and "block" groups (defined in Common.cpp)
extern const std::string documentGroupName;
extern const std::string blockGroupName;

///////////////////////////////////////////////////////////////////////
// Helper functions

// Trims the whitespace characters off the string (remove them from both 
// the beginning and the end).
void 
trimString(std::string& s);

// Formats the message like the follwing: "line <n>: <text>"
std::string
formatErrorMessage(int lineNumber, const std::string& text);

///////////////////////////////////////////////////////////////////////
// Type definitions
// A type for a collection of values.
typedef std::vector<CValue> ValueList;

///////////////////////////////////////////////////////////////////////
#endif // COMMON_H_1854_INCLUDED
