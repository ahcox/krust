# Krust Tests

# Make sure Vulkan functions are exposed:
add_definitions( -DVK_PROTOTYPES )

set(KRUST_THIRDPARTY_CATCH_DIR ${PROJECT_SOURCE_DIR}/thirdparty/catch)
set(KRUST_THIRDPARTY_GLM_DIR ${PROJECT_SOURCE_DIR}/thirdparty/glm)

include_directories(${KRUST_IO_PUBLIC_API_DIR} ${KRUST_THIRDPARTY_CATCH_DIR} ${KRUST_THIRDPARTY_GLM_DIR})

# Make sure Vulkan functions are exposed in the demos:
add_definitions( -DVK_PROTOTYPES )

add_executable (krust-test-simple
  krust-test-simple.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  README.md
)
target_include_directories(krust-test-simple SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(krust-test-simple krust-io krust krust-kernel pthread)

# ToDo: move this test to the krust-gm directory.

add_executable (krust-test-gm
  krust-test-gm-main.cpp
  krust-test-vec3.cpp
  krust-test-vec4.cpp
  krust-test-mat4.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  ${KRUST_GM_PUBLIC_API_HEADER_FILES}
)
target_include_directories(krust-test-gm SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(krust-test-gm krust-gm krust-kernel krust krust-io pthread)

include(CTest) # ToDo discover the catch tests automatically and populate vscode test gui (requires catch 2 v3 upgrade?).