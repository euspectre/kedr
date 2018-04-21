# About

KEDR tools collect data about the behaviour of the Linux kernel in runtime to reveal different kinds of bugs. The collected data can be analyzed either on-the-fly or post factum.

*KEDR is intended to detect data races and memory leaks - this is work in progress.*

# Prerequisites

* meson 0.45 or newer
* ninja
* GCC version 5.4 or newer with plugin support
* development files and build prerequisites for the target kernel (kernel version 4.14 and newer are supported)
* Python 3.5 or newer
* ...

It is assumed here that the current directory is the top source directory of KEDR. `build` subdirectory is used for building.

# Build

## Configuration

```console
mkdir build
meson --prefix=/usr ./build
```

## Building and installing the user-mode components

```console
cd build
ninja
ninja install
```

Among other things, the following components will be installed:

* <prefix>/(lib64|lib)/kedr/kedr-i13n.so - a GCC plugin that will instrument the kernel code;
* <prefix>/share/kedr/rules.yml - the rules the GCC plugin will use during instrumentation;
* <prefix>/src/kedr/kedr_helpers.{c,h} - the helper components, to be built into each kernel object (vmlinux or a module) to be analyzed.

## Building the kernel-mode components

Copy `kernel` subdirectory to a location where you would like to build the kernel-mode components. Then cd to that subdirectory and run `make`. The KEDR core module (`kernel/kedr/kedr.ko`) will be built.

# Self-tests

As root:

```console
cd build
meson test --verbose
```

This will build the core kernel module and the test modules for the currently running kernel and run a series of tests.

**Do not run these tests on a production system!** If there are bugs in KEDR, the tests may crash the kernel or make it hang.

# Usage

## Rebuild the components to be analyzed

First, it is necessary to rebuild the kernel components you would like to analyze, with specific compiler options and with helper components from KEDR. See build/tests/common_target/ (after you have built KEDR) for an example.

For each kernel object (vmlinux or a module) to be analyzed,  add `kedr_helpers.c` to its sources and make sure the header files from the kernel part of KEDR are available. Example:

```makefile
${module_name}-y += kedr_helpers.o
CFLAGS_kedr_helpers.o := -I "/home/eugene/work/kedr/kedr.git/kernel/include"
```

Adjust CFLAGS for each source file to be instrumented, so that the GCC plugin is used and the necessary C declarations are available:

```makefile
CFLAGS_some_file.o := \
  -fplugin="path/to/kedr-i13n.so" \
  -include "path/to/kedr_helpers.h" \
  -I "<prefix>/src/kedr/include"
```

Specify the path to the instrumentation rules in `KEDR_RULES_FILE` environment variable:

```console
export KEDR_RULES_FILE=path/to/rules.yml
```

*TBD: Perhaps, these steps will be automated in the future, so that the user does not have to specify all these settings explicitly.*

Then - build the kernel (or its selected components) as usual.

## Load the target kernel components and use KEDR

KEDR collects data in runtime, while the analyzed kernel components are running. To use it, boot the system with the respective kernel on the target machine and load KEDR core (`insmod path/to/kedr.ko`) there, then enable KEDR tools:

```console
echo 1 > /sys/kernel/kedr/enabled
```

KEDR core will then start monitoring the events in the given kernel components.

*TBD: describe different types of analysis when they are implemented.*

Note that if you are analyzing a module, you can load and enable KEDR core before or after that module is loaded (unlike KEDR 0.x where you had to load the core before the target module(s)).

When you are done with KEDR, disable it:

```console
echo 0 > /sys/kernel/kedr/enabled
```

Then you can unload `kedr.ko` or keep it loaded and re-enable it later when needed.

# Notes

## KEDR 1.x VS KEDR 0.x

KEDR 0.x instrumented the function calls in an unmodified kernel in runtime. This had some advantages (relatively easy to use, no need to rebuild the kernel) but also - severe limitations:

* only kernel modules could be analyzed, no support for vmlinux;
* KEDR core had to be loaded before the target modules;
* x86 only, because KEDR 0.x operated on the binary code and it was rather difficult to port it to other architectures;
* KEDR 0.x has grown quite complex internally, hard to maintain;
* complex constructs were used to track all needed alloc/free events, lots of functions to intercept - this is mostly because KEDR 0.x could not monitor vmlinux;
* etc.

KEDR 1.x uses a different approach, which may make it more difficult to use at the moment but removes most of these limitations.

Part of the work is done by GCC with a special plugin `kedr-i13n` when the kernel components to be analyzed are rebuilt. According to `rules.yml`, the plugin looks for the needed constructs in the code, e.g., calls to `__kmalloc` and `kfree`, and inserts some code before and/or after such calls. That added code usually gets the needed info from the arguments and return value of the target functions (for example, address and size of the allocated memory block) and passes it to the special functions, KEDR stubs.

The stubs do nothing by themselves but KEDR core can attach to and detach from them in runtime, at any moment. KEDR uses [Ftrace](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/trace/ftrace-uses.rst?h=v4.16) for that, similar to how [Livepatch](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/livepatch/livepatch.txt?h=v4.16) does it.

As a result of this split into compile-time and runtime operations, KEDR can now analyze the code from both vmlinux and the modules. KEDR core no longer has to intercept tons of functions, now it gets events like "memory allocation", "memory deallocation", etc., no matter which kernel function generated these events. Internally, KEDR core becomes smaller and easier to maintain.

Ftrace allows to "attach" KEDR to the analyzed code at any moment. Custom facilities to replace function calls are no longer needed because Ftrace offers a reliable way to do that, which is supported by the mainline kernels.

Although KEDR 1.x has been tested on x86_64 so far, it might be much easier to port it to other architectures supported by Livepatch than KEDR 0.x.
