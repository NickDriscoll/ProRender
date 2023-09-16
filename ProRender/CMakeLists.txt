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
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert")
foreach(file ${files})
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND "$ENV{VULKAN_SDK}/bin/dxc.exe" -spirv -T vs_6_7 -Fo "${file}.spv" "${file}"
		VERBATIM)
endforeach()

#Compile HLSL fragment shaders
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
foreach(file ${files})
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND "$ENV{VULKAN_SDK}/bin/dxc.exe" -spirv -T ps_6_7 -Fo "${file}.spv" "${file}"
		VERBATIM)
endforeach()

#Make bin output directory
add_custom_command(
			TARGET ProRender POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory
					"${CMAKE_CURRENT_BINARY_DIR}/bin")

add_custom_command(
	TARGET ProRender POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
			"${CMAKE_CURRENT_BINARY_DIR}/bin/shaders")


#Copy IR shaders to output directory
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.spv")
foreach(file ${files})
	add_custom_command(
			TARGET ProRender POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					${file}
                	${CMAKE_CURRENT_BINARY_DIR}/bin/shaders/)
endforeach()

#Copy SDL2.dll to output directory
add_custom_command(
	TARGET ProRender POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
			${CMAKE_SOURCE_DIR}/redist/SDL2.dll
			${CMAKE_CURRENT_BINARY_DIR}/bin/SDL2.dll)

add_custom_command(
		TARGET ProRender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
				${CMAKE_CURRENT_BINARY_DIR}/Debug/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}
				${CMAKE_CURRENT_BINARY_DIR}/bin/)



# TODO: Add tests and install targets if needed.
