#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Windows.h configures the SDK headers that publish these Win32 types and functions.
#include <windows.h> // IWYU pragma: keep
// IWYU pragma: begin_exports
#include <basetsd.h>
#include <errhandlingapi.h>
#include <libloaderapi.h>
#include <minwindef.h>
#include <processenv.h>
#include <windef.h>
#include <winerror.h>
#include <winnt.h>
#include <winuser.h>
// IWYU pragma: end_exports

#include <string>
#include <string_view>

namespace ioj::sandbox_host {
auto system_error(std::string_view operation, DWORD code) -> std::string;
}
