#pragma once

#include <codegen/schema.h>

#include <compare>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
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

struct ExternalType {
    codegen::CppType cpp_type;
    std::vector<std::string> registered_names;
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
                                    SoaType>;

struct TypeNode {
    TypeIdentity identity;
    std::string cpp_spelling;
    TypeDefinition definition;
    std::vector<TypeId> dependencies;
    std::vector<TypeId> users;
};

class TypeGraphBuilder;

class TypeGraph {
  public:
    auto types() const -> std::span<TypeNode const>;
    auto type(TypeId id) const -> TypeNode const&;
    auto find(TypeIdentity const& identity) const -> std::optional<TypeId>;
    auto find_declared(std::string const& module_name, std::string const& name) const
        -> std::optional<TypeId>;
    auto find_registered(std::string const& name) const -> std::optional<TypeId>;
    auto dependencies_of(TypeId id) const -> std::span<TypeId const>;
    auto users_of(TypeId id) const -> std::span<TypeId const>;
  private:
    friend class TypeGraphBuilder;
    friend auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph;

    std::vector<TypeNode> types_;
    std::map<TypeIdentity, TypeId> identities_;
    std::map<std::string, TypeId, std::less<>> registered_types_;
};

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph;

} // namespace lispb::schema
