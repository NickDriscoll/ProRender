#include <SDL3/SDL.h>
#include <imgui.h>
#include <math.h>
#include <stdio.h>
#include "tinyfiledialogs.h"

static constexpr float PI = 3.141592653589793f;

#define PRORENDER_UNUSED_PARAMETER(x) (void)x

#define PRORENDER_ASSERT(pred, expect)                                                               \
    if ((pred) != (expect)) {                                                                      \
        char error_message_buffer[256];                \
        sprintf(error_message_buffer, "Fatal error occurred in %s at line %i", __FILE__, __LINE__); \
        tinyfd_messageBox("FATAL ERROR", error_message_buffer, "ok", "error", 1);           \
        exit(-1);                                                                           \
    }

#define VKASSERT_OR_CRASH(pred) PRORENDER_ASSERT(pred, VK_SUCCESS)

ImGuiKey SDL2ToImGuiKey(int keycode);
int SDL2ToImGuiMouseButton(int button);

