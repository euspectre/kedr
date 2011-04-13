#!/bin/sh

# For each function in argument list verify,
# whether this functions is exported by the Linux kernel.
# Print space-separated list of functions, which are exported.

for function in $*; do
    if grep -E "^[[:xdigit:]]+[[:space:]]+T[[:space:]]+${function}$" /proc/kallsyms > /dev/null; then
        printf "${function} "
    fi
done