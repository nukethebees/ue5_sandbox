#pragma once

#include <lispb/schema/type_graph.h>

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct FieldOverrideId {
    lispb::schema::TypeId type;
    std::string field_name;

    auto operator<=>(FieldOverrideId const&) const = default;
};

struct VariantOverrides {
    std::map<lispb::schema::TypeId, std::string> packed_storage_types;
    std::map<FieldOverrideId, std::uint32_t> packed_field_widths;
    std::map<FieldOverrideId, std::string> soa_column_types;
    std::map<lispb::schema::TypeId, std::uint64_t> capacities;

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

    explicit LayoutWorkspace(lispb::schema::TypeGraph types = {},
                             std::uint64_t default_capacity = 65'536);

    auto types() const -> lispb::schema::TypeGraph const&;
    auto default_capacity() const -> std::uint64_t;
    auto element_count() const -> std::uint64_t;
    auto variants() const -> std::vector<Variant> const&;
    auto variant(std::uint64_t id) const -> Variant const*;
    auto active_variant() const -> Variant const&;
    auto active_variant_id() const -> std::uint64_t;
    auto revision() const -> std::uint64_t;
    auto graph_revision() const -> std::uint64_t;
    void replace_types(lispb::schema::TypeGraph types);

    auto select_variant(std::uint64_t id) -> bool;
    auto create_variant(std::string name) -> std::uint64_t;
    auto duplicate_variant(std::uint64_t source_id, std::string name)
        -> std::optional<std::uint64_t>;
    auto rename_variant(std::uint64_t id, std::string name) -> bool;
    auto reset_variant(std::uint64_t id) -> bool;
    auto delete_variant(std::uint64_t id) -> bool;

    auto set_packed_storage_type(lispb::schema::TypeId type, std::optional<std::string> spelling)
        -> bool;
    auto set_packed_field_width(lispb::schema::TypeId type,
                                std::string field_name,
                                std::optional<std::uint32_t> width) -> bool;
    auto set_soa_column_type(lispb::schema::TypeId type,
                             std::string column_name,
                             std::optional<std::string> spelling) -> bool;
    auto set_capacity(lispb::schema::TypeId type, std::optional<std::uint64_t> capacity) -> bool;
    auto set_element_count(std::uint64_t count) -> bool;
  private:
    auto editable_active_variant() -> Variant*;
    void note_change(Variant& variant);

    lispb::schema::TypeGraph types_;
    std::uint64_t default_capacity_{};
    std::uint64_t element_count_{1};
    std::vector<Variant> variants_;
    std::uint64_t active_variant_id_{baseline_variant_id};
    std::uint64_t next_variant_id_{1};
    std::uint64_t revision_{1};
    std::uint64_t graph_revision_{1};
};

} // namespace ioj::layout
