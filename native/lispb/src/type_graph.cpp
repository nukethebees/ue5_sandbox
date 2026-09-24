#include <lispb/schema/type_graph.h>

#include "packed_value_internal.h"

#include <codegen/schema/fixed_point_value.h>
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

auto integer_domain(TypeNode const& node) -> IntegerScalarType const* {
    if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
        return scalar;
    }
    if (auto const* external{std::get_if<ExternalType>(&node.definition)}) {
        return std::get_if<IntegerScalarType>(&external->semantics);
    }
    return nullptr;
}

auto packed_integer_domain(TypeNode const& node, codegen::PackedFieldSchema const& field)
    -> IntegerScalarType const* {
    if (field.bits.has_value() && !std::holds_alternative<IntegerScalarType>(node.definition)) {
        return nullptr;
    }
    return integer_domain(node);
}

class TypeGraphBuilder {
  public:
    explicit TypeGraphBuilder(codegen::Manifest const& manifest)
        : manifest_{manifest} {}

    auto build() -> TypeGraph {
        declare_types();
        bind_registered_types();
        resolve_definitions();
        resolve_type_uses();
        validate_nested_soa_types();
        validate_vector_equivalents();
        validate_aggregate_cycles();
        build_edges();
        return std::move(graph_);
    }
  private:
    void validate_nested_soa_types() const {
        std::set<TypeId> visited;
        auto visit = [&](auto&& self, TypeId const id) -> void {
            if (!visited.insert(id).second) {
                return;
            }
            auto const& soa{std::get<SoaType>(graph_.type(id).definition)};
            for (auto const& column : soa.columns) {
                if (!column.nested_type.has_value()) {
                    continue;
                }
                auto const& nested{graph_.type(*column.nested_type)};
                if (column.semantic_type.cpp_type.spelling != nested.identity.name) {
                    throw std::invalid_argument{"Nested SOA type does not match nested_schema: " +
                                                column.name};
                }
                self(self, *column.nested_type);
            }
        };
        for (std::size_t index{}; index < graph_.types_.size(); ++index) {
            auto const* soa{std::get_if<SoaType>(&graph_.types_[index].definition)};
            if (soa != nullptr && soa->source_kind == SoaSourceKind::structure &&
                soa->related_storage_name.has_value()) {
                visit(visit, TypeId{static_cast<std::uint32_t>(index)});
            }
        }
    }

    void validate_vector_equivalents() const {
        std::map<TypeId, SoaType const*> declared_vectors;
        for (auto const& node : graph_.types_) {
            auto const* soa{std::get_if<SoaType>(&node.definition)};
            if (soa != nullptr && soa->source_kind == SoaSourceKind::vector &&
                soa->equivalent_type.has_value()) {
                declared_vectors.emplace(soa->equivalent_type->type, soa);
            }
        }
        for (auto const& node : graph_.types_) {
            auto const* soa{std::get_if<SoaType>(&node.definition)};
            if (soa == nullptr || soa->source_kind != SoaSourceKind::structure ||
                soa->vector_components.empty() || !soa->equivalent_type.has_value()) {
                continue;
            }
            auto const found{declared_vectors.find(soa->equivalent_type->type)};
            if (found == declared_vectors.end()) {
                continue;
            }
            auto const& declared{*found->second};
            if (soa->vector_components != declared.vector_components ||
                soa->columns.size() != declared.columns.size()) {
                throw std::invalid_argument{"SOA '" + node.identity.name +
                                            "' vector components disagree with its equivalent "
                                            "vector declaration"};
            }
            for (std::size_t index{}; index < soa->columns.size(); ++index) {
                if (soa->columns[index].semantic_type.type !=
                    declared.columns[index].semantic_type.type) {
                    throw std::invalid_argument{"SOA '" + node.identity.name +
                                                "' vector element type disagrees with its "
                                                "equivalent vector declaration"};
                }
            }
        }
    }

    struct Declaration {
        TypeId id;
        std::size_t module_index{};
        std::size_t declaration_index{};
        std::optional<std::size_t> homogeneous_value_index;
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
            auto const* module{
                std::get_if<codegen::NormalModuleSchema>(&manifest_.modules[module_index])};
            if (module == nullptr) {
                continue;
            }
            for (std::size_t index{}; index < module->declarations.size(); ++index) {
                auto const& source{module->declarations[index]};
                if (auto const* layout{std::get_if<codegen::HomogeneousLayoutSchema>(&source)}) {
                    for (std::size_t value_index{}; value_index < layout->value_types.size();
                         ++value_index) {
                        declare(module_index,
                                index,
                                module->settings,
                                "F" + layout->name + layout->value_types[value_index].suffix,
                                HomogeneousStorageType{});
                        auto& generated{declarations_.back()};
                        generated.homogeneous_value_index = value_index;
                        graph_.types_[generated.id.value].owning_declaration = TypeIdentity{
                            .module_name = module->settings.name,
                            .namespace_name = module->settings.namespace_name.value_or(""),
                            .name = layout->name};
                    }
                    continue;
                }
                if (!codegen::has_primary_semantic_type(source)) {
                    continue;
                }
                auto definition{std::visit(
                    [&](auto const& value) -> TypeDefinition {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, codegen::EnumSchema>) {
                            return EnumType{};
                        } else if constexpr (std::is_same_v<T, codegen::IntegerScalarSchema>) {
                            return IntegerScalarType{};
                        } else if constexpr (std::is_same_v<T, codegen::LinearQuantizedSchema>) {
                            return LinearQuantizedType{};
                        } else if constexpr (std::is_same_v<T, codegen::IntegerVarintSchema>) {
                            return IntegerVarintType{};
                        } else if constexpr (std::is_same_v<T, codegen::FixedPointSchema>) {
                            return FixedPointType{};
                        } else if constexpr (std::is_same_v<T, codegen::MiniFloatSchema>) {
                            return MiniFloatType{};
                        } else if constexpr (std::is_same_v<T, codegen::OptionalSentinelSchema>) {
                            return OptionalSentinelType{};
                        } else if constexpr (std::is_same_v<T,
                                                            codegen::OptionalPresenceBitSchema>) {
                            return OptionalPresenceBitType{};
                        } else if constexpr (std::is_same_v<T, codegen::PackedValueSchema>) {
                            return PackedType{};
                        } else if constexpr (std::is_same_v<T, codegen::RecordSchema>) {
                            return RecordType{};
                        } else if constexpr (std::is_same_v<T, codegen::UnionSchema>) {
                            return UnionType{};
                        } else if constexpr (std::is_same_v<T, codegen::TaggedUnionSchema>) {
                            return TaggedUnionType{};
                        } else if constexpr (std::is_same_v<T, codegen::StaticTableSchema>) {
                            return StaticTableType{};
                        } else if constexpr (std::is_same_v<T, codegen::FacadeSchema>) {
                            return FacadeType{};
                        } else {
                            return SoaType{.backend = module->soa_backend,
                                           .source_kind =
                                               std::is_same_v<T, codegen::VectorSoaSchema>
                                                   ? SoaSourceKind::vector
                                                   : SoaSourceKind::structure};
                        }
                    },
                    source)};
                declare(module_index,
                        index,
                        module->settings,
                        codegen::declaration_name(source),
                        std::move(definition));
            }
        }
    }

    auto external_type(codegen::CppType cpp_type, std::vector<std::string> names = {})
        -> ExternalType {
        ExternalType result{.cpp_type = std::move(cpp_type), .registered_names = std::move(names)};
        auto const* metadata{
            codegen::external_scalar_schema(manifest_.types, result.cpp_type.spelling)};
        if (metadata == nullptr) {
            return result;
        }
        if (result.registered_names.empty()) {
            for (auto const& [name, registered] : manifest_.types) {
                if (codegen::native_spelling(registered.cpp_type.spelling) ==
                    codegen::native_spelling(result.cpp_type.spelling)) {
                    result.registered_names.push_back(name);
                }
            }
        }
        if (auto const* scalar{std::get_if<codegen::ExternalIntegerSchema>(metadata)}) {
            IntegerScalarType domain{.signedness = scalar->signedness,
                                     .minimum_value = scalar->minimum_value,
                                     .maximum_value = scalar->maximum_value,
                                     .bit_width = scalar->bit_width,
                                     .bit_width_auto = false};
            for (auto const& code : scalar->named_codes) {
                domain.named_codes.push_back({code.name, code.value, code.sentinel});
            }
            result.semantics = std::move(domain);
        } else if (auto const* format{std::get_if<codegen::FloatingPointFormat>(metadata)}) {
            result.semantics = *format;
        }
        return result;
    }

    void bind_registered_types() {
        for (auto const& [name, registered] : manifest_.types) {
            auto const& cpp_type{registered.cpp_type};
            auto const separator{cpp_type.spelling.rfind("::")};
            auto const declared_name{separator == std::string::npos
                                         ? cpp_type.spelling
                                         : cpp_type.spelling.substr(separator + 2)};
            if (auto const local{declarations_by_module_name_.find(std::pair{name, declared_name})};
                local != declarations_by_module_name_.end()) {
                if (!std::holds_alternative<std::monostate>(registered.semantics)) {
                    throw std::invalid_argument{"External scalar registration '@" + name +
                                                "' matches a LispB declaration"};
                }
                graph_.registered_types_.emplace(name, local->second);
                continue;
            }

            auto declarations{declarations_by_spelling_.find(cpp_type.spelling)};
            auto const* matches{
                declarations == declarations_by_spelling_.end() ? nullptr : &declarations->second};
            if (matches == nullptr) {
                auto const by_name{declarations_by_name_.find(cpp_type.spelling)};
                if (by_name != declarations_by_name_.end()) {
                    if (by_name->second.size() == 1) {
                        matches = &by_name->second;
                    }
                }
            }
            if (matches != nullptr) {
                if (!std::holds_alternative<std::monostate>(registered.semantics)) {
                    throw std::invalid_argument{"External scalar registration '@" + name +
                                                "' matches a LispB declaration"};
                }
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
                         external_type(cpp_type, {name}))};
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
                               external_type(codegen::CppType{spelling}))};
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

    void resolve_type_uses() {
        for (auto const& module : manifest_.modules) {
            std::visit(
                [&](auto const& schema) {
                    using T = std::decay_t<decltype(schema)>;
                    auto const& settings{schema.settings};
                    auto add = [&](std::optional<TypeIdentity> const& owner,
                                   std::string const& role,
                                   codegen::TypeRef const& reference) {
                        graph_.type_uses_.push_back(
                            {.module_name = settings.name,
                             .declaration = owner,
                             .role = role,
                             .target = resolve_ref(reference, settings.name)});
                    };
                    if constexpr (std::is_same_v<T, codegen::NormalModuleSchema>) {
                        for (auto const& declaration : schema.declarations) {
                            TypeIdentity const owner{
                                .module_name = settings.name,
                                .namespace_name = settings.namespace_name.value_or(""),
                                .name = codegen::declaration_name(declaration)};
                            codegen::visit_type_references(
                                declaration, [&](auto const& role, auto const& reference) {
                                    add(owner, role, reference);
                                });
                            auto registration = [&](std::string const& role,
                                                    std::string const& name) {
                                if (manifest_.types.contains(name)) {
                                    add(owner, role, codegen::TypeRef{"@" + name});
                                }
                            };
                            if (auto const* facade{
                                    std::get_if<codegen::FacadeSchema>(&declaration)}) {
                                for (auto const& name : facade->validation_dependencies) {
                                    registration("validation dependency", name);
                                }
                            }
                            if (auto const* soa{std::get_if<codegen::SoaSchema>(&declaration)}) {
                                for (auto const& function : soa->functions) {
                                    for (auto const& name : function.dependencies) {
                                        registration("function " + function.name + " dependency",
                                                     name);
                                    }
                                }
                                for (auto const& function : soa->mutable_view_functions) {
                                    for (auto const& name : function.dependencies) {
                                        registration(
                                            "view function " + function.name + " dependency", name);
                                    }
                                }
                            }
                        }
                        for (auto const& allocator : schema.soa_array_allocators) {
                            add(std::nullopt,
                                "module allocator " + allocator.prefix,
                                allocator.allocator);
                        }
                    } else if constexpr (std::is_same_v<T, codegen::SettingsModuleSchema>) {
                        for (auto const& setting : schema.settings_list) {
                            add(std::nullopt, "setting " + setting.name, setting.value_type);
                        }
                    }
                },
                module);
        }
    }

    auto resolve_normal_definition(codegen::NormalModuleSchema const& module,
                                   codegen::DeclarationSchema const& declaration)
        -> TypeDefinition {
        auto const& module_name{module.settings.name};
        return std::visit(
            [&](auto const& source) -> TypeDefinition {
                using T = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<T, codegen::EnumSchema>) {
                    EnumType type{.underlying_type = source.underlying_type.has_value()
                                                       ? std::optional{resolve_ref(
                                                             *source.underlying_type, module_name)}
                                                       : std::nullopt,
                                  .bit_width = source.bit_width,
                                  .signedness = source.signedness,
                                  .enumerators = {},
                                  .count = source.count};
                    type.enumerators.reserve(source.values.size());
                    for (auto const& value : source.values) {
                        type.enumerators.push_back({.name = value.name,
                                                    .explicit_value = value.initializer,
                                                    .display_name = value.display_name,
                                                    .serialized_name = value.serialized_name,
                                                    .hidden = value.hidden,
                                                    .sentinel = value.sentinel,
                                                    .count_sentinel = source.count.has_value() &&
                                                                      value.name == *source.count});
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::IntegerScalarSchema>) {
                    auto required_minimum{source.minimum_value};
                    auto required_maximum{source.maximum_value};
                    IntegerScalarType type{.signedness = source.signedness,
                                           .minimum_value = source.minimum_value,
                                           .maximum_value = source.maximum_value,
                                           .bit_width = 0,
                                           .bit_width_auto = !source.bit_width.has_value(),
                                           .named_codes = {},
                                           .relationship = std::nullopt};
                    for (auto const& code : source.named_codes) {
                        type.named_codes.push_back(
                            {.name = code.name, .value = code.value, .sentinel = code.sentinel});
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
                        auto target{resolve_ref(source.relationship->target, module_name)};
                        if (graph_.types_[target.type.value].identity.origin !=
                            TypeOrigin::declaration) {
                            throw std::invalid_argument{
                                "Integer scalar relationship target '" +
                                source.relationship->target.name +
                                "' must resolve to a declared semantic type"};
                        }
                        type.relationship = SemanticRelationship{.kind = source.relationship->kind,
                                                                 .target = std::move(target),
                                                                 .unit = source.relationship->unit};
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::LinearQuantizedSchema>) {
                    auto scalar{resolve_ref(source.source, module_name)};
                    if (integer_domain(graph_.types_[scalar.type.value]) == nullptr) {
                        throw std::invalid_argument{"Linear quantization '" + source.name +
                                                    "' source must have a declared integer domain"};
                    }
                    return LinearQuantizedType{.source = std::move(scalar),
                                               .bit_width = source.bit_width,
                                               .reserved_codes = source.reserved_codes,
                                               .clipping = source.clipping};
                } else if constexpr (std::is_same_v<T, codegen::IntegerVarintSchema>) {
                    auto scalar{resolve_ref(source.source, module_name)};
                    if (integer_domain(graph_.types_[scalar.type.value]) == nullptr) {
                        throw std::invalid_argument{"Integer varint '" + source.name +
                                                    "' source must have a declared integer domain"};
                    }
                    return IntegerVarintType{.source = std::move(scalar),
                                             .encoding = source.encoding};
                } else if constexpr (std::is_same_v<T, codegen::FixedPointSchema>) {
                    return FixedPointType{
                        .signedness = source.signedness,
                        .total_bits = source.total_bits,
                        .fractional_bits = source.fractional_bits,
                        .rounding = source.rounding,
                        .minimum_raw_value = source.minimum_value.transform([&](auto const& value) {
                            return *codegen::parse_fixed_point_value(value, source.fractional_bits);
                        }),
                        .maximum_raw_value = source.maximum_value.transform([&](auto const& value) {
                            return *codegen::parse_fixed_point_value(value, source.fractional_bits);
                        })};
                } else if constexpr (std::is_same_v<T, codegen::MiniFloatSchema>) {
                    return MiniFloatType{.sign_bits = source.sign_bits,
                                         .exponent_bits = source.exponent_bits,
                                         .significand_bits = source.significand_bits,
                                         .exponent_bias = source.exponent_bias};
                } else if constexpr (std::is_same_v<T, codegen::OptionalSentinelSchema>) {
                    auto scalar_ref{resolve_ref(source.source, module_name)};
                    auto const* scalar{integer_domain(graph_.types_[scalar_ref.type.value])};
                    if (scalar == nullptr) {
                        throw std::invalid_argument{"Optional sentinel representation '" +
                                                    source.name +
                                                    "' source must have a declared integer domain"};
                    }
                    auto const sentinel{std::ranges::find(
                        scalar->named_codes, source.sentinel, &PackedNamedCode::name)};
                    if (sentinel == scalar->named_codes.end() || !sentinel->sentinel) {
                        throw std::invalid_argument{"Optional sentinel representation '" +
                                                    source.name +
                                                    "' must name a source sentinel code"};
                    }
                    return OptionalSentinelType{.source = std::move(scalar_ref),
                                                .sentinel_name = source.sentinel,
                                                .sentinel_value = sentinel->value,
                                                .bit_width = scalar->bit_width};
                } else if constexpr (std::is_same_v<T, codegen::OptionalPresenceBitSchema>) {
                    auto scalar_ref{resolve_ref(source.source, module_name)};
                    auto const* scalar{integer_domain(graph_.types_[scalar_ref.type.value])};
                    if (scalar == nullptr) {
                        throw std::invalid_argument{"Optional presence-bit representation '" +
                                                    source.name +
                                                    "' source must have a declared integer domain"};
                    }
                    return OptionalPresenceBitType{.source = std::move(scalar_ref),
                                                   .payload_bits = scalar->bit_width,
                                                   .encoded_bits = scalar->bit_width + 1};
                } else if constexpr (std::is_same_v<T, codegen::PackedValueSchema>) {
                    PackedType type{.storage_type = resolve_ref(source.storage_type, module_name),
                                    .segments = {},
                                    .invalid_raw_value = source.invalid_value,
                                    .byte_order = source.byte_order,
                                    .bit_order = source.bit_order.value_or(
                                        codegen::PackedBitOrder::least_significant_first)};
                    type.segments.reserve(source.segments.size());
                    for (auto const& segment : source.segments) {
                        std::visit(
                            [&](auto const& value) {
                                using Segment = std::decay_t<decltype(value)>;
                                if constexpr (std::is_same_v<Segment, codegen::PackedFieldSchema>) {
                                    auto const scalar{codegen::detail::find_packed_integer_domain(
                                        value, manifest_.types, manifest_.modules, module_name)};
                                    PackedField field{
                                        .name = value.name,
                                        .semantic_type = resolve_ref(value.type, module_name),
                                        .bit_width = static_cast<std::uint32_t>(
                                            *codegen::detail::derive_packed_field_width(
                                                value, manifest_.types, manifest_.modules)),
                                        .bit_width_auto = !value.bits.has_value(),
                                        .kind = value.kind,
                                        .range_helper = value.range_helper,
                                        .minimum_value = scalar.has_value()
                                                           ? std::optional{scalar->minimum_value}
                                                           : value.minimum_value,
                                        .maximum_value = scalar.has_value()
                                                           ? std::optional{scalar->maximum_value}
                                                           : value.maximum_value,
                                        .named_codes = {},
                                        .relationship = std::nullopt};
                                    auto const& codes{scalar.has_value() ? scalar->named_codes
                                                                         : value.named_codes};
                                    for (auto const& code : codes) {
                                        field.named_codes.push_back({.name = code.name,
                                                                     .value = code.value,
                                                                     .sentinel = code.sentinel});
                                    }
                                    if (value.relationship.has_value()) {
                                        auto target{
                                            resolve_ref(value.relationship->target, module_name)};
                                        if (graph_.types_[target.type.value].identity.origin !=
                                            TypeOrigin::declaration) {
                                            throw std::invalid_argument{
                                                "Packed field relationship target '" +
                                                value.relationship->target.name +
                                                "' must resolve to a declared semantic type"};
                                        }
                                        field.relationship =
                                            SemanticRelationship{.kind = value.relationship->kind,
                                                                 .target = std::move(target),
                                                                 .unit = value.relationship->unit};
                                    }
                                    type.segments.emplace_back(std::move(field));
                                } else {
                                    type.segments.emplace_back(PackedReservedBits{
                                        .name = value.name,
                                        .bit_width = static_cast<std::uint32_t>(value.bits)});
                                }
                            },
                            segment);
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::RecordSchema>) {
                    RecordType type;
                    type.members.reserve(source.members.size());
                    for (auto const& member : source.members) {
                        auto relationship{std::optional<SemanticRelationship>{}};
                        if (member.relationship.has_value()) {
                            auto target{resolve_ref(member.relationship->target, module_name)};
                            if (graph_.types_[target.type.value].identity.origin !=
                                TypeOrigin::declaration) {
                                throw std::invalid_argument{
                                    "Record member relationship target '" +
                                    member.relationship->target.name +
                                    "' must resolve to a declared semantic type"};
                            }
                            relationship = SemanticRelationship{.kind = member.relationship->kind,
                                                                .target = std::move(target),
                                                                .unit = member.relationship->unit};
                        }
                        type.members.push_back(
                            {.name = member.name,
                             .semantic_type = resolve_ref(member.type, module_name),
                             .count = member.count,
                             .relationship = std::move(relationship)});
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::UnionSchema>) {
                    UnionType type;
                    type.alternatives.reserve(source.alternatives.size());
                    for (auto const& alternative : source.alternatives) {
                        type.alternatives.push_back(
                            {.name = alternative.name,
                             .semantic_type = resolve_ref(alternative.type, module_name),
                             .count = alternative.count});
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::TaggedUnionSchema>) {
                    auto discriminant{resolve_ref(source.discriminant, module_name)};
                    if (!std::holds_alternative<EnumType>(
                            graph_.types_[discriminant.type.value].definition)) {
                        throw std::invalid_argument{
                            "Tagged union '" + source.name +
                            "' discriminant must resolve to an enum declaration"};
                    }
                    TaggedUnionType type{.discriminant = std::move(discriminant),
                                         .alternatives = {}};
                    for (auto const& alternative : source.alternatives) {
                        auto const& enumeration{std::get<EnumType>(
                            graph_.types_[type.discriminant.type.value].definition)};
                        auto const tag{std::ranges::find(
                            enumeration.enumerators, alternative.tag, &Enumerator::name)};
                        if (tag == enumeration.enumerators.end()) {
                            throw std::invalid_argument{"Tagged union '" + source.name +
                                                        "' alternative '" + alternative.name +
                                                        "' maps unknown discriminant tag '" +
                                                        alternative.tag + "'"};
                        }
                        if (tag->sentinel || tag->count_sentinel) {
                            throw std::invalid_argument{"Tagged union '" + source.name +
                                                        "' alternative '" + alternative.name +
                                                        "' cannot map sentinel tag '" +
                                                        alternative.tag + "'"};
                        }
                        type.alternatives.push_back(
                            {.name = alternative.name,
                             .semantic_type = resolve_ref(alternative.type, module_name),
                             .count = alternative.count,
                             .tag = alternative.tag});
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::SoaSchema>) {
                    SoaType type{.backend = module.soa_backend,
                                 .source_kind = SoaSourceKind::structure,
                                 .columns = {},
                                 .equivalent_type = source.equivalent_type.has_value()
                                                      ? std::optional{resolve_ref(
                                                            *source.equivalent_type, module_name)}
                                                      : std::nullopt,
                                 .related_storage_name = source.single_allocation,
                                 .vector_components = source.vector_components};
                    type.columns.reserve(source.members.size());
                    for (auto const& member : source.members) {
                        std::optional<TypeId> nested;
                        if (member.nested_schema.has_value()) {
                            nested = local_declaration(module_name, *member.nested_schema);
                            if (!nested.has_value()) {
                                throw std::invalid_argument{"Unknown nested semantic type '" +
                                                            *member.nested_schema +
                                                            "' in module '" + module_name + "'"};
                            }
                        }
                        auto relationship{std::optional<SemanticRelationship>{}};
                        if (member.relationship.has_value()) {
                            auto target{resolve_ref(member.relationship->target, module_name)};
                            if (std::holds_alternative<ExternalType>(
                                    graph_.type(target.type).definition)) {
                                throw std::invalid_argument{
                                    "SOA '" + source.name + "' member '" + member.name +
                                    "' relationship target '" + member.relationship->target.name +
                                    "' must resolve to a declared semantic type"};
                            }
                            relationship = SemanticRelationship{.kind = member.relationship->kind,
                                                                .target = std::move(target),
                                                                .unit = member.relationship->unit};
                        }
                        type.columns.push_back(
                            {.name = member.name,
                             .semantic_type = resolve_ref(member.type, module_name),
                             .kind = member.kind,
                             .nested_type = nested,
                             .relationship = std::move(relationship)});
                    }
                    if (!type.vector_components.empty()) {
                        if (type.columns.empty()) {
                            throw std::invalid_argument{"SOA '" + source.name +
                                                        "' vector components require columns"};
                        }
                        auto const element{type.columns.front().semantic_type.type};
                        for (auto const& column : type.columns) {
                            if (column.semantic_type.type != element) {
                                throw std::invalid_argument{
                                    "SOA '" + source.name +
                                    "' vector components must have the same resolved type"};
                            }
                        }
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::VectorSoaSchema>) {
                    SoaType type{.backend = module.soa_backend,
                                 .source_kind = SoaSourceKind::vector,
                                 .columns = {},
                                 .equivalent_type =
                                     resolve_ref(source.equivalent_type, module_name),
                                 .related_storage_name = std::nullopt,
                                 .vector_components = source.components};
                    auto const value_type{resolve_ref(source.value_type, module_name)};
                    type.columns.reserve(source.components.size());
                    for (auto const& component : source.components) {
                        type.columns.push_back({.name = component, .semantic_type = value_type});
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::StaticTableSchema>) {
                    StaticTableType type;
                    for (auto const& row : source.rows) {
                        type.rows.push_back(row.name);
                    }
                    for (auto const& column : source.columns) {
                        type.columns.push_back(
                            {.name = column.name,
                             .semantic_type = resolve_ref(column.type, module_name),
                             .count = source.rows.size()});
                    }
                    for (auto const& group : source.groups) {
                        type.groups.push_back({.name = group.name,
                                               .result_type = resolve_ref(group.type, module_name),
                                               .columns = group.columns});
                    }
                    return type;
                } else if constexpr (std::is_same_v<T, codegen::FacadeSchema>) {
                    FacadeType type{.target = resolve_ref(source.target_type, module_name),
                                    .target_member_name = source.target_member_name,
                                    .reference_target = source.reference_target};
                    for (auto const& method : source.methods) {
                        FacadeMethod resolved{
                            .name = method.name,
                            .return_type = resolve_ref(method.return_type, module_name),
                            .target_name = method.target_name.value_or(method.name),
                            .is_const = method.is_const,
                            .is_noexcept = method.is_noexcept};
                        for (auto const& parameter : method.parameters) {
                            resolved.parameters.push_back(
                                {.name = parameter.name,
                                 .type = resolve_ref(parameter.type, module_name),
                                 .default_value = parameter.default_value});
                        }
                        type.methods.push_back(std::move(resolved));
                    }
                    return type;
                } else {
                    throw std::logic_error{"Nonsemantic declaration has no TypeGraph definition"};
                }
            },
            declaration);
    }

    void resolve_definitions() {
        for (auto const primary_pass : {true, false}) {
            for (auto const& declaration : declarations_) {
                auto const& module{std::get<codegen::NormalModuleSchema>(
                    manifest_.modules[declaration.module_index])};
                auto const& source{module.declarations[declaration.declaration_index]};
                auto const primary_definition{
                    std::holds_alternative<codegen::EnumSchema>(source) ||
                    std::holds_alternative<codegen::IntegerScalarSchema>(source)};
                if (primary_definition == primary_pass) {
                    if (declaration.homogeneous_value_index.has_value()) {
                        auto const& layout{std::get<codegen::HomogeneousLayoutSchema>(source)};
                        auto const& value{layout.value_types[*declaration.homogeneous_value_index]};
                        HomogeneousStorageType type{
                            .components = layout.components,
                            .value_type = resolve_ref(value.type, module.settings.name),
                            .input_members = layout.input_members,
                            .view_template_name = "T" + layout.name + "View"};
                        if (value.equivalent_type.has_value()) {
                            type.equivalent_type =
                                resolve_ref(*value.equivalent_type, module.settings.name);
                        }
                        for (auto const& input : value.input_types) {
                            type.input_types.push_back(resolve_ref(input, module.settings.name));
                        }
                        graph_.types_[declaration.id.value].definition = std::move(type);
                        continue;
                    }
                    graph_.types_[declaration.id.value].definition =
                        resolve_normal_definition(module, source);
                }
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
                            std::holds_alternative<TaggedUnionType>(definition) ||
                            std::holds_alternative<StaticTableType>(definition)) {
                            validate_aggregate_cycle(member.semantic_type.type, states, path);
                        }
                    }
                } else if constexpr (std::is_same_v<Aggregate, UnionType>) {
                    for (auto const& alternative : aggregate.alternatives) {
                        auto const& definition{
                            graph_.type(alternative.semantic_type.type).definition};
                        if (std::holds_alternative<RecordType>(definition) ||
                            std::holds_alternative<UnionType>(definition) ||
                            std::holds_alternative<TaggedUnionType>(definition) ||
                            std::holds_alternative<StaticTableType>(definition)) {
                            validate_aggregate_cycle(alternative.semantic_type.type, states, path);
                        }
                    }
                } else if constexpr (std::is_same_v<Aggregate, TaggedUnionType>) {
                    for (auto const& alternative : aggregate.alternatives) {
                        auto const& definition{
                            graph_.type(alternative.semantic_type.type).definition};
                        if (std::holds_alternative<RecordType>(definition) ||
                            std::holds_alternative<UnionType>(definition) ||
                            std::holds_alternative<TaggedUnionType>(definition) ||
                            std::holds_alternative<StaticTableType>(definition)) {
                            validate_aggregate_cycle(alternative.semantic_type.type, states, path);
                        }
                    }
                } else if constexpr (std::is_same_v<Aggregate, StaticTableType>) {
                    for (auto const& column : aggregate.columns) {
                        validate_aggregate_cycle(column.semantic_type.type, states, path);
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
                std::holds_alternative<TaggedUnionType>(definition) ||
                std::holds_alternative<StaticTableType>(definition)) {
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
                    } else if constexpr (std::is_same_v<Definition, StaticTableType>) {
                        for (auto const& column : definition.columns) {
                            add_dependency(node, column.semantic_type.type);
                        }
                        for (auto const& group : definition.groups) {
                            add_dependency(node, group.result_type.type);
                        }
                    } else if constexpr (std::is_same_v<Definition, FacadeType>) {
                        add_dependency(node, definition.target.type);
                        for (auto const& method : definition.methods) {
                            add_dependency(node, method.return_type.type);
                            for (auto const& parameter : method.parameters) {
                                add_dependency(node, parameter.type.type);
                            }
                        }
                    } else if constexpr (std::is_same_v<Definition, HomogeneousStorageType>) {
                        add_dependency(node, definition.value_type.type);
                        if (definition.equivalent_type.has_value()) {
                            add_dependency(node, definition.equivalent_type->type);
                        }
                        for (auto const& input : definition.input_types) {
                            add_dependency(node, input.type);
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

auto TypeGraph::type_uses() const -> std::span<TypeUse const> {
    return type_uses_;
}

auto TypeGraph::types_for_declaration(TypeIdentity const& identity) const -> std::vector<TypeId> {
    std::vector<TypeId> result;
    for (std::size_t index{}; index < types_.size(); ++index) {
        auto const& node{types_[index]};
        if (node.owning_declaration.value_or(node.identity) == identity) {
            result.push_back(TypeId{static_cast<std::uint32_t>(index)});
        }
    }
    return result;
}

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph {
    codegen::validate_manifest(manifest);
    return TypeGraphBuilder{manifest}.build();
}

} // namespace lispb::schema
