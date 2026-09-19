#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ioj::layout {

enum class SchemaKind { packed_value, standard_library_soa };

struct SchemaId {
    SchemaKind kind{SchemaKind::packed_value};
    std::string module_name;
    std::string schema_name;

    auto operator<=>(SchemaId const&) const = default;
};

enum class PackedFieldKind { unsigned_integer, enumeration };

struct PackedField {
    std::string name;
    std::string logical_type;
    std::uint32_t bit_width{};
    PackedFieldKind kind{PackedFieldKind::unsigned_integer};
};

struct PackedLayout {
    SchemaId id;
    std::string storage_type;
    std::vector<PackedField> fields;
    std::optional<std::uint64_t> invalid_raw_value;
};

struct SoaColumn {
    std::string name;
    std::string logical_type;
};

struct SoaLayout {
    SchemaId id;
    std::vector<SoaColumn> columns;
};

using LayoutDefinition = std::variant<PackedLayout, SoaLayout>;

class SchemaCatalog {
  public:
    auto add(LayoutDefinition definition) -> bool;
    auto find(SchemaId const& id) const -> LayoutDefinition const*;
    auto items() const -> std::vector<LayoutDefinition> const&;
  private:
    std::vector<LayoutDefinition> items_;
};

} // namespace ioj::layout
