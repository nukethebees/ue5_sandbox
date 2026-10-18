#pragma once

#include <filesystem>

namespace lispb {
class Project;
namespace codegen::sexpr {
class Syntax;
}

auto load_project(codegen::sexpr::Syntax& syntax, std::filesystem::path const& path) -> Project;

} // namespace lispb
