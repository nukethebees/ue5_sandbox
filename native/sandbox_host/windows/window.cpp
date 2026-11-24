#include "window.h"

#include "../engine.h"
#include "library.h"
#include "system.h"

#include <bit>
#include <expected>

namespace ioj::sandbox_host {
Window::~Window() {
    if (handle_) {
        DestroyWindow(handle_);
    }
}

auto Window::handle() const -> HWND {
    return handle_;
}

auto Window::closed() const -> bool {
    return closed_;
}

auto CALLBACK Window::procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) -> LRESULT {
    auto* window{std::bit_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA))};
    if (message == WM_NCCREATE) {
        window = static_cast<Window*>(std::bit_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetLastError(0);
        if (!SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window)) &&
            GetLastError()) {
            return FALSE;
        }
    }

    if (!window) {
        return DefWindowProcW(hwnd, message, wp, lp);
    }

    switch (message) {
        case WM_SIZE: {
            if (wp != SIZE_MINIMIZED) {
                window->size_changed_ = true;
            }
            break;
        }
        case WM_CLOSE: {
            window->closed_ = true;
            return 0;
        }
        case WM_NCDESTROY: {
            window->handle_ = nullptr;
            window->closed_ = true;
            break;
        }
    }

    if (window->forward_) {
        return window->forward_(hwnd, message, wp, lp);
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

auto Window::create(HINSTANCE instance) -> Result {
    WNDCLASSW cls{};
    cls.lpfnWndProc = procedure;
    cls.hInstance = instance;
    cls.lpszClassName = L"SandboxHost";
    cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    if (!RegisterClassW(&cls)) {
        return Result{std::unexpect, system_error("RegisterClassW SandboxHost", GetLastError())};
    }

    handle_ = CreateWindowExW(0,
                              cls.lpszClassName,
                              L"Sandbox",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              startup_size.width,
                              startup_size.height,
                              nullptr,
                              nullptr,
                              instance,
                              this);
    if (!handle_) {
        return Result{std::unexpect, system_error("CreateWindowExW Sandbox", GetLastError())};
    }

    return {};
}

auto Window::show(Library const& library) -> Result {
    forward_ = library.wnd_proc;
    sync_ = library.sync;
    if (auto result{synchronize(true)}; !result) {
        return result;
    }

    ShowWindow(handle_, SW_SHOW);
    return {};
}

void Window::disconnect() {
    forward_ = nullptr;
    sync_ = nullptr;
}

auto Window::synchronize_resize() -> Result {
    if (sync_ && size_changed_ && !IsIconic(handle_)) {
        return synchronize(false);
    }
    return {};
}
}
