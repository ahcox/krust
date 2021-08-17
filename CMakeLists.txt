cmake_minimum_required (VERSION 2.8.11)

project (Krust)

# Feature flags to control build:
set(KRUST_BUILD_CONFIG_DISABLE_LOGGING OFF CACHE BOOL "Set this to turn off and compile out logging code.")
if(KRUST_BUILD_CONFIG_DISABLE_LOGGING)
    add_definitions(-DKRUST_BUILD_CONFIG_DISABLE_LOGGING)
endif()

SET(KRUST_CMAKE_DIR "${PROJECT_SOURCE_DIR}/tools/cmake")
set(CMAKE_MODULE_PATH "${KRUST_CMAKE_DIR};${CMAKE_MODULE_PATH}")

# Used to decide if the current build is a Visual Studio one:
set(VS_BUILD_REGEX "(MSBuild|msdev|devenv|nmake)")

# Krust external configuration variables ("CACHE" variables):
# (Run ccmake in build directory to set them or use cmake gui)
if(WIN32)
  set(VULKAN_INCLUDE_DIRECTORY "C:/VulkanSDK/1.0.3.1/Include" CACHE STRING "Path to the Vulkan headers you want to build against. Set this to the include directory of the Vulkan SDK you are using.")
  set(VULKAN_LIBRARY_DIRECTORY "C:/VulkanSDK/1.0.3.1/Source/lib" CACHE STRING "Path to the Vulkan icd library you want to build against. Set this to the library directory of the Vulkan SDK you are using.")
else()
  set(VULKAN_INCLUDE_DIRECTORY "/usr/include/" CACHE STRING "Path to the Vulkan headers you want to build against. You may be building multiple Vulkan SDKs (debug, release, versions), so make sure to set this to the correct one.")
  set(VULKAN_LIBRARY_DIRECTORY "/usr/lib/" CACHE STRING "Path to the Vulkan icd library you want to build against. You may be building multiple Vulkan SDKs (debug, release, versions), so make sure to set this to the correct one.")
endif()
MESSAGE( STATUS "[top] VULKAN_INCLUDE_DIRECTORY:" ${VULKAN_INCLUDE_DIRECTORY} )
MESSAGE( STATUS "[top] VULKAN_LIBRARY_DIRECTORY:" ${VULKAN_LIBRARY_DIRECTORY} )
set(KRUST_ASSERT_LEVEL "5" CACHE STRING "Level for Krust assertions from 1 (fewest, rarest assertions), to 5 (most, most frequent assertions)" )

find_library(VULKAN_LIBRARY NAMES vulkan-1 vulkan HINTS ${VULKAN_LIBRARY_DIRECTORY})

# Compiler options:
if(CMAKE_BUILD_TOOL MATCHES "${VS_BUILD_REGEX}")
  # Visual Studio doesn't need any flags to turn on new features:
  set(CXX_11PLUS_FLAG "")
else()
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
  CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
  CHECK_CXX_COMPILER_FLAG("-std=c++1y" COMPILER_SUPPORTS_CXX1Y)
  CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
  CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
  if(COMPILER_SUPPORTS_CXX17)
      set(CXX_11PLUS_FLAG "-std=c++17")
  elseif(COMPILER_SUPPORTS_CXX14)
      set(CXX_11PLUS_FLAG "-std=c++14")
  elseif(COMPILER_SUPPORTS_CXX1Y)
      set(CXX_11PLUS_FLAG "-std=c++1y")
  elseif(COMPILER_SUPPORTS_CXX11)
      set(CXX_11PLUS_FLAG "-std=c++11")
  elseif(COMPILER_SUPPORTS_CXX0X)
      set(CXX_11PLUS_FLAG "-std=c++0x")
  else()
      set(CXX_11PLUS_FLAG "-error=\"C++11+ not supported\"")
      message(STATUS "The compiler ${CMAKE_CXX_COMPILER} doesn't recognise gcc flags for enabling C++11 or greater standards.")
  endif()
endif()

# Give the user the chance to override the flag using a tool like ccmake via a
# cached variable:
if(CMAKE_BUILD_TOOL MATCHES "${VS_BUILD_REGEX}")
	set(KRUST_CXX_STRICTNESS_FLAGS "/W4 /WX /MP3")
else()
  set(KRUST_CXX_STRICTNESS_FLAGS "-Wall -Werror")
endiF()

set(KRUST_CXX_EXTRA_COMPILER_FLAGS "${CXX_11PLUS_FLAG} ${KRUST_CXX_STRICTNESS_FLAGS}" CACHE STRING "Extra flags for the compiler. E.g., enable C++ 11+ here if your compiler is not GCC compatible.")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KRUST_CXX_EXTRA_COMPILER_FLAGS}")

# Krust internal build system variables:

set(KRUST_PUBLIC_API_DIR ${PROJECT_SOURCE_DIR}/krust/public-api)
set(KRUST_INTERNAL_DIR ${PROJECT_SOURCE_DIR}/krust/internal)
set(KRUST_IO_PUBLIC_API_DIR ${PROJECT_SOURCE_DIR}/krust-io/public-api)
set(KRUST_IO_INTERNAL_DIR ${PROJECT_SOURCE_DIR}/krust-io/internal)
# SET(KRUST_IO_INTERNAL_DIR "${PROJECT_SOURCE_DIR}/krust-io/internal") #@ToDo < What is this?
# The platform specific stuff is currently hardcoded to linux with xcb windowing:
if(WIN32)
  # ToDo try: add_definitions(-DWIN32_LEAN_AND_MEAN)
  SET(KRUST_IO_PLATFORM_WINDOWING_DIR "${PROJECT_SOURCE_DIR}/krust-io/platform/win32")
  SET(KRUST_IO_PLATFORM_WINDOWING_PUBLIC_API_DIR "${PROJECT_SOURCE_DIR}/krust-io/platform/win32/public-api")
else()
  SET(KRUST_IO_PLATFORM_WINDOWING_DIR "${PROJECT_SOURCE_DIR}/krust-io/platform/xcb")
  SET(KRUST_IO_PLATFORM_WINDOWING_PUBLIC_API_DIR "${PROJECT_SOURCE_DIR}/krust-io/platform/xcb/public-api")
endif()

# Global include directories for all targets:

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${KRUST_PUBLIC_API_DIR})
include_directories(${KRUST_IO_PUBLIC_API_DIR})
include_directories(${KRUST_IO_PLATFORM_WINDOWING_DIR})

# #defines for all libraries and executables:
add_definitions(-DKRUST_ASSERT_LEVEL=${KRUST_ASSERT_LEVEL})

# Core Krust library:
add_subdirectory (krust)

# Helper library to open a window and respond to input:
add_subdirectory (krust-io)

# Graphics maths library
add_subdirectory (krust-gm)
if(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
    add_compile_definitions(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
endif()

# Examples of Krust using Krust-io:
add_subdirectory (krust-examples)

# Unit tests:
add_subdirectory (krust-tests)
