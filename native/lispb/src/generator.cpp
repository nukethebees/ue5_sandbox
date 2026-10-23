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
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
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
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_allocator_variants.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
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
#include <lispb/schema/homogeneous_storage_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/union_alternatives.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
                auto const scalar{((graph.source_declaration(node)).kind() ==
                                           lispb::codegen::DeclarationKind::integer_scalar
                                       ? std::optional{graph.source_declaration(node)}
                                       : std::nullopt)};
                if (scalar &&
                    store.integer_scalars.cpp_emission[store.integer_scalars.index(*scalar)] ==
                        IntegerScalarCppEmission::alias) {
                    self(self, found->second);
                }
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

auto scalar_constant_expression(CppIR& ir, CppTypeId const type, PackedIntegerValue const value)
    -> Expr {
    if (value.negative && value.magnitude == (std::uint64_t{1} << 63)) {
        return ir.static_cast_expr(type,
                                   ir.parenthesized(ir.binary(BinaryOperator::subtract,
                                                              ir.literal("-9223372036854775807LL"),
                                                              ir.literal("1"))));
    }

    std::string literal{value.negative ? "-" : ""};
    literal += std::to_string(value.magnitude);
    if (!value.negative &&
        value.magnitude > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
        literal += "ULL";
    }
    return ir.static_cast_expr(type, ir.literal(literal));
}

auto scalar_cpp_type(detail::LoweringContext& context, TypeRefId reference) -> CppTypeId {
    auto type{context.resolve(reference)};
    auto& cpp{context.ir.types};
    auto const spelling{std::string{cpp.spelling(type)}};
    if (cpp.dependencies(type).empty()) {
        if (spelling.starts_with("std::uint") || spelling.starts_with("std::int")) {
            type = cpp.derive(type, spelling, std::array{cpp.dependency(spelling, "cstdint")});
        } else if ((spelling.starts_with("uint") || spelling.starts_with("int")) &&
                   spelling != "int") {
            type = cpp.derive(type, spelling, std::array{cpp.dependency(spelling, "CoreTypes.h")});
        }
    }
    return type;
}

auto lower_scalar(detail::LoweringContext& context, DeclarationHandle scalar)
    -> detail::DeclarationEmission {
    auto& ir{context.ir};
    auto const& scalars{context.declarations.integer_scalars};
    auto const row{scalars.index(scalar)};
    auto const& codes{context.declarations.packed_named_codes};
    auto const scalar_name{context.text(scalars.name[row])};
    NodeListBuilder declarations{ir};
    if (scalars.cpp_emission[row] == IntegerScalarCppEmission::alias) {
        auto const cpp_type{scalar_cpp_type(context, *scalars.cpp_type[row])};
        declarations.add(ir.using_declaration(ir.strings.intern(scalar_name), cpp_type));
        return {.header = declarations.build()};
    }
    if (scalars.cpp_emission[row] != IntegerScalarCppEmission::none) {
        assert(scalars.cpp_type[row].has_value());
        auto const cpp_type{scalar_cpp_type(context, *scalars.cpp_type[row])};
        auto const constant_type{ir.types.derive(
            cpp_type, "inline constexpr " + std::string{ir.types.spelling(cpp_type)})};
        for (auto const code_id : context.declarations.items(scalars.named_codes[row])) {
            auto const code{code_id};
            auto const code_row{codes.index(code)};
            auto const code_name{context.text(codes.name[code_row])};
            declarations.add(
                ir.variable(constant_type,
                            scalar_name + "_" + code_name,
                            scalar_constant_expression(ir, cpp_type, codes.value[code_row])),
                1);
        }
        if (scalars.cpp_emission[row] == IntegerScalarCppEmission::constants_with_names) {
            auto const lookup{ir.function(scalar_name + "_name", "auto")};
            ir.set_function_return_type(
                lookup,
                ir.types.intern("std::string_view",
                                {ir.types.dependency("std::string_view", "string_view")}),
                ReturnSyntax::trailing);
            ir.set_function_parameters(
                lookup,
                {ir.parameter(
                    ir.types.derive(cpp_type, std::string{ir.types.spelling(cpp_type)} + " const"),
                    "value")});
            auto& flags{ir.function_flags(lookup)};
            flags.set_define_in_header(true);
            flags.set_is_constexpr(true);
            flags.set_is_nodiscard(true);
            ir.set_function_noexcept(lookup);

            std::vector<SwitchCaseId> cases;
            for (auto const code_id : context.declarations.items(scalars.named_codes[row])) {
                auto const code{code_id};
                auto const code_row{codes.index(code)};
                auto const code_name{context.text(codes.name[code_row])};
                cases.push_back(ir.switch_case(ir.named(scalar_name + '_' + code_name),
                                               ir.block({ir.return_stmt(ir.string_literal(
                                                   string_id(codes.name[code_row])))})));
            }
            NodeListBuilder body{ir};
            body.add(ir.switch_stmt(ir.named("value"), cases), 2)
                .add(ir.return_stmt(ir.init_list({})));
            ir.set_function_body(lookup, body.build());
            declarations.add(ir.header_function(lookup), 2);
        }
    }

    return {.header = declarations.build()};
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
                    std::vector<DeclarationHandle> schemas;
                    for (auto const declaration : declarations) {
                        if (auto const soa{
                                ((declaration).kind() == lispb::codegen::DeclarationKind::soa
                                     ? std::optional{declaration}
                                     : std::nullopt)}) {
                            schemas.push_back(*soa);
                        }
                    }
                    std::map<IdentifierId, DeclarationHandle> expanded;
                    for (auto const soa : expand_soa_allocator_variants(
                             store,
                             schemas,
                             store.normal_modules
                                 .soa_array_allocators[store.normal_modules.index(module)])) {
                        expanded.emplace(store.soas.name[store.soas.index(soa)], soa);
                    }
                    std::vector<detail::DeclarationEmission> emissions;
                    emissions.reserve(declarations.size() + 1);
                    emissions.push_back(generated_forward_declarations(ir, module_id, graph));
                    for (auto const index : declaration_emission_order(
                             store, module_id, declarations, graph, complete)) {
                        auto const declaration{declarations[index]};
                        emissions.push_back([&]() -> detail::DeclarationEmission {
                            auto const value{declaration};
                            switch (value.kind()) {
                                case lispb::codegen::DeclarationKind::integer_scalar: {

                                    return lower_scalar(context, value);
                                }
                                case lispb::codegen::DeclarationKind::packed_value: {

                                    return detail::lower_packed_value(
                                        context,
                                        value,
                                        *graph.find_declared(
                                            module_id,
                                            store.packed_values
                                                .name[store.packed_values.index(value)]));
                                }
                                case lispb::codegen::DeclarationKind::record: {

                                    return detail::lower_record(context, value);
                                }
                                case lispb::codegen::DeclarationKind::union_type: {

                                    return detail::lower_union(context, value);
                                }
                                case lispb::codegen::DeclarationKind::tagged_union: {

                                    return detail::lower_tagged_union(context, value);
                                }
                                case lispb::codegen::DeclarationKind::static_table: {

                                    return detail::lower_static_table(context, value);
                                }
                                case lispb::codegen::DeclarationKind::soa: {

                                    return detail::lower_soa_declaration(
                                        context, value, module, expanded);
                                }
                                case lispb::codegen::DeclarationKind::homogeneous_layout: {

                                    return detail::lower_homogeneous(context, value);
                                }
                                case lispb::codegen::DeclarationKind::facade: {

                                    return detail::lower_facade(context, value);
                                }
                                case lispb::codegen::DeclarationKind::settings_schema:
                                    return detail::lower_settings_schema(context, value);
                                case lispb::codegen::DeclarationKind::enumeration: {

                                    return detail::lower_enum(
                                        context,
                                        value,
                                        settings,
                                        store.normal_modules
                                                .enum_helper_namespace[store.normal_modules.index(
                                                    module)]
                                            ? std::optional{string_id(
                                                  *store.normal_modules.enum_helper_namespace
                                                       [store.normal_modules.index(module)])}
                                            : std::nullopt);
                                }
                                case lispb::codegen::DeclarationKind::vector_soa: {

                                    return detail::lower_vector(
                                        context,
                                        value,
                                        store.normal_modules
                                            .soa_backend[store.normal_modules.index(module)]);
                                }
                                case lispb::codegen::DeclarationKind::fixed_point:
                                case lispb::codegen::DeclarationKind::integer_varint:
                                case lispb::codegen::DeclarationKind::linear_quantized:
                                case lispb::codegen::DeclarationKind::mini_float:
                                case lispb::codegen::DeclarationKind::optional_presence_bit:
                                case lispb::codegen::DeclarationKind::optional_sentinel:
                                    return {};
                                default:
                                    std::unreachable();
                            }
                        }());
                    }
                    for (auto const allocator : store.items(
                             store.normal_modules
                                 .soa_array_allocators[store.normal_modules.index(module)])) {
                        for (auto const& soa : schemas) {
                            emissions.push_back(detail::lower_soa_declaration(
                                context, soa, module, expanded, allocator));
                        }
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
