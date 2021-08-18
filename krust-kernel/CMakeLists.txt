# Krust kernel library

# Krust internal build system variables:

set(KRUST_KERNEL_PUBLIC_API_DIR ${PROJECT_SOURCE_DIR}/krust-kernel/public-api)
set(KRUST_KERNEL_INTERNAL_DIR ${PROJECT_SOURCE_DIR}/krust-kernel/internal)

# Global include directories for all targets:

include_directories(${KRUST_KERNEL_PUBLIC_API_DIR})

# Define the main library build:

FILE(GLOB KRUST_KERNEL_PUBLIC_API_CXX_FILES "${KRUST_KERNEL_PUBLIC_API_DIR}/*.cpp")
FILE(GLOB KRUST_KERNEL_PUBLIC_API_HEADER_FILES "${KRUST_KERNEL_PUBLIC_API_DIR}/*.h")
# Export the KRUST_PUBLIC_API_HEADER_FILES variable to the parent scope:
set(KRUST_KERNEL_PUBLIC_API_HEADER_FILES ${KRUST_KERNEL_PUBLIC_API_HEADER_FILES} PARENT_SCOPE)

add_library(krust-kernel
   ${KRUST_KERNEL_PUBLIC_API_CXX_FILES}
   ${KRUST_KERNEL_PUBLIC_API_HEADER_FILES}
   README.md
)
