#include "lowering.h"
#include "lowering_utils.h"

#include <codegen/generator.h>
#include <codegen/path_utils.h>
#include <codegen/source_loader.h>
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

auto forward_declaration_kind(lispb::schema::TypeNode const& node)
    -> std::optional<std::string_view> {
    using namespace lispb::schema;
    if (node.identity.origin != TypeOrigin::declaration) {
        return std::nullopt;
    }
    return std::visit(
        [](auto const& definition) -> std::optional<std::string_view> {
            using T = std::decay_t<decltype(definition)>;
            if constexpr (std::is_same_v<T, UnionType>) {
                return "union";
            } else if constexpr (std::is_same_v<T, FacadeType>) {
                return "class";
            } else if constexpr (std::is_same_v<T, SoaType>) {
                return definition.layout_only ? std::nullopt
                                              : std::optional<std::string_view>{"struct"};
            } else if constexpr (std::is_same_v<T, RecordType> ||
                                 std::is_same_v<T, TaggedUnionType> ||
                                 std::is_same_v<T, PackedType> ||
                                 std::is_same_v<T, StaticTableType> ||
                                 std::is_same_v<T, HomogeneousStorageType>) {
                return "struct";
            } else {
                return std::nullopt;
            }
        },
        node.definition);
}

auto uses_forward_declaration(lispb::schema::ResolvedTypeRef const& reference,
                              lispb::schema::TypeGraph const& graph) -> bool {
    auto const form{reference.physical.form};
    return reference.physical.names_semantic_type &&
           (form == PhysicalTypeForm::object_pointer ||
            form == PhysicalTypeForm::lvalue_reference ||
            form == PhysicalTypeForm::rvalue_reference) &&
           forward_declaration_kind(graph.type(reference.type)).has_value();
}

auto generated_forward_declarations(NormalModuleSchema const& module,
                                    lispb::schema::TypeGraph const& graph)
    -> detail::DeclarationEmission {
    std::set<lispb::schema::TypeId> targets;
    for (auto const& use : graph.type_uses()) {
        if (use.module_name == module.settings.name &&
            uses_forward_declaration(use.target, graph) &&
            graph.type(use.target.type).identity.module_name == module.settings.name) {
            targets.insert(use.target.type);
        }
    }
    NodeListBuilder declarations;
    for (auto const target : targets) {
        auto const& node{graph.type(target)};
        declarations.add(ForwardDeclaration{
            node.cpp_spelling.substr(node.cpp_spelling.rfind("::") == std::string::npos
                                         ? 0
                                         : node.cpp_spelling.rfind("::") + 2),
            std::string{*forward_declaration_kind(node)}});
    }
    return {.header = declarations.build(), .source_dependencies = false};
}

auto declaration_emission_order(NormalModuleSchema const& module,
                                lispb::schema::TypeGraph const& graph) -> std::vector<std::size_t> {
    std::map<lispb::schema::TypeId, std::size_t> declarations;
    auto const count{module.declarations.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const type{graph.find_declared(module.settings.name,
                                            declaration_name(module.declarations[index]))};
        if (type.has_value()) {
            declarations.emplace(*type, index);
        }
    }

    std::vector<std::size_t> order;
    std::vector<bool> emitted(count);
    order.reserve(count);
    auto visit = [&](auto&& self, std::size_t const index) -> void {
        if (emitted[index]) {
            return;
        }
        emitted[index] = true;
        auto const type{graph.find_declared(module.settings.name,
                                            declaration_name(module.declarations[index]))};
        if (type.has_value()) {
            for (auto const dependency_type : graph.dependencies_of(*type)) {
                auto const found{declarations.find(dependency_type)};
                if (found == declarations.end()) {
                    continue;
                }
                auto const* scalar{
                    std::get_if<IntegerScalarSchema>(&module.declarations[found->second])};
                if (scalar != nullptr && scalar->cpp_emission == IntegerScalarCppEmission::alias) {
                    self(self, found->second);
                }
            }
            auto dependency = [&](lispb::schema::ResolvedTypeRef const& reference) {
                if (uses_forward_declaration(reference, graph)) {
                    return;
                }
                if (auto const found{declarations.find(reference.type)};
                    found != declarations.end()) {
                    self(self, found->second);
                }
            };
            std::visit(
                [&](auto const& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, lispb::schema::RecordType>) {
                        for (auto const& member : value.members) {
                            dependency(member.semantic_type);
                        }
                    } else if constexpr (std::is_same_v<T, lispb::schema::SoaType>) {
                        for (auto const& column : value.columns) {
                            dependency(column.semantic_type);
                        }
                    } else if constexpr (std::is_same_v<T, lispb::schema::UnionType> ||
                                         std::is_same_v<T, lispb::schema::TaggedUnionType>) {
                        if constexpr (std::is_same_v<T, lispb::schema::TaggedUnionType>) {
                            dependency(value.discriminant);
                        }
                        for (auto const& alternative : value.alternatives) {
                            dependency(alternative.semantic_type);
                        }
                    }
                },
                graph.type(*type).definition);
        }
        order.push_back(index);
    };
    for (std::size_t index{}; index < count; ++index) {
        visit(visit, index);
    }
    return order;
}

auto resolve_soa_cpp_references(Manifest const& manifest, lispb::schema::TypeGraph const& graph)
    -> Manifest {
    auto resolved{manifest};
    std::size_t next{};
    for (auto& variant : resolved.modules) {
        auto* module{std::get_if<NormalModuleSchema>(&variant)};
        if (module == nullptr) {
            continue;
        }
        for (auto& declaration : module->declarations) {
            visit_type_references(declaration, [&](std::string const& role, TypeRef& reference) {
                if (role.find("relationship") != std::string::npos) {
                    return;
                }
                if (auto const* soa{std::get_if<SoaSchema>(&declaration)}) {
                    for (auto const& member : soa->members) {
                        if (member.kind == SoaMemberKind::nested &&
                            role == "member " + member.name) {
                            return;
                        }
                    }
                }
                auto const target{graph.find_reference(reference, module->settings.name)};
                if (!target) {
                    return;
                }
                auto const& node{graph.type(*target)};
                if (!std::holds_alternative<lispb::schema::SoaType>(node.definition)) {
                    return;
                }
                auto const logical{node.identity.namespace_name.empty()
                                       ? node.identity.name
                                       : node.identity.namespace_name + "::" + node.identity.name};
                if (node.cpp_spelling == logical) {
                    return;
                }
                if (node.cpp_spelling.empty()) {
                    throw std::invalid_argument{"Logical layout schema '" + node.identity.name +
                                                "' has no C++ owner for " + role};
                }
                auto type{resolve_type(reference, manifest.types)};
                auto canonical{reference};
                canonical.name = node.cpp_spelling;
                type.spelling = resolve_type(canonical, {}).spelling;
                std::string key;
                do {
                    key = "resolved_soa_" + std::to_string(next++);
                } while (resolved.types.contains(key));
                resolved.types.emplace(key, RegisteredTypeSchema{type});
                reference = TypeRef{"@" + key};
            });
        }
    }
    return resolved;
}

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

auto scalar_cpp_type(TypeRef const& reference, TypeRegistry const& types) -> CppType {
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

auto lower_scalar(IntegerScalarSchema const& scalar, TypeRegistry const& types)
    -> detail::DeclarationEmission {
    NodeListBuilder declarations;
    if (scalar.cpp_emission == IntegerScalarCppEmission::alias) {
        auto const cpp_type{scalar_cpp_type(*scalar.cpp_type, types)};
        declarations.add(
            raw("using " + scalar.name + " = " + cpp_type.spelling + ";", cpp_type.dependencies));
        return {.header = declarations.build()};
    }
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

} // namespace

auto lower_modules(Manifest const& input) -> std::vector<Module> {
    auto const type_graph{lispb::schema::resolve_type_graph(input)};
    auto const manifest{resolve_soa_cpp_references(input, type_graph)};
    std::vector<Module> result;
    for (auto const& schema : manifest.modules) {
        std::visit(
            [&](auto const& module) {
                using T = std::decay_t<decltype(module)>;
                if constexpr (std::is_same_v<T, NormalModuleSchema>) {
                    std::vector<detail::DeclarationEmission> emissions;
                    emissions.reserve(module.declarations.size());
                    emissions.push_back(generated_forward_declarations(module, type_graph));
                    for (auto const index : declaration_emission_order(module, type_graph)) {
                        auto const& declaration{module.declarations[index]};
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
                } else if constexpr (std::is_same_v<T, SettingsModuleSchema>) {
                    result.push_back(detail::lower_settings_module(module, manifest.types));
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
    auto const registry{load_type_registry(types)};
    auto const manifest{load_sources(registry, modules)};
    auto const files{render_modules(lower_modules(manifest))};
    lispb::Compilation result;
    for (auto const& source : registry.sources) {
        result.dependencies.push_back(source.path);
    }
    result.dependencies.insert(result.dependencies.end(), modules.begin(), modules.end());
    for (auto const& file : files) {
        result.artifacts.push_back(file);
    }
    return result;
}

} // namespace codegen
