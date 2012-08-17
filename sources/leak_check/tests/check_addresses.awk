#!/bin/awk -f
# check_addresses.awk
# 
# This script checks if the detailed report prepared by LeakCheck 
# ('possible_leaks' or 'unallocated_frees' file) contains information
# about the blocks with given addresses.
# The addresses of interest must be specified in hexadecimal format - to be
# exact, as if "0x%x" was used in printf to prepare string representation
# for each of these.
#
# Total number of records ('totalRecords') in the report is checked as well
# (set it to the total number of detected memory leaks / unallocated frees).
# 
# Usage: 
#   awk -f check_addresses.awk \
#       -v totalRecords=<expected_total_number_of_records> \
#       -v addrKmalloc=<address> \
#       -v addrGfp=<address> \
#       -v addrKmemdup=<address> \
#       -v addrVmalloc=<address> \
#       <input_report_file>
########################################################################

BEGIN {
    addrKmallocFound = 0
    addrGfpFound = 0
    addrKmemdupFound = 0
    addrVmallocFound = 0
    records = 0
    unknownFound = 0
}

/^Address:/ {
	
    split($0, parts, "[ \\t,;]")
    block = parts[2]

	# Plain '==' without "t" would be simpler but mawk 1.3.3 seems to 
	# consider "0xffff88002ada0600" equal to "0xffff88002ada0b80" if 
	# one or both are the values of the variables set with -v and are
	# compared with '==' as they are.
    if (block "t" == addrKmalloc "t") {
        addrKmallocFound = 1
    } else if (block "t" == addrGfp "t") {
        addrGfpFound = 1
    } else if (block "t" == addrKmemdup "t") {
        addrKmemdupFound = 1
    } else if (block "t" == addrVmalloc "t") {
        addrVmallocFound = 1
    } else {
        printf("Unknown block address found: %s\n", block) > "/dev/stderr"
        unknownFound = 1
    }
    ++records
    next
}

END {
    if (records != totalRecords) {
        printf("Total number of records is %d in the input file but it should be %d\n", 
            records, totalRecords) > "/dev/stderr"
        exit 1
    }
    
    if (unknownFound) {
        exit 1
    }
    
    if (!addrKmallocFound) {
        printf("A record for block %s allocated by kmalloc() is missing.\n", 
            addrKmalloc) > "/dev/stderr"
        exit 1
    }
    
    if (!addrGfpFound) {
        printf("A record for block %s allocated by __get_free_pages() is missing.\n", 
            addrGfp) > "/dev/stderr"
        exit 1
    }
    
    if (!addrKmemdupFound) {
        printf("A record for block %s allocated by kmemdup() is missing.\n", 
            addrKmemdup) > "/dev/stderr"
        exit 1
    }
    
    if (!addrVmallocFound) {
        printf("A record for block %s allocated by vmalloc() is missing.\n", 
            addrVmalloc) > "/dev/stderr"
        exit 1
    }
}

