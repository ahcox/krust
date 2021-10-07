# Krust graphics maths library

set(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD ON CACHE BOOL "Use the experimental C++ standard SIMD short vector support in GCC-only.")
set(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD ON BOOL "Use the experimental C++ standard SIMD short vector support in GCC-only." PARENT_SCOPE)
if(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
    add_compile_definitions(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
endif()

# Krust internal build system variables:

set(KRUST_GM_PUBLIC_API_DIR ${PROJECT_SOURCE_DIR}/krust-gm/public-api)
set(KRUST_GM_INTERNAL_DIR ${PROJECT_SOURCE_DIR}/krust-gm/internal)

# Global include directories for all targets:

include_directories(${KRUST_GM_PUBLIC_API_DIR})

# Define the main library build:

FILE(GLOB KRUST_GM_PUBLIC_API_CXX_FILES "${KRUST_GM_PUBLIC_API_DIR}/*.cpp")
FILE(GLOB KRUST_GM_PUBLIC_API_HEADER_FILES "${KRUST_GM_PUBLIC_API_DIR}/*.h")
# Export the KRUST_PUBLIC_API_HEADER_FILES variable to the parent scope:
set(KRUST_GM_PUBLIC_API_HEADER_FILES ${KRUST_GM_PUBLIC_API_HEADER_FILES} PARENT_SCOPE)

add_library(krust-gm
   ${KRUST_GM_PUBLIC_API_CXX_FILES}
   ${KRUST_GM_PUBLIC_API_HEADER_FILES}
   README.md
)
target_link_libraries(krust-gm PUBLIC krust-kernel)
