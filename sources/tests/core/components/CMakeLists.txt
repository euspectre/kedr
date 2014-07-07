# Need to copy source file from there
set(CORE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/core")
set(arch_dir "arch/${Kbuild_ARCH}")

kbuild_include_directories(${CORE_SOURCE_DIR})

add_subdirectory(base)
add_subdirectory(instrumentor)
add_subdirectory(functions_support)
add_subdirectory(target_detector)

