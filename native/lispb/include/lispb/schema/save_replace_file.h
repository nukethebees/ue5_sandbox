#pragma once

#include <filesystem>

namespace lispb::schema::detail {

using SaveReplaceFile = void (*)(std::filesystem::path const&, std::filesystem::path const&);

} // namespace lispb::schema::detail
