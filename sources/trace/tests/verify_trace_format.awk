#!/bin/awk -f

BEGIN {
	FS = "\t"
	timestamp = -1
}
# 
($2 !~ "^\\[[[:digit:]]+\\]") || ($3 !~ "^[[:digit:]]+\\.[[:digit:]]+") {
	print "Line '" $0 "' has incorrect format."
	exit 1
}

{
	if((timestamp != -1) && ($3 < timestamp))
	{
		print "Line '" $0 "' has disordered timestamp."
		exit 1
	}
	timestamp = $3
}

