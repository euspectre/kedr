#!/bin/sh

# Update dependencies file with template filenames, used in generation.
#
# Usage:
#
# update_deps file templates_dir

if test $# -ne 2; then
    printf "Usage: update_deps file templates_dir\n"
    exit 1
fi

deps_file=$1
templates_dir=$2

# newline symbol for use in variables.
# Note, that printf format string can use standard "\n".
nl='
'

old_deps=
if test -f $deps_file; then
    old_deps=`cat $deps_file`
fi

#template_files_in_dir <dir>
#
# Print full names of template files in the given directory. 
# Each name is enclosed into "".
template_files_in_dir()
{
    if test -d $1; then
        find "$1" -name "*.tpl" -printf "\"%p\"\n"
    fi
    #
    # Also add dependency on directory itself.
    #
    # If  <name>.tpl file will be added to directory and some template
    # refers to that name, then interpretation of that reference change
    # from parameter reference to template reference. Reverse effect will
    # be from deleting <name>.tpl file from directory.
    printf "\"$1\"\n"
}


new_deps="SET(deps_list${nl}"\
`template_files_in_dir ${templates_dir}/document`"${nl}"\
`template_files_in_dir ${templates_dir}/block`"${nl}"\
")${nl}"

if test -f $deps_file; then
    old_deps=`cat $deps_file`"${nl}"
    if test "${old_deps}" = "${new_deps}"; then
        # Already up-to-date
        exit 0
    #else
        #debug
        #printf "Dependencies has changed from:\n%s\nto:\n%s" "${old_deps}" "${new_deps}"
    fi
fi

printf "%s" "${new_deps}" > $deps_file

#debug
#printf "$deps_file has been updated\n"