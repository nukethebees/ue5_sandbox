#pragma once

#include <filesystem>

namespace lispb {
class Project;
class EditableProjectDocument;
namespace codegen::sexpr {
class Syntax;
}

auto load_project(codegen::sexpr::Syntax& syntax, std::filesystem::path const& path) -> Project;
auto load_editable_project_document(std::filesystem::path const& path) -> EditableProjectDocument;

} // namespace lispb
