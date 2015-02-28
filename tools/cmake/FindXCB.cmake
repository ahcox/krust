#
# Simple minimal xcb finder 
# Pull in to a CMakeLists.txt with the following after setting CMAKE_MODULE_PATH:
#   find_package(XCB)
#
# Gives you the following variables:
# XCB_FOUND
# XCB_INCLUDE_PATH
# XCB_LIBRARIES
#

IF (NOT WIN32)

  find_path( XCB_INCLUDE_PATH xcb/xcb.h
    /usr/include
    DOC "Path to the xcb header.")

  find_library( XCB_LIBRARY
    NAMES xcb
    PATHS
    /usr/lib
    DOC "Path to the xcb library for linking")

  set(XCB_LIBRARIES ${XCB_LIBRARY})

ENDIF (NOT WIN32)

IF (XCB_INCLUDE_PATH)
  SET( XCB_FOUND 1 CACHE STRING "Set to 1 if XCB is found, 0 otherwise")
ELSE (XCB_INCLUDE_PATH)
  SET( XCB_FOUND 0 CACHE STRING "Set to 0 if XCB is not found, 1 otherwise")
ENDIF (XCB_INCLUDE_PATH)

# Hide XCB_FOUND in the ccmake GUI by default:
MARK_AS_ADVANCED( XCB_FOUND )

