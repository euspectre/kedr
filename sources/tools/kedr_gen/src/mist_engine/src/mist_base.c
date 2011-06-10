/// mist.c 
/// Implementation of Minimal String Template engine (MiST) API: 
/// basic functional elements.

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

#include <assert.h>

#include "mist_base.h"
#include "mist_file_utils.h"
#include "mist_string_utils.h"

///////////////////////////////////////////////////////////////////////////
// Structure definitions
///////////////////////////////////////////////////////////////////////////

struct CMistTemplate_;
typedef struct CMistTemplate_ CMistTemplate;

struct CMistPlaceholder_;
typedef struct CMistPlaceholder_ CMistPlaceholder;

/// Types of placeholers
typedef enum EMistPlaceholderType_
{
    /// a plain placeholder: just a reference to an attribute or subtemplate
    MPH_PLAIN   = 0,
    
    /// a placeholder with join(...) directive
    MPH_JOIN    = 1,
    
    /// a placeholder that is used for the conditional inclusion of templates
    MPH_COND    = 2,
    
    /// the number of placeholder types
    MPH_NTYPES
} EMistPlaceholderType;

/// A structure representing a placeholder.
/// Note that the templates '*tpl_then' and '*tpl_else' are not owned by the
/// template group like other templates do. It is the placeholder that is 
/// responsible for them.
struct CMistPlaceholder_
{
    /// Type of the placeholder.
    EMistPlaceholderType type;
    
    /// Name of the placeholder.
    char* name;
    
    /// The template this placeholder refers to.
    /// If type is MPH_COND, this template represents the result of the  
    /// conditional construct.
    CMistTemplate* tpl;
    
    /// Separator string used to join the values of the template.
    /// Meaningful for MPH_JOIN type only.
    char* sep;
    
    /// A template representing the conditional expression to be evaluated.
    /// Meaningful for MPH_COND type only. Must not be NULL in this case.
    CMistTemplate* tpl_cond;
    
    /// A template to be included if the corresponding value of 'tpl' 
    /// is NOT EMPTY. 
    /// Meaningful for MPH_COND type only. Must not be NULL in this case.
    CMistTemplate* tpl_then;
    
    /// A template to be included if the corresponding value of 'tpl' 
    /// is EMPTY.
    /// Meaningful for MPH_COND type only. Must not be NULL in this case.
    CMistTemplate* tpl_else;
    
    /// If the conditional expression has the following form:
    /// "concat(<name>)", 'is_concat' must be nonzero, 0 otherwise.
    /// Meaningful for MPH_COND type only.
    int is_concat;
};

/// A template itself is a sequence of interleaving string chunks and
/// pointers to placeholder structures. Different placeholder structures are
/// used for different placeholders even if these have the same name.
/// The sequence begins and ends with string chunks. The string chunks may
/// contain empty strings.
struct CMistTemplate_
{
    /// Name of the template. It is often the name of the file from which 
    /// the template was loaded.
    char* name;
    
    /// A collection of current values (strings) of this template.
    CGrowingArray vals;

    /// An array of string chunks.
    CGrowingArray sch;  
    
    /// An array of placeholder structures.
    CGrowingArray ph;   

    /// This flag is nonzero if the template has already been evaluated
    /// during the current evaluation of the main template in the group,
    /// 0 otherwise.
    /// This is to avoid evaluation of the template when it is not necessary.
    int is_evaluated;
};

/// A structure representing a group of templates. Placeholders are common
/// only within such a group.
/// Each template in the group must have a unique name.
struct CMistTemplateGroup_
{
    /// The templates (sorted by name for faster searching)
	CGrowingArray tpl;  	
	
	/// A pointer to the main (top-level) template.
	CMistTemplate* main;
};

/// Token types (see CMistToken below).
/// Meaningful values of this type should be nonnegative and less than MTT_NTYPES
typedef enum EMistTokenType_
{
    MTT_SCH     = 0,    /// string chunk 
    MTT_PH      = 1,    /// placeholder with or without join()
    MTT_IF      = 2,    /// if-keyword
    MTT_ELSE    = 3,    /// else-keyword
    MTT_ENDIF   = 4,    /// endif-keyword
    MTT_NTYPES          /// number of token types
} EMistTokenType;

/// This structure represents a token located in the string which a template 
/// is to be loaded from. 
/// 
/// If the token represents a placeholder, [beg, end) specify its contents, i.e
/// 'beg' points right after the begin marker, 'end' points to the end marker.
/// 
/// If the token represents a 'keyword placeholder' except 'if' ('else'
/// 'endif), [beg, end) is a valid range (probably empty) somewhere
/// between the begin and end markers but it is not specified which one.
/// 
/// As for "if" keyword placeholder, [beg, end) define the conditional expression
/// (it follows "if" keyword) with whitespace chars stripped from both sides of it.
typedef struct CMistToken_
{
    /// Type of the token
    EMistTokenType type;
    
    /// [beg, end) define a range the token occupies in the source string
    const char* beg;
    const char* end;
} CMistToken;

///////////////////////////////////////////////////////////////////////////
// Private variables
///////////////////////////////////////////////////////////////////////////

// Whitespace chars
static const char wspace[] = " \t\n\r";
static size_t sz_wspace = sizeof(wspace) / sizeof(wspace[0]) - 1;

// Format strings for various errors
static const char* errNoBeginMarker = 
    "no matching begin marker found for the end marker (line %u of the template)";
static const char* errNoEndMarker = 
    "no matching end marker found for the begin marker (line %u of the template)";
static const char* errTooManyBeginMarkers = 
    "too many begin markers found for the end marker (line %u of the template)";
static const char* errInvalidPHName = 
    "invalid placeholder: \"%s\" (line %u of the template)";
static const char* errUnableToLoadConf = 
    "unable to load configuration from \"%s\": %s";
static const char* errDupParam = 
    "\"%s\" is set more than once in the .cfg file in \"%s\"";
static const char* errMissingParam = 
    "required parameter \"%s\" is empty or missing from the .cfg file in \"%s\"";
static const char* errReadDirFailed = 
    "unable to read the contents of \"%s\"";
static const char* errReadFileFailed = 
    "unable to read the contents of \"%s\"";
static const char* errWriteFileFailed = 
    "unable to write to \"%s\"";
static const char* errNoTplFiles = 
    "template files (*.tpl) are not found in \"%s\"";
static const char* errFailedToLoadTpl = 
    "failed to load template \"%s\": %s";
static const char* errMainTplMultivalued = 
    "multi-valued top-level template in \"%s\" template group";
static const char* errCreateDirFailed = 
    "failed to create directory for file \"%s\"";
static const char* errIfWithoutEndif = 
    "found \"if\" without matching \"endif\" (line %u of the template)";
static const char* errElseWithoutEndif = 
    "found \"else\" without matching \"endif\" (line %u of the template)";
static const char* errElseWithoutIf = 
    "found \"else\" without matching \"if\" (line %u of the template)";
static const char* errEndifWithoutIf = 
    "found \"endif\" without matching \"if\" (line %u of the template)";
static const char* errBadGroupName = 
    "\"%s\" is not allowed as the name of a template group (directory: \"%s\")";
static const char* errBadTemplateName = 
    "\"%s\" is not allowed as the name of a template (file: \"%s\")";
static const char* errInvalidTemplateName = 
    "invalid name of a template: \"%s\"";
static const char* errNoMainTemplate = 
    "the main template (\"%s\") is missing from the template group";
static const char* errUnspecifiedError = 
    "unspecified error";
static const char* errEmptyParam = 
    "parameter \"%s\" has empty value in the .cfg file in \"%s\"";

///////////////////////////////////////////////////////////////////////////
// Declarations: "private methods"
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// CMistPlaceholder

/// Create a placeholder with given name (a copy of 'name') and separator
/// (a copy of 'separator' if it is not NULL, NULL otherwise). 
/// If separator == NULL, 'type' will be set to MPH_PLAIN, otherwise to MPH_JOIN.
/// Return a pointer to the newly created structure if successful, NULL otherwise.
static CMistPlaceholder*
mist_placeholder_create(const char* name, const char* separator);

/// Destroys the placeholder structure.
static void
mist_placeholder_destroy(CMistPlaceholder* ph);

/// Return the the number of characters in the value(s) in this placeholder 
/// after the evaluation of the templates and possibly joining.
/// The template referred to by this placeholder must be evaluated before
/// calling this function.
static size_t
mist_placeholder_max_length(CMistPlaceholder* ph);

/// Join the values corresponding to the placeholder with appropriate separator
/// in between and place the result to the specified buffer 'buf'. The buffer
/// must provide enough space to contain the resulting character sequence.
/// Symbol '\0' may not be appended to the sequence.
/// The function returns a pointer to the position in the buffer right after
/// the last written character.
/// Don't call this function for placeholders with type other than MPH_JOIN.
static char* 
mist_placeholder_join_values(const CMistPlaceholder* ph, char* buf);

/// Evaluate the template(s) referenced by this placeholder. 
/// The resulting values are stored as the values of 'ph->tpl'.
static EMistErrorCode
mist_placeholder_evaluate(CMistPlaceholder* ph);

///////////////////////////////////////////////////////////////////////////
// CMistTemplate

/// Create a template structure with a given name (a copy of 'name').
/// The created template still contains no string chunks, no placeholders and
/// values.
/// Return a pointer to the newly created structure if successful, NULL otherwise.
static CMistTemplate*
mist_template_create(const char* name);

/// Destroys the template. The arrays of values, string chunks and placeholders
/// are also destroyed (with their contents).
static void 
mist_template_destroy(CMistTemplate* mt);

/// Clear the colection of values stored in the template and set is_evaluated 
/// flag to 0. 
/// The arrays of placeholders and string chunks remain unchanged.
static void 
mist_template_clear_values(CMistTemplate* mt);

/// Add a copy of the specified value ('val') to the collection of values 
/// in the template.
static EMistErrorCode
mist_template_add_value(CMistTemplate* mt, const char* val);

/// Create a template with the given name ('name') from the specified string ('src') 
/// extracting string chunks and creating appropriate placeholder structures.
/// The function creates a new template structure and returns a pointer to it in *pmt.
/// *pmt will be NULL in case of failure.
///
/// The placeholders in the source string are substrings denoted by the specified
/// begin and end markers (e.g. "<%" and "%>", "$" and "$" (same), etc.)
static EMistErrorCode
mist_template_from_string(CMistTemplate** pmt, const char* name, 
                          const char* src,
                          const char* begin_marker, const char* end_marker,
                          char** error_descr);
                          
/// A qsort-style comparison function for the CMistTemplate* pointers
/// (gets CMistTemplate** values as the arguments).
/// Returns the result of comparision of the templates' names via strcmp().
static int
mist_template_compare(const void* lhs, const void* rhs);

/// Construct the value(s) of the template. All placeholders it contains
/// should already have been connected to appropriate templates.
static EMistErrorCode
mist_template_evaluate(CMistTemplate* mt);

/// Return the number of values that the template will have after it is
/// evaluated. All referenced templates must be evaluated before calling
/// this function.
static size_t
mist_template_num_values(CMistTemplate* mt);

/// Return the number of characters that the longest of the values of the 
/// template will have after evaluation.
static size_t
mist_template_max_length(CMistTemplate* mt);

/// Break the string 'src' into a sequence of tokens ("lexical analysis"),
/// return the sequence as an array of CMistToken* pointers.
/// The function returns a pointer to this array if successful, NULL otherwise.
/// The returned array must be destroyed with its elements and the pointer to it 
/// must be freed when no longer needed.
static CGrowingArray*
mist_template_tokenize_string(const char* src,
                              const char* begin_marker, const char* end_marker,
                              char** error_descr);

/// Determine the type of a token from the specified range [beg, end) and
/// create a CMistToken structure for the token.
/// The pointer to the newly created token will be returned in *ptok.
/// *ptok will be NULL in case of failure.
static EMistErrorCode
mist_token_from_ph_string(CMistToken** ptok, const char* beg, const char* end);

///////////////////////////////////////////////////////////////////////////
/// CMistTemplate - match() and parse() functions.
/// 
/// All these functions take the array of tokens ('tokens') and the index 
/// in this array ('*istart') to begin parsing from. The functions update 
/// '*istart' as they process tokens.
/// 
/// For error reporting, the functions also take the pointer to the source
/// string ('src') being parsed - to determine the line number in case of an
/// error - and 'error_descr'.
/// 
/// As for match() functions, the caller must ensure that they are called with
/// the current token being of the appropriate type.
/// 
/// The functions return one of the following values:
/// - MIST_OK if successful, 
/// - MIST_SYNTAX_ERROR in case of a syntax error, 
/// - MIST_OUT_OF_MEMORY if there is not enough memory.
///////////////////////////////////////////////////////////////////////////

/// Process the array of tokens to create a template ('**pt') with specified
/// name ('name').
static EMistErrorCode
mist_parse_template(CGrowingArray* tokens, size_t* istart,
    CMistTemplate** pt, const char* name,
    const char* src, char** error_descr);
    
/// Process the array of tokens to create a placeholder of some kind ('**pp').
static EMistErrorCode
mist_parse_ph_expression(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr);
    
/// Process the array of tokens to create a plain placeholder or the one with
/// join ('**pp').
static EMistErrorCode
mist_parse_placeholder(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr);
    
/// Process the array of tokens to create a placeholder for a conditional 
/// construct ('**pp').
static EMistErrorCode
mist_parse_conditional(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr);
    
/// Get the string chunk from the current token and add it to the specified
/// template. 
static EMistErrorCode
mist_match_string_chunk(CGrowingArray* tokens, size_t* istart,
    CMistTemplate* mt,
    const char* src, char** error_descr);
    
/// Get the name of the attribute / subtemplate to be evaluated and create
/// a placeholder ('**pp') with this name.
static EMistErrorCode
mist_match_if(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr);

/// Get the "else" token.
static EMistErrorCode
mist_match_else(CGrowingArray* tokens, size_t* istart,
    const char* src, char** error_descr);

/// Get the "endif" token.
static EMistErrorCode
mist_match_endif(CGrowingArray* tokens, size_t* istart,
    const char* src, char** error_descr);

/// Parse the conditional expression in [beg; end) substring. The substring
/// must not be empty and must not begin or end with a whitespace.
/// The expression may be a name of a template or may have the form of 
/// 'concat(<name>)' (so called "concat-expression").
/// 
/// The function returns a copy of the name referred to in the expression.
/// NULL is returned is there is not enough memory. 
/// The returned pointer must be freed when no longer needed.
/// 
/// 'is_concat' must not be NULL. If the expression is a concat-expression,
/// '*is_concat' will be set to 1 by the function, to 0 otherwise.
static char*
mist_parse_cond_expr(const char* beg, const char* end, int* is_concat);

///////////////////////////////////////////////////////////////////////////
// CMistTemplateGroup
                          
/// Look through the templates in the group, find the placeholders that do not
/// refer to any templates present in the group. These placeholders are 
/// considered to refer to the attributes. Create CMistTemplate structures 
/// for these placeholders and add them to the group.
static EMistErrorCode
mist_tg_create_attrs(CMistTemplateGroup* mtg);

/// Recursively process the placeholders in the template 'tpl' itself and in
/// the branch templates of the conditionals it contains (if any). Look for 
/// the placeholders that do not refer to any templates present in the group
/// and collect their names in '*attrs'.
/// This function is used by mist_tg_create_attrs().
/// 'mtg->tpl' array must be sorted before calling this function.
static EMistErrorCode
mist_tg_collect_attrs_for_template(CMistTemplateGroup* mtg, 
    CMistTemplate* tpl, CGrowingArray* attrs);

/// Connect the placeholders to the templates with the same name.
/// The templates referred to by each placeholder must exist in the group.
/// 'mtg->tpl' array must be sorted before calling this function.
static void
mist_tg_connect_templates(CMistTemplateGroup* mtg);

/// Recursively process the placeholders in the template 'tpl' itself and in
/// the branch templates of the conditionals it contains (if any). 
/// Connect each placeholder to an appropriate template in the group.
/// This function is used by mist_tg_connect_templates().
static void
mist_tg_connect_templates_for_template(CMistTemplateGroup* mtg, CMistTemplate* tpl);

/// Load the configuration for the template group to be created from the specified
/// directory. In case of success, '*tpath' will contain the template of the 
/// path to the file to be generated, '*begm' and '*endm' - begin and end markers
/// for the placeholders. These 3 strings should be freed by the caller when no
/// longer needed.
static EMistErrorCode
mist_tg_load_conf(const char* dir, char** tpath, char** begm, char** endm, 
                  char** error_descr);

/// Traverse the directory, read the .tpl files it contains and store their
/// names and the contents (each as a single string) in the specified arrays.
/// The function will create (initialize) 'tpl_names' and 'tpl_strings' but it 
/// is the caller's responsibility to destroy these arrays when they are no
/// longer needed.
static EMistErrorCode
mist_tg_process_dir(const char* dir, 
                    CGrowingArray* tpl_names, CGrowingArray* tpl_strings,
                    char** error_descr);

///////////////////////////////////////////////////////////////////////////
// CMistToken

/// Create a new token structure with the data specified.
/// The function returns a pointer to the newly created structure if successful,
/// NULL if there is not enough memory to complete the operation.
static CMistToken* 
mist_token_create(EMistTokenType type, const char* beg, const char* end);

/// Destroy the specified token structure.
static void
mist_token_destroy(CMistToken* token);

///////////////////////////////////////////////////////////////////////////
// Other functions

/// Copy 'str' to each of 'nvals' strings contained in 'pos' and add appropriate
/// offsets to the elements of 'pos' for them to point right after the copied strings.
/// [NB] This function is used by mist_template_evaluate().
static void
mist_copy_string(char** pos, size_t nvals, const char* str);

/// Copy values of 'ph' to the appropriate strings contained in 'pos' (there are
/// 'nvals' strings there) taking join into account and add appropriate
/// offsets to the elements of 'pos' for them to point right after the copied strings.
/// If there are less than 'nvals' values in 'ph' (but not zero), the last one is
/// copied to the remaining strings.
/// If there are no values in 'ph', the function does nothing as if there were
/// an empty string there.
/// [NB] This function is used by mist_template_evaluate().
static void
mist_copy_placeholder_values(char** pos, size_t nvals, const CMistPlaceholder* ph);

#ifndef NDEBUG
/// Return nonzero if [beg, end) is a valid range (neither 'beg' nor 'end' are
/// NULL and beg <= end). For safety reasons, the following test is not used:
/// beg + strlen(beg) >= end, because the string 'beg' points to may be not 
/// null-terminated.
///
/// The function is intended for use in debug mode only.
static int
mist_is_range_valid(const char* beg, const char* end);
#endif

///////////////////////////////////////////////////////////////////////////
// Implementation: "private methods"
///////////////////////////////////////////////////////////////////////////

/// A destructor for placeholder structures.
/// Can be handy for grar_destroy_with_elements().
/// 'user_data' argument is not used.
static void
mist_placeholder_dtor(void* doomed, void* user_data)
{
    assert(doomed != NULL);
    mist_placeholder_destroy((CMistPlaceholder*)doomed);

    return;
}

/// A destructor for template structures.
/// Can be handy for grar_destroy_with_elements().
/// user_data argument is not used.
static void
mist_template_dtor(void* doomed, void* user_data)
{
    assert(doomed != NULL);
    mist_template_destroy((CMistTemplate*)doomed);

    return;
}

/// A destructor for CMistToken structures.
/// Can be handy for grar_destroy_with_elements().
/// 'user_data' argument is not used.
static void
mist_token_dtor(void* doomed, void* user_data)
{
    assert(doomed != NULL);
    mist_token_destroy((CMistToken*)doomed);

    return;
}

///////////////////////////////////////////////////////////////////////////
static CMistPlaceholder*
mist_placeholder_create(const char* name, const char* separator)
{
    assert(name != NULL);
    assert(!mist_name_is_bad(name));
    
    CMistPlaceholder* ph = (CMistPlaceholder*)malloc(sizeof(CMistPlaceholder));
    if (ph == NULL)
    {
        return NULL;
    }
    
    ph->name = (char*)strdup(name);
    if (ph->name == NULL)
    {
        free(ph);
        return NULL;
    }
    
    if (separator != NULL)
    {
        ph->sep = (char*)strdup(separator);
        if (ph->sep == NULL)
        {
            free(ph->name);
            free(ph);
            return NULL;
        }
        ph->type = MPH_JOIN;
    }
    else
    {
        ph->sep = NULL;
        ph->type = MPH_PLAIN;
    }
    
    ph->is_concat = 0;
    
    // No templates referred to so far
    ph->tpl = NULL;
    ph->tpl_cond = NULL;
    ph->tpl_then = NULL;
    ph->tpl_else = NULL;
    
    return ph;
}

static void
mist_placeholder_destroy(CMistPlaceholder* ph)
{
    assert(ph != NULL);
    assert(ph->name != NULL);
    
    if (ph->type == MPH_COND)
    {
        // tpl, tpl_then and / or tpl_else could be NULL here even 
        // for type == MPH_COND if this function is called to clean up after 
        // some error.
        if (ph->tpl != NULL)
        {
            mist_template_destroy(ph->tpl);
        }
        if (ph->tpl_then != NULL)
        {
            mist_template_destroy(ph->tpl_then);
        }
        if (ph->tpl_else != NULL)
        {
            mist_template_destroy(ph->tpl_else);
        }
        
        // tpl_cond is either NULL (in case of some errors) or a pointer 
        // to a template owned by the template group. The group will destroy 
        // that template when necessary.
    }
    
    free(ph->name);
    free(ph->sep);
    free(ph);
    
    return;
}

static CMistTemplate*
mist_template_create(const char* name)
{
    assert(name != NULL);
    assert(!mist_name_is_bad(name));
    
    CMistTemplate* mt = (CMistTemplate*)malloc(sizeof(CMistTemplate));
    if (mt == NULL)
    {
        return NULL;
    }
    
    mt->name = (char*)strdup(name);
    if (mt->name == NULL)
    {
        free(mt);
        return NULL;
    }
    
    if (grar_create(&(mt->vals)) == 0)    
    {
        free(mt->name);
        free(mt);
        return NULL;
    }
    
    if (grar_create(&(mt->sch)) == 0)    
    {
        grar_destroy(&(mt->vals));
        free(mt->name);
        free(mt);
        return NULL;
    }
    
    if (grar_create(&(mt->ph)) == 0)    
    {
        grar_destroy(&(mt->sch));
        grar_destroy(&(mt->vals));
        free(mt->name);
        free(mt);
        return NULL;
    }
    
    mt->is_evaluated = 0;
    
    return mt;
}

static void 
mist_template_destroy(CMistTemplate* mt)
{
    assert(mt != NULL);
    assert(mt->name != NULL);
    
    free(mt->name);
    
    grar_destroy_with_elements(&mt->vals, NULL, NULL);
    grar_destroy_with_elements(&mt->sch, NULL, NULL);
    grar_destroy_with_elements(&mt->ph, mist_placeholder_dtor, NULL);
    
    free(mt);
    
    return;
}

static void 
mist_template_clear_values(CMistTemplate* mt)
{
    assert(mt != NULL);
    
    // free the values first then clear the array
    size_t sz = grar_get_size(&(mt->vals));
    char** strings = grar_get_c_array(&(mt->vals), char*);
    for (size_t i = 0; i < sz; ++i)
    {
        free(strings[i]);
    }
    grar_clear(&(mt->vals));
    
    mt->is_evaluated = 0;
    return;
}

static EMistErrorCode
mist_template_add_value(CMistTemplate* mt, const char* val)
{
    assert(mt != NULL);
    assert(val != NULL);
    
    char* elem = (char*)strdup(val);
    if (elem == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    if (grar_add_element(&(mt->vals), elem) == 0)
    {
        free(elem);
        return MIST_OUT_OF_MEMORY;
    }
    
    return MIST_OK;
}

static CGrowingArray*
mist_template_tokenize_string(const char* src,
                              const char* begin_marker, const char* end_marker,
                              char** error_descr)
{
    assert(src != NULL);
    assert(begin_marker != NULL);
    assert(end_marker != NULL);
    assert(error_descr != NULL);

    // The markers mustn't be empty
    assert(begin_marker[0] != '\0');
    assert(end_marker[0] != '\0');

    *error_descr = NULL;
    
    CGrowingArray* tokens = (CGrowingArray*)malloc(sizeof(CGrowingArray));
    if (tokens == NULL)
    {
        return NULL;
    }
    if (!grar_create(tokens))
    {
        free(tokens);
        return NULL;
    }
    
    size_t beg_len = strlen(begin_marker);
    size_t end_len = strlen(end_marker);

    const char* str = src;
    const char* beg = NULL;
    const char* end = NULL;

    EMistErrorCode ec = MIST_OK;
    
    while (1)
    {
        beg = strstr(str, begin_marker);
        if (beg == NULL)
        {
            end = strstr(str, end_marker);
            if (end != NULL)
            {
                // found invalid end marker
                ec = MIST_SYNTAX_ERROR;
                mist_format_parse_error(error_descr, errNoBeginMarker, 
                    mist_line_num_for_ptr(src, end));
                break;
            }
            else
            {
                // no more begin and end markers found => just a string chunk
                CMistToken* tok = mist_token_create(MTT_SCH, str, str + strlen(str));
                if (tok == NULL)
                {
                    ec = MIST_OUT_OF_MEMORY;
                    break;
                }
                if (grar_add_element(tokens, tok) == 0)
                {
                    mist_token_destroy(tok);
                    ec = MIST_OUT_OF_MEMORY;
                    break;
                }
                
            } // end if (end != NULL)
            break;
        }

        // beg != NULL here, i.e. found a begin marker
        end = strstr(beg + beg_len, end_marker);

        if (end == NULL) // found a begin marker but no end marker
        {
            ec = MIST_SYNTAX_ERROR;
            mist_format_parse_error(error_descr, errNoEndMarker, 
                mist_line_num_for_ptr(src, beg));
            break;
        }
        
        const char* pos = beg + beg_len; 
		// [pos, end) define the contents of a placeholder of some kind
        
        char* ss = mist_get_substring(pos, end);
        if (ss == NULL)
        {
            ec = MIST_OUT_OF_MEMORY;
            break;
        }
        
        const char* tmp = strstr(ss, begin_marker);
        if (tmp != NULL)
        {
            // error: unexpected begin marker is found
            ec = MIST_SYNTAX_ERROR;
            mist_format_parse_error(error_descr, errTooManyBeginMarkers, 
				mist_line_num_for_ptr(src, end));
			free(ss);
            break;
        }
		free(ss);

        ss = mist_get_substring(str, beg);
        if (ss == NULL)
        {
            ec = MIST_OUT_OF_MEMORY;
            break;
        }
        
        tmp = strstr(ss, end_marker);
        if (tmp != NULL)
        {
            // error: found lone end marker
            ec = MIST_SYNTAX_ERROR;
            mist_format_parse_error(error_descr, errNoBeginMarker, 
				mist_line_num_for_ptr(src, str + (tmp - ss)));
			free(ss);
            break;
        }
		free(ss);
        
        // determine the type of the placeholder and create appropriate token
        CMistToken* tok_ph = NULL;
        ec = mist_token_from_ph_string(&tok_ph, pos, end);
                        
        if (ec != MIST_OK)
        { 
            if (ec == MIST_BAD_NAME)
            {
                char* ph_str = mist_get_substring(pos, end);
                if (ph_str != NULL)
                {
                    *error_descr = (char*)malloc(strlen(errInvalidPHName) + (end - pos) + MAX_NUM_DIGITS + 1);
                    if (*error_descr != NULL)
                    {
                        sprintf(*error_descr, errInvalidPHName, ph_str, 
                            mist_line_num_for_ptr(src, pos));
                    }
                    free(ph_str);
                }
                ec = MIST_SYNTAX_ERROR;
            }
            break;
        }
        assert(tok_ph != NULL);
        
        // found a new string chunk [str, beg) and a new placeholder [pos, end).
        CMistToken* tok_sch = mist_token_create(MTT_SCH, str, beg);
        if (tok_sch == NULL)
        {
            ec = MIST_OUT_OF_MEMORY;
            mist_token_destroy(tok_ph);
            break;
        }
        
        // add the string chunk and the placeholder structure to the template
        if (grar_add_element(tokens, tok_sch) == 0)
        {
            ec = MIST_OUT_OF_MEMORY;
            mist_token_destroy(tok_ph);
            mist_token_destroy(tok_sch);
            break;
        }
        
        if (grar_add_element(tokens, tok_ph) == 0)
        {
            ec = MIST_OUT_OF_MEMORY;
            mist_token_destroy(tok_ph);
            // 'tok_sch' is now owned by 'tokens' and will be destroyed when 
            // this array is destroyed (see below)
            break;
        }
        
        str = end + end_len;
    } // end while (1)
    
    if (ec != MIST_OK)
    {
        grar_destroy_with_elements(tokens, mist_token_dtor, NULL);
        free(tokens);
        return NULL;
    }
    
    return tokens;    
}

static EMistErrorCode
mist_token_from_ph_string(CMistToken** ptok, const char* beg, const char* end)
{
    assert(ptok != NULL);
    assert(mist_is_range_valid(beg, end));
    
    static const char ref_if[] = "if";
    static const size_t ref_if_len = sizeof(ref_if) / sizeof(ref_if[0]) - 1;
    static const char ref_else[] = "else";
    static const size_t ref_else_len = sizeof(ref_else) / sizeof(ref_else[0]) - 1;
    static const char ref_endif[] = "endif";
    static const size_t ref_endif_len = sizeof(ref_endif) / sizeof(ref_endif[0]) - 1;
    
    *ptok = NULL;
    
    // skip the whitespace from both ends of the string [beg, end)
    beg = mist_find_in_range_first_not_of(beg, end, wspace, sz_wspace);
    if (beg == end) // the string contains only whitespace chars
    {
        return MIST_BAD_NAME;
    }
    end = mist_find_in_range_last_not_of(beg, end, wspace, sz_wspace);
    ++end; // step past the last character
    
    // get the end of the first (may be the only) part of the reference
    const char* pos = mist_find_in_range_first_of(beg, end, wspace, sz_wspace);
    size_t rlen = pos - beg;
    
    if (rlen == ref_if_len && strncmp(beg, ref_if, rlen) == 0) // "if"
    {
        pos = mist_find_in_range_first_not_of(pos, end, wspace, sz_wspace);
        if (pos == end) // no conditional expression found
        {
            return MIST_BAD_NAME;
        }
        *ptok = mist_token_create(MTT_IF, pos, end);
        if (*ptok == NULL)
        {
            return MIST_OUT_OF_MEMORY;
        }
    }
    else if (rlen == ref_else_len && strncmp(beg, ref_else, rlen) == 0) // "else"
    {
        if (pos != end) // the placeholder contains more than just "else"
        {
            return MIST_BAD_NAME;
        }
        *ptok = mist_token_create(MTT_ELSE, beg, end);
        if (*ptok == NULL)
        {
            return MIST_OUT_OF_MEMORY;
        }
    }
    else if (rlen == ref_endif_len && strncmp(beg, ref_endif, rlen) == 0) // "endif"
    {
        if (pos != end) // the placeholder contains more than just "endif"
        {
            return MIST_BAD_NAME;
        }
        *ptok = mist_token_create(MTT_ENDIF, beg, end);
        if (*ptok == NULL)
        {
            return MIST_OUT_OF_MEMORY;
        }
    }
    else // placeholder contains something that is not a keyword - leave as is
    {
        *ptok = mist_token_create(MTT_PH, beg, end);
        if (*ptok == NULL)
        {
            return MIST_OUT_OF_MEMORY;
        }
    }
    
    return MIST_OK;
}

static EMistErrorCode
mist_template_from_string(CMistTemplate** pmt, const char* name, 
                          const char* src,
                          const char* begin_marker, const char* end_marker,
                          char** error_descr)
{
    assert(pmt != NULL);
    assert(name != NULL);
    assert(src != NULL);
    assert(begin_marker != NULL);
    assert(end_marker != NULL);
    assert(error_descr != NULL);
    
    *error_descr = NULL;
    *pmt = NULL;
    
    // The markers mustn't be empty
    assert(begin_marker[0] != '\0');
    assert(end_marker[0] != '\0');

    // Stage 1 - lexical analysis
    CGrowingArray* tokens = mist_template_tokenize_string(src, 
        begin_marker, end_marker, error_descr);
    if (tokens == NULL)
    {
        return MIST_FAILED_TO_LOAD_TEMPLATE;
    }
    
    size_t istart = 0;
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    
    // Stage 2 - syntax analysis
    EMistErrorCode ec = mist_parse_template(tokens, &istart, pmt, name, src, error_descr);
    assert(istart <= ntok);
    
    if (ec == MIST_OK && istart < ntok)
    {
        // stopped before "else" or "endif" without matching "if"
        CMistToken* t = grar_get_element(tokens, CMistToken*, istart);
        assert(t != NULL);
        assert(t->type == MTT_ELSE || t->type == MTT_ENDIF);
        
        mist_format_parse_error(error_descr, 
            (t->type == MTT_ELSE) ? errElseWithoutIf : errEndifWithoutIf, 
            mist_line_num_for_ptr(src, t->beg));
        ec = MIST_SYNTAX_ERROR;
    }
    if (ec != MIST_OK)
    {
        if (*pmt != NULL)
        {
            mist_template_destroy(*pmt);
            *pmt = NULL;
        }
        
        grar_destroy_with_elements(tokens, mist_token_dtor, NULL);
        free(tokens);
        return MIST_FAILED_TO_LOAD_TEMPLATE;
    }
    
    grar_destroy_with_elements(tokens, mist_token_dtor, NULL);
    free(tokens);
    return ec;
}

///////////////////////////////////////////////////////////////////////////
/// CMistTemplate - match() and parse() functions.
static EMistErrorCode
mist_parse_template(CGrowingArray* tokens, size_t* istart,
    CMistTemplate** pt, const char* name,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);
    
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
    
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;
    
    assert(pt != NULL);
    *pt = mist_template_create(name);
    if (*pt == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    EMistErrorCode ec = mist_match_string_chunk(tokens, istart, *pt, src, error_descr);
    if (ec != MIST_OK)
    {
        mist_template_destroy(*pt);
        *pt = NULL;
        return ec;
    }
    
    while (*istart < ntok)
    {
        // process the placeholder first
        CMistPlaceholder* ph = NULL;
        ec = mist_parse_ph_expression(tokens, istart, &ph, src, error_descr);
        if (ec != MIST_OK)
        {
            break;
        }
        
        // the template must not end here, and if it does, probably
        // there is a problem in mist_template_tokenize_string().
        assert(*istart < ntok);
        
        // check if parsing stopped before "else" or "endif"
        CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
        assert(t != NULL);
        if (t->type == MTT_ELSE || t->type == MTT_ENDIF)
        {
            assert(ph == NULL);
            break;
        }
        
        // add the newly created placeholder structure to the template
        assert(ph != NULL);
        if (grar_add_element(&((*pt)->ph), ph) == 0)
        {
            ec = MIST_OUT_OF_MEMORY;
            break;
        }
        
        // process the string chunk
        ec = mist_match_string_chunk(tokens, istart, *pt, src, error_descr);
        if (ec != MIST_OK)
        {
            break;
        }
    }
    
    if (ec != MIST_OK)
    {
        mist_template_destroy(*pt);
        *pt = NULL;
    }
    return ec;
}
    
static EMistErrorCode
mist_parse_ph_expression(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);

#ifndef NDEBUG
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
#endif
        
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;
    
    assert(pp != NULL);
    *pp = NULL;
    
    CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
        
    EMistErrorCode ec = MIST_OK;
    if (t->type == MTT_IF)
    {
        ec = mist_parse_conditional(tokens, istart, pp, src, error_descr);
        assert(*pp != NULL || ec != MIST_OK);
    }
    else if (t->type == MTT_PH)
    {
        ec = mist_parse_placeholder(tokens, istart, pp, src, error_descr);
        assert(*pp != NULL || ec != MIST_OK);
        
        if (ec == MIST_BAD_NAME)
        {
            char* ph_str = mist_get_substring(t->beg, t->end);
            if (ph_str != NULL)
            {
                *error_descr = (char*)malloc(strlen(errInvalidPHName) + 
                    (t->end - t->beg) + MAX_NUM_DIGITS + 1);
                if (*error_descr != NULL)
                {
                    sprintf(*error_descr, errInvalidPHName, ph_str, 
                        mist_line_num_for_ptr(src, t->beg));
                }
                free(ph_str);
            }
            ec = MIST_SYNTAX_ERROR;
        }
    }
    else
    {
        assert(t->type == MTT_ELSE || t->type == MTT_ENDIF);
        // do nothing, just return to the upper level
    }
    
    return ec;
}
    
static EMistErrorCode
mist_parse_placeholder(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);

#ifndef NDEBUG    
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
#endif
    
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;
    
    assert(pp != NULL);
    *pp = NULL;
    
    static const char join_name[] = "join";
    static const size_t join_name_len = sizeof(join_name) / sizeof(join_name[0]) - 1;
    
    CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
    assert(t->type == MTT_PH);
    ++(*istart);
    
    EMistErrorCode ec = MIST_OK;
    CMistString* expr = mist_string_create_from_range(t->beg, t->end);
    if (expr == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    CMistString* sep = NULL;
    mist_string_trim(expr); // remove whitespace chars from both ends
    char* name = expr->str;
    
    char* pos = strchr(expr->str, ':');
    if (pos != NULL)
    {
        // join directive expected
        *pos = '\0';
        mist_string_trim(expr); // trim trailing whitespace from the name
        
        expr->str = pos + 1;
        mist_string_trim(expr); // trim leading whitespace from the directive
        if (expr->str[0] == '\0' || 
            strncmp(expr->str, join_name, join_name_len) != 0)
        {
            // nothing after the colon except whitespace
            mist_string_destroy(expr);
            return MIST_BAD_NAME;
        }
        
        expr->str += join_name_len;
        mist_string_trim(expr);
        size_t len = strlen(expr->str);
        
        if (expr->str[0] != '(' || expr->str[len - 1] != ')')
        {
            // something wrong with the parentheses
            mist_string_destroy(expr);
            return MIST_BAD_NAME;
        }
        
        expr->str[len - 1] = '\0';
        expr->str += 1;
        
        sep = mist_string_create(expr->str);
        if (sep == NULL)
        {
            mist_string_destroy(expr);
            return MIST_OUT_OF_MEMORY;
        }
        
        ec = mist_string_unescape(sep);
        if (ec != MIST_OK)
        {
            mist_string_destroy(sep);
            mist_string_destroy(expr);
            return ec;
        }
    }
    
    // expr->str should point to the name of the placeholder now
    // without leading and trailing whitespace chars
    if (mist_name_is_bad(name))
    {
        if (sep != NULL)
        {
            mist_string_destroy(sep);
        }
        mist_string_destroy(expr);
        return MIST_BAD_NAME;
    }
    
    // finally, create the placeholder
    *pp = mist_placeholder_create(name, (sep == NULL ? NULL : sep->str));
    if (*pp == NULL)
    {
        ec = MIST_OUT_OF_MEMORY;
    }
    
    if (sep != NULL)
    {
        mist_string_destroy(sep);
    }
    mist_string_destroy(expr);
    
    return ec;
}
    
static EMistErrorCode
mist_parse_conditional(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);
    
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
    
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;
    
    assert(pp != NULL);
    *pp = NULL;
    
    CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
    CMistToken* t_if = t;
    
    static const char* name_then = "then_branch";
    static const char* name_else = "else_branch";
    
    // process "if"
    EMistErrorCode ec = mist_match_if(tokens, istart, pp, src, error_descr);
    if (ec != MIST_OK)
    {
        assert(*pp == NULL);
        return ec;
    }
    
    // the stream of tokens mustn't end with a placeholder
    assert(*istart < ntok);
    assert(*pp != NULL);
    ec = mist_parse_template(tokens, istart, &((*pp)->tpl_then), name_then, src, error_descr);
    
    if (ec == MIST_OK && *istart >= ntok)
    {
        // premature end of the token stream
        mist_format_parse_error(error_descr, errIfWithoutEndif, mist_line_num_for_ptr(src, t->beg));
        ec = MIST_SYNTAX_ERROR;
    }
    if (ec != MIST_OK)
    {
        mist_placeholder_destroy(*pp);
        *pp = NULL;
        return ec;
    }
    
    // if "else"-branch exists, process it
    t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
    if (t->type == MTT_ELSE)
    {
        ec = mist_match_else(tokens, istart, src, error_descr);
        if (ec != MIST_OK)
        {
            mist_placeholder_destroy(*pp);
            *pp = NULL;
            return ec;
        }
        
        // the stream of tokens mustn't end with a placeholder
        assert(*istart < ntok);
        
        ec = mist_parse_template(tokens, istart, &((*pp)->tpl_else), name_else, src, error_descr);
        if (ec == MIST_OK && *istart >= ntok)
        {
            // premature end of the token stream
            mist_format_parse_error(error_descr, errElseWithoutEndif, mist_line_num_for_ptr(src, t->beg));
            ec = MIST_SYNTAX_ERROR;
        }
        if (ec != MIST_OK)
        {
            mist_placeholder_destroy(*pp);
            *pp = NULL;
            return ec;
        }
        
        // *istart must have been changed.
        t = grar_get_element(tokens, CMistToken*, *istart);
        assert(t != NULL);
    }
    else // t->type != MTT_ELSE here
    {
        // Create "else"-branch anyway (as if it was present but empty).
        (*pp)->tpl_else = mist_template_create(name_else);
        char* sch = (char*)strdup("");
        
        if ((*pp)->tpl_else == NULL || sch == NULL || 
            grar_add_element(&((*pp)->tpl_else->sch), sch) == 0)
        {
            mist_placeholder_destroy(*pp);
            free(sch);
            *pp = NULL;
            return MIST_OUT_OF_MEMORY;
        }
    }
    
    // look for "endif"
    if (t->type == MTT_ENDIF)
    {
        ec = mist_match_endif(tokens, istart, src, error_descr);
        if (ec != MIST_OK)
        {
            mist_placeholder_destroy(*pp);
            *pp = NULL;
            return ec;
        }
    }
    else // found dangling "else" 
    {
        assert(t->type == MTT_ELSE);
        mist_format_parse_error(error_descr, errIfWithoutEndif, mist_line_num_for_ptr(src, t_if->beg));
        ec = MIST_SYNTAX_ERROR;
        
        mist_placeholder_destroy(*pp);
        *pp = NULL;
        
        return ec;
    }
    
    return ec;
}
    
static EMistErrorCode
mist_match_string_chunk(CGrowingArray* tokens, size_t* istart,
    CMistTemplate* mt,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);
    
#ifndef NDEBUG
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
#endif
    
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;
    
    assert(mt != NULL);
    
    CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
    assert(t->type == MTT_SCH);
    ++(*istart);
    
    // Add the string chunk to the template
    char* sch = mist_get_substring(t->beg, t->end);
    if (sch == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    if (grar_add_element(&(mt->sch), sch) == 0)
    {
        free(sch);
        return MIST_OUT_OF_MEMORY;
    }
    
    return MIST_OK;
}
    
static EMistErrorCode
mist_match_if(CGrowingArray* tokens, size_t* istart,
    CMistPlaceholder** pp,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);
    
#ifndef NDEBUG
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
#endif
    
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;
    
    assert(pp != NULL);
    *pp = NULL;
    
    CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
    assert(t->type == MTT_IF);
    ++(*istart);
    
    int is_concat = 0;
    char* name = mist_parse_cond_expr(t->beg, t->end, &is_concat);
    
    if (name == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    if (mist_name_is_bad(name))
    {
        *error_descr = (char*)malloc(strlen(errInvalidPHName) + (t->end - t->beg) + MAX_NUM_DIGITS + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errInvalidPHName, name, mist_line_num_for_ptr(src, t->beg));
        }
        
        free(name);
        return MIST_SYNTAX_ERROR;
    }
    
    *pp = mist_placeholder_create(name, NULL);
    if (*pp == NULL)
    {
        free(name);
        return MIST_OUT_OF_MEMORY;
    }
    
    (*pp)->type = MPH_COND;
    (*pp)->is_concat = is_concat;
    
    // this template will hold the resulting values of the conditional construct
    (*pp)->tpl = mist_template_create(name); 
    free(name);
    
    if ((*pp)->tpl == NULL)
    {
        mist_placeholder_destroy(*pp);
        *pp = NULL;
        return MIST_OUT_OF_MEMORY;
    }
    
    return MIST_OK;
}

static EMistErrorCode
mist_match_else(CGrowingArray* tokens, size_t* istart,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);
    
#ifndef NDEBUG
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
#endif
    
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;

#ifndef NDEBUG    
    CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
    assert(t->type == MTT_ELSE);
#endif
    
    ++(*istart);
    
    return MIST_OK;
}

static EMistErrorCode
mist_match_endif(CGrowingArray* tokens, size_t* istart,
    const char* src, char** error_descr)
{
    assert(tokens != NULL);
    assert(istart != NULL);
    
#ifndef NDEBUG
    size_t ntok = grar_get_size(tokens);
    assert(ntok >= 1);
    assert(*istart < ntok);
#endif
    
    assert(src != NULL);
    assert(error_descr != NULL);
    *error_descr = NULL;

#ifndef NDEBUG    
    CMistToken* t = grar_get_element(tokens, CMistToken*, *istart);
    assert(t != NULL);
    assert(t->type == MTT_ENDIF);
#endif    
    
    ++(*istart);
    
    return MIST_OK;
}

///////////////////////////////////////////////////////////////////////////
static int
mist_template_compare(const void* lhs, const void* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    const CMistTemplate** tleft  = (const CMistTemplate**)lhs;
    const CMistTemplate** tright = (const CMistTemplate**)rhs;
    
    assert(*tleft != NULL);
    assert(*tright != NULL);

    assert((*tleft)->name != NULL);
    assert((*tright)->name != NULL);

    return strcmp((*tleft)->name, (*tright)->name);
}

static EMistErrorCode
mist_tg_create_attrs(CMistTemplateGroup* mtg)
{
    assert(mtg != NULL);
    
    // Sort the templates for faster searching
    grar_sort(&(mtg->tpl), mist_template_compare);
    
    CGrowingArray attrs;
    if (!grar_create(&attrs))
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    size_t num = grar_get_size(&(mtg->tpl));
    assert(num != 0);
    CMistTemplate** tpl = grar_get_c_array(&(mtg->tpl), CMistTemplate*);
    
    for (size_t i = 0; i < num; ++i) // for each template ...
    {
        EMistErrorCode ec = mist_tg_collect_attrs_for_template(mtg, tpl[i], &attrs);
        if (ec != MIST_OK)
        {
            grar_destroy(&attrs);
            return ec;
        }
    }
    
    size_t nattr = grar_get_size(&attrs);
    if (nattr != 0)
    {
        // Create template structures once for each name of an attribute
        grar_string_sort(&attrs);
        size_t i = 0;
        while (i < nattr)
        {
            CMistTemplate* tp = mist_template_create(grar_get_element(&attrs, const char*, i));
            if (tp == NULL)
            {
                grar_destroy(&attrs);
                return MIST_OUT_OF_MEMORY;
            }
            
            if (grar_add_element(&(mtg->tpl), tp) == 0)
            {
                mist_template_destroy(tp);
                grar_destroy(&attrs);
                return MIST_OUT_OF_MEMORY;
            }
            
            ++i;
            //skip to the next name 
            while (i < nattr && 
                   strcmp(tp->name, grar_get_element(&attrs, const char*, i)) == 0)
            {
                ++i;
            }
        }
    }
    
    grar_destroy(&attrs);
    return MIST_OK;
}

static EMistErrorCode
mist_tg_collect_attrs_for_template(CMistTemplateGroup* mtg, 
    CMistTemplate* tpl, CGrowingArray* attrs)
{
    assert(mtg != NULL);
    assert(tpl != NULL);
    assert(attrs != NULL);
    
    // a fake template - only to search for the templates with particular names
    CMistTemplate comp;
    CMistTemplate* pcomp = &comp;
    
    size_t nph = grar_get_size(&(tpl->ph));
    CMistPlaceholder** phs = grar_get_c_array(&(tpl->ph), CMistPlaceholder*);
    
    for (size_t p = 0; p < nph; ++p) // for each placeholder in the template ...
    {
        comp.name = phs[p]->name;
        if (grar_find(&(mtg->tpl), &pcomp, mist_template_compare) == -1)
        {
            // found a name of an attribute
            if (grar_add_element(attrs, comp.name) == 0)
            {
                return MIST_OUT_OF_MEMORY;
            }
        }
        
        if (phs[p]->type == MPH_COND)
        {
            assert(phs[p]->tpl_then != NULL);
            assert(phs[p]->tpl_else != NULL);
            
            EMistErrorCode ec = mist_tg_collect_attrs_for_template(mtg, phs[p]->tpl_then, attrs);
            if (ec != MIST_OK)
            {
                return ec;
            }
            
            ec = mist_tg_collect_attrs_for_template(mtg, phs[p]->tpl_else, attrs);
            if (ec != MIST_OK)
            {
                return ec;
            }
        }
    } // end for p
    
    return MIST_OK;
}

static void
mist_tg_connect_templates(CMistTemplateGroup* mtg)
{
    assert(mtg != NULL);
    
    size_t num = grar_get_size(&(mtg->tpl));
    assert(num != 0);
    CMistTemplate** tpl = grar_get_c_array(&(mtg->tpl), CMistTemplate*);
    
    for (size_t i = 0; i < num; ++i) // for each template ...
    {
        mist_tg_connect_templates_for_template(mtg, tpl[i]);
    }
    
    return;
}

static void
mist_tg_connect_templates_for_template(CMistTemplateGroup* mtg, CMistTemplate* tpl)
{
    assert(mtg != NULL);
    assert(tpl != NULL);
    
    // a fake template - only to search for the templates with particular names
    CMistTemplate comp;
    CMistTemplate* pcomp = &comp;
    
    size_t nph = grar_get_size(&(tpl->ph));
    CMistPlaceholder** phs = grar_get_c_array(&(tpl->ph), CMistPlaceholder*);
    
    for (size_t p = 0; p < nph; ++p) // for each placeholder in the template ...
    {
        comp.name = phs[p]->name;
        int ind = grar_find(&(mtg->tpl), &pcomp, mist_template_compare);
        assert(ind != -1); // the template must have been present
        
        CMistTemplate* tpl = grar_get_element(&(mtg->tpl), CMistTemplate*, ind);
        assert(tpl != NULL);
        
        if (phs[p]->type == MPH_COND)
        {
            // the found template represents the conditional expression
            phs[p]->tpl_cond = tpl;
            
            // process the branches
            assert(phs[p]->tpl_then != NULL);
            assert(phs[p]->tpl_else != NULL);
            
            mist_tg_connect_templates_for_template(mtg, phs[p]->tpl_then);
            mist_tg_connect_templates_for_template(mtg, phs[p]->tpl_else);
        }
        else
        {
            // not a conditional, so we connect the template as usual
            phs[p]->tpl = tpl;
        }
    } // end for p
    
    return;
}

static EMistErrorCode
mist_template_evaluate(CMistTemplate* mt)
{
    assert(mt != NULL);
    
    if (mt->is_evaluated)
    {
        // nothing to do
        return MIST_OK;
    }
    
    size_t nph = grar_get_size(&(mt->ph));
    size_t nsc = grar_get_size(&(mt->sch));
    if (nsc == 0)
    {
        // No string chunks means that this is actually an attribute.
        // If no values have been set for it, create an empty string and use it
        // as its only value.
        assert(nph == 0);
        
        if (grar_get_size(&(mt->vals)) == 0)
        {
            char* empty = (char*)strdup("");
            if (empty == NULL || 
                grar_add_element(&(mt->vals), empty) == 0)
            {
                free(empty);
                return MIST_OUT_OF_MEMORY;
            }
        }
        
        mt->is_evaluated = 1;
        return MIST_OK;
    }
    // there should be at least one string chunk
    assert(nsc == nph + 1);
    
    CMistPlaceholder** phs = grar_get_c_array(&(mt->ph), CMistPlaceholder*);
    assert(phs != NULL);
    
    // clear old values
    mist_template_clear_values(mt);

    // set 'evaluated' flag here to prevent infinite recursion below
    mt->is_evaluated = 1;
    
    // the 1st pass: evaluate the placeholders (without joining values for now)
    for (size_t p = 0; p < nph; ++p)
    {
        assert(phs[p] != NULL);
        EMistErrorCode ec = mist_placeholder_evaluate(phs[p]);
        if (ec != MIST_OK)
        {
            return ec;
        }
    }
    
    // find the number of values to be constructed and maximum length of a value
    size_t nvals = mist_template_num_values(mt);
    size_t max_len = mist_template_max_length(mt);
    assert(nvals != 0);
    
    // create an array to store current positions in the values
    char** pos = (char**)malloc(nvals * sizeof(char*));
    if (pos == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    // a temporary storage for the values being created
    CGrowingArray tga;
    if (!grar_create(&tga))
    {
        free(pos);
        return MIST_OUT_OF_MEMORY;
    }
    
    // allocate memory for the values
    for (size_t i = 0; i < nvals; ++i)
    {
        char* buf = (char*)malloc(max_len + 1);
        if (buf == NULL)
        {
            free(pos);
            return MIST_OUT_OF_MEMORY;
        }
        
        if (grar_add_element(&tga, buf) == 0)
        {
            free(buf);
            free(pos);
            grar_destroy_with_elements(&tga, NULL, NULL);
            return MIST_OUT_OF_MEMORY;
        }
        
        pos[i] = buf;
    }
    
    // the 2nd pass: construct the values
    const char** schs = grar_get_c_array(&(mt->sch), const char*);
    
    // first, append string chunk #0 (it must always exist)
    mist_copy_string(pos, nvals, schs[0]);
    
    // append remaining string chunks and values of placeholders
    for (size_t i = 0; i < nph; ++i)
    {
        mist_copy_placeholder_values(pos, nvals, phs[i]);
        mist_copy_string(pos, nvals, schs[i + 1]);
    }
    
    // append terminating 0s
    for (size_t i = 0; i < nvals; ++i)
    {
        *(pos[i]) = '\0';
    }

    // It is only now when mt->vals is updated, otherwise something bad would have
    // happened above (where mist_copy_placeholder_values() is called) in case
    // of recursion in the templates.
    grar_swap(&(mt->vals), &tga);
    assert(grar_get_size(&tga) == 0);
    
    free(pos);
    grar_destroy(&tga);
    return MIST_OK;
}

static EMistErrorCode
mist_placeholder_evaluate(CMistPlaceholder* ph)
{
    assert(ph != NULL);
    assert(ph->tpl != NULL);
    
    // This template will contain the results of evaluation
    CMistTemplate* mt = ph->tpl;
    EMistErrorCode ec = MIST_OK;
    
    if (ph->type == MPH_COND)
    {
        assert(ph->tpl_cond != NULL);
        assert(ph->tpl_then != NULL);
        assert(ph->tpl_else != NULL);
        
        mist_template_clear_values(mt);        
        assert(grar_get_size(&(mt->vals)) == 0);
        
        mist_template_clear_values(ph->tpl_then);        
        assert(grar_get_size(&(ph->tpl_then->vals)) == 0);
        
        mist_template_clear_values(ph->tpl_else);        
        assert(grar_get_size(&(ph->tpl_else->vals)) == 0);
        
        // evaluate the condition first
        ec = mist_template_evaluate(ph->tpl_cond);
        if (ec != MIST_OK)
        {
            return ec;
        }
                
        CGrowingArray* cond = &(ph->tpl_cond->vals);
        assert(cond != NULL);

        // 'cond' now contains the values of the conditional expression
        size_t c = 0; // index of the condition to be checked
        size_t ncond = grar_get_size(cond);
        assert(ncond >= 1);
        
        if (ph->is_concat)
        {
            // check if the condition has at least one non-empty value
            // and evaluate the corresponding branch
            int cexpr = 0; 
            for (c = 0; c < ncond; ++c)
            {
                assert(grar_get_element(cond, const char*, c) != NULL);
                cexpr = (*grar_get_element(cond, const char*, c) != '\0');
                
                if (cexpr)
                {
                    break;
                }
            }
            
            CMistTemplate* branch = (cexpr) ? ph->tpl_then : ph->tpl_else;
            ec = mist_template_evaluate(branch);
            if (ec != MIST_OK)
            {
                return ec;
            }
            
            size_t nvals = grar_get_size(&(branch->vals));
            assert(nvals >= 1);
            
            // the resulting values are the same as in the chosen branch
            for (size_t elem = 0; elem < nvals; ++elem)
            {
                assert(grar_get_element(&(branch->vals), const char*, elem) != NULL);
                char* to_add = (char*)strdup(grar_get_element(&(branch->vals), const char*, elem));
                if (to_add == NULL || 
                    grar_add_element(&(mt->vals), to_add) == 0)
                {
                    free(to_add);
                    ec = MIST_OUT_OF_MEMORY;
                    break;
                }
            }
        }
        else // not a concat-expression
        {   
            // the 1st pass: determine which branches to evaluate
            // and how many values they have
            int cexpr = 0;
            size_t ntotal = ncond;  // number of values to process (upper bound)
            for (c = 0; c < ncond; ++c)
            {
                assert(grar_get_element(cond, const char*, c) != NULL);
                cexpr = (*grar_get_element(cond, const char*, c) != '\0');
                CMistTemplate* branch = (cexpr) ? ph->tpl_then : ph->tpl_else;
                
                // evaluate the selected branch (no-op if it has already been evaluated)
                ec = mist_template_evaluate(branch);
                if (ec != MIST_OK)
                {
                    return ec;
                }
                
                size_t nvals = grar_get_size(&(branch->vals));
                if (nvals > ntotal)
                {
                    ntotal = nvals;
                }
            }
            assert(ntotal >= 1);
            
            // the 2nd pass: determine the values of the conditional construct
            assert(grar_get_element(cond, const char*, 0) != NULL);
            cexpr = (*grar_get_element(cond, const char*, 0) != '\0');
            
            c = 0; // again, start with the first condition
            for (size_t i = 0; i < ntotal; ++i) 
            {
                // select appropriate branch
                CMistTemplate* branch = (cexpr) ? ph->tpl_then : ph->tpl_else;
                assert(branch->is_evaluated);
                
                size_t nvals = grar_get_size(&(branch->vals));
                assert(nvals >= 1);
                
                // If there are no more values left in the chosen branch, use 
                // the last one. There must be at least one value in the branch.
                size_t elem = (i < nvals) ? i : (nvals - 1);
                
                // Copy the value #elem to the array of results
                assert(grar_get_element(&(branch->vals), const char*, elem) != NULL);
                char* to_add = (char*)strdup(grar_get_element(
                    &(branch->vals), const char*, elem));
                if (to_add == NULL || 
                    grar_add_element(&(mt->vals), to_add) == 0)
                {
                    free(to_add);
                    ec = MIST_OUT_OF_MEMORY;
                    break;
                }
                
                // if we have run out of conditions, we'll use the last one until the end
                if (c + 1 < ncond)
                {
                    ++c;
                    assert(grar_get_element(cond, const char*, c) != NULL);
                    cexpr = (*grar_get_element(cond, const char*, c) != '\0');
                }
            } // end for
        } // end if (ph->is_concat) ...
        
        if (ec == MIST_OK)
        {
            mt->is_evaluated = 1;
        }
    }
    else // not a conditional, evaluate as usual
    {
        ec = mist_template_evaluate(mt);
    }
    
    return ec;
}

static size_t
mist_placeholder_max_length(CMistPlaceholder* ph)
{
    assert(ph != NULL);
    assert(ph->tpl != NULL);
    assert(ph->tpl->is_evaluated);
    
    size_t nvals = grar_get_size(&(ph->tpl->vals));
    if (nvals == 0)
    {
        // if there are no values, it is OK, just nothing to do here
        return 0;
    }
    
    size_t len = 0;
    if (ph->type == MPH_JOIN)
    {
        assert(ph->sep != NULL);
        
        // there will be join - compute the length of the resulting string
        len = grar_string_total_length(&(ph->tpl->vals)) + (nvals - 1) * strlen(ph->sep);
    }
    else
    {
        // no join, so get the length of the longest value
        const char** vals = grar_get_c_array(&(ph->tpl->vals), const char*);
        for (size_t i = 0; i < nvals; ++i)
        {
            assert(vals[i] != NULL);
            size_t cur_len = strlen(vals[i]);
            
            if (len < cur_len)
            {
                len = cur_len;
            }
        } // end for i
    }
    
    return len; 
}

static char* 
mist_placeholder_join_values(const CMistPlaceholder* ph, char* buf)
{
    assert(ph != NULL);
    assert(ph->tpl != NULL);
    assert(ph->type == MPH_JOIN);
    assert(ph->sep != NULL);
    assert(buf != NULL);
    assert(ph->is_concat == 0);
    
    size_t nvals = grar_get_size(&(ph->tpl->vals));
    if (nvals == 0)
    {
        // there are no values, nothing to do here
        return buf;
    }
    
    // there is at least one value
    const char** vals = grar_get_c_array(&(ph->tpl->vals), const char*);
    assert(vals[0] != NULL);
    size_t sep_len = strlen(ph->sep);
    
    size_t len = strlen(vals[0]);
    if (len != 0)
    {
        strncpy(buf, vals[0], len);
        buf += len;
    }
    
    for (size_t i = 1; i < nvals; ++i)
    {
        assert(vals[i] != NULL);
        len = strlen(vals[i]);
        
        // append separator and then the next value
        if (sep_len != 0)
        {
            strncpy(buf, ph->sep, sep_len);
            buf += sep_len;
        }
        
        if (len != 0)
        {
            strncpy(buf, vals[i], len);
            buf += len;
        }
    }
    
    return buf; 
}

static size_t
mist_template_max_length(CMistTemplate* mt)
{
    assert(mt != NULL);
    
    size_t len = grar_string_total_length(&(mt->sch));
    size_t nph = grar_get_size(&(mt->ph));
    CMistPlaceholder** phs = grar_get_c_array(&(mt->ph), CMistPlaceholder*);
    
    for (size_t i = 0; i < nph; ++i)
    {
        assert(phs[i] != NULL);
        len += mist_placeholder_max_length(phs[i]);
    }
    return len; 
}

static size_t
mist_template_num_values(CMistTemplate* mt)
{
    assert(mt != NULL);
    
    // for a non-attribute template, there will be at least one value
    size_t num = 1;
    
    size_t nph = grar_get_size(&(mt->ph));
    CMistPlaceholder** phs = grar_get_c_array(&(mt->ph), CMistPlaceholder*);
    
    for (size_t i = 0; i < nph; ++i)
    {
        assert(phs[i] != NULL);
        assert(phs[i]->tpl != NULL);
        
        // only if there is no join, there could be more than one value in the 
        // placeholder
        if (phs[i]->type != MPH_JOIN)
        {
            size_t nvals = grar_get_size(&(phs[i]->tpl->vals));
            if (num < nvals)
            {
                num = nvals;
            }
        }
    }
    
    return num; 
}

static void
mist_copy_string(char** pos, size_t nvals, const char* str)
{
    assert(pos != NULL);
    assert(nvals != 0);
    assert(str != NULL);
    
    size_t len = strlen(str);
    if (len > 0)
    {
        for (size_t i = 0; i < nvals; ++i)
        {
            assert(pos[i] != NULL);
            strncpy(pos[i], str, len);
            pos[i] += len;
        }
    }
    return;
}

static void
mist_copy_placeholder_values(char** pos, size_t nvals, const CMistPlaceholder* ph)
{
    assert(pos != NULL);
    assert(nvals != 0);
    assert(ph != NULL);
    assert(ph->tpl != NULL);
    
    size_t pvals = grar_get_size(&(ph->tpl->vals));
    if (pvals == 0)
    {
        return;
    }
    
    if (ph->type == MPH_JOIN)
    {
        assert(pos[0] != NULL);
        
        char* tpos = mist_placeholder_join_values(ph, pos[0]);
        size_t len = tpos - pos[0];
        if (len > 0)
        {
            for (size_t i = 1; i < nvals; ++i)
            {
                assert(pos[i] != NULL);
                strncpy(pos[i], pos[0], len);
                pos[i] += len;
            }
        }
        pos[0] = tpos;
    }
    else // no join
    {
        assert(nvals >= pvals);
        const char** vals = grar_get_c_array(&(ph->tpl->vals), const char*);
        
        size_t i;
        size_t len = 0;
        for (i = 0; i < pvals; ++i)
        {
            assert(pos[i] != NULL);
            assert(vals[i] != NULL);
            
            len = strlen(vals[i]);
            if (len > 0)
            {
                strncpy(pos[i], vals[i], len);
                pos[i] += len;
            }
        }
        
        // use the last value for the remaining strings
        if (len > 0)
        {
            for (; i < nvals; ++i)
            {
                assert(pos[i] != NULL);
                strncpy(pos[i], vals[pvals - 1], len);
                pos[i] += len;
            }
        }
    }
    
    return;
}

static EMistErrorCode
mist_tg_load_conf(const char* dir, char** tpath, char** begm, char** endm, 
                  char** error_descr)
{
    assert(dir != NULL);
    assert(mist_dir_exists(dir));
    assert(!mist_dir_is_special(dir));
    assert(tpath != NULL);
    assert(begm != NULL);
    assert(endm != NULL);
    assert(error_descr != NULL);
    
    *error_descr = NULL;
        
    // default begin and end markers for the placeholders
    const char* def_beg = "<$";
    const char* def_end = "$>";
    
    static char fpt_name[] = "FILE_PATH_TEMPLATE";
    static char beg_name[] = "PH_BEGIN_MARKER";
    static char end_name[] = "PH_END_MARKER";
    
    *begm = NULL;
    *endm = NULL;
    *tpath = NULL;
    
    CStringMap* params = smap_create();
    if (params == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    // load configuration first
    char* err = NULL;
    EMistErrorCode ec = mist_load_config_file_from_dir(dir, params, &err);
    if (ec != MIST_OK)
    {   
        if (err == NULL)
        {
            err = (char*)strdup(errUnspecifiedError);
            if (err == NULL)
            {
                smap_destroy(params);
                return ec;
            }
        }
        
        *error_descr = (char*)malloc(strlen(errUnableToLoadConf) + 
            strlen(dir) + strlen(err) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errUnableToLoadConf, dir, err);
        }
        free(err);
        smap_destroy(params);
        return ec;
    }
    
    const char* key = smap_check_duplicate_keys(params);
    if (key != NULL)
    {
        *error_descr = (char*)malloc(
            strlen(errDupParam) + strlen(key) + strlen(dir) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errDupParam, key, dir);
        }
        smap_destroy(params);
        return MIST_DUP_PARAM;
    }
    
    const char* val = smap_lookup(params, &beg_name[0]);
    if (val != NULL && val[0] == '\0')
    {
        smap_destroy(params);
        
        *error_descr = (char*)malloc(
            strlen(errEmptyParam) + strlen(&beg_name[0]) + strlen(dir) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errEmptyParam, &beg_name[0], dir);
        }
        
        return MIST_MISSING_PARAM;
    }

    *begm = (char*)strdup((val != NULL) ? val : def_beg);
    if (*begm == NULL)
    {
        smap_destroy(params);
        return MIST_OUT_OF_MEMORY;
    }
    
    val = smap_lookup(params, &end_name[0]);
    if (val != NULL && val[0] == '\0')
    {
        free(*begm);
        *begm = NULL;
        smap_destroy(params);
        
        *error_descr = (char*)malloc(
            strlen(errEmptyParam) + strlen(&end_name[0]) + strlen(dir) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errEmptyParam, &end_name[0], dir);
        }
        
        return MIST_MISSING_PARAM;
    }
    
    *endm = (char*)strdup((val != NULL) ? val : def_end);
    if (*endm == NULL)
    {
        free(*begm);
        *begm = NULL;
        smap_destroy(params);
        return MIST_OUT_OF_MEMORY;
    }
    
    const char* path = smap_lookup(params, &fpt_name[0]);
    if (path == NULL || path[0] == '\0')
    {
        *error_descr = (char*)malloc(
            strlen(errMissingParam) + strlen(&fpt_name[0]) + strlen(dir) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errMissingParam, &fpt_name[0], dir);
        }
        
        free(*begm);
        *begm = NULL;
        
        free(*endm);
        *endm = NULL;
        
        smap_destroy(params);
        return MIST_MISSING_PARAM;
    }
    
    *tpath = (char*)strdup(path);
    if (*tpath == NULL)
    {
        free(*begm);
        *begm = NULL;
        
        free(*endm);
        *endm = NULL;
        
        smap_destroy(params);
        return MIST_OUT_OF_MEMORY;
    }
    
    smap_destroy(params);
    return MIST_OK;
}

static EMistErrorCode
mist_tg_process_dir(const char* dir, 
                    CGrowingArray* tpl_names, CGrowingArray* tpl_strings,
                    char** error_descr)
{
    assert(dir != NULL);
    assert(tpl_names != NULL);
    assert(tpl_strings != NULL);
    assert(error_descr != NULL);
        
    static const char tpl_ext[] = ".tpl";
    static const size_t tpl_ext_len = sizeof(tpl_ext) / sizeof(tpl_ext[0]) - 1;
    
    *error_descr = NULL;
    
    // initialize the arrays
    if (!grar_create(tpl_names))
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    if (!grar_create(tpl_strings))
    {
        grar_destroy(tpl_names);
        return MIST_OUT_OF_MEMORY;
    }
    
    // traverse the directory
    DIR* d = opendir(dir);
    if (d == NULL)
    {
        *error_descr = (char*)malloc(strlen(errReadDirFailed) + strlen(dir) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errReadDirFailed, dir);
        }
        grar_destroy(tpl_names);
        grar_destroy(tpl_strings);
        return MIST_READ_DIR_FAILED;
    }
  
    struct dirent* entry = readdir(d);
    while (entry != NULL)
    {
        size_t len = strlen(entry->d_name);
        int idx = (int)len - (int)tpl_ext_len;
        if (len > tpl_ext_len &&
            !strncmp(&(entry->d_name[idx]), tpl_ext, tpl_ext_len))
        {
            char* pfile = mist_path_sum(dir, entry->d_name);
            if (pfile == NULL)
            {
                closedir(d);
                grar_destroy_with_elements(tpl_names, NULL, NULL);
                grar_destroy_with_elements(tpl_strings, NULL, NULL);
                return MIST_OUT_OF_MEMORY;
            }

            if (mist_file_exists(pfile))
            {
                size_t name_len = len - tpl_ext_len;
                char* name = (char*)malloc(name_len + 1);
                if (name == NULL)
                {
                    free(pfile);
                    closedir(d);
                    grar_destroy_with_elements(tpl_names, NULL, NULL);
                    grar_destroy_with_elements(tpl_strings, NULL, NULL);
                    return MIST_OUT_OF_MEMORY;
                }
                strncpy(name, entry->d_name, name_len);
                name[name_len] = '\0';
                
                if (mist_name_is_bad(name))
                {
                    *error_descr = (char*)malloc(strlen(errBadTemplateName) + 
                        strlen(name) + strlen(pfile) + 1);
                    if (*error_descr != NULL)
                    {
                        sprintf(*error_descr, errBadTemplateName, name, pfile);
                    }
                    
                    free(name);
                    free(pfile);
                    closedir(d);
                    grar_destroy_with_elements(tpl_names, NULL, NULL);
                    grar_destroy_with_elements(tpl_strings, NULL, NULL);
                    
                    return MIST_BAD_NAME;
                }
                
                // read the file to string
                char* contents = NULL;
                EMistErrorCode ec = mist_file_read_all(pfile, &contents);
                if (ec != MIST_OK)
                {
                    if (ec == MIST_OPEN_FILE_FAILED || 
                        ec == MIST_READ_FILE_FAILED)
                    {
                        *error_descr = (char*)malloc(strlen(errReadFileFailed) + strlen(pfile) + 1);
                        if (*error_descr != NULL)
                        {
                            sprintf(*error_descr, errReadFileFailed, pfile);
                        }
                    }
                    
                    free(name);
                    free(pfile);
                    closedir(d);
                    grar_destroy_with_elements(tpl_names, NULL, NULL);
                    grar_destroy_with_elements(tpl_strings, NULL, NULL);
                    return ec;
                }
                
                if (!grar_add_element(tpl_names, name))
                {
                    free(name);
                    free(contents);
                    free(pfile);
                    closedir(d);
                    grar_destroy_with_elements(tpl_names, NULL, NULL);
                    grar_destroy_with_elements(tpl_strings, NULL, NULL);
                    return MIST_OUT_OF_MEMORY;
                }
                
                if (!grar_add_element(tpl_strings, contents))
                {
                    free(contents);
                    // must not free 'name' here: it is owned by 'tpl_names' now
                    free(pfile);
                    closedir(d);
                    grar_destroy_with_elements(tpl_names, NULL, NULL);
                    grar_destroy_with_elements(tpl_strings, NULL, NULL);
                    return MIST_OUT_OF_MEMORY;
                }
            } // end if mist_file_exists()
            
            free(pfile);
        }
        entry = readdir(d);
    } // end while
    
    closedir(d);
    
    if (grar_get_size(tpl_names) == 0)
    {
        grar_destroy(tpl_names);
        grar_destroy(tpl_strings);
        
        *error_descr = (char*)malloc(strlen(errNoTplFiles) + strlen(dir) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errNoTplFiles, dir);
        }
        return MIST_NO_TPL_FILES;
    }
    
    return MIST_OK;
}

#ifndef NDEBUG
// The function is used in assert() statements only.
static int
mist_is_range_valid(const char* beg, const char* end)
{
    return (beg != NULL && 
            end != NULL && 
            beg <= end);
}
#endif

///////////////////////////////////////////////////////////////////////////
// CMistToken
static CMistToken* 
mist_token_create(EMistTokenType type, const char* beg, const char* end)
{
    assert(mist_is_range_valid(beg, end));
    assert((unsigned long)type < MTT_NTYPES);
    
    CMistToken* token = (CMistToken*)malloc(sizeof(CMistToken));
    if (token == NULL)
    {
        return NULL;
    }
    
    token->type = type;
    token->beg = beg;
    token->end = end;
    
    return token;
}

static void
mist_token_destroy(CMistToken* token)
{
    assert(token != NULL);
    free(token);
    return;
}

static char*
mist_parse_cond_expr(const char* beg, const char* end, int* is_concat)
{
    assert(mist_is_range_valid(beg, end));
    assert(beg < end);
    assert(is_concat != NULL);
    
    // The substring to parse must not begin or end with a whitespace.
    assert(mist_find_in_range_first_not_of(beg, end, wspace, sz_wspace) == beg);
    assert(mist_find_in_range_last_not_of(beg, end, wspace, sz_wspace) + 1 == end);
    
    *is_concat = 0;
    
    static const char concat[] = "concat";
    static const size_t concat_len = sizeof(concat) / sizeof(concat[0]) - 1;
    
    const char* nend = end - 1; 
    size_t len = end - beg;
    if ((*nend != ')') ||
        (len < concat_len + 3)  || // +2 for parentheses and at least 1 character in the name.
        (strncmp(beg, &concat[0], concat_len) != 0))
    {
        // cannot be a concat-expression, assume it is just a name
        return mist_get_substring(beg, end);
    }
    
    const char* nbeg = mist_find_in_range_first_not_of(beg + concat_len, nend, wspace, sz_wspace);
    // NB> If nbeg == nend, *nbeg would be ')', so there is no need 
    // to check that nbeg != nend separately.
    if (*nbeg != '(') 
    {
        // not a concat-expression, assume it is just a name
        return mist_get_substring(beg, end);
    }
    assert(nend - nbeg > 0); // otherwise (end - beg) would be less than 
                             // (concat_len + 3) above
    
    nbeg = mist_find_in_range_first_not_of(nbeg + 1, nend, wspace, sz_wspace);
    if (nbeg == nend) // only whitespace characters are between parentheses
    {
        // assume [beg; end) is just a name
        return mist_get_substring(beg, end);
    }
    
    nend = mist_find_in_range_last_not_of(nbeg, nend, wspace, sz_wspace);
    ++nend;
    
    *is_concat = 1;
    return mist_get_substring(nbeg, nend);
}

///////////////////////////////////////////////////////////////////////////
// Implementation: "public methods"
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// CMistTemplateGroup

CMistTemplateGroup*
mist_tg_create_impl(const char* name_main, 
               CGrowingArray* tpl_names, CGrowingArray* tpl_strings,
               const char* begin_marker, const char* end_marker,
               size_t* bad_index, char** error_descr)
{
    assert(name_main != NULL);
    assert(name_main[0] != '\0');
    
    assert(tpl_names != NULL);
    assert(tpl_strings != NULL);
    
    assert(begin_marker != NULL);
    assert(end_marker != NULL);
    assert(begin_marker[0] != '\0');
    assert(end_marker[0] != '\0');
    
    assert(grar_get_size(tpl_names) != 0);
    assert(grar_get_size(tpl_names) == grar_get_size(tpl_strings));
    
    assert(error_descr != NULL);
    *error_descr = NULL;
    
    if (bad_index != NULL)
    {
        *bad_index = (size_t)(-1);
    }
    
    CMistTemplateGroup* tg = (CMistTemplateGroup*)malloc(sizeof(CMistTemplateGroup));
    if (tg == NULL)
    {
        return NULL;
    }
    
    if (grar_create(&(tg->tpl)) == 0)
    {
        free(tg);
        return NULL;
    }
    // If something wrong happens below, we can call mist_tg_destroy_impl 
    // to clean up because all the members of 'tg' are now initialized.
    
    size_t num = grar_get_size(tpl_names);
    for (size_t i = 0; i < num; ++i)
    {
        // try to load the templates
        CMistTemplate* tp = NULL;
        const char* name = grar_get_element(tpl_names, const char*, i);
        
        if (mist_name_is_bad(name))
        {
            if (bad_index != NULL)
            {
                *bad_index = i;
            }
            
            *error_descr = (char*)malloc(strlen(errInvalidTemplateName) + strlen(name) + 1);
            if (*error_descr != NULL)
            {
                sprintf(*error_descr, errInvalidTemplateName, name);
            }
            
            mist_tg_destroy_impl(tg);
            return NULL;
        }
        
        EMistErrorCode ec = mist_template_from_string(&tp,
            name,    // name of the template
            grar_get_element(tpl_strings, const char*, i),  // string to load from
            begin_marker, end_marker,                       // markers for placeholders
            error_descr);
        
        if (ec != MIST_OK)
        {
            if (bad_index != NULL)
            {
                *bad_index = i;
            }
            mist_tg_destroy_impl(tg);
            return NULL;
        }
        
        assert(!mist_name_is_bad(tp->name));
        if (grar_add_element(&(tg->tpl), tp) == 0)
        {
            // The template #i is OK but there is not enough memory
            mist_template_destroy(tp);
            mist_tg_destroy_impl(tg);
            return NULL;
        }
    }
    
    // The templates have been loaded successfully.
    // Now create CMistTemplate structures for attributes, 
    // sort the templates by name, lookup the main one and make placeholder-template 
    // connections.
    EMistErrorCode ec = mist_tg_create_attrs(tg);
    if (ec != MIST_OK)
    {
        mist_tg_destroy_impl(tg);
        return NULL;
    }
    
    grar_sort(&(tg->tpl), mist_template_compare);
    
    // Lookup the main template
    CMistTemplate comp;
    CMistTemplate* pcomp = &comp;
    comp.name = (char*)strdup(name_main);
    if (comp.name == NULL)
    {
        mist_tg_destroy_impl(tg);
        return NULL;
    }
    
    int ind = grar_find(&(tg->tpl), &pcomp, mist_template_compare);
    if (ind < 0)
    {
        // the template with name 'name_main' is missing
        *error_descr = (char*)malloc(strlen(errNoMainTemplate) + strlen(name_main) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errNoMainTemplate, name_main);
        }
        
        // *bad_index will remain (-1).
        
        free(comp.name);
        mist_tg_destroy_impl(tg);
        return NULL;
    }
    free(comp.name);
    
    tg->main = grar_get_element(&(tg->tpl), CMistTemplate*, ind);
    assert(tg->main != NULL);
    
    // Make placeholder-template connections
    mist_tg_connect_templates(tg);
    
    return tg;
}

CMistTemplateGroup*
mist_tg_create_single_impl(const char* name, const char* str,
               const char* begin_marker, const char* end_marker,
               char** error_descr)
{
    assert(name != NULL);
    assert(name[0] != '\0');
    assert(str != NULL);
    
    assert(begin_marker != NULL);
    assert(end_marker != NULL);
    assert(begin_marker[0] != '\0');
    assert(end_marker[0] != '\0');
    
    assert(error_descr != NULL);
    *error_descr = NULL;

    CMistTemplateGroup* tg = NULL;
    
    CGrowingArray tpl_names;
    CGrowingArray tpl_strings;
    
    if (!grar_create(&tpl_names))
    {
        return NULL;
    }
    
    // CGrowingArray contains elements of type TGAElem which is not const
    // by default. Although 'name' and 'str' strings (see below) must never 
    // be changed by this function or mist_tg_create_impl(), we must cast
    // the respective pointers to TGAElem explicitly thus removing const 
    // specifier to work around this limitation of CGrowingArray. 
    // Yes, it is ugly, but it seems to be safer and in fact less error-prone 
    // than other alternatives available.
    if (!grar_add_element(&tpl_names, (TGAElem)name))
    {
        grar_destroy(&tpl_names);
        return NULL;
    }
    
    if (!grar_create(&tpl_strings))
    {
        grar_destroy(&tpl_names);
        return NULL;
    }
    if (!grar_add_element(&tpl_strings, (TGAElem)str))
    {
        grar_destroy(&tpl_strings);
        grar_destroy(&tpl_names);
        return NULL;
    }
    
    size_t bad_index;
    tg = mist_tg_create_impl(name, 
                        &tpl_names, &tpl_strings, 
                        begin_marker, end_marker,
                        &bad_index, error_descr);
    
    grar_destroy(&tpl_strings);
    grar_destroy(&tpl_names);
    return tg;
}

void
mist_tg_destroy_impl(CMistTemplateGroup* mtg)
{
    assert(mtg != NULL);
    grar_destroy_with_elements(&(mtg->tpl), mist_template_dtor, NULL);
    free(mtg);
    return;
}

EMistErrorCode
mist_tg_add_value_impl(CMistTemplateGroup* mtg, const char* name, const char* val)
{
    assert(mtg != NULL);
    assert(name != NULL);
    //assert(!mist_name_is_bad(name)); - too strict here
    assert(val != NULL);
    
    // Lookup the template first
    CMistTemplate comp;
    CMistTemplate* pcomp = &comp;
    comp.name = (char*)strdup(name);
    if (comp.name == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    int ind = grar_find(&(mtg->tpl), &pcomp, mist_template_compare);
    free(comp.name);
    
    if (ind != -1) // found the template with name 'name'
    {
        CMistTemplate* tp = grar_get_element(&(mtg->tpl), CMistTemplate*, ind);
        assert(tp != NULL);
        
        EMistErrorCode ec = mist_template_add_value(tp, val);
        return ec;
    }
    else
    {
        return MIST_OK;
    }
}

CGrowingArray*
mist_tg_evaluate_impl(CMistTemplateGroup* mtg)
{
    assert(mtg != NULL);
    assert(grar_get_size(&(mtg->tpl)) != 0);
    assert(mtg->main != NULL);
    
    // Clear 'evaluated' flag for each template before evaluating them
    size_t n = grar_get_size(&(mtg->tpl));
    CMistTemplate** tpl = grar_get_c_array(&(mtg->tpl), CMistTemplate*);
    for (size_t i = 0; i < n; ++i)
    {
        assert(tpl[i] != NULL);
        tpl[i]->is_evaluated = 0;
    }
        
    // Evaluate the templates
    EMistErrorCode ec = mist_template_evaluate(mtg->main);
    if (ec != MIST_OK)
    {
        return NULL;
    }
    
    return &(mtg->main->vals);
}

void
mist_tg_clear_values_impl(CMistTemplateGroup* mtg)
{
    assert(mtg != NULL);
    
    size_t n = grar_get_size(&(mtg->tpl));
    assert(n > 0);
    
    CMistTemplate** tpl = grar_get_c_array(&(mtg->tpl), CMistTemplate*);
    for (size_t i = 0; i < n; ++i)
    {
        assert(tpl[i] != NULL);
        mist_template_clear_values(tpl[i]);
        tpl[i]->is_evaluated = 0;
    }
    
    return;
}

EMistErrorCode
mist_tg_load_from_dir_impl(const char* dir, 
                      CMistTemplateGroup** main_tg, CMistTemplateGroup** path_tg,
                      char** error_descr)
{
    assert(dir != NULL);
    assert(main_tg != NULL);
    assert(path_tg != NULL);
    assert(error_descr != NULL);
    
    static char fp_tpl_name[] = "file_path";
    
    *main_tg = NULL;
    *path_tg = NULL;
    *error_descr = NULL;
    
    char* path = NULL;
    char* begm = NULL;
    char* endm = NULL;
    
    char* err = NULL;
    
    char* name_main = mist_path_get_last(dir);
    if (name_main == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    
    if (mist_name_is_bad(name_main))
    {
        *error_descr = (char*)malloc(strlen(errBadGroupName) + 
            strlen(dir) + strlen(name_main) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errBadGroupName, name_main, dir);
        }
        
        free(name_main);
        return MIST_BAD_NAME;
    }
    
    EMistErrorCode ec = mist_tg_load_conf(dir, &path, &begm, &endm, error_descr);
    if (ec != MIST_OK)
    {
        free(name_main);
        return ec;
    }
    
    // traverse the directory and process the files it contains
    CGrowingArray tpl_names;
    CGrowingArray tpl_strings;
    
    ec = mist_tg_process_dir(dir, &tpl_names, &tpl_strings, error_descr);
    if (ec != MIST_OK)
    {
        free(name_main);
        free(path);
        free(begm);
        free(endm);
        return ec;
    }
    assert(grar_get_size(&tpl_names) == grar_get_size(&tpl_strings));
        
    // create the groups of templates 
    *path_tg = mist_tg_create_single_impl(&fp_tpl_name[0], path, begm, endm, &err);
    if (*path_tg == NULL)
    {
        if (err != NULL) 
        {
            *error_descr = 
                (char*)malloc(strlen(errFailedToLoadTpl) + strlen(&fp_tpl_name[0]) + strlen(err) + 1);
            if (*error_descr != NULL)
            {
                sprintf(*error_descr, errFailedToLoadTpl, &fp_tpl_name[0], err);
            }
            free(err);
        }
        grar_destroy_with_elements(&tpl_names, NULL, NULL);
        grar_destroy_with_elements(&tpl_strings, NULL, NULL);
        free(name_main);
        free(path);
        free(begm);
        free(endm);
        return MIST_FAILED_TO_LOAD_TEMPLATE;
    }
    
    size_t bad_index = -1;
    *main_tg = mist_tg_create_impl(name_main, &tpl_names, &tpl_strings, begm, endm,
        &bad_index, &err);
    if (*main_tg == NULL)
    {
        if (err != NULL) 
        {
            const char* bad = NULL;
            if (bad_index != (size_t)(-1))
            {
                bad = grar_get_element(&tpl_names, const char*, bad_index);
            }
            else
            {
                bad = name_main;
            }
            assert(bad != NULL);
            
            *error_descr = 
                (char*)malloc(strlen(errFailedToLoadTpl) + strlen(bad) + strlen(err) + 1);
            if (*error_descr != NULL)
            {
                sprintf(*error_descr, errFailedToLoadTpl, bad, err);
            }
            free(err);
        }
        
        mist_tg_destroy_impl(*path_tg);
        *path_tg = NULL; //*path_tg must be NULL in case of failure
        
        grar_destroy_with_elements(&tpl_names, NULL, NULL);
        grar_destroy_with_elements(&tpl_strings, NULL, NULL);
        free(name_main);
        free(path);
        free(begm);
        free(endm);
        return MIST_FAILED_TO_LOAD_TEMPLATE;
    }
    
    grar_destroy_with_elements(&tpl_names, NULL, NULL);
    grar_destroy_with_elements(&tpl_strings, NULL, NULL);
    free(name_main);
    free(path);
    free(begm);
    free(endm);
    return ec;
}

EMistErrorCode
mist_tg_set_values_impl(CMistTemplateGroup* mtg, CStringMap* sm)
{
    assert(mtg != NULL);
    assert(sm != NULL);
    
    size_t n = smap_get_size(sm);
    TStringPair** vmap = smap_as_array(sm);
    
    for (size_t i = 0; i < n; ++i)
    {
        EMistErrorCode ec = mist_tg_add_value_impl(mtg, vmap[i]->key, vmap[i]->val);
        if (ec != MIST_OK)
        {
            return ec;
        }
    }
    
    return MIST_OK;
}

EMistErrorCode
mist_tg_generate_file_impl(CMistTemplateGroup* contents_tg, const char* path,
    CStringMap* params, char** error_descr)
{
    assert(contents_tg != NULL);
    assert(path != NULL);
    assert(params != NULL);
    assert(error_descr != NULL);
    
    *error_descr = NULL;
    
    EMistErrorCode ec = mist_tg_set_values_impl(contents_tg, params);
    if (ec != MIST_OK)
    {
        return ec;
    }
    
    // generate the contents of the file
    CGrowingArray* vals = mist_tg_evaluate_impl(contents_tg);
    if (vals == NULL)
    {
        return MIST_OUT_OF_MEMORY;
    }
    assert(grar_get_size(vals) > 0);
    
    // there should be a single resulting value
    if (grar_get_size(vals) != 1)
    {   
        assert(contents_tg->main != NULL);
        assert(contents_tg->main->name != NULL);
        
        *error_descr = 
            (char*)malloc(strlen(errMainTplMultivalued) + strlen(contents_tg->main->name) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errMainTplMultivalued, contents_tg->main->name);
        }
        return MIST_MAIN_TPL_MULTIVALUED;
    }
    
    const char* contents = grar_get_element(vals, const char*, 0);
    assert(contents != NULL);
    
    // create directory for the output file if necessary.
    if (!mist_create_path_for_file(path))
    {
        *error_descr = (char*)malloc(strlen(errCreateDirFailed) + strlen(path) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errCreateDirFailed, path);
        }
        return MIST_CREATE_DIR_FAILED;
    }
    
    // write output to the file
    FILE* fd = fopen(path, "w");
    if (fd == NULL)
    {
        *error_descr = 
            (char*)malloc(strlen(errWriteFileFailed) + strlen(path) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errWriteFileFailed, path);
        }
        return MIST_WRITE_FILE_FAILED;
    }
    
    size_t len = strlen(contents);
    len = fwrite(contents, 1, len, fd);
    if (ferror(fd))
    {
        *error_descr = 
            (char*)malloc(strlen(errWriteFileFailed) + strlen(path) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errWriteFileFailed, path);
        }
        fclose(fd);
        return MIST_WRITE_FILE_FAILED;
    }
    
    fclose(fd);
    return MIST_OK;
}

const char*
mist_tg_generate_path_string_impl(CMistTemplateGroup* path_tg, CStringMap* params, 
    char** error_descr)
{
    assert(path_tg != NULL);
    assert(params != NULL);
    assert(error_descr != NULL);
    
    *error_descr = NULL;
    
    EMistErrorCode ec = mist_tg_set_values_impl(path_tg, params);
    if (ec != MIST_OK)
    {
        return NULL;
    }
    
    // generate path to the output file
    CGrowingArray* vals = mist_tg_evaluate_impl(path_tg);
    if (vals == NULL)
    {
        return NULL;
    }
    assert(grar_get_size(vals) > 0);
    
    // there should be a single resulting value
    if (grar_get_size(vals) != 1)
    {   
        assert(path_tg->main != NULL);
        assert(path_tg->main->name != NULL);
        
        *error_descr = 
            (char*)malloc(strlen(errMainTplMultivalued) + strlen(path_tg->main->name) + 1);
        if (*error_descr != NULL)
        {
            sprintf(*error_descr, errMainTplMultivalued, path_tg->main->name);
        }
        return NULL;
    }
    
    const char* path = grar_get_element(vals, const char*, 0);
    assert(path != NULL);
    assert(*path != '\0');
    
    return path;
}
///////////////////////////////////////////////////////////////////////////
