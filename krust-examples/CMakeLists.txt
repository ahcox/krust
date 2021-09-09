# Krust Examples

# Make sure Vulkan functions are exposed:
add_definitions( -DVK_PROTOTYPES )

include_directories(${KRUST_IO_PUBLIC_API_DIR})

# Make sure Vulkan functions are exposed in the demos:
add_definitions( -DVK_PROTOTYPES )

# Find glslangValidator for compiling shaders:

if (NOT GLSLANG_INSTALL_DIR AND DEFINED ENV{GLSLANG_INSTALL_DIR})
    set(GLSLANG_INSTALL_DIR $ENV{GLSLANG_INSTALL_DIR})
endif()

message(STATUS "Using cmake find_program to look for glslangValidator")
set(GLSLANG_VALIDATOR_NAME "glslangValidator")
if(WIN32)
    set(GLSLANG_VALIDATOR_NAME "glslangValidator.exe")
endif()

if(GLSLANG_INSTALL_DIR)
    find_program(GLSLANG_VALIDATOR NAMES ${GLSLANG_VALIDATOR_NAME} HINTS "${GLSLANG_INSTALL_DIR}/bin")
else()
    find_program(GLSLANG_VALIDATOR NAMES ${GLSLANG_VALIDATOR_NAME})
endif()



add_executable (clear
  clear/clear.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  README.md
)
target_include_directories(clear SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(clear krust-io krust)



add_executable (clear2
  clear2/clear2.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  README.md
)
target_include_directories(clear2 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(clear2 krust-io krust)



add_custom_command(COMMENT "Compiling compute1 shader"
                   OUTPUT compute1.comp.spv
                   COMMAND ${GLSLANG_VALIDATOR} -V -o ${CMAKE_CURRENT_BINARY_DIR}/compute1_cmake.comp.spv
                           ${PROJECT_SOURCE_DIR}/krust-examples/compute1/compute1.comp.glsl
                   MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/krust-examples/compute1/compute1.comp.glsl
                   DEPENDS ${PROJECT_SOURCE_DIR}/krust-examples/compute1/compute1.comp.glsl ${GLSLANG_VALIDATOR})

add_executable (compute1
  compute1/compute1.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  ${PROJECT_SOURCE_DIR}/krust-examples/compute1/compute1.comp.glsl
  README.md
)
target_include_directories(compute1 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(compute1 krust-io krust)



add_custom_command(COMMENT "Compiling rt1 shader"
                   OUTPUT rt1.comp.spv
                   COMMAND ${GLSLANG_VALIDATOR} -V -o ${CMAKE_CURRENT_BINARY_DIR}/rt1.comp.spv
                           ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt1.comp.glsl
                   MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt1.comp.glsl
                   DEPENDS ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt1.comp.glsl
                           ${PROJECT_SOURCE_DIR}/krust-examples/rt1/intersections.inc.glsl
                           ${PROJECT_SOURCE_DIR}/krust-examples/rt1/utils.inc.glsl
                           ${GLSLANG_VALIDATOR})

add_custom_command(COMMENT "Compiling rt2 shader"
                           OUTPUT rt2.comp.spv
                           COMMAND ${GLSLANG_VALIDATOR} -V -o ${CMAKE_CURRENT_BINARY_DIR}/rt2.comp.spv
                                   ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt2.comp.glsl
                           MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt2.comp.glsl
                           DEPENDS ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt2.comp.glsl
                                   ${PROJECT_SOURCE_DIR}/krust-examples/rt1/intersections.inc.glsl
                                   ${PROJECT_SOURCE_DIR}/krust-examples/rt1/utils.inc.glsl
                                   ${GLSLANG_VALIDATOR})

add_custom_command(COMMENT "Compiling ray tracing in one week grey diffuse sphere shader"
                                   OUTPUT rtow_diffuse_grey.comp.spv
                                   COMMAND ${GLSLANG_VALIDATOR} -V -o ${CMAKE_CURRENT_BINARY_DIR}/rtow_diffuse_grey.comp.spv
                                           ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_diffuse_grey.comp.glsl
                                   MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_diffuse_grey.comp.glsl
                                   DEPENDS ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_diffuse_grey.comp.glsl
                                           ${PROJECT_SOURCE_DIR}/krust-examples/rt1/intersections.inc.glsl
                                           ${PROJECT_SOURCE_DIR}/krust-examples/rt1/utils.inc.glsl
                                           ${GLSLANG_VALIDATOR})

add_custom_command(COMMENT "Compiling ray tracing in one week grey diffuse sphere shader"
                                           OUTPUT rtow_materials.comp.spv
                                           COMMAND ${GLSLANG_VALIDATOR} -V -o ${CMAKE_CURRENT_BINARY_DIR}/rtow_materials.comp.spv
                                                   ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_materials.comp.glsl
                                           MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_materials.comp.glsl
                                           DEPENDS ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_materials.comp.glsl
                                                   ${PROJECT_SOURCE_DIR}/krust-examples/rt1/intersections.inc.glsl
                                                   ${PROJECT_SOURCE_DIR}/krust-examples/rt1/utils.inc.glsl
                                                   ${GLSLANG_VALIDATOR})

add_executable (rt1
  rt1/rt1.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt1.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rt2.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_diffuse_grey.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/rt1/rtow_materials.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/rt1/intersections.inc.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/rt1/utils.inc.glsl
  README.md
)
target_include_directories(rt1 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(rt1 krust-io krust)
# ToDo: target_link_libraries(rt1 krust-io krust-gm krust)


# Diagnostic messages:
MESSAGE( STATUS "[examples] KRUST_PUBLIC_API_HEADER_FILES: " ${KRUST_PUBLIC_API_HEADER_FILES})
MESSAGE( STATUS "[examples] VULKAN_INCLUDE_DIRECTORY: " ${VULKAN_INCLUDE_DIRECTORY})
