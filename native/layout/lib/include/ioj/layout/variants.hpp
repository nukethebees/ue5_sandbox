#pragma once

#include <ioj/layout/variant_settings.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::layout {

// Value state only: the owning Document supplies transaction history.
class Variants {
  public:
    static constexpr std::uint64_t baseline_id{0};

    auto size() const -> std::size_t;
    auto ids() const -> std::vector<std::uint64_t> const&;
    auto contains(std::uint64_t id) const -> bool;
    auto name(std::uint64_t id) const -> std::string const&;
    auto settings(std::uint64_t id) const -> VariantSettings const&;
    auto create(std::string name) -> std::uint64_t;
    auto duplicate(std::uint64_t source, std::string name) -> std::uint64_t;
    void rename(std::uint64_t id, std::string name);
    void reset(std::uint64_t id);
    void erase(std::uint64_t id);
    void set_packed_storage(std::uint64_t id,
                            lispb::schema::DeclarationId type,
                            std::optional<std::string> value);
    void set_field_width(std::uint64_t id,
                         lispb::schema::DeclarationId type,
                         std::string member,
                         std::optional<std::uint32_t> value);
    void set_column_type(std::uint64_t id,
                         lispb::schema::DeclarationId type,
                         std::string member,
                         std::optional<std::string> value);
    void set_capacity(std::uint64_t id,
                      lispb::schema::DeclarationId type,
                      std::optional<std::uint64_t> value);
    void rename_member(lispb::schema::DeclarationId type,
                       std::string_view old_name,
                       std::string_view new_name);
    void erase_member(lispb::schema::DeclarationId type, std::string_view name);
    void erase_declaration(lispb::schema::DeclarationId type);
    auto operator==(Variants const&) const -> bool = default;
  private:
    auto index(std::uint64_t id) const -> std::size_t;
    auto edit(std::uint64_t id) -> VariantSettings&;
    std::vector<std::uint64_t> ids_{baseline_id};
    std::vector<std::string> names_{"Baseline"};
    std::vector<VariantSettings> settings_{1};
    std::uint64_t next_id_{1};
};

}
