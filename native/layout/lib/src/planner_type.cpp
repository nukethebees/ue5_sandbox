#include <ioj/layout/planner_type.hpp>

#include <ioj/layout/analyzer.hpp>

#include <algorithm>
#include <type_traits>
#include <variant>

namespace ioj::layout {

auto external_dependencies(lispb::schema::TypeGraph const& types)
    -> std::vector<ExternalDependency> {
    std::map<std::string, ExternalDependency> grouped;
    auto append_unique = [](auto& values, auto const& value) {
        if (std::ranges::find(values, value) == values.end()) {
            values.push_back(value);
        }
    };
    for (std::size_t index{}; index < types.types().size(); ++index) {
        auto const& node{types.types()[index]};
        auto const* external{std::get_if<lispb::schema::ExternalType>(&node.definition)};
        if (external == nullptr) {
            continue;
        }
        if (node.cpp_spelling == "void" || node.cpp_spelling == "auto" ||
            node.cpp_spelling == "decltype(auto)") {
            continue;
        }
        auto& entry{grouped[codegen::native_spelling(node.cpp_spelling)]};
        entry.cpp_spelling = codegen::native_spelling(node.cpp_spelling);
        entry.types.push_back(lispb::schema::TypeId{static_cast<std::uint32_t>(index)});
        entry.semantics_known =
            entry.semantics_known || !std::holds_alternative<std::monostate>(external->semantics);
        for (auto const& name : external->registered_names) {
            append_unique(entry.registered_names, name);
        }
    }
    for (std::size_t index{}; index < types.type_uses().size(); ++index) {
        auto const& use{types.type_uses()[index]};
        auto const& node{types.type(use.target.type)};
        if (!std::holds_alternative<lispb::schema::ExternalType>(node.definition)) {
            continue;
        }
        auto const found{grouped.find(codegen::native_spelling(node.cpp_spelling))};
        if (found == grouped.end()) {
            continue;
        }
        auto& entry{found->second};
        entry.uses.push_back(index);
        append_unique(entry.modules, use.module_name);
        if (use.declaration.has_value()) {
            append_unique(entry.declarations, *use.declaration);
        }
    }
    std::vector<ExternalDependency> result;
    for (auto& [spelling, entry] : grouped) {
        result.push_back(std::move(entry));
    }
    std::ranges::stable_sort(result, [](auto const& left, auto const& right) {
        return left.declarations.size() > right.declarations.size();
    });
    return result;
}

namespace {

auto has_error(std::vector<Diagnostic> const& diagnostics) -> bool {
    return std::ranges::any_of(diagnostics, [](auto const& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::error;
    });
}

auto status(std::vector<Diagnostic> const& diagnostics, bool const facts_available)
    -> LayoutStatus {
    if (has_error(diagnostics)) {
        return LayoutStatus::error;
    }
    return facts_available ? LayoutStatus::available : LayoutStatus::unknown;
}

} // namespace

auto integer_source_reference(lispb::schema::TypeNode const& node) -> std::string {
    return node.identity.origin == lispb::schema::TypeOrigin::registered_external
             ? "@" + node.identity.name
             : node.cpp_spelling;
}

auto declaration_capabilities(lispb::schema::TypeNode const& node) -> DeclarationCapabilities {
    auto result{std::visit(
        [](auto const& definition) -> DeclarationCapabilities {
            using Type = std::decay_t<decltype(definition)>;
            if constexpr (std::is_same_v<Type, lispb::schema::ExternalType>) {
                return {.kind = DeclarationKind::external, .inspectable = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::EnumType>) {
                return {.kind = DeclarationKind::enumeration,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::IntegerScalarType>) {
                return {.kind = DeclarationKind::integer_scalar,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::LinearQuantizedType>) {
                return {.kind = DeclarationKind::linear_quantized,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::IntegerVarintType>) {
                return {.kind = DeclarationKind::integer_varint,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::FixedPointType>) {
                return {.kind = DeclarationKind::fixed_point,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::MiniFloatType>) {
                return {.kind = DeclarationKind::mini_float,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::OptionalSentinelType>) {
                return {.kind = DeclarationKind::optional_sentinel,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::OptionalPresenceBitType>) {
                return {.kind = DeclarationKind::optional_presence_bit,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::PackedType>) {
                return {.kind = DeclarationKind::packed,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true,
                        .supports_variant_overrides = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::RecordType>) {
                return {.kind = DeclarationKind::record,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::UnionType>) {
                return {.kind = DeclarationKind::union_,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::TaggedUnionType>) {
                return {.kind = DeclarationKind::tagged_union,
                        .inspectable = true,
                        .editable = true,
                        .physical_analysis_available = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::StaticTableType>) {
                return {
                    .kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::FacadeType>) {
                return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::HomogeneousStorageType>) {
                return {.kind = DeclarationKind::homogeneous_layout,
                        .inspectable = true,
                        .editable = true};
            } else {
                auto const supported{definition.backend == codegen::SoaBackend::standard_library};
                auto const vector{definition.source_kind == lispb::schema::SoaSourceKind::vector};
                return {.kind = vector ? DeclarationKind::vector_soa : DeclarationKind::soa,
                        .inspectable = true,
                        .editable = supported && !vector,
                        .physical_analysis_available = supported,
                        .supports_variant_overrides = supported};
            }
        },
        node.definition)};
    return result;
}

auto declaration_capabilities(codegen::DeclarationSchema const& declaration)
    -> DeclarationCapabilities {
    return std::visit(
        [](auto const& schema) -> DeclarationCapabilities {
            using Type = std::decay_t<decltype(schema)>;
            if constexpr (std::is_same_v<Type, codegen::HomogeneousLayoutSchema>) {
                return {.kind = DeclarationKind::homogeneous_layout,
                        .inspectable = true,
                        .editable = true};
            } else if constexpr (std::is_same_v<Type, codegen::StaticTableSchema>) {
                return {
                    .kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
            } else if constexpr (std::is_same_v<Type, codegen::FacadeSchema>) {
                return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
            } else {
                return {};
            }
        },
        declaration);
}

auto declaration_kind_label(DeclarationKind const kind) -> char const* {
    switch (kind) {
        case DeclarationKind::external:
            return "external";
        case DeclarationKind::enumeration:
            return "enum";
        case DeclarationKind::integer_scalar:
            return "integer scalar";
        case DeclarationKind::linear_quantized:
            return "linear quantized";
        case DeclarationKind::integer_varint:
            return "integer varint";
        case DeclarationKind::fixed_point:
            return "fixed point";
        case DeclarationKind::mini_float:
            return "mini float";
        case DeclarationKind::optional_sentinel:
            return "optional sentinel";
        case DeclarationKind::optional_presence_bit:
            return "optional presence bit";
        case DeclarationKind::packed:
            return "packed value";
        case DeclarationKind::record:
            return "record";
        case DeclarationKind::union_:
            return "union";
        case DeclarationKind::tagged_union:
            return "tagged union";
        case DeclarationKind::soa:
            return "SoA";
        case DeclarationKind::vector_soa:
            return "vector-soa";
        case DeclarationKind::homogeneous_layout:
            return "layout";
        case DeclarationKind::static_table:
            return "table";
        case DeclarationKind::facade:
            return "facade";
    }
    return "unknown";
}

auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId const type,
                        Variant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t const default_capacity) -> LayoutStatus {
    auto const targets{
        Analyzer::derive_relationship_target_facts(types, variant, abi, default_capacity)};
    return declaration_status(types,
                              type,
                              variant,
                              abi,
                              default_capacity,
                              1,
                              SoaAllocationStrategy::separate_columns,
                              targets);
}

auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId const type,
                        Variant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t const default_capacity,
                        std::uint64_t const element_count,
                        SoaAllocationStrategy const allocation_strategy,
                        std::span<RelationshipTargetFacts const> relationship_targets)
    -> LayoutStatus {
    if (!type.valid() || type.value >= types.types().size()) {
        return LayoutStatus::error;
    }
    auto const& definition{types.type(type).definition};
    if (!declaration_capabilities(types.type(type)).physical_analysis_available) {
        return LayoutStatus::unknown;
    }
    if (std::holds_alternative<lispb::schema::EnumType>(definition)) {
        auto const analysis{Analyzer::analyze_enum(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.backing_facts.has_value());
    }
    if (std::holds_alternative<lispb::schema::PackedType>(definition)) {
        auto const analysis{Analyzer::analyze_packed(
            types, type, variant, abi, element_count, relationship_targets)};
        return status(analysis.diagnostics, analysis.storage_facts.has_value());
    }
    if (std::holds_alternative<lispb::schema::RecordType>(definition)) {
        auto const analysis{Analyzer::analyze_record(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.size_bytes.has_value());
    }
    if (std::holds_alternative<lispb::schema::UnionType>(definition)) {
        auto const analysis{Analyzer::analyze_union(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.size_bytes.has_value());
    }
    if (std::holds_alternative<lispb::schema::TaggedUnionType>(definition)) {
        auto const analysis{Analyzer::analyze_tagged_union(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.size_bytes.has_value());
    }
    if (auto const* soa{std::get_if<lispb::schema::SoaType>(&definition)}) {
        if (soa->backend != codegen::SoaBackend::standard_library) {
            return LayoutStatus::unknown;
        }
        auto const analysis{Analyzer::analyze_soa(
            types, type, variant, abi, default_capacity, allocation_strategy)};
        return status(analysis.diagnostics, analysis.total_payload_bytes.has_value());
    }
    if (std::holds_alternative<lispb::schema::ExternalType>(definition)) {
        return LayoutStatus::unknown;
    }
    if (std::holds_alternative<lispb::schema::IntegerScalarType>(definition)) {
        return status(
            Analyzer::analyze_integer_scalar(types, type, relationship_targets).diagnostics, true);
    }
    if (std::holds_alternative<lispb::schema::LinearQuantizedType>(definition)) {
        return LayoutStatus::available;
    }
    if (std::holds_alternative<lispb::schema::IntegerVarintType>(definition)) {
        return status(Analyzer::analyze_integer_varint(types, type, element_count).diagnostics,
                      true);
    }
    if (std::holds_alternative<lispb::schema::FixedPointType>(definition)) {
        return status(Analyzer::analyze_fixed_point(types, type, element_count).diagnostics, true);
    }
    if (std::holds_alternative<lispb::schema::MiniFloatType>(definition)) {
        return status(Analyzer::analyze_mini_float(types, type, element_count).diagnostics, true);
    }
    if (std::holds_alternative<lispb::schema::OptionalSentinelType>(definition)) {
        return status(Analyzer::analyze_optional_sentinel(types, type, element_count).diagnostics,
                      true);
    }
    return status(Analyzer::analyze_optional_presence_bit(types, type, element_count).diagnostics,
                  true);
}

} // namespace ioj::layout
