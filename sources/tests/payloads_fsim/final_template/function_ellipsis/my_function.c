#include "my_function.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

module_author("Andrey Tsyvarev");
module_license("GPL");

char* my_kasprintf(const char* fmt, ...)
{
	char* result;
	va_list args;
	va_start(args, fmt);
	
	result = kvasprintf(fmt, args);
	
	va_end(args);
	
	return result;
	
}

int __init my_module_init(void)
{
	return 0;
}

void __exit my_module_exit(void)
{
}

module_init(my_module_exit);
module_exit(my_module_exit);
