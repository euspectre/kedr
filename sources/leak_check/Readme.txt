kedr_leak_check
A tool based on KEDR framework that checks kernel modules for memory leaks
=======================================================================

"kedr_leak_check" is a tool (plugin, payload module) based on KEDR 
framework that allows, as its name implies, to check the target kernel 
module for memory leaks.

To do so, "kedr_leak_check" intercepts the calls to more than 20 kernel 
functions that deal with memory allocation and deallocation.

Unlike call monitoring facilities already provided by KEDR, this tool does 
not rely on trace events to operate. "kedr_leak_check" outputs nothing to 
the trace, it performs the analysis internally and outputs the results 
only. This can be more convenient compared to using call monitoring with 
subsequent analysis of the trace, for example, because the trace may grow 
very quickly and eventually become really huge. In one of our analysis 
sessions, the trace grew as large as 10 gigabytes in 20 minutes or so. The 
output of "kedr_leak_check" is generally much smaller in size.

[NB] For the present, "kedr_leak_check" cannot be used simultaneously with 
other kinds of payload modules provided by KEDR. In particular, you cannot 
use "kedr_leak_check" and fault simulation for memory-related functions at 
the same time. 
=======================================================================

Prerequisites
-------------

"kedr_leak_check" needs KEDR 0.1 installed (http://kedr.berlios.de/). 
The remaining prerequisites are the same as those of KEDR itself 
(see http://kedr.berlios.de/kedr-doc/getting_started.html).

[NB] "kedr_leak_check" may be included in the future releases of KEDR but 
is distributed separately for the present.
=======================================================================

Building
--------

To build "kedr_leak_check", it is enough to run "make".

If KEDR is installed to the directory (install prefix) different from 
"/usr/local", you need to specify it explicitly in KEDR_INSTALL_DIR 
variable. Example:

	KEDR_INSTALL_DIR=/home/tester/programs make

"kedr_leak_check.ko" kernel module should be built as a result. It is 
not necessary to do anything special to install it, it works as is.
=======================================================================

Typical Usage
-------------

"kedr_leak_check" can be used like any other payload module for KEDR
(see http://kedr.berlios.de/kedr-doc/using_kedr.html)

You can create a configuration file to avoid typing too much each time you 
start KEDR. The configuration file may look like this (replace the path to 
kedr_leak_check.ko with what it is on your system):

------------kedr_leak_check.conf------------
# Specify the full path to the payload module and, optionally,
# the parameters.
payload /home/tester/work/kedr_leak_check/kedr_leak_check.ko
--------------------------------------------

Now you can start KEDR normally:
	kedr start <target_name> kedr_leak_check.conf

Load the target module and do something with it as usual, then unload the 
target. Do not stop KEDR yet.

Take a look at "/sys/kernel/debug/kedr_leak_check" directory. Here we 
assume that debugfs is mounted to "/sys/kernel/debug". If it is not, you 
should mount it: 
	mount debugfs -t debugfs /sys/kernel/debug

There should be the following files in "kedr_leak_check" directory:
- info: 
	+ information about the target module (its name, addresses of 
		the "init" and "core" memory areas); 
	+ total number of memory allocations performed by the module;
	+ number of possible memory leaks (allocations without matching frees); 
	+ number of free-like calls without matching allocation calls;

- possible_leaks:
	+ information about each detected memory leak: address and size of 
		the memory block and a portion of the call stack of allocation;

- unallocated_frees:
	+ information about each free-like call without  matching allocation
		call: address of the memory block and a portion of the call stack 
		of that deallocation call.

[NB] "unallocated_frees" file should normally be empty. If it is not empty 
in some of your analysis sessions, it could be a problem in 
"kedr_leak_check" itself (e.g., the target module used some allocation 
function that "kedr_leak_check" was unaware of). Please report it to 
http://developer.berlios.de/bugs/?group_id=11780

Examples of "info" and "possible_leaks" files from a real analysis session
(target: "vboxsf" module from VirtualBox Guest Additions 3.2.10, the 
memory leak caught here is fixed in 3.2.12):

info:
--------------------------------------------
Target module: "vboxsf", init area at 0xe0ebc000, core area at 0xe14d6000
Memory allocations: 55
Possible leaks: 15
Unallocated frees: 0
--------------------------------------------

possible_leaks:
--------------------------------------------
Block at 0xc867dc80, size: 20; stack trace of the allocation:
[<e14d8571>] sf_make_path+0x51/0x1a0 [vboxsf]
[<e14d8ef0>] sf_path_from_dentry+0x160/0x1b0 [vboxsf]
[<e14d6a9b>] sf_lookup+0x5b/0x290 [vboxsf]
[<c0302746>] __lookup_hash+0xd6/0x120
[<c030517f>] do_last+0xcf/0x520
[<c03057b2>] do_filp_open+0x1e2/0x550
[<c02f8ee8>] do_sys_open+0x58/0x130

<...>

Block at 0xc7e5c3c0, size: 32; stack trace of the allocation:
[<e14d8571>] sf_make_path+0x51/0x1a0 [vboxsf]
[<e14d8ef0>] sf_path_from_dentry+0x160/0x1b0 [vboxsf]
[<e14d6a9b>] sf_lookup+0x5b/0x290 [vboxsf]
[<c0303052>] do_lookup+0x172/0x1d0
[<c03037c6>] link_path_walk+0x2a6/0x910
[<c0303f29>] path_walk+0x49/0xb0
[<c0304099>] do_path_lookup+0x59/0x90
--------------------------------------------

The format of stack traces is the same as it is used to output data about 
warnings and errors to the system log:

[<call_address>] <function_name>+<offset_in_func>/<size_of_func> [<module>]

To be exact, each address corresponds to the instruction following the 
relevant call.
=======================================================================

Analyzing the Results
---------------------

GDB, Objdump or some other tools of this kind can be used to locate the 
places in the source code corresponding to the entries in the stack traces.

The detailed description can be found here, for example:
http://kedr.berlios.de/kedr-doc/using_kedr.html, section "Analyzing the 
Trace".

The names of the functions from "init" area (those marked with "__init" in 
the source file of the target module) cannot be resolved and the relevant 
stack trace entries contain only raw call addresses. This is because name 
resolution is done when "init" area has already been dropped from memory. 

Using the the start address of the "init" area that "info" file shows and 
the technique described in "Analyzing the Trace" referred to above, you can 
overcome this.
=======================================================================

Stack Depth
-----------

The maximum number of stack frames displayed is controlled by "stack_depth" 
parameter of the module. That is, at most this many stack frames will be 
shown. 

"stack_depth" parameter is an unsigned integer, not greater than 16. 
Default value: 12. 

For example, to display at most 10 stack frames for each 
allocation/deallocation, modify the above configuration file as follows:

------------kedr_leak_check.conf------------
# Specify the full path to the payload module and, optionally,
# the parameters.
payload /home/tester/work/kedr_leak_check/kedr_leak_check.ko stack_depth=10
--------------------------------------------
=======================================================================

Notes
-----

When the target module is loaded, the output files are cleared, the results 
are reset. Please take this into account when loading and unloading the 
target module more than once while "kedr_leak_check" is loaded.

As usual with debugfs, the output files live only as long as 
kedr_leak_check.ko module is loaded. In particular, after unloading the 
target, please collect the results first and only after that reload the 
target or stop KEDR.
