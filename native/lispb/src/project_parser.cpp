#include "project_parser.h"

#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/path_base.h>
#include <lispb/project.h>
#include <lispb/strings.h>

#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace lispb {
namespace sexpr = codegen::sexpr;
using codegen::sexpr::Fields;
using codegen::sexpr::FormId;
using codegen::sexpr::SourceSpan;

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
    if (!syntax_.is_list(form) || syntax_.children(form).size() != 2 ||
        (syntax_.head(form) != "project-path" && syntax_.head(form) != "build-path")) {
        parser_.fail(form,
                     std::string{purpose} + " must be (project-path ...) or (build-path ...)");
    }
    auto const path_id{parser_.text_id(syntax_.child(form, 1), purpose)};
    auto const value{std::filesystem::path{syntax_.strings().view(path_id)}};
    if (value.is_absolute() || value.has_root_path()) {
        parser_.fail(syntax_.child(form, 1), std::string{purpose} + " must be relative");
    }
    project.set_output(target,
                       syntax_.head(form) == "project-path" ? PathBase::project : PathBase::build,
                       path_id);
}

auto ProjectParser::parse_profile(FormId const form) -> KernelProfile {
    auto const value{parser_.text(form, "kernel profile")};
    if (value == "unreal") {
        return KernelProfile::unreal;
    }
    if (value == "standard") {
        return KernelProfile::standard;
    }
    parser_.fail(form, "unknown kernel profile '" + std::string{value} + "'");
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
    project_fields.validate({"language-version", "project-root"},
                            {"cpp-schema", "slate", "kernel", "material", "group"});
    if (parser_.text(project_fields.required("language-version"), "language version") != "1") {
        parser_.fail(project_fields.required("language-version"),
                     "unsupported Lispb language version; expected 1");
    }

    Project project{syntax_.strings()};
    project.root = (project_path.parent_path() /
                    parser_.text(project_fields.required("project-root"), "project root"))
                       .lexically_normal();
    std::optional<DiagnosticId> first_error;
    for (auto const declaration : project_fields.declarations()) {
        try {
            auto const head{syntax_.head(declaration)};
            Fields const fields{parser_, declaration, head, 1};
            if (head == "cpp-schema") {
                fields.validate({"types", "sources", "output-root"});
                auto const target{
                    project.append(TargetKind::cpp_schema,
                                   parser_.identifier(fields.positional(0), "target name"))};
                project.set_types(target, parser_.text_id(fields.required("types"), "types path"));
                project.set_sources(target, paths(project, fields.required("sources"), "sources"));
                output_path(project, target, fields.required("output-root"), "output root");
            } else if (head == "slate") {
                fields.validate({"sources", "include-directories", "output-root"});
                auto const target{project.append(
                    TargetKind::slate, parser_.identifier(fields.positional(0), "target name"))};
                project.set_sources(target, paths(project, fields.required("sources"), "sources"));
                if (auto const includes{fields.optional("include-directories")}) {
                    project.set_includes(target, paths(project, *includes, "include directories"));
                }
                output_path(project, target, fields.required("output-root"), "output root");
            } else if (head == "kernel") {
                fields.validate({"sources"}, {"emission"});
                auto const sources{paths(project, fields.required("sources"), "sources")};
                for (auto const emission : fields.declarations()) {
                    Fields const emission_fields{parser_, emission, "emission", 1};
                    emission_fields.validate({"profile", "output-root"});
                    auto const target{project.append(
                        TargetKind::kernel,
                        parser_.identifier(emission_fields.positional(0), "target name"))};
                    project.set_sources(target, sources);
                    project.set_profile(target, parse_profile(emission_fields.required("profile")));
                    output_path(
                        project, target, emission_fields.required("output-root"), "output root");
                }
            } else if (head == "material") {
                fields.validate({"source", "artifact"});
                auto const target{project.append(
                    TargetKind::material, parser_.identifier(fields.positional(0), "target name"))};
                auto const source{parser_.text_id(fields.required("source"), "source path")};
                project.set_sources(target, project.append_paths(std::span{&source, 1}));
                output_path(project, target, fields.required("artifact"), "artifact path");
            } else if (head == "group") {
                fields.validate({"targets"});
                auto const name{parser_.identifier(fields.positional(0), "group name")};
                project.add_group(name, names(fields.required("targets"), "targets"));
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
