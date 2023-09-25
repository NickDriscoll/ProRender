# CMakeList.txt : CMake project for ProRender, include source and define
# project specific logic here.
#

set(CMAKE_BUILD_TYPE Debug)

# Add source to this project's executable.
add_executable (
	ProRender
	"ProRender.cpp"
	"ProRender.h"
	"VulkanGraphicsDevice.cpp"
	"VulkanGraphicsDevice.h"
	"VulkanWindow.h"
	"VulkanWindow.cpp"
	"slotmap.h"
	"header_libs.cpp"
)

#Include SDL2
target_include_directories(ProRender PUBLIC ${SDL_INCLUDE_DIRS})
target_link_libraries(ProRender PUBLIC ${SDL2_LIBRARIES})

#Include Vulkan
target_include_directories(ProRender PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(ProRender PUBLIC ${Vulkan_LIBRARIES})

#Include hlslpp
target_include_directories(ProRender PUBLIC "${HLSLPP_INCLUDE_DIRS}")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ProRender PROPERTY CXX_STANDARD 20)
endif()

#Make bin output directory
add_custom_command(
			TARGET ProRender POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory
					"${CMAKE_CURRENT_BINARY_DIR}/bin")

add_custom_command(
	TARGET ProRender POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
			"${CMAKE_CURRENT_BINARY_DIR}/bin/shaders")

add_custom_command(
	TARGET ProRender POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
			"${CMAKE_CURRENT_BINARY_DIR}/bin/images")

#Compile HLSL vertex shaders
#message("Current source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert")
foreach(file ${files})
	#message(INFO "vert shader ${file}")
	cmake_path(GET file FILENAME filename)
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND "$ENV{VULKAN_SDK}/bin/dxc" -spirv -T vs_6_7 -Fo "${CMAKE_CURRENT_BINARY_DIR}/bin/shaders/${filename}.spv" "${file}"
		VERBATIM)
endforeach()

#Compile HLSL fragment shaders
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
foreach(file ${files})
	#message(INFO "frag shader ${file}")
	cmake_path(GET file FILENAME filename)
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND "$ENV{VULKAN_SDK}/bin/dxc" -spirv -T ps_6_7 -Fo "${CMAKE_CURRENT_BINARY_DIR}/bin/shaders/${filename}.spv" "${file}"
		VERBATIM)
endforeach()


file(GLOB files "${CMAKE_SOURCE_DIR}/images/*")
foreach(file ${files})
	#message(INFO "frag shader ${file}")
	cmake_path(GET file FILENAME filename)
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
				${file}
				${CMAKE_CURRENT_BINARY_DIR}/bin/images/)
endforeach()

#Copy SDL2.dll to output directory
#add_custom_command(
#	TARGET ProRender POST_BUILD
#	COMMAND ${CMAKE_COMMAND} -E copy
#			${CMAKE_SOURCE_DIR}/redist/SDL2.dll
#			${CMAKE_CURRENT_BINARY_DIR}/bin/SDL2.dll)

#TODO: Figure out how to make this work on windows and linux simultaneously
add_custom_command(
		TARGET ProRender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
				${CMAKE_CURRENT_BINARY_DIR}/Debug/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}	#windows
				#${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}			#linux
				${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}10${CMAKE_EXECUTABLE_SUFFIX})



# TODO: Add tests and install targets if needed.
