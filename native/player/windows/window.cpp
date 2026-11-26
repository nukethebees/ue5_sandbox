#include "player/windows/window.h"

#include "player/windows/unreal_library.h"

#include <commctrl.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

namespace ioj {
namespace {
auto CALLBACK forward_window_message(
    HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) -> LRESULT {
    // UELibrary_WndProc falls back to DefWindowProc, bypassing SDL's original procedure.
    // Give SDL the window-management messages and let Slate handle input and activation.
    switch (message) {
        case WM_CLOSE:
        case WM_SYSCOMMAND:
        case WM_NCCALCSIZE:
        case WM_NCHITTEST:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCLBUTTONDBLCLK:
        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE:
        case WM_WINDOWPOSCHANGING:
        case WM_WINDOWPOSCHANGED:
        case WM_SIZE:
        case WM_MOVE:
        case WM_GETMINMAXINFO:
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
        case WM_NCDESTROY:
            return DefSubclassProc(hwnd, message, wp, lp);
        default:
            return UELibrary_WndProc(hwnd, message, wp, lp);
    }
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
