#include <SDL3/SDL.h>
#include <imgui.h>

#define PI 3.141592653589793f

#define PRORENDER_UNUSED_PARAMETER(x) (void)x

ImGuiKey SDL2ToImGuiKey(int keycode);
int SDL2ToImGuiMouseButton(int button);

