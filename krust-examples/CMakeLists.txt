# Krust Examples

# Make sure Vulkan functions are exposed:
add_definitions( -DVK_PROTOTYPES )

include_directories(${KRUST_IO_PUBLIC_API_DIR})

# Make sure Vulkan functions are exposed in the demos:
add_definitions( -DVK_PROTOTYPES )

add_executable (clear clear/clear.cpp ${KRUST_PUBLIC_API_HEADER_FILES})
target_include_directories(clear SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(clear krust-io krust)

add_executable (clear2 clear2/clear2.cpp ${KRUST_PUBLIC_API_HEADER_FILES})
target_include_directories(clear2 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(clear2 krust-io krust)

# Diagnostic messages:
MESSAGE( STATUS "[examples] KRUST_PUBLIC_API_HEADER_FILES: " ${KRUST_PUBLIC_API_HEADER_FILES})
MESSAGE( STATUS "[examples] VULKAN_INCLUDE_DIRECTORY: " ${VULKAN_INCLUDE_DIRECTORY})