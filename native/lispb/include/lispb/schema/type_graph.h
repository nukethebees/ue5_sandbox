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
    bool count_sentinel{};
};

struct EnumType {
    ResolvedTypeRef underlying_type;
    std::vector<Enumerator> enumerators;
    std::optional<std::string> count;
};

struct PackedField {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::uint32_t bit_width{};
    codegen::PackedFieldKind kind{codegen::PackedFieldKind::unsigned_integer};
    bool range_helper{};
};

struct PackedType {
    ResolvedTypeRef storage_type;
    std::vector<PackedField> fields;
    std::optional<std::uint64_t> invalid_raw_value;
};

enum class SoaSourceKind { structure, vector };

struct SoaColumn {
    std::string name;
    ResolvedTypeRef semantic_type;
    codegen::SoaMemberKind kind{codegen::SoaMemberKind::array};
    std::optional<TypeId> nested_type;
};

struct SoaType {
    codegen::SoaBackend backend{codegen::SoaBackend::unreal};
    SoaSourceKind source_kind{SoaSourceKind::structure};
    std::vector<SoaColumn> columns;
    std::optional<std::string> related_storage_name;
};

using TypeDefinition = std::variant<ExternalType, EnumType, PackedType, SoaType>;

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
