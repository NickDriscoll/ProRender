# CMakeList.txt : CMake project for ProRender, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (
	ProRender
	"ProRender.cpp"
	"ProRender.h"
)

#Include SDL2
target_include_directories(ProRender PUBLIC ${SDL_INCLUDE_DIRS})
target_link_libraries(ProRender PUBLIC ${SDL2_LIBRARIES})

#Include Vulkan
target_include_directories(ProRender PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(ProRender PUBLIC ${Vulkan_LIBRARIES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ProRender PROPERTY CXX_STANDARD 20)
endif()

#Compile HLSL vertex shaders
message("Current source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert")
foreach(file ${files})
	message(INFO "vert shader ${file}")
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND "dxc" -spirv -T vs_6_7 -Fo "${file}.spv" "${file}"
		VERBATIM)
endforeach()

#Compile HLSL fragment shaders
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
foreach(file ${files})
	message(INFO "frag shader ${file}")
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND "dxc" -spirv -T ps_6_7 -Fo "${file}.spv" "${file}"
		VERBATIM)
endforeach()

# TODO: Add tests and install targets if needed.
