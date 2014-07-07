#!/bin/awk -f

BEGIN {
	if(count_expected == 0)
	{
		print "'count_expected' variable should be assigned before verification."
		exit 1
	}
	block = -1
	count = 0
}

/block_in/ {
	if(block == 1)
	{
		print "Unexpected line '" $0 "' in the trace."
		print "Two 'block_in' without 'block_out' between them"
		exit 1
	}
	block = 1
	count = count + 1
}

/block_out/ {
	if(block == 0)
	{
		print "Unexpected line '" $0 "' in the trace."
		print "Two 'block_out' without 'block_in' between them"
		exit 1;
	}
	if(block == -1)
	{
		print "Unexpected line '" $0 "' in the trace."
		print "'block_out' without 'block_in' before it."
		exit 1
	}

	block = 0
}


END {
	if(block != 0)
	{
		print "Unexpected end of trace."
		exit 1
	}
	if(count != count_expected)
	{
		print "Number of blocks is " count ", but " count_expected " was expected."
		exit 1
	}
}

