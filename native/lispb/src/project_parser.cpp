#include "project_parser.h"

#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/path_base.h>
#include <lispb/project.h>
#include <lispb/project_property.h>
#include <lispb/strings.h>
#include <lispb/target_kind.h>

#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace lispb {
namespace sexpr = codegen::sexpr;
using codegen::sexpr::Fields;
using codegen::sexpr::FormId;
using codegen::sexpr::SourceSpan;
using P = ProjectProperty;

ProjectParser::ProjectParser(codegen::sexpr::Syntax& syntax)
    : syntax_{syntax}
    , parser_{syntax} {}

auto ProjectParser::paths(Project& project, FormId const form, std::string_view const purpose)
    -> codegen::sexpr::HandleRange {
    if (!syntax_.is_list(form)) {
        parser_.fail(form, std::string{purpose} + " must be a list");
    }
    std::vector<StringId> values;
    for (auto const child : syntax_.children(form)) {
        values.push_back(parser_.text_id(child, purpose));
    }
    return project.append_paths(values);
}
auto ProjectParser::names(FormId const form, std::string_view const purpose)
    -> std::vector<StringId> {
    if (!syntax_.is_list(form)) {
        parser_.fail(form, std::string{purpose} + " must be a list");
    }
    std::vector<StringId> result;
    for (auto const child : syntax_.children(form)) {
        result.push_back(parser_.identifier(child, purpose));
    }
    return result;
}
void ProjectParser::output_path(Project& project,
                                TargetId const target,
                                FormId const form,
                                std::string_view const purpose) {
    auto const base{try_parse_serialized_path_base(syntax_.head(form))};
    if (!syntax_.is_list(form) || syntax_.children(form).size() != 2 || !base) {
        parser_.fail(form,
                     std::string{purpose} + " must be (project-path ...) or (build-path ...)");
    }
    auto const path_id{parser_.text_id(syntax_.child(form, 1), purpose)};
    auto const value{std::filesystem::path{syntax_.strings().view(path_id)}};
    if (value.is_absolute() || value.has_root_path()) {
        parser_.fail(syntax_.child(form, 1), std::string{purpose} + " must be relative");
    }
    project.set_output(target, *base, path_id);
}

auto ProjectParser::parse(codegen::sexpr::SourceFileId file,
                          codegen::sexpr::HandleRange forms,
                          std::filesystem::path const& project_path) -> Project {
    if (forms.count != 1 || syntax_.head(syntax_.forms(forms).front()) != "lispb-project") {
        auto const span{(forms.count == 0) ? SourceSpan{.file = file}
                                           : syntax_.token_span(syntax_.forms(forms).front())};
        parser_.fail(span, "project must contain exactly one 'lispb-project' form");
    }

    Fields const project_fields{parser_, syntax_.forms(forms).front(), "lispb-project", 0};
    project_fields.validate({P::language_version, P::project_root},
                            {"cpp-schema", "slate", "kernel", "material", "group"});
    if (parser_.text(project_fields.required(P::language_version), "language version") != "1") {
        parser_.fail(project_fields.required(P::language_version),
                     "unsupported Lispb language version; expected 1");
    }

    Project project{syntax_.strings()};
    project.root = (project_path.parent_path() /
                    parser_.text(project_fields.required(P::project_root), "project root"))
                       .lexically_normal();
    std::optional<DiagnosticId> first_error;
    for (auto const declaration : project_fields.declarations()) {
        try {
            auto const head{syntax_.head(declaration)};
            Fields const fields{parser_, declaration, head, 1};
            auto const kind{try_parse_serialized_target_kind(head)};
            if (kind == TargetKind::cpp_schema) {
                fields.validate({P::types, P::sources, P::output_root});
                auto const target{
                    project.append(TargetKind::cpp_schema,
                                   parser_.identifier(fields.positional(0), "target name"))};
                project.set_types(target, parser_.text_id(fields.required(P::types), "types path"));
                project.set_sources(target, paths(project, fields.required(P::sources), "sources"));
                output_path(project, target, fields.required(P::output_root), "output root");
            } else if (kind == TargetKind::slate) {
                fields.validate({P::sources, P::include_directories, P::output_root});
                auto const target{project.append(
                    TargetKind::slate, parser_.identifier(fields.positional(0), "target name"))};
                project.set_sources(target, paths(project, fields.required(P::sources), "sources"));
                if (auto const includes{fields.optional(P::include_directories)}) {
                    project.set_includes(target, paths(project, *includes, "include directories"));
                }
                output_path(project, target, fields.required(P::output_root), "output root");
            } else if (kind == TargetKind::kernel) {
                fields.validate({P::sources, P::output_root});
                auto const target{project.append(
                    TargetKind::kernel, parser_.identifier(fields.positional(0), "target name"))};
                project.set_sources(target, paths(project, fields.required(P::sources), "sources"));
                output_path(project, target, fields.required(P::output_root), "output root");
            } else if (kind == TargetKind::material) {
                fields.validate({P::source, P::artifact});
                auto const target{project.append(
                    TargetKind::material, parser_.identifier(fields.positional(0), "target name"))};
                auto const source{parser_.text_id(fields.required(P::source), "source path")};
                project.set_sources(target, project.append_paths(std::span{&source, 1}));
                output_path(project, target, fields.required(P::artifact), "artifact path");
            } else if (kind == TargetKind::group) {
                fields.validate({P::targets});
                auto const name{parser_.identifier(fields.positional(0), "group name")};
                project.add_group(name, names(fields.required(P::targets), "targets"));
            }
        } catch (sexpr::SourceError const& error) {
            if (!first_error) {
                first_error = error.diagnostic();
            }
        }
    }
    if (first_error) {
        throw sexpr::SourceError{syntax_.sources().diagnostics(), syntax_.sources(), *first_error};
    }

    try {
        project.resolve_groups();
    } catch (std::invalid_argument const& error) {
        parser_.fail(syntax_.forms(forms).front(), error.what());
    }
    return project;
}

}
