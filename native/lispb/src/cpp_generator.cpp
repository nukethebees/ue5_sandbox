#include <codegen/generator.h>
#include <codegen/path_utils.h>
#include <codegen/source_loader.h>

#include "lowering.h"
#include "lowering_utils.h"

#include <codegen/validation.h>
#include <lispb/schema/type_graph.h>

#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace codegen {
namespace {

template <typename>
inline constexpr bool unlowered_declaration_schema{false};

auto lower_umbrella(UmbrellaModuleSchema const& module) -> Module {
    NodeListBuilder nodes;
    for (auto const& header : module.headers) {
        nodes.add(Include{header, false});
    }
    if (!module.settings.prelude_lines.empty()) {
        nodes.new_lines(2).add(raw(detail::join_lines(module.settings.prelude_lines)));
    }
    return Module{
        .name = module.settings.name,
        .header =
            CppFile{
                .path = module.settings.header,
                .nodes = nodes.build(),
                .clang_format_off = true,
                .include_order = module.settings.include_order,
            },
    };
}

auto lower_semantic_module(ModuleSettings const& settings) -> Module {
    NodeListBuilder nodes;
    if (!settings.prelude_lines.empty()) {
        nodes.add(raw(detail::join_lines(settings.prelude_lines)));
    }
    return Module{
        .name = settings.name,
        .header = CppFile{.path = settings.header,
                          .nodes = nodes.build(),
                          .clang_format_off = true,
                          .include_order = settings.include_order},
    };
}

auto scalar_constant_literal(CppType const& type, PackedIntegerValue const value) -> std::string {
    std::string literal;
    if (value.negative && value.magnitude == (std::uint64_t{1} << 63)) {
        literal = "(-9223372036854775807LL - 1)";
    } else {
        literal = value.negative ? "-" : "";
        literal += std::to_string(value.magnitude);
        if (!value.negative && value.magnitude > static_cast<std::uint64_t>(
                                                     (std::numeric_limits<std::int64_t>::max)())) {
            literal += "ULL";
        }
    }
    return "static_cast<" + type.spelling + ">(" + literal + ")";
}

auto scalar_cpp_type(TypeRef const& reference, std::map<std::string, CppType> const& types)
    -> CppType {
    auto type{resolve_type(reference, types)};
    if (type.dependencies.empty() &&
        (type.spelling.starts_with("std::uint") || type.spelling.starts_with("std::int"))) {
        type.dependencies.push_back({type.spelling, "cstdint", {}});
    } else if (type.dependencies.empty() &&
               (type.spelling.starts_with("uint") || type.spelling.starts_with("int")) &&
               type.spelling != "int") {
        type.dependencies.push_back({type.spelling, "CoreTypes.h", {}});
    }
    return type;
}

auto lower_scalar(IntegerScalarSchema const& scalar, std::map<std::string, CppType> const& types)
    -> detail::DeclarationEmission {
    NodeListBuilder declarations;
    if (scalar.cpp_emission != IntegerScalarCppEmission::none) {
        if (!scalar.cpp_type.has_value()) {
            throw std::invalid_argument{"Integer scalar '" + scalar.name +
                                        "' constants emission requires a C++ type"};
        }
        auto const cpp_type{scalar_cpp_type(*scalar.cpp_type, types)};
        for (auto const& code : scalar.named_codes) {
            auto const declaration{"inline constexpr " + cpp_type.spelling + " " + scalar.name +
                                   "_" + code.name + "{" +
                                   scalar_constant_literal(cpp_type, code.value) + "};"};
            declarations.add(raw(declaration, cpp_type.dependencies), 1);
        }
        if (scalar.cpp_emission == IntegerScalarCppEmission::constants_with_names) {
            auto dependencies{cpp_type.dependencies};
            dependencies.push_back(TypeDependency{"std::string_view", "string_view", {}});

            std::ostringstream lookup;
            lookup << "[[nodiscard]] constexpr auto " << scalar.name << "_name("
                   << cpp_type.spelling
                   << " const value) noexcept -> std::string_view {\n"
                      "    switch (value) {\n";
            for (auto const& code : scalar.named_codes) {
                lookup << "        case " << scalar.name << '_' << code.name << ": {\n"
                       << "            return " << render(string_literal(code.name)) << ";\n"
                       << "        }\n";
            }
            lookup << "    }\n\n"
                      "    return {};\n"
                      "}";
            declarations.add(raw(lookup.str(), std::move(dependencies)), 2);
        }
    }

    return {.header = declarations.build()};
}

auto lower_scalar_module(ScalarModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    NodeListBuilder declarations;
    for (auto const& scalar : module.scalars) {
        declarations.append(lower_scalar(scalar, types).header);
    }
    auto declaration_nodes{declarations.build()};
    NodeListBuilder header_nodes;
    if (!declaration_nodes.empty()) {
        header_nodes.add(IncludeDependencies{}, 2);
    }
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(detail::join_lines(module.settings.prelude_lines)), 2);
    }
    if (module.settings.namespace_name.has_value() && !declaration_nodes.empty()) {
        header_nodes.add(Namespace{*module.settings.namespace_name, std::move(declaration_nodes)});
    } else {
        header_nodes.append(std::move(declaration_nodes));
    }
    return Module{
        .name = module.settings.name,
        .header = CppFile{.path = module.settings.header,
                          .nodes = header_nodes.build(),
                          .clang_format_off = true,
                          .include_order = module.settings.include_order},
    };
}

} // namespace

auto lower_modules(Manifest const& manifest) -> std::vector<Module> {
    auto const type_graph{lispb::schema::resolve_type_graph(manifest)};
    std::vector<Module> result;
    for (auto const& schema : manifest.modules) {
        std::visit(
            [&](auto const& module) {
                using T = std::decay_t<decltype(module)>;
                if constexpr (std::is_same_v<T, NormalModuleSchema>) {
                    std::vector<detail::DeclarationEmission> emissions;
                    emissions.reserve(module.declarations.size());
                    for (auto const& declaration : module.declarations) {
                        emissions.push_back(std::visit(
                            [&](auto const& value) -> detail::DeclarationEmission {
                                using D = std::decay_t<decltype(value)>;
                                if constexpr (std::is_same_v<D, IntegerScalarSchema>) {
                                    return lower_scalar(value, manifest.types);
                                } else if constexpr (std::is_same_v<D, PackedValueSchema>) {
                                    return detail::lower_packed_value(
                                        value, manifest.types, type_graph, module.settings.name);
                                } else if constexpr (std::is_same_v<D, RecordSchema>) {
                                    return detail::lower_record(value, manifest.types);
                                } else if constexpr (std::is_same_v<D, UnionSchema>) {
                                    return detail::lower_union(value, manifest.types);
                                } else if constexpr (std::is_same_v<D, TaggedUnionSchema>) {
                                    return detail::lower_tagged_union(value, manifest.types);
                                } else if constexpr (std::is_same_v<D, StaticTableSchema>) {
                                    return detail::lower_static_table(value, manifest.types);
                                } else if constexpr (std::is_same_v<D, SoaSchema>) {
                                    return detail::lower_soa_declaration(
                                        value, module, manifest.types, type_graph);
                                } else if constexpr (std::is_same_v<D, HomogeneousLayoutSchema>) {
                                    return detail::lower_homogeneous(value, manifest.types);
                                } else if constexpr (std::is_same_v<D, FacadeSchema>) {
                                    return detail::lower_facade(value, manifest.types);
                                } else if constexpr (std::is_same_v<D, EnumSchema>) {
                                    return detail::lower_enum(value,
                                                              module.settings,
                                                              module.enum_helper_namespace,
                                                              manifest.types);
                                } else if constexpr (std::is_same_v<D, VectorSoaSchema>) {
                                    return detail::lower_vector(
                                        value, module.soa_backend, manifest.types);
                                } else if constexpr (std::is_same_v<D, LinearQuantizedSchema> ||
                                                     std::is_same_v<D, IntegerVarintSchema> ||
                                                     std::is_same_v<D, FixedPointSchema> ||
                                                     std::is_same_v<D, MiniFloatSchema> ||
                                                     std::is_same_v<D, OptionalSentinelSchema> ||
                                                     std::is_same_v<D, OptionalPresenceBitSchema>) {
                                    return {};
                                } else {
                                    static_assert(unlowered_declaration_schema<D>);
                                }
                            },
                            declaration));
                    }
                    for (auto const& allocator : module.soa_array_allocators) {
                        for (auto const& declaration : module.declarations) {
                            if (auto const* soa{std::get_if<SoaSchema>(&declaration)}) {
                                emissions.push_back(detail::lower_soa_declaration(
                                    *soa, module, manifest.types, type_graph, allocator.prefix));
                            }
                        }
                    }
                    auto lowered{detail::assemble_module(module.settings, emissions)};
                    result.insert(result.end(),
                                  std::make_move_iterator(lowered.begin()),
                                  std::make_move_iterator(lowered.end()));
                } else if constexpr (std::is_same_v<T, EnumModuleSchema>) {
                    auto lowered{detail::lower_enum_module(module, manifest.types)};
                    result.insert(result.end(),
                                  std::make_move_iterator(lowered.begin()),
                                  std::make_move_iterator(lowered.end()));
                } else if constexpr (std::is_same_v<T, PackedValueModuleSchema>) {
                    result.push_back(
                        detail::lower_packed_value_module(module, manifest.types, type_graph));
                } else if constexpr (std::is_same_v<T, ScalarModuleSchema>) {
                    result.push_back(lower_scalar_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, RepresentationModuleSchema>) {
                    result.push_back(lower_semantic_module(module.settings));
                } else if constexpr (std::is_same_v<T, RecordModuleSchema>) {
                    result.push_back(detail::lower_record_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, UnionModuleSchema>) {
                    result.push_back(detail::lower_union_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, SoaModuleSchema>) {
                    result.push_back(detail::lower_soa_module(module, manifest.types, type_graph));
                } else if constexpr (std::is_same_v<T, StaticTableModuleSchema>) {
                    result.push_back(detail::lower_static_table_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, FacadeModuleSchema>) {
                    result.push_back(detail::lower_facade_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, SettingsModuleSchema>) {
                    result.push_back(detail::lower_settings_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, HomogeneousModuleSchema>) {
                    result.push_back(detail::lower_homogeneous_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, VectorModuleSchema>) {
                    result.push_back(detail::lower_vector_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, UmbrellaModuleSchema>) {
                    result.push_back(lower_umbrella(module));
                }
            },
            schema);
    }
    return result;
}

auto render_modules(std::vector<Module> const& modules) -> std::vector<GeneratedFile> {
    std::vector<GeneratedFile> result;
    std::set<std::string> paths;
    for (auto const& module : modules) {
        for (auto const* file : {module.header ? &*module.header : nullptr,
                                 module.source ? &*module.source : nullptr}) {
            if (file == nullptr) {
                continue;
            }
            auto const normalized{file->path.lexically_normal()};
            if (!paths.insert(codegen::output_path_key(normalized)).second) {
                throw std::invalid_argument{"Duplicate generated output path: " +
                                            normalized.string()};
            }
            auto generated_file{*file};
            generated_file.clang_format_off = false;
            generated_file.format_generated = true;
            result.push_back(GeneratedFile{normalized, render(generated_file), true});
        }
    }
    return result;
}

auto compile_sources(std::filesystem::path const& types,
                     std::span<std::filesystem::path const> const modules) -> lispb::Compilation {
    auto const manifest{load_sources(types, modules)};
    auto const files{render_modules(lower_modules(manifest))};
    lispb::Compilation result;
    result.dependencies.push_back(types);
    result.dependencies.insert(result.dependencies.end(), modules.begin(), modules.end());
    for (auto const& file : files) {
        result.artifacts.push_back(file);
    }
    return result;
}

} // namespace codegen
