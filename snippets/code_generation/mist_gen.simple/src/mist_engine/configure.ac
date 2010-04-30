# configure.ac
AC_PREREQ(2.61)

# Version-related definitions
m4_define([mist_major_version], [1])
m4_define([mist_minor_version], [0])
m4_define([mist_micro_version], [0])
m4_define([mist_version], [mist_major_version.mist_minor_version.mist_micro_version])
m4_define([mist_release_type],  [_final])

m4_define([mist_combine_ver],  [$1$2])
m4_define([mist_release_spec], [mist_combine_ver(mist_version, mist_release_type)])

# Initialization of Autoconf & Automake machinery
AC_INIT([mist_engine], [mist_release_spec], 
    [http://sourceforge.net/projects/template2code/develop])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.10.1 -Wall -Werror dist-bzip2 foreign tar-ustar])

MIST_ENGINE_VERSION_MAJOR=mist_major_version
MIST_ENGINE_VERSION_MINOR=mist_minor_version
MIST_ENGINE_VERSION_MICRO=mist_micro_version

AC_SUBST(MIST_ENGINE_VERSION_MAJOR)
AC_SUBST(MIST_ENGINE_VERSION_MINOR)
AC_SUBST(MIST_ENGINE_VERSION_MICRO)

# If CFLAGS value is not specified, use the default one
if test "x${CFLAGS}" = "x"; then
    CFLAGS="-Wall -g -O2 -DNDEBUG"
fi

AC_CANONICAL_HOST
AC_PROG_CC

if test "t$GCC" != "tyes"; then
    AC_MSG_ERROR([only GNU C compiler is currently supported])
fi
    
# Save the original value of ${CC} as the AC_PROG_CC_C99 may change ${CC}
# by adding some options to it.
CC_PLAIN=${CC}
AC_SUBST(CC_PLAIN)

AC_PROG_CC_C99
if test "t$ac_cv_prog_cc_c99" = "tno"; then
    AC_MSG_ERROR([the C compiler does not support C99 standard])
fi

AC_PROG_RANLIB

# Check if the host OS is MS Windows and we are building with MinGW
# At the same time, check if 64bit MS Windows system is used.
AC_MSG_CHECKING([if we are building on MS Windows OS with MinGW])
WIN_MACHINE_ARCH=X86
case "$host" in
  *-*-mingw*)
    mist_os_windows=yes
    case "$host" in
    x86_64-*-*)
      WIN_MACHINE_ARCH=X64
      ;;
    esac
    ;;
  *)
    mist_os_windows=no
    ;;
esac

# Check for Linux OS
case $host in
  *-*-linux*)
    mist_os_linux=yes
    ;;
esac

AC_MSG_RESULT([$mist_os_windows])
AC_SUBST(WIN_MACHINE_ARCH)

if test ! "t$mist_os_windows" = "tyes" && test ! "t$mist_os_linux" = "tyes"; then
    AC_MSG_ERROR([only Linux and Microsoft Windows platforms are currently supported])
fi
    
if test "t$mist_os_windows" = "tyes"; then
    AC_CHECK_TOOL(WINDRES, windres, no)
    if test "t$WINDRES" = "tno"; then
        AC_MSG_ERROR([windres is not found in PATH (usually it should be provided by MinGW).])
    fi
    AC_CHECK_PROG(ms_lib_exe, [lib.exe], [yes], [no])
else
    AC_PROG_LN_S
fi
AM_CONDITIONAL(MS_LIB_AVAILABLE, [test "t$ms_lib_exe" = "tyes"])
    
# The following conditional variables can be used in makefiles to provide
# for platform-specific options of compiler and linker, etc.
AM_CONDITIONAL(OS_WINDOWS, [test "t$mist_os_windows" = "tyes"])
AM_CONDITIONAL(OS_WINDOWS_X64, [test "t$WIN_MACHINE_ARCH" = "tX64"])
AM_CONDITIONAL(OS_LINUX, [test "t$mist_os_linux" = "tyes"])

# For now, just check that gcc version is 4.0 or newer and if so,
# it should support '-fvisibility' option on Linux.
GCC_MAJOR_VER=`${CC_PLAIN} -dumpversion | sed 's/\..*$//'`
AM_CONDITIONAL(GCC_HAS_FVISIBILITY, [test $GCC_MAJOR_VER -gt 3])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/mist_engine.c])
AC_CONFIG_FILES([ 
    Makefile
    src/Makefile
    src/mist_engine.h
    src/mist_engine.rc
])
AC_OUTPUT   
