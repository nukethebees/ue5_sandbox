#include <codegen/generator.h>

#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
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

auto generated_forward_declarations(CppIR& ir, ModuleHandle module, schema::TypeGraph const& graph)
    -> detail::DeclarationEmission {
    std::set<schema::TypeId> targets;
    for (auto const use : graph.type_uses()) {
        auto const signature{detail::is_record_signature(use, graph)};
        if (graph.module(use) == module &&
            uses_forward_declaration(graph.target(use),
                                     graph,
                                     signature ? graph.kind(use) : TypeReferenceKind::ordinary) &&
            (graph.module(graph.occurrences().type[graph.occurrences().index(graph.target(use))]) ==
                 module ||
             (signature && uses_cross_module_signature_forward_declaration(
                               graph.target(use), graph.kind(use), graph)))) {
            targets.insert(graph.occurrences().type[graph.occurrences().index(graph.target(use))]);
        }
    }
    NodeListBuilder declarations{ir};
    NodeListBuilder prefix{ir};
    for (auto const target : targets) {
        auto const node{target};
        auto const namespace_name{std::string{ir.strings.view(graph.namespace_name(node))}};
        auto declaration{ir.forward_declaration(
            ir.strings.intern(
                std::string{ir.strings.view(graph.cpp_spelling(node))
                                .substr(namespace_name.empty() ? 0 : namespace_name.size() + 2)}),
            *forward_declaration_kind(node, graph))};
        if (graph.module(node) == module) {
            declarations.add(declaration);
        } else if (namespace_name.empty()) {
            prefix.add(declaration);
        } else {
            NodeListBuilder children{ir};
            children.add(declaration);
            prefix.add(ir.namespace_node(ir.strings.intern(namespace_name), children.build()));
        }
    }
    return {.header_prefix = prefix.build(),
            .header = declarations.build(),
            .source_dependencies = false};
}

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

auto lower_s7_constructors(detail::LoweringContext& context) -> detail::DeclarationEmission {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    std::map<std::string, bool> tags;
    auto const count{records.s7_tag.size()};
    for (std::size_t row{}; row < count; ++row) {
        if (!records.s7_tag[row]) {
            continue;
        }
        auto const tag{context.text(*records.s7_tag[row])};
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
    auto const function{ir.function("s7_record_constructors", "auto")};
    ir.set_function_return_type(
        function,
        ir.types.intern("std::string_view",
                        {ir.types.dependency("std::string_view", "string_view")}),
        ReturnSyntax::trailing);
    ir.function_flags(function).set_define_in_header(true);
    ir.function_flags(function).set_is_constexpr(true);
    NodeListBuilder body{ir};
    body.add(ir.return_stmt(ir.string_literal(ir.strings.intern(source))));
    ir.set_function_body(function, body.build());
    NodeListBuilder declarations{ir};
    declarations.add(ir.header_function(function));
    return {.header = declarations.build(), .source_dependencies = false};
}

auto lower_umbrella(detail::LoweringContext& context, ModuleHandle module) -> ModuleId {
    auto const settings{context.declarations.umbrella_modules
                            .settings[context.declarations.umbrella_modules.index(module)]};
    NodeListBuilder nodes{context.ir};
    for (auto const header : context.declarations.items(
             context.declarations.umbrella_modules
                 .headers[context.declarations.umbrella_modules.index(module)])) {
        nodes.add(context.ir.include(header, IncludeStyle::quoted));
    }
    if (!context.declarations.module_settings_store
             .prelude_lines[context.declarations.module_settings_store.index(settings)]
             .empty()) {
        std::string prelude;
        for (auto const line : context.declarations.items(
                 context.declarations.module_settings_store
                     .prelude_lines[context.declarations.module_settings_store.index(settings)])) {
            if (!prelude.empty()) {
                prelude += '\n';
            }
            prelude += context.declarations.strings().view(line);
        }
        nodes.new_lines(2).add(context.ir.raw(prelude));
    }
    auto& ir{context.ir};
    auto const& settings_store{context.declarations.module_settings_store};
    auto const row{settings_store.index(settings)};
    auto const file{ir.files.append(settings_store.header[row], nodes.build())};
    ir.files.flags[ir.files.index(file)].set_clang_format_off(true);
    ir.files.set_include_order(file, context.declarations.items(settings_store.include_order[row]));
    return ir.modules.append(settings_store.name[row], file);
}

} // namespace

auto lower_modules(CppIR& ir, Manifest& manifest, schema::TypeGraph const& graph)
    -> std::vector<ModuleId> {
    auto const complete{detail::complete_dependencies(graph)};
    auto& store{manifest.declarations};
    std::vector<ModuleId> result;
    for (auto const module_id : manifest.modules) {
        detail::LoweringContext context{ir, store, manifest.types, graph, module_id};
        [&]() {
            auto const module{module_id};
            switch (module.kind()) {
                case lispb::codegen::ModuleKind::normal: {

                    auto const settings{
                        store.normal_modules.settings[store.normal_modules.index(module)]};
                    auto const declarations{store.items(
                        store.normal_modules.declarations[store.normal_modules.index(module)])};
                    std::vector<detail::DeclarationEmission> emissions;
                    emissions.reserve(declarations.size() + 1);
                    emissions.push_back(generated_forward_declarations(ir, module_id, graph));
                    for (auto const index : declaration_emission_order(
                             store, module_id, declarations, graph, complete)) {
                        auto const declaration{declarations[index]};
                        emissions.push_back([&]() -> detail::DeclarationEmission {
                            auto const value{declaration};
                            switch (value.kind()) {
                                case DeclarationKind::integer_scalar: {
                                    if (!store.integer_scalars.cpp_projection[value]) {
                                        return {};
                                    }
                                    return detail::lower_packed_value(
                                        context,
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
                                            context,
                                            value,
                                            *graph.find_declared(module_id,
                                                                 store.records.name[value]));
                                    }
                                    return detail::lower_record(context, value);
                                }
                                case DeclarationKind::union_type: {
                                    if (!store.unions.cpp_projection[value]) {
                                        return {};
                                    }
                                    if (store.unions.layout[value] == LayoutKind::packed) {
                                        return detail::lower_packed_value(
                                            context,
                                            value,
                                            *graph.find_declared(module_id,
                                                                 store.unions.name[value]));
                                    }
                                    return detail::lower_union(context, value);
                                }
                                case lispb::codegen::DeclarationKind::static_table: {

                                    return detail::lower_static_table(context, value);
                                }
                                case lispb::codegen::DeclarationKind::soa: {

                                    return detail::lower_soa_declaration(context, value);
                                }
                                case lispb::codegen::DeclarationKind::facade: {

                                    return detail::lower_facade(context, value);
                                }
                                case lispb::codegen::DeclarationKind::enumeration: {
                                    if (!store.enums.cpp_projection[value] &&
                                        !store.enums.unreal_projection[value]) {
                                        return {};
                                    }
                                    return detail::lower_enum(
                                        context,
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
                                                   context,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id, store.fixed_points.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::linear_quantized:
                                    return store.linear_quantizeds.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   context,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id,
                                                       store.linear_quantizeds.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::mini_float:
                                    return store.mini_floats.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   context,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id, store.mini_floats.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::optional_presence_bit:
                                    return store.optional_presence_bits.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   context,
                                                   value,
                                                   *graph.find_declared(
                                                       module_id,
                                                       store.optional_presence_bits.name[value]))
                                             : detail::DeclarationEmission{};
                                case DeclarationKind::optional_sentinel:
                                    return store.optional_sentinels.cpp_projection[value]
                                             ? detail::lower_packed_value(
                                                   context,
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
                        emissions.push_back(lower_s7_constructors(context));
                    }
                    auto lowered{detail::assemble_module(context, settings, emissions)};
                    result.insert(result.end(),
                                  std::make_move_iterator(lowered.begin()),
                                  std::make_move_iterator(lowered.end()));
                    break;
                }
                case lispb::codegen::ModuleKind::settings: {

                    result.push_back(detail::lower_settings_module(context, module));
                    break;
                }
                case lispb::codegen::ModuleKind::umbrella: {

                    result.push_back(lower_umbrella(context, module));
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
