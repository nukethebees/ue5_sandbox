#include <lispb/compiler.h>

#include "lispb/compile_mode.h"
#include "lispb/rooted_path.h"
#include <codegen/generator.h>
#include <codegen/schema/type_registry.h>
#include <codegen/sexpr/indices.h>
#include <kernel_codegen/compiler.h>
#include <lispb/material_compiler.h>
#include <lispb/project.h>
#include <lispb/project_loader.h>
#include <lispb/schema/type_graph.h>
#include <lispb/target_id.h>
#include <lispb/target_kind.h>
#include <lispb/temporary_string_scope.h>
#include <material_gen/MaterialIR.h>
#include <slate_codegen/compiler.h>
#include <slate_codegen/source_options.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace lispb {

static auto scheme_reference(schema::TypeGraph const& graph, std::span<StringId const> roots)
    -> std::string {
    auto& store{graph.declarations()};
    auto& strings{graph.strings()};
    auto const& fields{store.child_occurrences};
    std::string result{"# Scheme settings reference\n"};
    std::vector<schema::TypeId> records;
    for (auto const root : roots) {
        auto const before{records.size()};
        for (auto const type : graph.types()) {
            auto const record{graph.source_declaration(type)};
            if (record.value && record.kind() == codegen::DeclarationKind::record &&
                store.records.s7_tag[record] == root) {
                records.push_back(type);
            }
        }
        if (before == records.size()) {
            throw std::invalid_argument{"Unknown documentation root: " +
                                        std::string{strings.view(root)}};
        }
    }
    for (std::size_t index{}; index < records.size(); ++index) {
        auto const type{records[index]};
        auto const record{graph.source_declaration(type)};

        result += "\n## `";
        result += strings.view(*store.records.s7_tag[record]);
        result += "`\n\n| Member | Type | Enum values |\n| --- | --- | --- |\n";
        for (auto const member : store.items(store.records.members[record])) {
            auto const& options{fields.s7[member]};
            if (options.exclude) {
                continue;
            }
            std::string key{strings.view(options.name.value_or(fields.name[member]))};
            if (!options.name) {
                std::replace(key.begin(), key.end(), '_', '-');
            }
            auto const occurrence{graph.resolved(graph.module(type), fields.type[member])};
            auto const member_type{graph.occurrences().type[occurrence]};
            result += store.records.s7_positional[record] ? "| `" : "| `:";
            result += key + "` | `";
            result += strings.view(graph.name(member_type));
            result += "` |";
            auto const enumeration{graph.source_declaration(member_type)};
            if (enumeration.value && enumeration.kind() == codegen::DeclarationKind::record &&
                store.records.s7_tag[enumeration] &&
                std::find(records.begin(), records.end(), member_type) == records.end()) {
                records.push_back(member_type);
            }
            if (enumeration.value && enumeration.kind() == codegen::DeclarationKind::enumeration) {
                for (auto const value : store.items(store.enums.values[enumeration])) {
                    result += " `'";
                    result += strings.view(store.enumerators.serialized_name[value].value_or(
                        store.enumerators.name[value]));
                    result += '`';
                }
            }
            result += " |\n";
        }
    }
    return result;
}

Compiler::Compiler()
    : manifest_{
          1, lispb::codegen::TypeRegistry{storage_.registered_types}, storage_.declarations, {}}
    , types_{storage_.declarations, storage_.cpp_types}
    , cpp_{storage_.strings, storage_.cpp_types}
    , kernel_{storage_.strings}
    , material_{storage_.strings} {}

void Compiler::reset_compilation() {
    storage_.diagnostics.clear();
    output_ = {};
    material_ = material_synth::MaterialIR{storage_.strings};
    slate_ = {};
    kernel_.clear();
    cpp_.clear();
    types_ = schema::TypeGraph{storage_.declarations, storage_.cpp_types};
    manifest_.modules.clear();
    manifest_.types.clear();
    storage_.registered_types.clear();
    storage_.declarations.clear();
    storage_.syntax.clear();
    storage_.sources.clear_expansions();
    storage_.strings.reset_temporary();
    storage_.cpp_types.clear();
    storage_.sources.refresh();
}

auto Compiler::compile(Project const& project,
                       TargetId const target,
                       std::filesystem::path const& build_root,
                       CompileMode const mode) -> CompiledTarget& {
    reset_compilation();
    TemporaryStringScope temporary_strings{storage_.strings};
    auto const emit{mode == CompileMode::generate};
    auto& compilation{output_.compilation};
    auto const output{resolve(project.output_path(target), project.root, build_root)};
    switch (target.kind()) {
        case TargetKind::cpp_schema: {
            auto sources{project.sources(target)};
            for (auto& source : sources) {
                source = project.root / source;
            }
            lispb::codegen::compile_schema(storage_,
                                           cpp_,
                                           manifest_,
                                           types_,
                                           compilation,
                                           project.root / project.types(target),
                                           sources,
                                           emit);
            auto const documentation{project.documentation.find(target)};
            if (emit && documentation != project.documentation.end()) {
                compilation.add_text(documentation->second.path,
                                     scheme_reference(types_, documentation->second.roots),
                                     false);
            }
            output_.publication = {.path_base = project.root, .output_root = output};
            break;
        }
        case TargetKind::slate: {
            slate_codegen::compile_sources(
                storage_.syntax,
                slate_,
                cpp_,
                compilation,
                {.source_root = project.root,
                 .inputs = project.sources(target),
                 .include_directories = project.include_directories(target)},
                emit);
            output_.publication = {.path_base = output, .output_root = output};
            break;
        }
        case TargetKind::kernel: {
            auto const paths{project.sources(target)};
            std::vector<lispb::codegen::sexpr::SourceFileId> sources;
            sources.reserve(paths.size());
            for (auto const& source : paths) {
                auto const path{project.root / source};
                sources.push_back(storage_.sources.load(path));
                compilation.dependencies.push_back(path);
            }
            kernel_codegen::compile(storage_.syntax, kernel_, cpp_, compilation, sources, emit);
            output_.publication = {.path_base = output, .output_root = output};
            break;
        }
        case TargetKind::material: {
            auto const paths{project.sources(target)};
            assert(paths.size() == 1);
            auto const source{project.root / paths.front()};
            auto const file{storage_.sources.load(source, paths.front().generic_string())};
            compile_material(storage_.syntax,
                             file,
                             material_,
                             compilation,
                             source,
                             project.root,
                             output.filename(),
                             emit);
            output_.publication = {.path_base = output.parent_path(),
                                   .output_root = output.parent_path(),
                                   .track_outputs = false};
            break;
        }
        case TargetKind::group:
        case TargetKind::none:
            assert(false && "Compile an expanded concrete target");
            break;
    }
    return output_;
}

auto Compiler::expand(Project const& project, TargetId const target) -> std::string {
    reset_compilation();
    TemporaryStringScope temporary_strings{storage_.strings};
    if (target.kind() != TargetKind::slate) {
        throw std::invalid_argument{"expand is only supported for Slate targets"};
    }
    return slate_codegen::expand_sources(
        storage_.syntax,
        {.source_root = project.root,
         .inputs = project.sources(target),
         .include_directories = project.include_directories(target)});
}

} // namespace lispb
