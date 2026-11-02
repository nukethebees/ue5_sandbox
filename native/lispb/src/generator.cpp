#include <codegen/generator.h>

#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_renderer.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/complete_dependencies.h>
#include <codegen/generated_file.h>
#include <codegen/path_utils.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/source_loader.h>
#include <lispb/compilation.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/output.h>
#include <lispb/publication_options.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/facade_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

template <typename>
inline constexpr bool unlowered_declaration_schema{false};

auto declaration_emission_order(Declarations const& store,
                                ModuleHandle module,
                                std::span<DeclarationHandle const> declarations,
                                schema::TypeGraph const& graph,
                                detail::CompleteDependencies const& complete)
    -> std::vector<std::size_t> {
    std::map<DeclarationHandle, std::size_t> indices;
    auto const count{declarations.size()};
    for (std::size_t index{}; index < count; ++index) {
        indices.emplace(declarations[index], index);
    }
    std::vector<std::size_t> order;
    std::vector<bool> emitted(count);
    order.reserve(count);
    auto visit = [&](auto&& self, std::size_t index) -> void {
        if (emitted[index]) {
            return;
        }
        emitted[index] = true;
        auto const type{graph.find_declared(module, declaration_name(store, declarations[index]))};
        if (type) {
            for (auto const dependency : graph.dependencies_of(*type)) {
                auto const node{dependency};
                auto const found{indices.find(graph.source_declaration(node))};
                if (graph.module(node) != module || found == indices.end()) {
                    continue;
                }
                self(self, found->second);
            }
            for (auto const dependency : complete.of(*type)) {
                auto const node{dependency};
                auto const found{indices.find(graph.source_declaration(node))};
                if (graph.module(node) == module && found != indices.end()) {
                    self(self, found->second);
                }
            }
        }
        order.push_back(index);
    };
    for (std::size_t index{}; index < count; ++index) {
        visit(visit, index);
    }
    return order;
}

class ModuleLowering : public CppIRBuilder {
  public:
    explicit ModuleLowering(CppIR& ir)
        : CppIRBuilder{ir} {}

    /* **************************************** */
    // Module emission
    /* **************************************** */
    auto generated_forward_declarations(ModuleHandle module, schema::TypeGraph const& graph)
        -> detail::DeclarationEmission {
        std::set<schema::TypeId> targets;
        for (auto const use : graph.type_uses()) {
            auto const signature{detail::is_record_signature(use, graph)};
            if (graph.module(use) == module &&
                uses_forward_declaration(graph.target(use),
                                         graph,
                                         signature ? graph.kind(use)
                                                   : TypeReferenceKind::ordinary) &&
                (graph.module(
                     graph.occurrences().type[graph.occurrences().index(graph.target(use))]) ==
                     module ||
                 (signature && uses_cross_module_signature_forward_declaration(
                                   graph.target(use), graph.kind(use), graph)))) {
                targets.insert(
                    graph.occurrences().type[graph.occurrences().index(graph.target(use))]);
            }
        }
        auto decls{node_list()};
        auto prefix{node_list()};
        for (auto const target : targets) {
            auto const node{target};
            auto const namespace_name{std::string{text_view(graph.namespace_name(node))}};
            auto declaration{forward_declaration(
                store_text(std::string{
                    text_view(graph.cpp_spelling(node))
                        .substr(namespace_name.empty() ? 0 : namespace_name.size() + 2)}),
                *forward_declaration_kind(node, graph))};
            if (graph.module(node) == module) {
                decls.add(declaration);
            } else if (namespace_name.empty()) {
                prefix.add(declaration);
            } else {
                auto children{node_list()};
                children.add(declaration);
                prefix.add(namespace_node(store_text(namespace_name), children.build()));
            }
        }
        return {
            .header_prefix = prefix.build(), .header = decls.build(), .source_dependencies = false};
    }
    auto lower_s7_constructors(detail::LoweringContext& ctx) -> detail::DeclarationEmission {
        auto const& records{ctx.declarations.records};
        std::map<std::string, bool> tags;
        auto const count{records.s7_tag.size()};
        for (std::size_t row{}; row < count; ++row) {
            if (!records.s7_tag[row]) {
                continue;
            }
            auto const tag{ctx.text(*records.s7_tag[row])};
            if (tag.empty() ||
                tag.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_?!") !=
                    std::string::npos ||
                (tag.front() >= '0' && tag.front() <= '9')) {
                throw std::invalid_argument{std::format("Invalid S7 constructor tag '{}'", tag)};
            }
            auto const enabled{records.s7_constructor[row] != 0};
            auto const [entry, inserted]{tags.emplace(tag, enabled)};
            if (!inserted && entry->second != enabled) {
                throw std::invalid_argument{
                    std::format("Conflicting S7 constructor policies for '{}'", tag)};
            }
        }
        std::string source;
        for (auto const& [tag, enabled] : tags) {
            if (enabled) {
                source += std::format("(define ({} . arguments) (cons :{} arguments))\n", tag, tag);
            }
        }
        auto const function_id{function("s7_record_constructors", "auto")};
        set_function_return_type(function_id, std_types().string_view, ReturnSyntax::trailing);
        function_flags(function_id).set_define_in_header(true);
        function_flags(function_id).set_is_constexpr(true);
        auto body{node_list()};
        body.add(return_stmt(string_literal(store_text(source))));
        set_function_body(function_id, body.build());
        auto decls{node_list()};
        decls.add(header_function(function_id));
        return {.header = decls.build(), .source_dependencies = false};
    }
    auto lower_umbrella(detail::LoweringContext& ctx, ModuleHandle module) -> ModuleId {
        auto const settings{ctx.declarations.umbrella_modules
                                .settings[ctx.declarations.umbrella_modules.index(module)]};
        auto nodes{node_list()};
        for (auto const header : ctx.declarations.items(
                 ctx.declarations.umbrella_modules
                     .headers[ctx.declarations.umbrella_modules.index(module)])) {
            nodes.add(include(header, IncludeStyle::quoted));
        }
        if (!ctx.declarations.module_settings_store
                 .prelude_lines[ctx.declarations.module_settings_store.index(settings)]
                 .empty()) {
            std::string prelude;
            for (auto const line : ctx.declarations.items(
                     ctx.declarations.module_settings_store
                         .prelude_lines[ctx.declarations.module_settings_store.index(settings)])) {
                if (!prelude.empty()) {
                    prelude += '\n';
                }
                prelude += ctx.declarations.strings().view(line);
            }
            nodes.new_lines(2).add(raw(prelude));
        }
        auto const& settings_store{ctx.declarations.module_settings_store};
        auto const row{settings_store.index(settings)};
        auto const file{append_file(settings_store.header[row], nodes.build())};
        file_flags(file).set_clang_format_off(true);
        set_file_include_order(file, ctx.declarations.items(settings_store.include_order[row]));
        return append_module(settings_store.name[row], file);
    }
};

} // namespace

auto lower_modules(CppIR& ir, Manifest& manifest, schema::TypeGraph const& graph)
    -> std::vector<ModuleId> {
    ModuleLowering emitter{ir};
    auto const complete{detail::complete_dependencies(graph)};
    auto& store{manifest.declarations};
    std::vector<ModuleId> result;
    for (auto const module_id : manifest.modules) {
        detail::LoweringContext ctx{ir, store, manifest.types, graph, module_id};
        [&]() {
            auto const module{module_id};
            switch (module.kind()) {
                case lispb::codegen::ModuleKind::normal: {

                    auto const settings{
                        store.normal_modules.settings[store.normal_modules.index(module)]};
                    auto const decls{store.items(
                        store.normal_modules.declarations[store.normal_modules.index(module)])};
                    std::vector<detail::DeclarationEmission> emissions;
                    emissions.reserve(decls.size() + 1);
                    emissions.push_back(emitter.generated_forward_declarations(module_id, graph));
                    for (auto const index :
                         declaration_emission_order(store, module_id, decls, graph, complete)) {
                        auto const declaration{decls[index]};
                        emissions.push_back([&]() -> detail::DeclarationEmission {
                            auto const value{declaration};
                            switch (value.kind()) {
                                case DeclarationKind::integer_scalar: {
                                    if (!store.integer_scalars.cpp_projection[value]) {
                                        return {};
                                    }
                                    return detail::lower_packed_value(
                                        ctx,
                                        value,
                                        *graph.find_declared(module_id,
                                                             store.integer_scalars.name[value]));
                                }
                                case DeclarationKind::record: {
                                    if (!store.records.cpp_projection[value]) {
                                        return {};
                                    }
                                    if (store.records.layout[value] == LayoutKind::packed) {
                                        return detail::lower_packed_value(
                                            ctx,
                                            value,
                                            *graph.find_declared(module_id,
                                                                 store.records.name[value]));
                                    }
                                    return detail::lower_record(ctx, value);
                                }
                                case DeclarationKind::union_type: {
                                    if (!store.unions.cpp_projection[value]) {
                                        return {};
                                    }
                                    if (store.unions.layout[value] == LayoutKind::packed) {
                                        return detail::lower_packed_value(
                                            ctx,
                                            value,
                                            *graph.find_declared(module_id,
                                                                 store.unions.name[value]));
                                    }
                                    return detail::lower_union(ctx, value);
                                }
                                case lispb::codegen::DeclarationKind::static_table: {

                                    return detail::lower_static_table(ctx, value);
                                }
                                case lispb::codegen::DeclarationKind::soa: {

                                    return detail::lower_soa_declaration(ctx, value);
                                }
                                case lispb::codegen::DeclarationKind::facade: {

                                    return detail::lower_facade(ctx, value);
                                }
                                case lispb::codegen::DeclarationKind::enumeration: {
                                    if (!store.enums.cpp_projection[value] &&
                                        !store.enums.unreal_projection[value]) {
                                        return {};
                                    }
                                    return detail::lower_enum(
                                        ctx,
                                        value,
                                        settings,
                                        store.normal_modules
                                                .enum_helper_namespace[store.normal_modules.index(
                                                    module)]
                                            ? std::optional{*store.normal_modules
                                                                 .enum_helper_namespace
                                                                     [store.normal_modules.index(
                                                                         module)]}
                                            : std::nullopt);
                                }
                                case DeclarationKind::fixed_point:
                                    return store.fixed_points.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   ctx,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id, store.fixed_points.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::linear_quantized:
                                    return store.linear_quantizeds.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   ctx,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id,
                                                       store.linear_quantizeds.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::mini_float:
                                    return store.mini_floats.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   ctx,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id, store.mini_floats.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::optional_presence_bit:
                                    return store.optional_presence_bits.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   ctx,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id,
                                                       store.optional_presence_bits.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::optional_sentinel:
                                    return store.optional_sentinels.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   ctx,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id,
                                                       store.optional_sentinels.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::integer_varint:
                                    return {};
                                default:
                                    std::unreachable();
                            }
                        }());
                    }
                    if (store.normal_modules.s7_constructors[store.normal_modules.index(module)]) {
                        emissions.push_back(emitter.lower_s7_constructors(ctx));
                    }
                    auto lowered{detail::assemble_module(ctx, settings, emissions)};
                    result.insert(result.end(),
                                  std::make_move_iterator(lowered.begin()),
                                  std::make_move_iterator(lowered.end()));
                    break;
                }
                case lispb::codegen::ModuleKind::settings: {

                    auto lowered{detail::lower_settings_module(ctx, module)};
                    result.insert(result.end(),
                                  std::make_move_iterator(lowered.begin()),
                                  std::make_move_iterator(lowered.end()));
                    break;
                }
                case lispb::codegen::ModuleKind::umbrella: {

                    result.push_back(emitter.lower_umbrella(ctx, module));
                    break;
                }
                default:
                    std::unreachable();
            }
        }();
    }
    return result;
}

auto render_modules(CppIR& ir, std::span<ModuleId const> modules) -> std::vector<GeneratedFile> {
    std::vector<GeneratedFile> result;
    std::set<std::string> paths;
    for (auto const module : modules) {
        auto const row{ir.modules.index(module)};
        for (auto const file : {ir.modules.header[row], ir.modules.source[row]}) {
            if (file.value == 0) {
                continue;
            }
            auto const file_row{ir.files.index(file)};
            auto const normalized{
                std::filesystem::path{ir.strings.view(ir.files.path[file_row])}.lexically_normal()};
            if (!paths.insert(lispb::codegen::output_path_key(normalized)).second) {
                throw std::invalid_argument{"Duplicate generated output path: " +
                                            normalized.string()};
            }
            ir.files.flags[file_row].set_clang_format_off(false);
            ir.files.flags[file_row].set_format_generated(true);
            ir.merge_adjacent_namespaces(file);
            result.push_back(GeneratedFile{normalized, CppRenderer{ir}.render(file), true});
        }
    }
    return result;
}

void compile_schema(CompilerStorage& storage,
                    CppIR& ir,
                    Manifest& manifest,
                    schema::TypeGraph& graph,
                    lispb::Compilation& output,
                    std::filesystem::path const& types,
                    std::span<std::filesystem::path const> const modules,
                    bool const emit) {
    auto const registry{load_type_registry(storage, types)};
    manifest = load_sources(storage, registry, modules);
    graph = schema::resolve_type_graph(manifest);
    for (auto const& source : registry.sources) {
        output.dependencies.emplace_back(storage.sources.path(source));
    }
    output.dependencies.insert(output.dependencies.end(), modules.begin(), modules.end());
    if (!emit) {
        return;
    }
    auto files{render_modules(ir, lower_modules(ir, manifest, graph))};
    for (auto& file : files) {
        output.add_text(std::move(file.path), std::move(file.content), file.format_generated);
    }
}

auto generate_files(std::vector<GeneratedFile> const& files,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& output_root,
                    bool check_only) -> int {
    lispb::Compilation compilation;
    for (auto const& file : files) {
        compilation.add_text(file.path, file.content, file.format_generated);
    }
    return lispb::publish(
        compilation,
        {.path_base = project_root, .output_root = output_root, .check_only = check_only});
}

} // namespace lispb::codegen
