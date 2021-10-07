# Krust::IO

# Make sure Vulkan functions are exposed:
add_definitions( -DVK_PROTOTYPES )

# Build is hardcoded to xcb for now since there is no other option in the code:
#add_subdirectory (platform/xcb)

# Build the portable stuff:
#add_subdirectory (public-api)

# Portable public API:
FILE(GLOB KRUST_IO_PUBLIC_API_CXX_FILES "${KRUST_IO_PUBLIC_API_DIR}/*.cpp")
FILE(GLOB KRUST_IO_PUBLIC_API_HEADER_FILES "${KRUST_IO_PUBLIC_API_DIR}/*.h")

# Platform-specific public API:
FILE(GLOB KRUST_IO_PUBLIC_API_PLATFORM_WINDOWING_CXX_FILES "${KRUST_IO_PLATFORM_WINDOWING_PUBLIC_API_DIR}/*.cpp")
FILE(GLOB KRUST_IO_PUBLIC_API_PLATFORM_WINDOWING_HEADER_FILES "${KRUST_IO_PLATFORM_WINDOWING_PUBLIC_API_DIR}/*.h")
# --- FILE(GLOB KRUST_IO_INTERNAL_CXX_FILES "${KRUST_IO_INTERNAL_DIR}/*.cpp")

# Portable internal code:
FILE(GLOB KRUST_IO_INTERNAL_CXX_FILES "${KRUST_IO_INTERNAL_DIR}/*.cpp")
FILE(GLOB KRUST_IO_INTERNAL_HEADER_FILES "${KRUST_IO_INTERNAL_DIR}/*.h")

SET(KRUST_IO_CXX_FILES ${KRUST_IO_PUBLIC_API_CXX_FILES} ${KRUST_IO_PUBLIC_API_PLATFORM_WINDOWING_CXX_FILES} ${KRUST_IO_INTERNAL_CXX_FILES})

add_library(krust-io 
  ${KRUST_IO_CXX_FILES}
  ${KRUST_IO_PUBLIC_API_HEADER_FILES}
  ${KRUST_IO_PUBLIC_API_PLATFORM_WINDOWING_HEADER_FILES}
  ${KRUST_IO_INTERNAL_HEADER_FILES}
  README.md
)
target_include_directories(krust-io SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(krust-io PRIVATE ${VULKAN_LIBRARY} krust-kernel krust)

# Platform detection:
find_package(XCB)
IF(XCB_FOUND)
  message(STATUS "XCB Found. Header in ${XCB_INCLUDE_PATH}/xcb, library: ${XCB_LIBRARIES}")
  target_include_directories(krust-io SYSTEM PRIVATE ${XCB_INCLUDE_PATH})
  target_link_libraries(krust-io PRIVATE ${XCB_LIBRARIES})
  target_compile_definitions(krust-io PRIVATE "VK_USE_PLATFORM_XCB_KHR")
ENDIF(XCB_FOUND)