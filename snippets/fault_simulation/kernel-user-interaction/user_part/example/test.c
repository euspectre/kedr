#include "../fault_simulation.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	int result = 0;
	//for test that kernel module cannot be unloaded during library life
	//sleep(10); 
	result = set_indicator_fault_if_size_greater("kmalloc", 1000);
	if(result)
	{
		printf("Cannot set indicator: result is %d.\n", result);
	}
	else 
		printf("Indicator was set.\n");
	return 0;
}