#include "window.h"

#include "../engine.h"
#include "library.h"
#include "system.h"

#include <commctrl.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <expected>

namespace ioj::sandbox_host {
auto Window::handle() const -> HWND {
    return static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

auto Window::closed() const -> bool {
    return closed_;
}

auto CALLBACK Window::procedure(
    HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR context) -> LRESULT {
    auto& window{*reinterpret_cast<Window*>(context)};
    switch (message) {
        case WM_SIZE: {
            if (wp != SIZE_MINIMIZED) {
                window.size_changed_ = true;
            }
            break;
        }
        case WM_CLOSE: {
            window.closed_ = true;
            return 0;
        }
        case WM_NCDESTROY: {
            window.closed_ = true;
            break;
        }
    }

    return window.forward_(hwnd, message, wp, lp);
}

auto Window::show(Library const& library) -> Result {
    forward_ = library.wnd_proc;
    sync_ = library.sync;
    if (!SetWindowSubclass(handle(), procedure, 1, reinterpret_cast<DWORD_PTR>(this))) {
        return Result{std::unexpect, "Could not forward window messages to Unreal"};
    }
    if (auto result{synchronize(true)}; !result) {
        return result;
    }

    if (!SDL_ShowWindow(window_)) {
        return Result{std::unexpect, SDL_GetError()};
    }
    return {};
}

void Window::disconnect() {
    if (forward_) {
        RemoveWindowSubclass(handle(), procedure, 1);
    }
    forward_ = nullptr;
    sync_ = nullptr;
}

auto Window::synchronize_resize() -> Result {
    if (sync_ && size_changed_ && !IsIconic(handle())) {
        return synchronize(false);
    }
    return {};
}
}
