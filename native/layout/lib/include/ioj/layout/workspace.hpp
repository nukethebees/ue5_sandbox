#pragma once

#include <ioj/layout/model.hpp>

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct FieldOverrideId {
    SchemaId schema;
    std::string field_name;

    auto operator<=>(FieldOverrideId const&) const = default;
};

struct VariantOverrides {
    std::map<SchemaId, std::string> packed_storage_types;
    std::map<FieldOverrideId, std::uint32_t> packed_field_widths;
    std::map<FieldOverrideId, std::string> soa_column_types;
    std::map<SchemaId, std::uint64_t> capacities;

    auto operator==(VariantOverrides const&) const -> bool = default;
};

struct Variant {
    std::uint64_t id{};
    std::string name;
    VariantOverrides overrides;
    std::uint64_t revision{};
};

class LayoutWorkspace {
  public:
    static constexpr std::uint64_t baseline_variant_id{0};

    explicit LayoutWorkspace(SchemaCatalog catalog = {}, std::uint64_t default_capacity = 65'536);

    auto catalog() const -> SchemaCatalog const&;
    auto default_capacity() const -> std::uint64_t;
    auto variants() const -> std::vector<Variant> const&;
    auto variant(std::uint64_t id) const -> Variant const*;
    auto active_variant() const -> Variant const&;
    auto active_variant_id() const -> std::uint64_t;
    auto revision() const -> std::uint64_t;

    auto select_variant(std::uint64_t id) -> bool;
    auto create_variant(std::string name) -> std::uint64_t;
    auto duplicate_variant(std::uint64_t source_id, std::string name)
        -> std::optional<std::uint64_t>;
    auto rename_variant(std::uint64_t id, std::string name) -> bool;
    auto reset_variant(std::uint64_t id) -> bool;
    auto delete_variant(std::uint64_t id) -> bool;

    auto set_packed_storage_type(SchemaId const& schema, std::optional<std::string> spelling)
        -> bool;
    auto set_packed_field_width(SchemaId const& schema,
                                std::string field_name,
                                std::optional<std::uint32_t> width) -> bool;
    auto set_soa_column_type(SchemaId const& schema,
                             std::string column_name,
                             std::optional<std::string> spelling) -> bool;
    auto set_capacity(SchemaId const& schema, std::optional<std::uint64_t> capacity) -> bool;
  private:
    auto editable_active_variant() -> Variant*;
    void note_change(Variant& variant);

    SchemaCatalog catalog_;
    std::uint64_t default_capacity_{};
    std::vector<Variant> variants_;
    std::uint64_t active_variant_id_{baseline_variant_id};
    std::uint64_t next_variant_id_{1};
    std::uint64_t revision_{1};
};

} // namespace ioj::layout
