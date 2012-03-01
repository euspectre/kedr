"mempool_ops" example

This is a plugin to LeakCheck that tracks the calls to mempool_alloc() and
mempool_free() and uses LeakCheck to analyze consistency of these calls.

The example demonstrates how to create plugins to LeakCheck using our 
template-based generation system and to track custom allocation / 
deallocation operations.

Information about the functions to be tracked is in payload.data file.

Building:
	make

Usage:
	1. Start KEDR with LeakCheck for a chosen target module as usual.
	2. Load the plugin ("leak_check_mempool_ops.ko")
	3. Load the target module and do something with it.
	4. Unload the target module.
	5. Check the report in <debugfs>/kedr_leak_check/<target_name>/.
	6. Unload the plugin and then stop KEDR.
