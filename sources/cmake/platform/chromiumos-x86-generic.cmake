# This is a "toolchain" file for Chromium OS ("board" is "x86-generic").

########################################################################
# This toolchain file necessary to tell CMake we are going to perform 
# a cross-build (building for another system, "target system"). 
# When executing CMake, the path to this file should be specified in 
# -DCMAKE_TOOLCHAIN_FILE=...
#
# CMAKE_SYSTEM_VERSION should also be set then. It should contain the full 
# version string of the kernel, i.e., what "uname -r" would output on the 
# target system.
#
# Details: http://www.itk.org/Wiki/CMake_Cross_Compiling
# 
# Mandatory variables: 
# - CMAKE_SYSTEM_NAME - it actually indicates we are going to cross-build;
# - KEDR_ROOT_DIR or KEDR_SYSTEM_MAP_FILE (at least one of there should be 
#   defined) - these variables are necessary to locate System.map file, 
#   see below.
########################################################################

# KEDR_BOARD corresponds to BOARD environment variable on the machine 
# where Chromium OS is being built.
set (KEDR_BOARD "x86-generic")

# These variables are ARCH and CROSS_COMPILE variables used when building 
# the kernel for your target system. 
# ARCH defines the target architecture. KEDR_CROSS_COMPILE is actually a 
# prefix to identify appropriate cross-build tools like gcc, etc.
set (KEDR_ARCH "i386")
set (KEDR_CROSS_COMPILE "i686-pc-linux-gnu-")

set (KEDR_ROOT_DIR "/build/${KEDR_BOARD}/")

# When building for another system, System.map file for that system is used 
# to determine which functions are available (exported) there. 
# The default path to that file is 
# ${KEDR_ROOT_DIR}/boot/System.map-${KBUILD_VERSION_STRING}
# If you would like to use another path, please specify it in 
# KEDR_SYSTEM_MAP_FILE here
#set (KEDR_SYSTEM_MAP_FILE "some_path/boot/System.map")

set (CMAKE_SYSTEM_NAME "Linux")

# If you have defined KEDR_CROSS_COMPILE, you would probably want to also
# define C compiler here.
set (CMAKE_C_COMPILER "${KEDR_CROSS_COMPILE}gcc")
