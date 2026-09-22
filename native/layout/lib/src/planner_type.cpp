#include <ioj/layout/planner_type.hpp>

#include <ioj/layout/analyzer.hpp>

#include <algorithm>
#include <type_traits>
#include <variant>

namespace ioj::layout {
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

auto declaration_capabilities(lispb::schema::TypeNode const& node) -> DeclarationCapabilities {
    auto result{std::visit(
        [](auto const& definition) -> DeclarationCapabilities {
            using Type = std::decay_t<decltype(definition)>;
            if constexpr (std::is_same_v<Type, lispb::schema::ExternalType>) {
                return {.kind = DeclarationKind::external};
            } else if constexpr (std::is_same_v<Type, lispb::schema::EnumType>) {
                return {.kind = DeclarationKind::enumeration,
                        .visible = true,
                        .has_physical_layout = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::IntegerScalarType>) {
                return {.kind = DeclarationKind::integer_scalar, .visible = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::LinearQuantizedType>) {
                return {.kind = DeclarationKind::linear_quantized, .visible = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::IntegerVarintType>) {
                return {.kind = DeclarationKind::integer_varint, .visible = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::FixedPointType>) {
                return {.kind = DeclarationKind::fixed_point, .visible = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::MiniFloatType>) {
                return {.kind = DeclarationKind::mini_float, .visible = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::OptionalSentinelType>) {
                return {.kind = DeclarationKind::optional_sentinel, .visible = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::OptionalPresenceBitType>) {
                return {.kind = DeclarationKind::optional_presence_bit, .visible = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::PackedType>) {
                return {.kind = DeclarationKind::packed,
                        .visible = true,
                        .has_physical_layout = true,
                        .supports_variants = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::RecordType>) {
                return {
                    .kind = DeclarationKind::record, .visible = true, .has_physical_layout = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::UnionType>) {
                return {
                    .kind = DeclarationKind::union_, .visible = true, .has_physical_layout = true};
            } else if constexpr (std::is_same_v<Type, lispb::schema::TaggedUnionType>) {
                return {.kind = DeclarationKind::tagged_union,
                        .visible = true,
                        .has_physical_layout = true};
            } else {
                auto const supported{definition.backend == codegen::SoaBackend::standard_library};
                return {.kind = DeclarationKind::soa,
                        .visible = supported,
                        .has_physical_layout = supported,
                        .supports_variants = supported};
            }
        },
        node.definition)};
    return result;
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
        return status(Analyzer::analyze_integer_varint(types, type, 1).diagnostics, true);
    }
    if (std::holds_alternative<lispb::schema::FixedPointType>(definition)) {
        return status(Analyzer::analyze_fixed_point(types, type, 1).diagnostics, true);
    }
    if (std::holds_alternative<lispb::schema::MiniFloatType>(definition)) {
        return status(Analyzer::analyze_mini_float(types, type, 1).diagnostics, true);
    }
    if (std::holds_alternative<lispb::schema::OptionalSentinelType>(definition)) {
        return status(Analyzer::analyze_optional_sentinel(types, type, 1).diagnostics, true);
    }
    return status(Analyzer::analyze_optional_presence_bit(types, type, 1).diagnostics, true);
}

} // namespace ioj::layout
