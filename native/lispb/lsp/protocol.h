#pragma once
#include <codegen/sexpr/sources.h>

#include <nlohmann/json.hpp>

namespace lispb::lsp {
namespace fs = std::filesystem;
namespace sexpr = codegen::sexpr;
using Json = nlohmann::json;
auto file_path(std::string_view uri) -> fs::path;
auto file_uri(fs::path const& path) -> std::string;
auto utf16_length(std::string_view bytes) -> std::uint32_t;
auto position(sexpr::Sources const& sources, sexpr::SourceFileId file, std::uint32_t offset)
    -> Json;
auto offset_at(sexpr::Sources const& sources, sexpr::SourceFileId file, Json const& position)
    -> std::uint32_t;
auto range(sexpr::Sources const& sources, sexpr::SourceSpan span) -> Json;
auto read_message(std::istream& input) -> std::optional<Json>;
void write_message(std::ostream& output, Json const& message);
}
