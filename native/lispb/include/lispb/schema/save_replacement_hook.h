#pragma once

#include <lispb/schema/save_replace_file.h>

#include <filesystem>

namespace lispb::schema::detail {

using SaveReplacementHook = void (*)(std::filesystem::path const&,
                                     std::filesystem::path const&,
                                     SaveReplaceFile);

auto set_save_replacement_hook_for_testing(SaveReplacementHook hook) noexcept
    -> SaveReplacementHook;

} // namespace lispb::schema::detail
