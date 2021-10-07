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

# Creates a custom command to build a shader
function(add_shader_command shader_file_base)
   #message("Executing add_shader_command() function on ${shader_file_base}")
   set(expanded_dependencies)
   foreach(inc_dep IN LISTS ARGN)
       #message("dependency: ${inc_dep}")
       set(expanded "${PROJECT_SOURCE_DIR}/krust-examples/shaders/${inc_dep}")
       #message("expanded: ${expanded}")
       list(APPEND expanded_dependencies ${expanded})
    endforeach()
   #message("Expanded:  ${expanded_dependencies}")
   add_custom_command(COMMENT "Compiling shader ${shader_file_base}"
        OUTPUT ${shader_file_base}.spv
        COMMAND ${GLSLANG_VALIDATOR} -V -o ${CMAKE_CURRENT_BINARY_DIR}/${shader_file_base}.spv
                ${PROJECT_SOURCE_DIR}/krust-examples/shaders/${shader_file_base}.glsl
        MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/krust-examples/shaders/${shader_file_base}.glsl
        DEPENDS ${expanded_dependencies}
                ${GLSLANG_VALIDATOR})
endfunction()

add_executable (clear
  clear/clear.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  README.md
)
target_include_directories(clear SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(clear krust-io krust krust-kernel)


add_executable (clear2
  clear2/clear2.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  README.md
)
target_include_directories(clear2 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(clear2 krust-io krust krust-kernel)


add_executable (compute1
  compute1/compute1.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/compute1.comp.glsl
  README.md
)
target_include_directories(compute1 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(compute1 krust-io krust krust-kernel)
add_shader_command(compute1.comp)

add_executable (rt1
  rt1/rt1.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rt1.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rt2.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rtow_diffuse_grey.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rtow_materials.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/text_print.comp.glsl
  README.md
)
target_include_directories(rt1 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(rt1 krust-io krust krust-kernel)
# ToDo: target_link_libraries(rt1 krust-io krust-gm krust)

# Shaders to compile and for each one its list of included shader code dependencies:
add_shader_command(rt1.comp header.inc.glsl utils.inc.glsl intersections.inc.glsl)
add_shader_command(rt2.comp header.inc.glsl utils.inc.glsl intersections.inc.glsl)
add_shader_command(rtow_diffuse_grey.comp header.inc.glsl utils.inc.glsl intersections.inc.glsl)
add_shader_command(rtow_materials.comp header.inc.glsl utils.inc.glsl intersections.inc.glsl rtow_final_scene.inc.glsl)
add_shader_command(text_print.comp header.inc.glsl)

add_executable (ray_queries1
  ray_queries1/ray_queries1.cpp
  ${KRUST_PUBLIC_API_HEADER_FILES}
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rt1.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rt2.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rtow_diffuse_grey.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/rtow_materials.comp.glsl
  ${PROJECT_SOURCE_DIR}/krust-examples/shaders/text_print.comp.glsl
  README.md
)
target_include_directories(ray_queries1 SYSTEM PRIVATE ${VULKAN_INCLUDE_DIRECTORY})
target_link_libraries(ray_queries1 krust-io krust krust-kernel)

# I could add more of these to trigger shader compilation from commandline:
add_custom_target(text_print_shader DEPENDS text_print.comp.spv)

# Diagnostic messages:
MESSAGE( STATUS "[examples] KRUST_PUBLIC_API_HEADER_FILES: " ${KRUST_PUBLIC_API_HEADER_FILES})
MESSAGE( STATUS "[examples] VULKAN_INCLUDE_DIRECTORY: " ${VULKAN_INCLUDE_DIRECTORY})
