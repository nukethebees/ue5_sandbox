#include <lispb/schema/type_graph.h>

#include "packed_value_internal.h"

#include <codegen/validation.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lispb::schema {
namespace {

auto qualified_name(codegen::ModuleSettings const& settings, std::string const& name)
    -> std::string {
    return settings.namespace_name.has_value() ? *settings.namespace_name + "::" + name : name;
}

} // namespace

class TypeGraphBuilder {
  public:
    explicit TypeGraphBuilder(codegen::Manifest const& manifest)
        : manifest_{manifest} {}

    auto build() -> TypeGraph {
        declare_types();
        bind_registered_types();
        resolve_definitions();
        validate_aggregate_cycles();
        build_edges();
        return std::move(graph_);
    }
  private:
    struct Declaration {
        TypeId id;
        std::size_t module_index{};
        std::size_t declaration_index{};
    };

    auto add_type(TypeIdentity identity, std::string cpp_spelling, TypeDefinition definition)
        -> TypeId {
        if (graph_.types_.size() >= TypeId::invalid_value) {
            throw std::invalid_argument{"Semantic type graph contains too many types"};
        }
        if (graph_.identities_.contains(identity)) {
            throw std::invalid_argument{
                "Duplicate semantic type identity: " + identity.module_name + ":" +
                identity.namespace_name + ":" + identity.name};
        }
        auto const id{TypeId{static_cast<std::uint32_t>(graph_.types_.size())}};
        graph_.types_.push_back(TypeNode{.identity = identity,
                                         .cpp_spelling = std::move(cpp_spelling),
                                         .definition = std::move(definition)});
        graph_.identities_.emplace(std::move(identity), id);
        return id;
    }

    template <typename Definition>
    void declare(std::size_t const module_index,
                 std::size_t const declaration_index,
                 codegen::ModuleSettings const& settings,
                 std::string const& name,
                 Definition definition) {
        auto const namespace_name{settings.namespace_name.value_or("")};
        auto const id{add_type(TypeIdentity{.origin = TypeOrigin::declaration,
                                            .module_name = settings.name,
                                            .namespace_name = namespace_name,
                                            .name = name},
                               qualified_name(settings, name),
                               std::move(definition))};
        declarations_.push_back({id, module_index, declaration_index});
        declarations_by_module_name_.emplace(std::pair{settings.name, name}, id);
        declarations_by_spelling_[graph_.type(id).cpp_spelling].push_back(id);
        declarations_by_name_[name].push_back(id);
    }

    void declare_types() {
        for (std::size_t module_index{}; module_index < manifest_.modules.size(); ++module_index) {
            std::visit(
                [&](auto const& module) {
                    using Module = std::decay_t<decltype(module)>;
                    if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                        for (std::size_t index{}; index < module.enums.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.enums[index].name,
                                    EnumType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::ScalarModuleSchema>) {
                        for (std::size_t index{}; index < module.scalars.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.scalars[index].name,
                                    IntegerScalarType{});
                        }
                    } else if constexpr (std::is_same_v<Module,
                                                        codegen::RepresentationModuleSchema>) {
                        for (std::size_t index{}; index < module.linear_quantized.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.linear_quantized[index].name,
                                    LinearQuantizedType{});
                        }
                        for (std::size_t index{}; index < module.integer_varints.size(); ++index) {
                            declare(module_index,
                                    module.linear_quantized.size() + index,
                                    module.settings,
                                    module.integer_varints[index].name,
                                    IntegerVarintType{});
                        }
                        for (std::size_t index{}; index < module.fixed_points.size(); ++index) {
                            declare(module_index,
                                    module.linear_quantized.size() + module.integer_varints.size() +
                                        index,
                                    module.settings,
                                    module.fixed_points[index].name,
                                    FixedPointType{});
                        }
                        for (std::size_t index{}; index < module.optional_sentinels.size();
                             ++index) {
                            declare(module_index,
                                    module.linear_quantized.size() + module.integer_varints.size() +
                                        module.fixed_points.size() + index,
                                    module.settings,
                                    module.optional_sentinels[index].name,
                                    OptionalSentinelType{});
                        }
                        for (std::size_t index{}; index < module.optional_presence_bits.size();
                             ++index) {
                            declare(module_index,
                                    module.linear_quantized.size() + module.integer_varints.size() +
                                        module.fixed_points.size() +
                                        module.optional_sentinels.size() + index,
                                    module.settings,
                                    module.optional_presence_bits[index].name,
                                    OptionalPresenceBitType{});
                        }
                        for (std::size_t index{}; index < module.mini_floats.size(); ++index) {
                            declare(module_index,
                                    module.linear_quantized.size() + module.integer_varints.size() +
                                        module.fixed_points.size() +
                                        module.optional_sentinels.size() +
                                        module.optional_presence_bits.size() + index,
                                    module.settings,
                                    module.mini_floats[index].name,
                                    MiniFloatType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                        for (std::size_t index{}; index < module.values.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.values[index].name,
                                    PackedType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                        for (std::size_t index{}; index < module.records.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.records[index].name,
                                    RecordType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::UnionModuleSchema>) {
                        for (std::size_t index{}; index < module.unions.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.unions[index].name,
                                    UnionType{});
                        }
                        for (std::size_t index{}; index < module.tagged_unions.size(); ++index) {
                            declare(module_index,
                                    module.unions.size() + index,
                                    module.settings,
                                    module.tagged_unions[index].name,
                                    TaggedUnionType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                        for (std::size_t index{}; index < module.structs.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.structs[index].name,
                                    SoaType{.backend = module.backend});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                        declare(module_index,
                                0,
                                module.settings,
                                module.storage_name,
                                SoaType{.backend = module.backend,
                                        .source_kind = SoaSourceKind::vector});
                    }
                },
                manifest_.modules[module_index]);
        }
    }

    void bind_registered_types() {
        for (auto const& [name, cpp_type] : manifest_.types) {
            auto const separator{cpp_type.spelling.rfind("::")};
            auto const declared_name{separator == std::string::npos
                                         ? cpp_type.spelling
                                         : cpp_type.spelling.substr(separator + 2)};
            if (auto const local{declarations_by_module_name_.find(std::pair{name, declared_name})};
                local != declarations_by_module_name_.end()) {
                graph_.registered_types_.emplace(name, local->second);
                continue;
            }

            auto declarations{declarations_by_spelling_.find(cpp_type.spelling)};
            auto const* matches{
                declarations == declarations_by_spelling_.end() ? nullptr : &declarations->second};
            if (matches == nullptr) {
                auto const by_name{declarations_by_name_.find(cpp_type.spelling)};
                if (by_name != declarations_by_name_.end()) {
                    matches = &by_name->second;
                }
            }
            if (matches != nullptr) {
                if (matches->size() != 1) {
                    throw std::invalid_argument{"Registered type '@" + name +
                                                "' ambiguously matches multiple declarations of '" +
                                                cpp_type.spelling + "'"};
                }
                graph_.registered_types_.emplace(name, matches->front());
                continue;
            }

            auto const id{
                add_type(TypeIdentity{.origin = TypeOrigin::registered_external, .name = name},
                         cpp_type.spelling,
                         ExternalType{.cpp_type = cpp_type, .registered_names = {name}})};
            graph_.registered_types_.emplace(name, id);
        }
    }

    auto raw_external(std::string const& spelling) -> TypeId {
        if (auto const found{raw_external_types_.find(spelling)};
            found != raw_external_types_.end()) {
            return found->second;
        }
        auto const id{add_type(TypeIdentity{.origin = TypeOrigin::cpp_spelling, .name = spelling},
                               spelling,
                               ExternalType{.cpp_type = codegen::CppType{spelling}})};
        raw_external_types_.emplace(spelling, id);
        return id;
    }

    auto resolve_ref(codegen::TypeRef const& reference, std::string const& module_name)
        -> ResolvedTypeRef {
        auto const cpp_type{codegen::resolve_type(reference, manifest_.types)};
        if (reference.name.starts_with('@')) {
            auto const name{reference.name.substr(1)};
            auto const found{graph_.registered_types_.find(name)};
            if (found == graph_.registered_types_.end()) {
                throw std::invalid_argument{"Unknown semantic type reference: " + reference.name};
            }
            return {.type = found->second, .cpp_type = cpp_type};
        }

        if (auto const local{
                declarations_by_module_name_.find(std::pair{module_name, reference.name})};
            local != declarations_by_module_name_.end()) {
            return {.type = local->second, .cpp_type = cpp_type};
        }

        if (auto const declarations{declarations_by_spelling_.find(reference.name)};
            declarations != declarations_by_spelling_.end() && declarations->second.size() == 1) {
            return {.type = declarations->second.front(), .cpp_type = cpp_type};
        }
        return {.type = raw_external(reference.name), .cpp_type = cpp_type};
    }

    auto local_declaration(std::string const& module_name, std::string const& name) const
        -> std::optional<TypeId> {
        auto const found{declarations_by_module_name_.find(std::pair{module_name, name})};
        return found == declarations_by_module_name_.end() ? std::nullopt
                                                           : std::optional{found->second};
    }

    void resolve_definitions() {
        for (auto const primary_pass : {true, false}) {
            for (auto const& declaration : declarations_) {
                auto const& module_schema{manifest_.modules[declaration.module_index]};
                auto const primary_definition{
                    std::holds_alternative<codegen::EnumModuleSchema>(module_schema) ||
                    std::holds_alternative<codegen::ScalarModuleSchema>(module_schema)};
                if (primary_definition != primary_pass) {
                    continue;
                }
                std::visit(
                    [&](auto const& module) {
                        using Module = std::decay_t<decltype(module)>;
                        if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                            auto const& source{module.enums[declaration.declaration_index]};
                            EnumType type{.underlying_type = source.underlying_type.has_value()
                                                               ? std::optional{resolve_ref(
                                                                     *source.underlying_type,
                                                                     module.settings.name)}
                                                               : std::nullopt,
                                          .bit_width = source.bit_width,
                                          .signedness = source.signedness,
                                          .enumerators = {},
                                          .count = source.count};
                            type.enumerators.reserve(source.values.size());
                            for (auto const& value : source.values) {
                                type.enumerators.push_back(
                                    {.name = value.name,
                                     .explicit_value = value.initializer,
                                     .display_name = value.display_name,
                                     .serialized_name = value.serialized_name,
                                     .hidden = value.hidden,
                                     .sentinel = value.sentinel,
                                     .count_sentinel =
                                         source.count.has_value() && value.name == *source.count});
                            }
                            graph_.types_[declaration.id.value].definition = std::move(type);
                        } else if constexpr (std::is_same_v<Module, codegen::ScalarModuleSchema>) {
                            auto const& source{module.scalars[declaration.declaration_index]};
                            auto required_minimum{source.minimum_value};
                            auto required_maximum{source.maximum_value};
                            IntegerScalarType type{.signedness = source.signedness,
                                                   .minimum_value = source.minimum_value,
                                                   .maximum_value = source.maximum_value,
                                                   .bit_width = 0,
                                                   .bit_width_auto = !source.bit_width.has_value(),
                                                   .named_codes = {},
                                                   .relationship = std::nullopt};
                            type.named_codes.reserve(source.named_codes.size());
                            for (auto const& code : source.named_codes) {
                                type.named_codes.push_back({.name = code.name,
                                                            .value = code.value,
                                                            .sentinel = code.sentinel});
                                if (codegen::packed_integer_less(code.value, required_minimum)) {
                                    required_minimum = code.value;
                                }
                                if (codegen::packed_integer_less(required_maximum, code.value)) {
                                    required_maximum = code.value;
                                }
                            }
                            type.bit_width =
                                source.bit_width.value_or(*codegen::minimum_packed_integer_bits(
                                    required_minimum, required_maximum, source.signedness));
                            if (source.relationship.has_value()) {
                                auto target{
                                    resolve_ref(source.relationship->target, module.settings.name)};
                                if (graph_.types_[target.type.value].identity.origin !=
                                    TypeOrigin::declaration) {
                                    throw std::invalid_argument{
                                        "Integer scalar relationship target '" +
                                        source.relationship->target.name +
                                        "' must resolve to a declared semantic type"};
                                }
                                type.relationship =
                                    SemanticRelationship{.kind = source.relationship->kind,
                                                         .target = std::move(target),
                                                         .unit = source.relationship->unit};
                            }
                            graph_.types_[declaration.id.value].definition = std::move(type);
                        } else if constexpr (std::is_same_v<Module,
                                                            codegen::RepresentationModuleSchema>) {
                            if (declaration.declaration_index < module.linear_quantized.size()) {
                                auto const& source{
                                    module.linear_quantized[declaration.declaration_index]};
                                auto resolved_source{
                                    resolve_ref(source.source, module.settings.name)};
                                if (!std::holds_alternative<IntegerScalarType>(
                                        graph_.types_[resolved_source.type.value].definition)) {
                                    throw std::invalid_argument{
                                        "Linear quantization '" + source.name +
                                        "' source must resolve to an integer-scalar declaration"};
                                }
                                graph_.types_[declaration.id.value].definition =
                                    LinearQuantizedType{
                                        .source = std::move(resolved_source),
                                        .bit_width = source.bit_width,
                                        .reserved_codes = source.reserved_codes,
                                        .clipping = source.clipping,
                                    };
                            } else if (declaration.declaration_index <
                                       module.linear_quantized.size() +
                                           module.integer_varints.size()) {
                                auto const index{declaration.declaration_index -
                                                 module.linear_quantized.size()};
                                auto const& source{module.integer_varints[index]};
                                auto resolved_source{
                                    resolve_ref(source.source, module.settings.name)};
                                if (!std::holds_alternative<IntegerScalarType>(
                                        graph_.types_[resolved_source.type.value].definition)) {
                                    throw std::invalid_argument{
                                        "Integer varint '" + source.name +
                                        "' source must resolve to an integer-scalar declaration"};
                                }
                                graph_.types_[declaration.id.value].definition = IntegerVarintType{
                                    .source = std::move(resolved_source),
                                    .encoding = source.encoding,
                                };
                            } else if (declaration.declaration_index <
                                       module.linear_quantized.size() +
                                           module.integer_varints.size() +
                                           module.fixed_points.size()) {
                                auto const index{declaration.declaration_index -
                                                 module.linear_quantized.size() -
                                                 module.integer_varints.size()};
                                auto const& source{module.fixed_points[index]};
                                graph_.types_[declaration.id.value].definition = FixedPointType{
                                    .signedness = source.signedness,
                                    .total_bits = source.total_bits,
                                    .fractional_bits = source.fractional_bits,
                                    .rounding = source.rounding,
                                };
                            } else if (declaration.declaration_index <
                                       module.linear_quantized.size() +
                                           module.integer_varints.size() +
                                           module.fixed_points.size() +
                                           module.optional_sentinels.size()) {
                                auto const index{
                                    declaration.declaration_index - module.linear_quantized.size() -
                                    module.integer_varints.size() - module.fixed_points.size()};
                                auto const& source{module.optional_sentinels[index]};
                                auto resolved_source{
                                    resolve_ref(source.source, module.settings.name)};
                                auto const* scalar{std::get_if<IntegerScalarType>(
                                    &graph_.types_[resolved_source.type.value].definition)};
                                if (scalar == nullptr) {
                                    throw std::invalid_argument{
                                        "Optional sentinel representation '" + source.name +
                                        "' source must resolve to an integer-scalar declaration"};
                                }
                                auto const sentinel{std::ranges::find(
                                    scalar->named_codes, source.sentinel, &PackedNamedCode::name)};
                                if (sentinel == scalar->named_codes.end() || !sentinel->sentinel) {
                                    throw std::invalid_argument{
                                        "Optional sentinel representation '" + source.name +
                                        "' must name a source sentinel code"};
                                }
                                graph_.types_[declaration.id.value].definition =
                                    OptionalSentinelType{
                                        .source = std::move(resolved_source),
                                        .sentinel_name = source.sentinel,
                                        .sentinel_value = sentinel->value,
                                        .bit_width = scalar->bit_width,
                                    };
                            } else if (declaration.declaration_index <
                                       module.linear_quantized.size() +
                                           module.integer_varints.size() +
                                           module.fixed_points.size() +
                                           module.optional_sentinels.size() +
                                           module.optional_presence_bits.size()) {
                                auto const index{
                                    declaration.declaration_index - module.linear_quantized.size() -
                                    module.integer_varints.size() - module.fixed_points.size() -
                                    module.optional_sentinels.size()};
                                auto const& source{module.optional_presence_bits[index]};
                                auto resolved_source{
                                    resolve_ref(source.source, module.settings.name)};
                                auto const* scalar{std::get_if<IntegerScalarType>(
                                    &graph_.types_[resolved_source.type.value].definition)};
                                if (scalar == nullptr) {
                                    throw std::invalid_argument{
                                        "Optional presence-bit representation '" + source.name +
                                        "' source must resolve to an integer-scalar declaration"};
                                }
                                graph_.types_[declaration.id.value].definition =
                                    OptionalPresenceBitType{.source = std::move(resolved_source),
                                                            .payload_bits = scalar->bit_width,
                                                            .encoded_bits = scalar->bit_width + 1};
                            } else {
                                auto const index{
                                    declaration.declaration_index - module.linear_quantized.size() -
                                    module.integer_varints.size() - module.fixed_points.size() -
                                    module.optional_sentinels.size() -
                                    module.optional_presence_bits.size()};
                                auto const& source{module.mini_floats[index]};
                                graph_.types_[declaration.id.value].definition = MiniFloatType{
                                    .sign_bits = source.sign_bits,
                                    .exponent_bits = source.exponent_bits,
                                    .significand_bits = source.significand_bits,
                                    .exponent_bias = source.exponent_bias,
                                };
                            }
                        } else if constexpr (std::is_same_v<Module,
                                                            codegen::PackedValueModuleSchema>) {
                            auto const& source{module.values[declaration.declaration_index]};
                            PackedType type{
                                .storage_type =
                                    resolve_ref(source.storage_type, module.settings.name),
                                .segments = {},
                                .invalid_raw_value = source.invalid_value,
                                .byte_order = source.byte_order,
                                .bit_order = source.bit_order.value_or(
                                    codegen::PackedBitOrder::least_significant_first),
                            };
                            type.segments.reserve(source.segments.size());
                            for (auto const& segment : source.segments) {
                                std::visit(
                                    [&](auto const& value) {
                                        using Segment = std::decay_t<decltype(value)>;
                                        if constexpr (std::is_same_v<Segment,
                                                                     codegen::PackedFieldSchema>) {
                                            auto const* scalar{codegen::detail::find_integer_scalar(
                                                value.type, manifest_.types, manifest_.modules)};
                                            PackedField field{
                                                .name = value.name,
                                                .semantic_type =
                                                    resolve_ref(value.type, module.settings.name),
                                                .bit_width = static_cast<std::uint32_t>(
                                                    *codegen::detail::derive_packed_field_width(
                                                        value, manifest_.types, manifest_.modules)),
                                                .bit_width_auto = !value.bits.has_value(),
                                                .kind = value.kind,
                                                .range_helper = value.range_helper,
                                                .minimum_value =
                                                    scalar != nullptr
                                                        ? std::optional{scalar->minimum_value}
                                                        : value.minimum_value,
                                                .maximum_value =
                                                    scalar != nullptr
                                                        ? std::optional{scalar->maximum_value}
                                                        : value.maximum_value,
                                                .named_codes = {},
                                                .relationship = std::nullopt};
                                            auto const& named_codes{scalar != nullptr
                                                                        ? scalar->named_codes
                                                                        : value.named_codes};
                                            field.named_codes.reserve(named_codes.size());
                                            for (auto const& code : named_codes) {
                                                field.named_codes.push_back(
                                                    {.name = code.name,
                                                     .value = code.value,
                                                     .sentinel = code.sentinel});
                                            }
                                            if (value.relationship.has_value()) {
                                                auto target{resolve_ref(value.relationship->target,
                                                                        module.settings.name)};
                                                if (graph_.types_[target.type.value]
                                                        .identity.origin !=
                                                    TypeOrigin::declaration) {
                                                    throw std::invalid_argument{
                                                        "Packed field relationship target '" +
                                                        value.relationship->target.name +
                                                        "' must resolve to a declared semantic "
                                                        "type"};
                                                }
                                                field.relationship = SemanticRelationship{
                                                    .kind = value.relationship->kind,
                                                    .target = std::move(target),
                                                    .unit = value.relationship->unit};
                                            }
                                            type.segments.emplace_back(std::move(field));
                                        } else {
                                            type.segments.emplace_back(PackedReservedBits{
                                                .name = value.name,
                                                .bit_width =
                                                    static_cast<std::uint32_t>(value.bits)});
                                        }
                                    },
                                    segment);
                            }
                            graph_.types_[declaration.id.value].definition = std::move(type);
                        } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                            auto const& source{module.records[declaration.declaration_index]};
                            RecordType type;
                            type.members.reserve(source.members.size());
                            for (auto const& member : source.members) {
                                auto relationship{std::optional<SemanticRelationship>{}};
                                if (member.relationship.has_value()) {
                                    auto target{resolve_ref(member.relationship->target,
                                                            module.settings.name)};
                                    if (graph_.types_[target.type.value].identity.origin !=
                                        TypeOrigin::declaration) {
                                        throw std::invalid_argument{
                                            "Record member relationship target '" +
                                            member.relationship->target.name +
                                            "' must resolve to a declared semantic type"};
                                    }
                                    relationship =
                                        SemanticRelationship{.kind = member.relationship->kind,
                                                             .target = std::move(target),
                                                             .unit = member.relationship->unit};
                                }
                                type.members.push_back({.name = member.name,
                                                        .semantic_type = resolve_ref(
                                                            member.type, module.settings.name),
                                                        .count = member.count,
                                                        .relationship = std::move(relationship)});
                            }
                            graph_.types_[declaration.id.value].definition = std::move(type);
                        } else if constexpr (std::is_same_v<Module, codegen::UnionModuleSchema>) {
                            if (declaration.declaration_index < module.unions.size()) {
                                auto const& source{module.unions[declaration.declaration_index]};
                                UnionType type;
                                type.alternatives.reserve(source.alternatives.size());
                                for (auto const& alternative : source.alternatives) {
                                    type.alternatives.push_back(
                                        {.name = alternative.name,
                                         .semantic_type =
                                             resolve_ref(alternative.type, module.settings.name),
                                         .count = alternative.count});
                                }
                                graph_.types_[declaration.id.value].definition = std::move(type);
                            } else {
                                auto const& source{
                                    module.tagged_unions[declaration.declaration_index -
                                                         module.unions.size()]};
                                auto discriminant{
                                    resolve_ref(source.discriminant, module.settings.name)};
                                if (!std::holds_alternative<EnumType>(
                                        graph_.types_[discriminant.type.value].definition)) {
                                    throw std::invalid_argument{"Tagged union '" + source.name +
                                                                "' discriminant must resolve to an "
                                                                "enum declaration"};
                                }
                                TaggedUnionType type{.discriminant = std::move(discriminant),
                                                     .alternatives = {}};
                                type.alternatives.reserve(source.alternatives.size());
                                for (auto const& alternative : source.alternatives) {
                                    auto const& enum_type{std::get<EnumType>(
                                        graph_.types_[type.discriminant.type.value].definition)};
                                    auto const tag{std::ranges::find(
                                        enum_type.enumerators, alternative.tag, &Enumerator::name)};
                                    if (tag == enum_type.enumerators.end()) {
                                        throw std::invalid_argument{
                                            "Tagged union '" + source.name + "' alternative '" +
                                            alternative.name + "' maps unknown discriminant tag '" +
                                            alternative.tag + "'"};
                                    }
                                    if (tag->sentinel || tag->count_sentinel) {
                                        throw std::invalid_argument{
                                            "Tagged union '" + source.name + "' alternative '" +
                                            alternative.name + "' cannot map sentinel tag '" +
                                            alternative.tag + "'"};
                                    }
                                    type.alternatives.push_back(
                                        {.name = alternative.name,
                                         .semantic_type =
                                             resolve_ref(alternative.type, module.settings.name),
                                         .count = alternative.count,
                                         .tag = alternative.tag});
                                }
                                graph_.types_[declaration.id.value].definition = std::move(type);
                            }
                        } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                            auto const& source{module.structs[declaration.declaration_index]};
                            SoaType type{.backend = module.backend,
                                         .source_kind = SoaSourceKind::structure,
                                         .columns = {},
                                         .equivalent_type = source.equivalent_type.has_value()
                                                              ? std::optional{resolve_ref(
                                                                    *source.equivalent_type,
                                                                    module.settings.name)}
                                                              : std::nullopt,
                                         .related_storage_name = source.single_allocation};
                            type.columns.reserve(source.members.size());
                            for (auto const& member : source.members) {
                                std::optional<TypeId> nested;
                                if (member.nested_schema.has_value()) {
                                    nested = local_declaration(module.settings.name,
                                                               *member.nested_schema);
                                    if (!nested.has_value()) {
                                        throw std::invalid_argument{
                                            "Unknown nested semantic type '" +
                                            *member.nested_schema + "' in module '" +
                                            module.settings.name + "'"};
                                    }
                                }
                                auto relationship{std::optional<SemanticRelationship>{}};
                                if (member.relationship.has_value()) {
                                    auto target{resolve_ref(member.relationship->target,
                                                            module.settings.name)};
                                    if (std::holds_alternative<ExternalType>(
                                            graph_.type(target.type).definition)) {
                                        throw std::invalid_argument{
                                            "SOA '" + source.name + "' member '" + member.name +
                                            "' relationship target '" +
                                            member.relationship->target.name +
                                            "' must resolve to a declared semantic type"};
                                    }
                                    relationship =
                                        SemanticRelationship{.kind = member.relationship->kind,
                                                             .target = std::move(target),
                                                             .unit = member.relationship->unit};
                                }
                                type.columns.push_back({.name = member.name,
                                                        .semantic_type = resolve_ref(
                                                            member.type, module.settings.name),
                                                        .kind = member.kind,
                                                        .nested_type = nested,
                                                        .relationship = std::move(relationship)});
                            }
                            graph_.types_[declaration.id.value].definition = std::move(type);
                        } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                            SoaType type{.backend = module.backend,
                                         .source_kind = SoaSourceKind::vector,
                                         .columns = {},
                                         .equivalent_type = resolve_ref(module.equivalent_type,
                                                                        module.settings.name),
                                         .related_storage_name = std::nullopt};
                            auto const value_type{
                                resolve_ref(module.value_type, module.settings.name)};
                            type.columns.reserve(module.components.size());
                            for (auto const& component : module.components) {
                                type.columns.push_back(
                                    {.name = component, .semantic_type = value_type});
                            }
                            graph_.types_[declaration.id.value].definition = std::move(type);
                        }
                    },
                    module_schema);
            }
        }
    }

    void add_dependency(TypeNode& node, TypeId const dependency) {
        auto const own_id{static_cast<std::uint32_t>(std::addressof(node) - graph_.types_.data())};
        if (dependency.valid() && dependency.value != own_id &&
            std::ranges::find(node.dependencies, dependency) == node.dependencies.end()) {
            node.dependencies.push_back(dependency);
        }
    }

    void validate_aggregate_cycle(TypeId const type,
                                  std::vector<std::uint8_t>& states,
                                  std::vector<TypeId>& path) const {
        if (states[type.value] == 2) {
            return;
        }
        if (states[type.value] == 1) {
            auto message{std::string{"Illegal by-value aggregate cycle: "}};
            auto const start{std::ranges::find(path, type)};
            for (auto current{start}; current != path.end(); ++current) {
                message += graph_.type(*current).identity.name + " -> ";
            }
            message += graph_.type(type).identity.name;
            throw std::invalid_argument{std::move(message)};
        }

        states[type.value] = 1;
        path.push_back(type);
        std::visit(
            [&](auto const& aggregate) {
                using Aggregate = std::decay_t<decltype(aggregate)>;
                if constexpr (std::is_same_v<Aggregate, RecordType>) {
                    for (auto const& member : aggregate.members) {
                        auto const& definition{graph_.type(member.semantic_type.type).definition};
                        if (std::holds_alternative<RecordType>(definition) ||
                            std::holds_alternative<UnionType>(definition) ||
                            std::holds_alternative<TaggedUnionType>(definition)) {
                            validate_aggregate_cycle(member.semantic_type.type, states, path);
                        }
                    }
                } else if constexpr (std::is_same_v<Aggregate, UnionType>) {
                    for (auto const& alternative : aggregate.alternatives) {
                        auto const& definition{
                            graph_.type(alternative.semantic_type.type).definition};
                        if (std::holds_alternative<RecordType>(definition) ||
                            std::holds_alternative<UnionType>(definition) ||
                            std::holds_alternative<TaggedUnionType>(definition)) {
                            validate_aggregate_cycle(alternative.semantic_type.type, states, path);
                        }
                    }
                } else if constexpr (std::is_same_v<Aggregate, TaggedUnionType>) {
                    for (auto const& alternative : aggregate.alternatives) {
                        auto const& definition{
                            graph_.type(alternative.semantic_type.type).definition};
                        if (std::holds_alternative<RecordType>(definition) ||
                            std::holds_alternative<UnionType>(definition) ||
                            std::holds_alternative<TaggedUnionType>(definition)) {
                            validate_aggregate_cycle(alternative.semantic_type.type, states, path);
                        }
                    }
                }
            },
            graph_.type(type).definition);
        path.pop_back();
        states[type.value] = 2;
    }

    void validate_aggregate_cycles() const {
        std::vector<std::uint8_t> states(graph_.types_.size());
        std::vector<TypeId> path;
        for (std::uint32_t index{}; index < graph_.types_.size(); ++index) {
            auto const& definition{graph_.types_[index].definition};
            if (std::holds_alternative<RecordType>(definition) ||
                std::holds_alternative<UnionType>(definition) ||
                std::holds_alternative<TaggedUnionType>(definition)) {
                validate_aggregate_cycle(TypeId{index}, states, path);
            }
        }
    }

    void build_edges() {
        for (auto& node : graph_.types_) {
            std::visit(
                [&](auto const& definition) {
                    using Definition = std::decay_t<decltype(definition)>;
                    if constexpr (std::is_same_v<Definition, EnumType>) {
                        if (definition.underlying_type.has_value()) {
                            add_dependency(node, definition.underlying_type->type);
                        }
                    } else if constexpr (std::is_same_v<Definition, IntegerScalarType>) {
                        if (definition.relationship.has_value()) {
                            add_dependency(node, definition.relationship->target.type);
                        }
                    } else if constexpr (std::is_same_v<Definition, LinearQuantizedType>) {
                        add_dependency(node, definition.source.type);
                    } else if constexpr (std::is_same_v<Definition, IntegerVarintType>) {
                        add_dependency(node, definition.source.type);
                    } else if constexpr (std::is_same_v<Definition, OptionalSentinelType>) {
                        add_dependency(node, definition.source.type);
                    } else if constexpr (std::is_same_v<Definition, OptionalPresenceBitType>) {
                        add_dependency(node, definition.source.type);
                    } else if constexpr (std::is_same_v<Definition, PackedType>) {
                        add_dependency(node, definition.storage_type.type);
                        for (auto const& segment : definition.segments) {
                            if (auto const* field{std::get_if<PackedField>(&segment)}) {
                                add_dependency(node, field->semantic_type.type);
                                if (field->relationship.has_value()) {
                                    add_dependency(node, field->relationship->target.type);
                                }
                            }
                        }
                    } else if constexpr (std::is_same_v<Definition, RecordType>) {
                        for (auto const& member : definition.members) {
                            add_dependency(node, member.semantic_type.type);
                            if (member.relationship.has_value()) {
                                add_dependency(node, member.relationship->target.type);
                            }
                        }
                    } else if constexpr (std::is_same_v<Definition, UnionType>) {
                        for (auto const& alternative : definition.alternatives) {
                            add_dependency(node, alternative.semantic_type.type);
                        }
                    } else if constexpr (std::is_same_v<Definition, TaggedUnionType>) {
                        add_dependency(node, definition.discriminant.type);
                        for (auto const& alternative : definition.alternatives) {
                            add_dependency(node, alternative.semantic_type.type);
                        }
                    } else if constexpr (std::is_same_v<Definition, SoaType>) {
                        if (definition.equivalent_type.has_value()) {
                            add_dependency(node, definition.equivalent_type->type);
                        }
                        for (auto const& column : definition.columns) {
                            add_dependency(node, column.semantic_type.type);
                            if (column.nested_type.has_value()) {
                                add_dependency(node, *column.nested_type);
                            }
                            if (column.relationship.has_value()) {
                                add_dependency(node, column.relationship->target.type);
                            }
                        }
                    }
                },
                node.definition);
        }
        for (std::uint32_t index{}; index < graph_.types_.size(); ++index) {
            auto const user{TypeId{index}};
            for (auto const dependency : graph_.types_[index].dependencies) {
                graph_.types_[dependency.value].users.push_back(user);
            }
        }
    }

    codegen::Manifest const& manifest_;
    TypeGraph graph_;
    std::vector<Declaration> declarations_;
    std::map<std::pair<std::string, std::string>, TypeId> declarations_by_module_name_;
    std::map<std::string, std::vector<TypeId>, std::less<>> declarations_by_spelling_;
    std::map<std::string, std::vector<TypeId>, std::less<>> declarations_by_name_;
    std::map<std::string, TypeId, std::less<>> raw_external_types_;
};

auto TypeGraph::types() const -> std::span<TypeNode const> {
    return types_;
}

auto TypeGraph::type(TypeId const id) const -> TypeNode const& {
    if (!id.valid() || id.value >= types_.size()) {
        throw std::out_of_range{"Invalid semantic type id"};
    }
    return types_[id.value];
}

auto TypeGraph::find(TypeIdentity const& identity) const -> std::optional<TypeId> {
    auto const found{identities_.find(identity)};
    return found == identities_.end() ? std::nullopt : std::optional{found->second};
}

auto TypeGraph::find_declared(std::string const& module_name, std::string const& name) const
    -> std::optional<TypeId> {
    for (auto const& [identity, id] : identities_) {
        if (identity.origin == TypeOrigin::declaration && identity.module_name == module_name &&
            identity.name == name) {
            return id;
        }
    }
    return std::nullopt;
}

auto TypeGraph::find_registered(std::string const& name) const -> std::optional<TypeId> {
    auto const found{registered_types_.find(name)};
    return found == registered_types_.end() ? std::nullopt : std::optional{found->second};
}

auto TypeGraph::dependencies_of(TypeId const id) const -> std::span<TypeId const> {
    return type(id).dependencies;
}

auto TypeGraph::users_of(TypeId const id) const -> std::span<TypeId const> {
    return type(id).users;
}

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph {
    codegen::validate_manifest(manifest);
    return TypeGraphBuilder{manifest}.build();
}

} // namespace lispb::schema
