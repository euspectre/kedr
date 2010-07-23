/// mist_base.h
/// Contains declatation of basic MiST functional elements.

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

#ifndef MIST_BASE_H_2235_INCLUDED
#define MIST_BASE_H_2235_INCLUDED

#include "grar.h"
#include "smap.h"
#include "mist_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/// A template is little more than a string with named placeholders.
/// Each placeholder (a named "hole in the document") can be viewed as a reference 
/// to another template. 
/// Each template stores the results of its evaluation and is generally 
/// a multivalued entity.
/// An attribute (parameter) is considered here a degenerate template, i.e. the one 
/// without a string and placeholders. It just contains a collection of its 
/// values which are to be set explicitly.
///
/// Example. Consider a template named "My Template":
/// "ab<$PH1$>cd<$PH2$><$PH3$>ef<$PH4$>"
/// Here are the following string chunks: "ab", "cd", "", "ef", "";
/// placeholders: "PH1", "PH2", "PH3", "PH4".
/// '<$' and '$>' are begin and end markers of the placeholders.
/// [NB] You can specify such markers when loading the template from a string.
/// Assume all four placeholders refer to the attributes with the following values:
///   PH1 = {"XX"}
///   PH2 = {"YY", "ZZ", "TT"}
///   PH3 = {"UU", "WW"}
///   PH4 = {"VV", "SS"}
/// The evaluated template will then have three values:
///   {"abXXcdYYUUefVV", "abXXcdZZWWefSS", "abXXcdTTWWefSS"}
/// That is, no cartesian product of the sets of values is used. The first resulting 
/// value is constructed using all the first values of the attributes, the second one - 
/// using the second ones, etc. If there are less values in an attribute than 
/// necessary, its last value is used.
/// 
/// A placeholder may contain a directive to join the values of the corresponding
/// template using a given separator, in which case the result is a single value.
/// This can be used to create a repetitive structures like lists, etc.
/// 
/// Compare the following examples using the above "My Template" template.
/// 1. "<$My Template$>" evaluates to a three-valued entity:
///   {"abXXcdYYUUefVV", "abXXcdZZWWefSS", "abXXcdTTWWefSS"}
/// 2. "<$My Template: join(=*=)$>" evaluates to a single-valued entity:
///   {"abXXcdYYUUefVV=*=abXXcdZZWWefSS=*=abXXcdTTWWefSS"}
/// 3. "<$My Template: join()$>" evaluates to a single-valued entity:
///   {"abXXcdYYUUefVVabXXcdZZWWefSSabXXcdTTWWefSS"}
/// 4. "<$My Template: join([+))$>" evaluates to a single-valued entity:
///   {"abXXcdYYUUefVV[+)abXXcdZZWWefSS[+)abXXcdTTWWefSS"}
/// 
/// A template reference with 'join' directive has the following format:
/// <begin_marker>[wspace]*<template_name>[wspace]*<colon>[wspace]*join[wspace]*(<separator>)[wspace]*<end_marker>,
/// where 'wspace' is ' ', '\t', '\n' or '\r'; 'colon' is ':'; 
/// '*' means "zero or more times".
/// The following escaped characters are recognized in separators: "\t", "\n" and "\r".
/// 
/// A valid name of a template may contain only the characters allowed in a file name
/// because a template is typically stored in a file with the same name and some extension,
/// "tpl", for example. This also imposes additional restrictions on the length of the 
/// template name. 
/// In addition, the name must not begin with a dot.
/// Of course, the name of a template must not contain begin or end marker 
/// sequences.
/// 
/// A template group with common placeholders is represented by CMistTemplateGroup 
/// structure. The structure is opaque, its fields should not be accessed directly.
/// There is always one main ("top-level") template in each group, the one that 
/// is evaluated when we ask the group to evaluate itself. Other templates in  
/// the group may also be evaluated then if they are referred to by the main 
/// template or by the templates it refers to, etc.
/// The result of the evaluation of a template group can be multi-valued.
/// Normally, however, the main template should be organized so that it  
/// evaluates to only one value (it is usually saved to a file).
///
/// Each template in a group must have a unique name.
/// 
/// [NB] If there is a recursion in the templates contained in a group, 
/// the behaviour of mist_tg_* functions is undefined. 
/// Two things are guaranteed though:
/// 1. The functions will neither hang nor crash.
/// 2. When the group is evaluated, each template will be evaluated no more
/// than once.
/// That is, one can use these functions in case of recursion too but to what 
/// exactly the group will evaluate is undefined.
/// 
/// Here are some examples of recursion in the templates (not all possible 
/// cases are listed, of course):
/// 1) template "A" has a placeholder that refers to template "A";
/// 2) template "A" has a placeholder that refers to template "B", template "B" 
/// has a placeholder that refers to template "A";
/// 3) and so on, and so forth.

struct CMistTemplateGroup_;
typedef struct CMistTemplateGroup_ CMistTemplateGroup;

///////////////////////////////////////////////////////////////////////////
// Methods
///////////////////////////////////////////////////////////////////////////

/// Unless specifically stated, if a function takes pointer as an argument 
/// and NULL is passed to it as the value of this argument, behaviour of the 
/// function is undefined.
/// 
/// For example, if a function takes 'error_descr', the latter must not be 
/// NULL.
/// Note that in case of failure, the function may return the description 
/// of the error in '*error_descr' although it is not mandatory. 
/// This string should be freed by the caller when no longer needed.
/// If the function succeeds or just does not provide the description of the
/// error, it will set '*error_descr' to NULL. 

/// [NB] The functions with names ending in "_impl" are named so, because the 
/// public API of MiST Engine is implemented mostly as a bunch of wrappers
/// around some of these functions. Public MiST functions must not take 
/// CGrowingArray or CStringMap structures, etc. 
/// 
/// For the sake of efficiency, T2C and MiST Engine executable use these 
/// "_impl" functions rather than public MiST API

///////////////////////////////////////////////////////////////////////////
// CMistTemplateGroup

/// Creates an template group and returns a pointer to it. 
/// NULL is returned in case of a failure.
/// The group will have name 'name', which is the name of the main template
/// and therefore must be present in 'tpl_names' array.
/// 'tpl_name' and 'tpl_strings' - arrays that contain the names of the templates 
/// to be contained in the group and the strings to load the templates from.
/// 'tpl_name' and 'tpl_strings' should contain at least one element each:
/// it makes no sense to create a group with no templates in it.
/// 
/// 'begin_marker' and 'end_marker' specify the placeholders in the strings 
/// that the templates are loaded from.
///
/// In case of failure, if bad_index != NULL, '*bad_index' will be the index 
/// of the template that failed to load ('tpl_names->data[*bad_index]' will be
/// its name). If no error occured or something wrong happened the group as 
/// a whole (out of memory, for example), '*bad_index' will be (size_t)(-1).
CMistTemplateGroup*
mist_tg_create_impl(const char* name_main, 
    CGrowingArray* tpl_names, CGrowingArray* tpl_strings,
    const char* begin_marker, const char* end_marker,
    size_t* bad_index, char** error_descr);

/// Like mist_tg_create_impl(), except only one template is created. Its name
/// is 'name' and its contents are loaded from string 'str'.
CMistTemplateGroup*
mist_tg_create_single_impl(const char* name, const char* str,
    const char* begin_marker, const char* end_marker,
    char** error_descr);

/// Destroys the template group (and all the templates it contains). 
void
mist_tg_destroy_impl(CMistTemplateGroup* mtg);

/// Adds a copy of the specified value to the template (attribute, usually) 
/// with the given name. Does nothing if there is no such attribute - this case
/// is not considered an error. No attribute - no problem.
EMistErrorCode
mist_tg_add_value_impl(CMistTemplateGroup* mtg, const char* name, const char* val);

/// Set the values of the attributes in the group of templates. 
/// <name_of_attribute, value> pairs are stored in the string map. 
/// The map MAY contain more than one <name_of_attribute, value> pair with the 
/// same 'name_of_attribute' in which case all these values are set for this 
/// multi-valued attribute.
/// The function leaves previously set values intact, i.e. it can be called 
/// several times with different string maps.
EMistErrorCode
mist_tg_set_values_impl(CMistTemplateGroup* mtg, CStringMap* sm);

/// Evaluate the group, i.e. construct the string value(s) of its main (top-level)
/// template performing all necessary substitutions, joining, etc.
/// The result can be multivalued, so the function returns a pointer to 
/// the array of these string values in case of success. NULL is returned in
/// case of failure (typically, if there is not enough memory).
/// The array and the strings contained in it are owned by the group and 
/// must not be freed by the caller.
CGrowingArray*
mist_tg_evaluate_impl(CMistTemplateGroup* mtg);

/// Clear the values of each template contained in the group (including
/// attributes but excluding special templates for "then" and "else" branches
/// as well as the result of the conditionals: these are NOT contained in the 
/// group, and their values will be cleared separately before evaluation). 
/// Use this function to 'reset' the group before setting new values for the
/// attributes in it.
void
mist_tg_clear_values_impl(CMistTemplateGroup* mtg);

/// Load the templates from the specified directory ('dir'). The directory must
/// exist. 
/// The function looks for a .cfg file in the directory, loads it and then tries
/// to create two template groups:
/// - '*main_tg' for the contents of the file to be generated;
/// - '*path_tg' for the path to this file.
///
/// Each template in '*main_tg' is loaded from a file in 'dir' having "tpl" 
/// extension. The name of the template is the name of the file. All such files
/// are loaded. One of the files should have the same name as the directory, it
/// is the main (top-level) template in this group.
///
/// There is the only template in '*path_tg'. It is loaded from the value of 
/// FILE_PATH_TEMPLATE parameter in the .cfg file.
/// 
/// If PH_BEGIN_MARKER and/or PH_END_MARKER parameters are defined in the .cfg
/// file, their values are used instead of the default begin and end markers
/// of the placeholders in the templates.
///
/// '*main_tg' and '*path_tg' will be set to NULL in case of failure.
EMistErrorCode
mist_tg_load_from_dir_impl(const char* dir, 
                      CMistTemplateGroup** main_tg, CMistTemplateGroup** path_tg,
                      char** error_descr);

/// Set the values of the parameters specified in 'params' in 'contents_tg' 
/// template group, evaluate the group and then attempt to store the results 
/// in an output file at the path specified.
/// The path to the file is the resulting value of 'path_tg', the contents of the
/// file - 'contents_tg'.
EMistErrorCode
mist_tg_generate_file_impl(CMistTemplateGroup* contents_tg, const char* path,
    CStringMap* params, char** error_descr);
    
/// Set the values of the parameters specified in 'params' in 'path_tg' template 
/// group, evaluate the group and return the result.
///
/// This function should be used to process the file path template to create a
/// string containing the path to the output file. 
/// If evaluation yields a multivalued result, it is considered an error.
/// The function returns a pointer to the constructed path on success, NULL on
/// failure. The resulting string is owned by 'path_tg' and must not be freed 
/// or altered by the caller.
const char*
mist_tg_generate_path_string_impl(CMistTemplateGroup* path_tg, CStringMap* params, 
    char** error_descr);

///////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
}
#endif

#endif // MIST_BASE_H_2235_INCLUDED
