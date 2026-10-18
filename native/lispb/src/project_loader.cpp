#include <lispb/project_loader.h>

#include "project_parser.h"

#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/parser.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/compiler_storage.h>
#include <lispb/editable_project_document.h>
#include <lispb/project.h>
#include <lispb/project_source_list_range.h>
#include <lispb/target_kind.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace lispb {
using codegen::sexpr::Fields;

auto load_project(codegen::sexpr::Syntax& syntax, std::filesystem::path const& path) -> Project {
    auto const project_path{std::filesystem::absolute(path).lexically_normal()};
    auto const file{syntax.sources().load(project_path)};
    auto const forms{codegen::sexpr::read_forms(syntax, file)};
    return ProjectParser{syntax}.parse(file, forms, project_path);
}

auto load_editable_project_document(std::filesystem::path const& path) -> EditableProjectDocument {
    EditableProjectDocument result;
    result.path_ = std::filesystem::absolute(path).lexically_normal();
    auto& syntax{result.storage_->syntax};
    codegen::sexpr::Parser parser{syntax};
    result.project_ = load_project(syntax, result.path_);
    result.source_ = result.storage_->sources.load(result.path_);

    auto const forms{lispb::codegen::sexpr::read_forms(syntax, result.source_)};
    Fields const project_fields{parser, syntax.forms(forms).front(), "lispb-project", 0};
    for (auto const declaration_id : project_fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax.head(declaration) != "cpp-schema") {
            continue;
        }
        Fields const fields{parser, declaration, "cpp-schema", 1};
        auto const target_name{parser.text(fields.positional(0), "target name")};
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
