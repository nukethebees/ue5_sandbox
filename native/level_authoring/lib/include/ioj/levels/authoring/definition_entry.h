#pragma once
#include <ioj/levels/diagnostics.h>

#include <expected>
#include <filesystem>
#include <string>

namespace ioj::levels::authoring {
template <typename Definition>
struct DefinitionEntry {
    std::filesystem::path source_path{};
    std::expected<Definition, Diagnostics> definition;
    std::string source_text{};
};
}
