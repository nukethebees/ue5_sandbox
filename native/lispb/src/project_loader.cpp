#include <lispb/project_loader.h>

#include "project_parser.h"

#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/project.h>

namespace lispb {
auto load_project(codegen::sexpr::Syntax& syntax, std::filesystem::path const& path) -> Project {
    auto const project_path{std::filesystem::absolute(path).lexically_normal()};
    auto const file{syntax.sources().load(project_path)};
    return parse_project(syntax, file, project_path);
}
auto parse_project(codegen::sexpr::Syntax& syntax,
                   codegen::sexpr::SourceFileId file,
                   std::filesystem::path const& path) -> Project {
    auto const forms{codegen::sexpr::read_forms(syntax, file)};
    return ProjectParser{syntax}.parse(
        file, forms, std::filesystem::absolute(path).lexically_normal());
}
}
