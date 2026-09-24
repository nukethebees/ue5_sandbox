#pragma once

#include <codegen/schema.h>

#include <compare>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace lispb::schema {

enum class TypeOrigin { declaration, registered_external, cpp_spelling };

struct TypeIdentity {
    TypeOrigin origin{TypeOrigin::declaration};
    std::string module_name;
    std::string namespace_name;
    std::string name;

    auto operator<=>(TypeIdentity const&) const = default;
};

struct TypeId {
    inline static constexpr std::uint32_t invalid_value{std::numeric_limits<std::uint32_t>::max()};

    std::uint32_t value{invalid_value};

    [[nodiscard]] auto valid() const -> bool { return value != invalid_value; }
    auto operator<=>(TypeId const&) const = default;
};

struct ResolvedTypeRef {
    TypeId type;
    codegen::CppType cpp_type;
};

struct TypeUse {
    std::string module_name;
    std::optional<TypeIdentity> declaration;
    std::string role;
    ResolvedTypeRef target;
};

struct Enumerator {
    std::string name;
    std::optional<std::string> explicit_value;
    std::optional<std::string> display_name;
    std::optional<std::string> serialized_name;
    bool hidden{};
    bool sentinel{};
    bool count_sentinel{};
};

struct EnumType {
    std::optional<ResolvedTypeRef> underlying_type;
    std::optional<std::uint32_t> bit_width;
    std::optional<bool> signedness;
    std::vector<Enumerator> enumerators;
    std::optional<std::string> count;
};

struct PackedNamedCode {
    std::string name;
    codegen::PackedIntegerValue value;
    bool sentinel{};
};

struct SemanticRelationship {
    codegen::SemanticRelationKind kind{codegen::SemanticRelationKind::references};
    ResolvedTypeRef target;
    std::optional<codegen::SemanticRelationUnit> unit;
};

struct PackedField {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::uint32_t bit_width{};
    bool bit_width_auto{};
    codegen::PackedFieldKind kind{codegen::PackedFieldKind::unsigned_integer};
    bool range_helper{};
    std::optional<codegen::PackedIntegerValue> minimum_value;
    std::optional<codegen::PackedIntegerValue> maximum_value;
    std::vector<PackedNamedCode> named_codes;
    std::optional<SemanticRelationship> relationship;
    std::optional<codegen::PackedIntegerValue> default_value{};
};

struct PackedReservedBits {
    std::string name;
    std::uint32_t bit_width{};
};

using PackedSegment = std::variant<PackedField, PackedReservedBits>;

struct PackedType {
    ResolvedTypeRef storage_type;
    std::vector<PackedSegment> segments;
    std::optional<std::uint64_t> invalid_raw_value;
    std::optional<codegen::PackedByteOrder> byte_order;
    codegen::PackedBitOrder bit_order{codegen::PackedBitOrder::least_significant_first};
    std::optional<std::uint64_t> default_raw_value{};
};

struct RecordMember {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::optional<std::uint64_t> count;
    std::optional<SemanticRelationship> relationship;
};

struct RecordType {
    std::vector<RecordMember> members;
};

struct UnionAlternative {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::optional<std::uint64_t> count;
};

struct UnionType {
    std::vector<UnionAlternative> alternatives;
};

struct TaggedUnionAlternative {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::optional<std::uint64_t> count;
    std::string tag;
};

struct TaggedUnionType {
    ResolvedTypeRef discriminant;
    std::vector<TaggedUnionAlternative> alternatives;
};

struct IntegerScalarType {
    bool signedness{};
    codegen::PackedIntegerValue minimum_value;
    codegen::PackedIntegerValue maximum_value;
    std::uint32_t bit_width{};
    bool bit_width_auto{};
    std::vector<PackedNamedCode> named_codes;
    std::optional<SemanticRelationship> relationship;
    std::optional<ResolvedTypeRef> cpp_representation{};
};

struct ExternalType {
    codegen::CppType cpp_type;
    std::vector<std::string> registered_names;
    std::variant<std::monostate, IntegerScalarType, codegen::FloatingPointFormat> semantics;
};

struct LinearQuantizedType {
    ResolvedTypeRef source;
    std::uint32_t bit_width{};
    std::uint64_t reserved_codes{};
    codegen::QuantizationClipping clipping{codegen::QuantizationClipping::reject};
};

struct IntegerVarintType {
    ResolvedTypeRef source;
    codegen::IntegerVarintEncoding encoding{codegen::IntegerVarintEncoding::unsigned_varint};
};

struct FixedPointType {
    bool signedness{};
    std::uint32_t total_bits{};
    std::uint32_t fractional_bits{};
    codegen::FixedPointRounding rounding{codegen::FixedPointRounding::nearest_even};
    std::optional<codegen::PackedIntegerValue> minimum_raw_value;
    std::optional<codegen::PackedIntegerValue> maximum_raw_value;
};

struct MiniFloatType {
    std::uint32_t sign_bits{};
    std::uint32_t exponent_bits{};
    std::uint32_t significand_bits{};
    std::int32_t exponent_bias{};
};

struct OptionalSentinelType {
    ResolvedTypeRef source;
    std::string sentinel_name;
    codegen::PackedIntegerValue sentinel_value;
    std::uint32_t bit_width{};
};

struct OptionalPresenceBitType {
    ResolvedTypeRef source;
    std::uint32_t payload_bits{};
    std::uint32_t encoded_bits{};
};

enum class SoaSourceKind { structure, vector };

struct SoaColumn {
    std::string name;
    ResolvedTypeRef semantic_type;
    codegen::SoaMemberKind kind{codegen::SoaMemberKind::array};
    std::optional<TypeId> nested_type;
    std::optional<SemanticRelationship> relationship;
};

struct SoaType {
    codegen::SoaBackend backend{codegen::SoaBackend::unreal};
    SoaSourceKind source_kind{SoaSourceKind::structure};
    std::vector<SoaColumn> columns;
    std::optional<ResolvedTypeRef> equivalent_type;
    std::optional<std::string> related_storage_name;
    std::vector<std::string> vector_components;
};

struct StaticTableGroup {
    std::string name;
    ResolvedTypeRef result_type;
    std::vector<std::string> columns;
};

struct StaticTableType {
    std::vector<std::string> rows;
    std::vector<RecordMember> columns;
    std::vector<StaticTableGroup> groups;
};

struct FacadeParameter {
    std::string name;
    ResolvedTypeRef type;
    std::optional<std::string> default_value;
};

struct FacadeMethod {
    std::string name;
    ResolvedTypeRef return_type;
    std::vector<FacadeParameter> parameters;
    std::string target_name;
    bool is_const{};
    bool is_noexcept{};
};

struct FacadeType {
    ResolvedTypeRef target;
    std::string target_member_name;
    bool reference_target{};
    std::vector<FacadeMethod> methods;
};

struct HomogeneousStorageType {
    std::vector<std::string> components;
    ResolvedTypeRef value_type;
    std::optional<ResolvedTypeRef> equivalent_type;
    std::vector<std::string> input_members;
    std::vector<ResolvedTypeRef> input_types;
    std::string view_template_name;
};

using TypeDefinition = std::variant<ExternalType,
                                    EnumType,
                                    IntegerScalarType,
                                    LinearQuantizedType,
                                    IntegerVarintType,
                                    FixedPointType,
                                    MiniFloatType,
                                    OptionalSentinelType,
                                    OptionalPresenceBitType,
                                    PackedType,
                                    RecordType,
                                    UnionType,
                                    TaggedUnionType,
                                    SoaType,
                                    StaticTableType,
                                    FacadeType,
                                    HomogeneousStorageType>;

struct TypeNode {
    TypeIdentity identity;
    std::string cpp_spelling;
    TypeDefinition definition;
    std::vector<TypeId> dependencies;
    std::vector<TypeId> users;
    std::optional<TypeIdentity> owning_declaration;
};

auto integer_domain(TypeNode const& node) -> IntegerScalarType const*;
auto packed_integer_domain(TypeNode const& node, codegen::PackedFieldSchema const& field)
    -> IntegerScalarType const*;

class TypeGraphBuilder;

class TypeGraph {
  public:
    auto types() const -> std::span<TypeNode const>;
    auto type(TypeId id) const -> TypeNode const&;
    auto find(TypeIdentity const& identity) const -> std::optional<TypeId>;
    auto find_declared(std::string const& module_name, std::string const& name) const
        -> std::optional<TypeId>;
    auto find_registered(std::string const& name) const -> std::optional<TypeId>;
    // Uses the resolver's registered/local/qualified lookup without creating a raw external type.
    auto find_reference(codegen::TypeRef const& reference, std::string const& module_name) const
        -> std::optional<TypeId>;
    auto dependencies_of(TypeId id) const -> std::span<TypeId const>;
    auto users_of(TypeId id) const -> std::span<TypeId const>;
    auto type_uses() const -> std::span<TypeUse const>;
    auto types_for_declaration(TypeIdentity const& identity) const -> std::vector<TypeId>;
  private:
    friend class TypeGraphBuilder;
    friend auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph;

    std::vector<TypeNode> types_;
    std::vector<TypeUse> type_uses_;
    std::map<TypeIdentity, TypeId> identities_;
    std::map<std::string, TypeId, std::less<>> registered_types_;
    std::map<std::pair<std::string, std::string>, TypeId> declarations_by_module_name_;
    std::map<std::string, std::vector<TypeId>, std::less<>> declarations_by_spelling_;
};

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph;

} // namespace lispb::schema
