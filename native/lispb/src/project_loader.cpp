#include <lispb/project_loader.h>

#include "lispb/kernel_profile.h"
#include "lispb/path_base.h"
#include "lispb/rooted_path.h"
#include "lispb/target_kind.h"
#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_location.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/compiler_storage.h>
#include <lispb/editable_project_document.h>
#include <lispb/project.h>
#include <lispb/project_source_list_range.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb {
namespace {
using codegen::sexpr::Fields;
using codegen::sexpr::FormId;
using codegen::sexpr::SourceError;
using codegen::sexpr::SourceSpan;

[[noreturn]] void
    fail(codegen::sexpr::Syntax const& syntax, FormId const form, std::string const& message) {
    throw SourceError{syntax.sources(), syntax.token_span(form), message};
}

auto text(codegen::sexpr::Syntax const& syntax, FormId const form, std::string_view const purpose)
    -> std::string {
    return std::string{lispb::codegen::sexpr::text(
        syntax, form, purpose, lispb::codegen::sexpr::throw_source_error)};
}

auto text_id(codegen::sexpr::Syntax const& syntax,
             FormId const form,
             std::string_view const purpose) -> StringId {
    static_cast<void>(
        codegen::sexpr::text(syntax, form, purpose, codegen::sexpr::throw_source_error));
    return syntax.text_id(form);
}
auto identifier(codegen::sexpr::Syntax const& syntax,
                FormId const form,
                std::string_view const purpose) -> IdentifierId {
    auto const value{text_id(syntax, form, purpose)};
    return syntax.kind(form) == codegen::sexpr::TokenKind::atom ? syntax.identifier(form)
                                                                : syntax.strings().intern(value);
}
auto paths(Project& project,
           codegen::sexpr::Syntax const& syntax,
           FormId const form,
           std::string_view const purpose) -> codegen::sexpr::HandleRange {
    if (!syntax.is_list(form)) {
        fail(syntax, form, std::string{purpose} + " must be a list");
    }
    std::vector<StringId> values;
    for (auto const child : syntax.children(form)) {
        values.push_back(text_id(syntax, child, purpose));
    }
    return project.append_paths(values);
}
auto names(codegen::sexpr::Syntax const& syntax, FormId const form, std::string_view const purpose)
    -> std::vector<IdentifierId> {
    if (!syntax.is_list(form)) {
        fail(syntax, form, std::string{purpose} + " must be a list");
    }
    std::vector<IdentifierId> result;
    for (auto const child : syntax.children(form)) {
        result.push_back(identifier(syntax, child, purpose));
    }
    return result;
}
void output_path(Project& project,
                 TargetId const target,
                 codegen::sexpr::Syntax const& syntax,
                 FormId const form,
                 std::string_view const purpose) {
    if (!syntax.is_list(form) || syntax.children(form).size() != 2 ||
        (syntax.head(form) != "project-path" && syntax.head(form) != "build-path")) {
        fail(
            syntax, form, std::string{purpose} + " must be (project-path ...) or (build-path ...)");
    }
    auto const path_id{text_id(syntax, syntax.child(form, 1), purpose)};
    auto const value{std::filesystem::path{syntax.strings().view(path_id)}};
    if (value.is_absolute() || value.has_root_path()) {
        fail(syntax, syntax.child(form, 1), std::string{purpose} + " must be relative");
    }
    project.set_output(
        target, syntax.head(form) == "project-path" ? PathBase::project : PathBase::build, path_id);
}

auto parse_profile(codegen::sexpr::Syntax const& syntax, FormId const form) -> KernelProfile {
    auto const value{text(syntax, form, "kernel profile")};
    if (value == "unreal") {
        return KernelProfile::unreal;
    }
    if (value == "standard") {
        return KernelProfile::standard;
    }
    fail(syntax, form, "unknown kernel profile '" + value + "'");
}

} // namespace

auto load_project(lispb::codegen::sexpr::Syntax& syntax, std::filesystem::path const& path)
    -> Project {
    auto const project_path{std::filesystem::absolute(path).lexically_normal()};
    auto const file{syntax.sources().load(project_path)};
    auto const forms{lispb::codegen::sexpr::read_forms(syntax, file)};
    if (forms.count != 1 || syntax.head(syntax.forms(forms).front()) != "lispb-project") {
        auto const span{(forms.count == 0) ? SourceSpan{.file = file}
                                           : syntax.token_span(syntax.forms(forms).front())};
        throw SourceError{
            syntax.sources(), span, "project must contain exactly one 'lispb-project' form"};
    }

    Fields const project_fields{syntax, syntax.forms(forms).front(), "lispb-project", 0};
    project_fields.validate({"language-version", "project-root"},
                            {"cpp-schema", "slate", "kernel", "material", "group"});
    if (text(syntax, project_fields.required("language-version"), "language version") != "1") {
        fail(syntax,
             project_fields.required("language-version"),
             "unsupported Lispb language version; expected 1");
    }

    Project project{syntax.strings()};
    project.root = (project_path.parent_path() /
                    text(syntax, project_fields.required("project-root"), "project root"))
                       .lexically_normal();
    for (auto const declaration : project_fields.declarations()) {
        auto const head{syntax.head(declaration)};
        Fields const fields{syntax, declaration, head, 1};
        if (head == "cpp-schema") {
            fields.validate({"types", "sources", "output-root"});
            auto const target{project.append(
                TargetKind::cpp_schema, identifier(syntax, fields.positional(0), "target name"))};
            project.set_types(target, text_id(syntax, fields.required("types"), "types path"));
            project.set_sources(target,
                                paths(project, syntax, fields.required("sources"), "sources"));
            output_path(project, target, syntax, fields.required("output-root"), "output root");
        } else if (head == "slate") {
            fields.validate({"sources", "include-directories", "output-root"});
            auto const target{project.append(
                TargetKind::slate, identifier(syntax, fields.positional(0), "target name"))};
            project.set_sources(target,
                                paths(project, syntax, fields.required("sources"), "sources"));
            if (auto const includes{fields.optional("include-directories")}) {
                project.set_includes(target,
                                     paths(project, syntax, *includes, "include directories"));
            }
            output_path(project, target, syntax, fields.required("output-root"), "output root");
        } else if (head == "kernel") {
            fields.validate({"sources"}, {"emission"});
            auto const sources{paths(project, syntax, fields.required("sources"), "sources")};
            for (auto const emission : fields.declarations()) {
                Fields const emission_fields{syntax, emission, "emission", 1};
                emission_fields.validate({"profile", "output-root"});
                auto const target{project.append(
                    TargetKind::kernel,
                    identifier(syntax, emission_fields.positional(0), "target name"))};
                project.set_sources(target, sources);
                project.set_profile(target,
                                    parse_profile(syntax, emission_fields.required("profile")));
                output_path(project,
                            target,
                            syntax,
                            emission_fields.required("output-root"),
                            "output root");
            }
        } else if (head == "material") {
            fields.validate({"source", "artifact"});
            auto const target{project.append(
                TargetKind::material, identifier(syntax, fields.positional(0), "target name"))};
            auto const source{text_id(syntax, fields.required("source"), "source path")};
            project.set_sources(target, project.append_paths(std::span{&source, 1}));
            output_path(project, target, syntax, fields.required("artifact"), "artifact path");
        } else if (head == "group") {
            fields.validate({"targets"});
            auto const name{identifier(syntax, fields.positional(0), "group name")};
            project.add_group(name, names(syntax, fields.required("targets"), "targets"));
        }
    }

    project.resolve_groups();
    return project;
}

auto load_editable_project_document(std::filesystem::path const& path) -> EditableProjectDocument {
    EditableProjectDocument result;
    result.path_ = std::filesystem::absolute(path).lexically_normal();
    auto& syntax{result.storage_->syntax};
    result.project_ = load_project(syntax, result.path_);
    result.source_ = result.storage_->sources.load(result.path_);

    auto const forms{lispb::codegen::sexpr::read_forms(syntax, result.source_)};
    Fields const project_fields{syntax, syntax.forms(forms).front(), "lispb-project", 0};
    for (auto const declaration_id : project_fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax.head(declaration) != "cpp-schema") {
            continue;
        }
        Fields const fields{syntax, declaration, "cpp-schema", 1};
        auto const target_name{text(syntax, fields.positional(0), "target name")};
        auto const& sources_form{fields.required("sources")};
        auto const target{result.project_.find_target(target_name)};
        assert(target.has_value() && target->kind() == TargetKind::cpp_schema);
        auto const sources{result.project_.sources(*target)};
        auto const indentation{
            syntax.children(sources_form).empty()
                ? syntax.sources().location(syntax.token_span(declaration)).column + 3
                : syntax.sources()
                          .location(syntax.token_span(syntax.children(sources_form).front()))
                          .column -
                      1};
        auto const items_begin{static_cast<std::uint32_t>(result.source_item_begins_.size())};
        assert(syntax.children(sources_form).size() == sources.size());
        for (std::size_t index{}; index < syntax.children(sources_form).size(); ++index) {
            auto const& source_form{syntax.children(sources_form)[index]};
            result.source_item_begins_.push_back(syntax.token_span(source_form).offset);
            result.source_item_ends_.push_back(syntax.span(source_form).end_offset);
        }
        result.source_lists_.emplace(
            *target,
            ProjectSourceListRange{
                .begin_offset = syntax.token_span(sources_form).offset,
                .end_offset = syntax.span(sources_form).end_offset,
                .closing_offset = syntax.closing_span(sources_form).offset,
                .item_indentation = indentation,
                .original_sources = result.project_.source_range(*target),
                .original_items = {items_begin, static_cast<std::uint32_t>(sources.size())}});
    }
    return result;
}

} // namespace lispb
