#pragma once
#include <ioj/levels/diagnostics.h>

#include <expected>
#include <filesystem>
#include <string>

namespace ioj::levels::authoring {
template <typename Definition>
using DefinitionReadResult = std::expected<Definition, Diagnostics>;

template <typename Definition>
struct DefinitionEntry {
    std::filesystem::path source_path{};
    DefinitionReadResult<Definition> definition;
    std::string source_text{};
};
}
