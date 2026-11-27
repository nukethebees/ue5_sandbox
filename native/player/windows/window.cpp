#include "player/windows/window.h"

#include "player/windows/unreal_library.h"

#include <commctrl.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

namespace ioj {
namespace {
auto CALLBACK forward_window_message(
    HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) -> LRESULT {
    // SDL owns the window procedure and its input devices. Slate still needs the window
    // activation notifications for viewport focus, capture and popup dismissal.
    if (message == WM_ACTIVATE || message == WM_ACTIVATEAPP) {
        UELibrary_WndProc(hwnd, message, wp, lp);
    }
    return DefSubclassProc(hwnd, message, wp, lp);
}
}

auto native_handle(SDL_Window* window) -> HWND {
    return static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

auto connect_unreal_window(SDL_Window* window) -> ApplicationResult {
    if (!SetWindowSubclass(native_handle(window), forward_window_message, 1, 0)) {
        return std::unexpected{"Could not forward window messages to Unreal"};
    }
    return {};
}

void disconnect_unreal_window(SDL_Window* window) {
    RemoveWindowSubclass(native_handle(window), forward_window_message, 1);
}
}
