#!/bin/awk -f
# check_summary.awk
# 
# This script checks if the summary report prepared by LeakCheck ('info' 
# file) contains expected data.
# 
# Usage: 
#   awk -f check_summary.awk \
#       -v expectedAllocs=<expected_total_alloc_count> \
#       -v expectedLeaks=<expected_leak_count> \
#       -v expectedBadFrees=<expected_unallocated_frees_count> \
#       <input_summary_report_file>
#
# Instead of <expected_total_alloc_count>, "nonzero" can be used if it
# is only needed to check that some allocations happened.
########################################################################

function printError(msg) {
	print "Error at " NR ": " msg > "/dev/stderr"
	exit 1
}

BEGIN {
    totalAllocs = 0
    possibleLeaks = 0
    unallocatedFrees = 0
}

/^Allocations:/ {
    sub("^Allocations:[\\t ]*", "")
    totalAllocs = $0
    next
}

/^Possible leaks:/ {
    sub("^Possible leaks:[\\t ]*", "")
    possibleLeaks = $0
    next
}

/^Unallocated frees:/ {
    sub("^Unallocated frees:[\\t ]*", "")
    unallocatedFrees = $0
    next
}

END {
	if (expectedAllocs "T" == "nonzeroT") {
		if (totalAllocs == 0) {
			printf("Total number of allocations must not be 0 but it is.\n") > "/dev/stderr"
	        exit 1
		}
	}
    else if (totalAllocs != expectedAllocs) {
        printf("Total number of allocations is %d but it should be %d\n", 
            totalAllocs, expectedAllocs) > "/dev/stderr"
        exit 1
    }
    
    if (possibleLeaks != expectedLeaks) {
        printf("Number of possible leaks is %d but it should be %d\n", 
            possibleLeaks, expectedLeaks) > "/dev/stderr"
        exit 1
    }

    if (unallocatedFrees != expectedBadFrees) {
        printf("Number of unallocated frees is %d but it should be %d\n", 
            unallocatedFrees, expectedBadFrees) > "/dev/stderr"
        exit 1
    }
}

