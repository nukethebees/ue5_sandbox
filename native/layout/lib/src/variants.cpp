#include <ioj/layout/variants.hpp>

#include <algorithm>
#include <cassert>
#include <utility>

namespace ioj::layout {

auto Variants::size() const -> std::size_t {
    return ids_.size();
}
auto Variants::ids() const -> std::vector<VariantId> const& {
    return ids_;
}
auto Variants::contains(VariantId const id) const -> bool {
    return std::ranges::find(ids_, id) != ids_.end();
}
auto Variants::index(VariantId const id) const -> std::size_t {
    auto const found{std::ranges::find(ids_, id)};
    assert(found != ids_.end());
    return static_cast<std::size_t>(found - ids_.begin());
}
auto Variants::name(VariantId const id) const -> std::string const& {
    return names_[index(id)];
}
auto Variants::settings(VariantId const id) const -> VariantSettings const& {
    return settings_[index(id)];
}
auto Variants::edit(VariantId const id) -> VariantSettings& {
    assert(id != VariantId{});
    return settings_[index(id)];
}
void Variants::create(VariantId id, std::string name) {
    assert(!name.empty());
    assert(id.value != 0 && !contains(id));
    ids_.push_back(id);
    names_.push_back(std::move(name));
    settings_.emplace_back();
}
void Variants::duplicate(VariantId const source, VariantId id, std::string name) {
    auto copy{settings(source)};
    create(id, std::move(name));
    edit(id) = std::move(copy);
}
void Variants::rename(VariantId const id, std::string name) {
    assert(id != VariantId{} && !name.empty());
    names_[index(id)] = std::move(name);
}
void Variants::reset(VariantId const id) {
    edit(id) = {};
}
void Variants::erase(VariantId const id) {
    assert(id != VariantId{});
    auto const row{static_cast<std::ptrdiff_t>(index(id))};
    ids_.erase(ids_.begin() + row);
    names_.erase(names_.begin() + row);
    settings_.erase(settings_.begin() + row);
}
void Variants::set_packed_storage(VariantId const id,
                                  lispb::schema::DeclarationId const type,
                                  std::optional<std::string> value) {
    auto& columns{edit(id)};
    auto const found{std::ranges::find(columns.packed_types, type)};
    if (found == columns.packed_types.end()) {
        if (value) {
            columns.packed_types.push_back(type);
            columns.packed_storage.push_back(std::move(*value));
        }
        return;
    }
    auto const row{found - columns.packed_types.begin()};
    if (value) {
        columns.packed_storage[row] = std::move(*value);
    } else {
        columns.packed_types.erase(found);
        columns.packed_storage.erase(columns.packed_storage.begin() + row);
    }
}
void Variants::set_field_width(VariantId const id,
                               lispb::schema::DeclarationId const type,
                               lispb::IdentifierId member,
                               std::optional<std::uint32_t> const value) {
    auto& columns{edit(id)};
    auto const count{columns.field_types.size()};
    auto row{std::size_t{0}};
    for (; row < count; ++row) {
        if (columns.field_types[row] == type && columns.field_names[row] == member) {
            break;
        }
    }
    if (row == count) {
        if (value) {
            columns.field_types.push_back(type);
            columns.field_names.push_back(member);
            columns.field_widths.push_back(*value);
        }
        return;
    }
    if (value) {
        columns.field_widths[row] = *value;
    } else {
        auto const offset{static_cast<std::ptrdiff_t>(row)};
        columns.field_types.erase(columns.field_types.begin() + offset);
        columns.field_names.erase(columns.field_names.begin() + offset);
        columns.field_widths.erase(columns.field_widths.begin() + offset);
    }
}
void Variants::set_column_type(VariantId const id,
                               lispb::schema::DeclarationId const type,
                               lispb::IdentifierId member,
                               std::optional<std::string> value) {
    auto& columns{edit(id)};
    auto const count{columns.column_types.size()};
    auto row{std::size_t{0}};
    for (; row < count; ++row) {
        if (columns.column_types[row] == type && columns.column_names[row] == member) {
            break;
        }
    }
    if (row == count) {
        if (value) {
            columns.column_types.push_back(type);
            columns.column_names.push_back(member);
            columns.column_spelling.push_back(std::move(*value));
        }
        return;
    }
    if (value) {
        columns.column_spelling[row] = std::move(*value);
    } else {
        auto const offset{static_cast<std::ptrdiff_t>(row)};
        columns.column_types.erase(columns.column_types.begin() + offset);
        columns.column_names.erase(columns.column_names.begin() + offset);
        columns.column_spelling.erase(columns.column_spelling.begin() + offset);
    }
}
void Variants::set_capacity(VariantId const id,
                            lispb::schema::DeclarationId const type,
                            std::optional<std::uint64_t> const value) {
    auto& columns{edit(id)};
    auto const found{std::ranges::find(columns.capacity_types, type)};
    if (found == columns.capacity_types.end()) {
        if (value) {
            columns.capacity_types.push_back(type);
            columns.capacities.push_back(*value);
        }
        return;
    }
    auto const row{found - columns.capacity_types.begin()};
    if (value) {
        columns.capacities[row] = *value;
    } else {
        columns.capacity_types.erase(found);
        columns.capacities.erase(columns.capacities.begin() + row);
    }
}

void Variants::rename_member(lispb::schema::DeclarationId const type,
                             lispb::IdentifierId const old_name,
                             lispb::IdentifierId const new_name) {
    for (auto& columns : settings_) {
        auto const fields{columns.field_types.size()};
        for (std::size_t row{}; row < fields; ++row) {
            if (columns.field_types[row] == type && columns.field_names[row] == old_name) {
                columns.field_names[row] = new_name;
            }
        }
        auto const columns_count{columns.column_types.size()};
        for (std::size_t row{}; row < columns_count; ++row) {
            if (columns.column_types[row] == type && columns.column_names[row] == old_name) {
                columns.column_names[row] = new_name;
            }
        }
    }
}
void Variants::erase_member(lispb::schema::DeclarationId const type,
                            lispb::IdentifierId const name) {
    auto const count{ids_.size()};
    for (std::size_t row{1}; row < count; ++row) {
        set_field_width(ids_[row], type, name, std::nullopt);
        set_column_type(ids_[row], type, name, std::nullopt);
    }
}
void Variants::erase_declaration(lispb::schema::DeclarationId const type) {
    auto const count{ids_.size()};
    for (std::size_t variant_row{1}; variant_row < count; ++variant_row) {
        auto const id{ids_[variant_row]};
        set_packed_storage(id, type, std::nullopt);
        set_capacity(id, type, std::nullopt);
        auto& columns{settings_[variant_row]};
        for (auto row{columns.field_types.size()}; row > 0; --row) {
            if (columns.field_types[row - 1] == type) {
                set_field_width(id, type, columns.field_names[row - 1], std::nullopt);
            }
        }
        for (auto row{columns.column_types.size()}; row > 0; --row) {
            if (columns.column_types[row - 1] == type) {
                set_column_type(id, type, columns.column_names[row - 1], std::nullopt);
            }
        }
    }
}

}
