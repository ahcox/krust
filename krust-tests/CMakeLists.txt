# Krust Tests

# Make sure Vulkan functions are exposed:
add_definitions( -DVK_PROTOTYPES )

set(KRUST_THIRDPARTY_CATCH_DIR ${PROJECT_SOURCE_DIR}/thirdparty/catch)

include_directories(${KRUST_IO_PUBLIC_API_DIR} ${KRUST_THIRDPARTY_CATCH_DIR})

# Make sure Vulkan functions are exposed in the demos:
add_definitions( -DVK_PROTOTYPES )

add_executable (krust-test-simple
  krust-test-simple.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  README.md
)
target_include_directories(krust-test-simple SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(krust-test-simple krust-io krust)
