# Needed for all CMakeLists files, otherwise cmake will warn
cmake_minimum_required (VERSION 2.8.3)

project(kmodule)

include(cmake_useful)
find_package(Kbuild)
include(kbuild_system)

kbuild_add_definitions("${KBUILD_COMPILE_DEFINITIONS}")
kbuild_include_directories(${KBUILD_INCLUDE_DIRECTORIES})

kbuild_add_module(cmKbuildTryCompile ${SOURCES})
kbuild_link_module(cmKbuildTryCompile ${KBUILD_LINK_MODULE})

kbuild_finalize_linking()
