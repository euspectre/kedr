# Need to copy source file from there
set(CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_subdirectory(base)
add_subdirectory(instrumentor)
add_subdirectory(functions_support)
add_subdirectory(target_detector)

