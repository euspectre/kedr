"annotations" example

The module is almost the same in this example as in "sample_target" 
example. The difference is, device_create() and device_destroy() are 
annotated here with the special calls for LeakCheck to track these 
operations.

This example demonstrates how to use LeakCheck API in a module that is
not a plugin to KEDR by itself. One common use case is annotating custom
resource allocation/deallocation operations in a module you develop or,
at least, can rebuild.

See also the comments in module.c.

It is assumed that KEDR is installed to /usr/local.

Building:
	make

Usage:
	1. Start KEDR for "leak_check_annotations" as the target module:
	kedr start leak_check_annotations

	2. Load LeakCheck core:
	insmod /usr/local/lib/modules/`uname -r`/misc/kedr_leak_check.ko

Note that it would be OK to start LeakCheck along with KEDR as usual here. 
But this would also load its default plugins that track common memory 
alloc/free operations. In this example, it is clearer if we see the results 
of our annotated module only, i.e. if other events are not reported. 

	3. Load leak_check_annotations.ko. 

Character devices /dev/cfake* should be created by udev as a result.

	4. Try writing something to these devices (as root):
	echo "Something" > /dev/cfake0
	...

	5. Unload "leak_check_annotations" module.

	6. Check the report in 
<debugfs>/kedr_leak_check/leak_check_annotations/.

LeakCheck should report the allocations of struct device instances now.
	
	7. Unload LeakCheck core
	rmmod kedr_leak_check

	6. Stop KEDR.

You can also try to remove one of the annotations from the code, rebuild 
"leak_check_annotations" module, perform the steps described above and see 
how the LeakCheck's report changes.
