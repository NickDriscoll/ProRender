#include <SDL3/SDL.h>
#include <imgui.h>
#include <math.h>
#include <stdio.h>
#include "tinyfiledialogs.h"

static constexpr float PI = 3.141592653589793f;

#define PRORENDER_UNUSED_PARAMETER(x) (void)x

#define ASSERT_OR_CRASH(pred, expect)                                                               \
    if ((pred) != expect) {                                                                      \
        char buffer[64];                \
        sprintf(buffer, "Fatal error occurred in %s at line %i", __FILE__, __LINE__); \
        tinyfd_messageBox("FATAL ERROR", buffer, "ok", "error", 1);           \
        exit(-1);                                                                           \
    }

#define VKASSERT_OR_CRASH(pred) ASSERT_OR_CRASH(pred, VK_SUCCESS)

ImGuiKey SDL2ToImGuiKey(int keycode);
int SDL2ToImGuiMouseButton(int button);

