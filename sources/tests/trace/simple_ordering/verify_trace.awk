#!/bin/awk -f

BEGIN {
	next_id = 0
}

/test_message 0/ {
	if(next_id != 0)
	{
		print "Unexpected line '" $0 "' in the trace."
		exit 1;
	}
	next_id = next_id + 1;
}

/test_message 1/ {
	if(next_id != 1)
	{
		print "Unexpected line '" $0 "' in the trace."
		exit 1;
	}
	next_id = next_id + 1;
}

/test_message 2/ {
	if(next_id != 2)
	{
		print "Unexpected line '" $0 "' in the trace."
		exit 1;
	}
	next_id = next_id + 1;
}

/test_message 3/ {
	if(next_id != 3)
	{
		print "Unexpected line '" $0 "' in the trace."
		exit 1;
	}
	next_id = next_id + 1;
}

/test_message 4/ {
	if(next_id != 4)
	{
		print "Unexpected line '" $0 "' in the trace."
		exit 1;
	}
	next_id = next_id + 1;
}

END {
	if(next_id != 5)
	{
		print "Unexpected end of trace."
		exit 1;
	}
}

