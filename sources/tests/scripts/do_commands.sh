#!/bin/sh

# ./do_commands file load|unload
# Process commands, given in file(first argument).
#
#  In "load" mode evaluate all strings in file, which starts with 'on_load'.
# ('on_load' keyword with spaces are stripped before executing)
# In "unload" mode evaluate all strings in file, which starts with 'on_unload',
# in reverse order.
#
#  If in "load" mode execution of some string returns not 0, stop to execute strings and
# "rollback" - process all previous strings in "unload" mode, and finally return 1.
# If in "unload" mode execution of some string returns not 0, stop to execute and return 1.
#
#  Sequence 
#
# some_command || ! printf error_msg
#
# may be used with keywords to trace error message in case of error.
#

newline='
'

usage_string="./do_commands file 'load'|'unload'"

if test $# -ne 2; then
    printf "{$usage_string}\n"
    exit 1
fi

file="$1"
if ! test -r "$file"; then
    printf "'%s' file does not exist or cannot be read.\n" "$file"
    exit 1
fi
mode="$2"

# execute command_line
#
# Execute command line, passed to it.
execute()
{
#    printf "%s\n" "execute: $*"
    eval $*
}

# parse_conf_on_load conf_file
#
# Translate config file to the list of commands in load mode
# Each command string is prepended with a string, contains ordinal number of the source string.
parse_conf_on_load()
{
    if ! test -r "$1"; then
        printf "parse_conf_on_load: cannot read file '%s'.\n" "$1"
        exit 1
    fi

    sed -n -e "s/^on_load[[:blank:]]\{1,\}//; t on_load; d; :on_load; =; p" "$1"
}

# parse_conf_on_unload conf_file [line_number]
#
# Translate config file to the list of commands for unload mode.
# Optional line_number argument means to translate
# only up to 'line_number' line(exclusive) in the configuration file.
parse_conf_on_unload()
{
    if ! test -r "$1"; then
        printf "parse_conf_on_load: cannot read file '%s'.\n" "$1"
        exit 1
    fi

    if test $2; then
        up_to_line="${2}q"
    else
        up_to_line=":start"
    fi
    
    sed -n  -e "${up_to_line}" -e "s/^on_unload[[:blank:]]\{1,\}//; t on_unload; d; :on_unload; p"\
            "$1" | sed '1!G;h;$!d'
}

# execute_conf_on_load conf_file
#
# Execute lines in the configuration file in 'load' mode.
execute_conf_on_load()
{
    OLD_IFS=${IFS}
    IFS="$newline"
    line_number=
    for LINE in `parse_conf_on_load "$1"`; do
        IFS=${OLD_IFS}
        if test -z ${line_number}; then
            # Store executed line for rollback in case of error, when this line will be executed.
            line_number="${LINE}"
        else
            if ! execute ${LINE}; then 
                printf "Error occured in \"load\" mode. Performing rollback.\n"
                rollback "$1" "$line_number"
                return 1
            fi
            line_number=
        fi
        IFS="$newline"
    done;
    IFS=${OLD_IFS}
}

# execute_conf_on_unload conf_file
#
# Execute lines in the configuration file in 'on_load' mode.
execute_conf_on_unload()
{
    OLD_IFS=${IFS}
    IFS="$newline"
    for LINE in `parse_conf_on_unload "$1"`; do
        IFS=${OLD_IFS}
        if ! execute ${LINE}; then
            printf "Error occured in \"unload\" mode. Aborting.\n"
            return 1
        fi
        IFS="$newline"
    done;
    IFS=${OLD_IFS}
}
# rollback conf_file string_number
#
# Rollback first 'string_number' commands from the config files(performs unload operation for them)
rollback()
{
    OLD_IFS=${IFS}
    IFS="$newline"
    for LINE in `parse_conf_on_unload "$1" "$2"`; do
        IFS=${OLD_IFS}
        execute ${LINE}
        IFS="$newline"
    done;
    IFS=${OLD_IFS}
}


case ${mode} in
load)
    execute_conf_on_load "$file"
    ;;
unload)
    execute_conf_on_unload "$file"
    ;;
*)
    printf "%s\n" "Incorrect mode '$mode' to operate in (should be 'load' or 'unload')."
esac