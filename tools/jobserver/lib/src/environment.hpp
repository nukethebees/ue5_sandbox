#pragma once

#include "jobserver/types.hpp"

#include <Windows.h>

#include <algorithm>
#include <cwchar>
#include <expected>

namespace jobserver::detail {
inline auto environment_text(std::string const& text) -> std::wstring {
    return path_from_utf8(text).wstring();
}

inline auto capture_environment() -> std::expected<std::vector<std::string>, Error> {
    std::vector<std::string> entries;
    auto const environment{GetEnvironmentStringsW()};
    if (environment == nullptr) {
        return std::unexpected(Error{"environment_failed", "Could not read process environment"});
    }
    for (auto current{environment}; *current != L'\0'; current += std::wcslen(current) + 1) {
        entries.push_back(path_to_utf8(std::filesystem::path{current}));
    }
    FreeEnvironmentStringsW(environment);
    return entries;
}

inline auto make_environment(Command const& command) -> std::expected<std::vector<wchar_t>, Error> {
    auto const base{command.base_environment
                        ? std::expected<std::vector<std::string>, Error>{*command.base_environment}
                        : capture_environment()};
    if (!base) {
        return std::unexpected(base.error());
    }
    std::vector<std::wstring> entries;
    for (auto const& entry : *base) {
        entries.push_back(environment_text(entry));
    }

    for (auto const& change : command.environment) {
        if (change.name.empty() || change.name.find('=') != std::string::npos ||
            change.name.find('\0') != std::string::npos ||
            (change.value && change.value->find('\0') != std::string::npos)) {
            return std::unexpected(Error{"invalid_environment", "Invalid environment change"});
        }
        auto const prefix{environment_text(change.name) + L'='};
        std::erase_if(entries, [&](std::wstring const& entry) {
            return entry.size() >= prefix.size() &&
                   _wcsnicmp(entry.c_str(), prefix.c_str(), prefix.size()) == 0;
        });
        if (change.value) {
            entries.push_back(prefix + environment_text(*change.value));
        }
    }
    std::ranges::sort(entries, [](auto const& left, auto const& right) {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    });
    std::vector<wchar_t> block;
    for (auto const& entry : entries) {
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
    }
    if (block.empty()) {
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}
}
