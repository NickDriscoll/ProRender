# CMakeList.txt : CMake project for ProRender, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (
	ProRender
	"main.cpp"
	"VulkanGraphicsDevice.cpp"
	"VulkanWindow.cpp"
	"VulkanRenderer.cpp"
	"ImguiRenderer.cpp"
	"utils.cpp"
	"header_libs.cpp"

	"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs.c"

	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_demo.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp"
)

#set_property(TARGET ProRender PROPERTY CXX_STANDARD 20)
add_compile_definitions(ImTextureID=int)

#Turn on highest warning level + warnings as errors for this target
if(MSVC)
  target_compile_options(ProRender PRIVATE /W4 /WX)
else()
  target_compile_options(ProRender PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

#Include SDL2
target_include_directories(ProRender SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/external/SDL/include")
target_link_libraries(ProRender PUBLIC SDL3-static)

#Include Vulkan
target_include_directories(ProRender SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(ProRender PUBLIC ${Vulkan_LIBRARIES})

#Include hlslpp
target_include_directories(ProRender SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/external/hlslpp/include")

#Include Dear ImGUI
target_include_directories(ProRender SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/external/imgui")

#Include fastgltf
target_include_directories(ProRender SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/external/fastgltf/include")
target_link_libraries(ProRender PUBLIC fastgltf)

#Include the headers that sit directly in external
target_include_directories(ProRender SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/external")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ProRender PROPERTY CXX_STANDARD 20)
endif()

#Make bin output directory
add_custom_command(
			TARGET ProRender POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory
					"${CMAKE_SOURCE_DIR}/bin")

add_custom_command(
	TARGET ProRender POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
			"${CMAKE_SOURCE_DIR}/bin/shaders")

add_custom_command(
	TARGET ProRender POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
			"${CMAKE_SOURCE_DIR}/bin/images")

add_custom_command(
	TARGET ProRender POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
			"${CMAKE_SOURCE_DIR}/bin/models")

#Compile HLSL vertex shaders
#message("Current source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert")
foreach(file ${files})
	#message(INFO "vert shader ${file}")
	cmake_path(GET file FILENAME filename)
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND "$ENV{VULKAN_SDK}/bin/dxc" -spirv -T vs_6_7 -Fo "${CMAKE_SOURCE_DIR}/bin/shaders/${filename}.spv" "${file}"
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
		COMMAND "$ENV{VULKAN_SDK}/bin/dxc" -spirv -T ps_6_7 -Fo "${CMAKE_SOURCE_DIR}/bin/shaders/${filename}.spv" "${file}"
		VERBATIM)
endforeach()


file(GLOB files "${CMAKE_SOURCE_DIR}/data/images/*")
foreach(file ${files})
	#message(INFO "frag shader ${file}")
	cmake_path(GET file FILENAME filename)
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
				${file}
				${CMAKE_SOURCE_DIR}/bin/images/)
endforeach()

file(GLOB files "${CMAKE_SOURCE_DIR}/data/models/*")
foreach(file ${files})
	#message(INFO "frag shader ${file}")
	cmake_path(GET file FILENAME filename)
	add_custom_command(
		TARGET ProRender
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
				${file}
				${CMAKE_SOURCE_DIR}/bin/models/)
endforeach()

#The path of the compiled binary varies by OS
if (WIN32)
	set(BIN_PATH "${CMAKE_CURRENT_BINARY_DIR}/Debug/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
endif(WIN32)
if (UNIX)
	set(BIN_PATH "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
endif(UNIX)

#Copy executable to bin directory
add_custom_command(
		TARGET ProRender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		${BIN_PATH} ${CMAKE_SOURCE_DIR}/bin/)

# TODO: Add tests and install targets if needed.
