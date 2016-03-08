# Collection of methods for test KEDR control script
#

# get_kedr_* [<file>]
#
# Parse given file as output of 'kedr status' command,
# extract value of some property and print it.
#
# If <file> is not given, parse input stream.
#
# If property is not found, print nothing.

# Extract KEDR status.
get_kedr_status()
{
    if test "$#" -eq "0"; then
        f="-"
    else
        f="$1"
    fi
    sed -ne "/^KEDR status:/ { s/^KEDR status:[[:blank:]]//; p}" $f
}

# Extract list of payloads.
get_kedr_payloads()
{
    if test "$#" -eq "0"; then
        f="-"
    else
        f="$1"
    fi
    sed -ne "/^Payloads:/ { s/^Payloads:[[:blank:]]//; p}" $f
}

# Extract target name.
get_kedr_target()
{
    if test "$#" -eq "0"; then
        f="-"
    else
        f="$1"
    fi
    sed -ne "/^Target:/ { s/^Target:[[:blank:]]//; p}" $f
}

# Extract status of target.
get_kedr_target_status()
{
    if test "$#" -eq "0"; then
        f="-"
    else
        f="$1"
    fi
    sed -ne "/^Target status:/ { s/^Target status:[[:blank:]]//; p}" $f
}
