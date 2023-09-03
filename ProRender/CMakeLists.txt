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

# TODO: Add tests and install targets if needed.
