#pragma once

#include <ioj/layout/variant_id.hpp>
#include <ioj/layout/variant_settings.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

// Authored variants in SoA form. Member identifiers belong to the owning Document's strings.
class Variants {
  public:
    auto size() const -> std::size_t;
    auto ids() const -> std::vector<VariantId> const&;
    auto contains(VariantId id) const -> bool;
    auto name(VariantId id) const -> std::string const&;
    auto settings(VariantId id) const -> VariantSettings const&;
    void create(VariantId id, std::string name);
    void duplicate(VariantId source, VariantId id, std::string name);
    void rename(VariantId id, std::string name);
    void reset(VariantId id);
    void erase(VariantId id);
    void set_packed_storage(VariantId id,
                            lispb::schema::DeclarationId type,
                            std::optional<std::string> value);
    void set_field_width(VariantId id,
                         lispb::schema::DeclarationId type,
                         lispb::IdentifierId member,
                         std::optional<std::uint32_t> value);
    void set_column_type(VariantId id,
                         lispb::schema::DeclarationId type,
                         lispb::IdentifierId member,
                         std::optional<std::string> value);
    void set_capacity(VariantId id,
                      lispb::schema::DeclarationId type,
                      std::optional<std::uint64_t> value);
    void rename_member(lispb::schema::DeclarationId type,
                       lispb::IdentifierId old_name,
                       lispb::IdentifierId new_name);
    void erase_member(lispb::schema::DeclarationId type, lispb::IdentifierId name);
    void erase_declaration(lispb::schema::DeclarationId type);
    auto operator==(Variants const&) const -> bool = default;
  private:
    auto index(VariantId id) const -> std::size_t;
    auto edit(VariantId id) -> VariantSettings&;
    std::vector<VariantId> ids_{VariantId{}};
    std::vector<std::string> names_{"Baseline"};
    std::vector<VariantSettings> settings_{1};
};

}
