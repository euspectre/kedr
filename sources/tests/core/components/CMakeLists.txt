if(KERNEL_PART)
    set(arch_dir "arch/${Kbuild_ARCH}")
    kbuild_include_directories(${CORE_SOURCE_DIR})
endif(KERNEL_PART)

add_subdirectory(base)
add_subdirectory(instrumentor)
add_subdirectory(functions_support)
add_subdirectory(target_detector)

