# About

TBD

# Prerequisites

* meson 0.44 or newer
* ninja
* GCC version 5.4 or newer with plugin support
* development files and build prerequisites for the target kernel
* ...

If you plan to run the self-tests for KEDR, Python 3.5 is also needed.

It is assumed in each of the subsections here that the top source directory of KEDR is the current directory and building is done in `build` subdirectory there.

# Build

## Configuration

```console
mkdir build
meson --prefix=/usr ./build
```

## Building the user-mode components

```console
cd build
ninja
```

## Building the kernel-mode components

Copy `kernel` subdirectory to a location where you would like to build the kernel-mode components. Then cd to that subdirectory there and run `make`. `kernel/kedr/kedr.ko` will be built.

# Self-tests

As root:

```console
cd build
meson test --verbose
```

This will build the core kernel module and the test modules for the currently running kernel and run a series of tests.

**Do not run these tests on a production system!** If there are bugs in KEDR, the tests may crash the kernel or make it hang.

# Usage

TBD
