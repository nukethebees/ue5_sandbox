#pragma once

#include <codegen/sexpr/indices.h>

#include <filesystem>

namespace lispb {
class Project;
namespace codegen::sexpr {
class Syntax;
}

auto load_project(codegen::sexpr::Syntax& syntax, std::filesystem::path const& path) -> Project;
auto parse_project(codegen::sexpr::Syntax& syntax,
                   codegen::sexpr::SourceFileId file,
                   std::filesystem::path const& path) -> Project;

} // namespace lispb
