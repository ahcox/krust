# Krust core library

# Make sure Vulkan functions are exposed:
add_definitions( -DVK_PROTOTYPES )

# Krust internal build system variables:

set(KRUST_PUBLIC_API_DIR ${PROJECT_SOURCE_DIR}/krust/public-api)
set(KRUST_INTERNAL_DIR ${PROJECT_SOURCE_DIR}/krust/internal)

# Global include directories for all targets:

include_directories(${KRUST_PUBLIC_API_DIR})

# Spawn library build into subdirectories:

# Public API and export variables to parent scope:
add_subdirectory (public-api)
set(KRUST_PUBLIC_API_HEADER_FILES ${KRUST_PUBLIC_API_HEADER_FILES} PARENT_SCOPE)

# Internals:
add_subdirectory (internal)
set(KRUST_INTERNAL_HEADER_FILES ${KRUST_INTERNAL_HEADER_FILES} PARENT_SCOPE)

MESSAGE( STATUS "[top] KRUST_PUBLIC_API_HEADER_FILES: " ${KRUST_PUBLIC_API_HEADER_FILES})

# Define the main library build:

FILE(GLOB KRUST_PUBLIC_API_CXX_FILES "${KRUST_PUBLIC_API_DIR}/*.cpp")
FILE(GLOB KRUST_INTERNAL_CXX_FILES "${KRUST_INTERNAL_DIR}/*.cpp")

add_library(krust 
   ${KRUST_PUBLIC_API_CXX_FILES}
   ${KRUST_INTERNAL_CXX_FILES}
   ${KRUST_PUBLIC_API_HEADER_FILES}
   ${KRUST_INTERNAL_HEADER_FILES}
   # Put the root text files here as there is nowhere better:
   ../README.md ../BUILD.md ../LICENSE.md
   README.md
)
target_include_directories(krust SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(krust PRIVATE ${VULKAN_LIBRARY})
target_link_libraries(krust PUBLIC krust-kernel)
