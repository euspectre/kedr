#!/bin/sh
grep -E "^[[:xdigit:]]+[[:space:]]+T[[:space:]]+$1$" /proc/kallsyms > /dev/null