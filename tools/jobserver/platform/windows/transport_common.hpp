#pragma once

#include "jobserver/platform/transport.hpp"

#include <Windows.h>

#include <memory>

namespace jobserver::platform::windows_detail {
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;

auto user_sid() -> std::wstring const&;
auto read_message(HANDLE handle) -> MessageResult;
auto write_message(HANDLE handle, std::string const& message) -> IoResult;
}
