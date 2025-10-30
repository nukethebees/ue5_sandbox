#include "Sandbox/inventory/actor_components/InventoryComponent.h"

#include "Algo/RandomShuffle.h"

#include "Sandbox/combat/weapons/actors/AmmoItem.h"
#include "Sandbox/combat/weapons/actors/WeaponBase.h"
#include "Sandbox/utilities/enums.h"
#include "Sandbox/utilities/grids.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UInventoryComponent::UInventoryComponent() {}

bool UInventoryComponent::add_item(AActor& actor, TScriptInterface<IInventoryItem> item) {
    INIT_PTR_OR_RETURN_VALUE(inventory, actor.GetComponentByClass<UInventoryComponent>(), false);
    return inventory->add_item(item);
}
bool UInventoryComponent::add_item(TScriptInterface<IInventoryItem> item) {
    constexpr auto logger{NestedLogger<"add_item">()};
    auto const free_point{find_free_point(*item)};

    if (!free_point) {
        logger.log_warning(TEXT("Couldn't find a free spot for %s in the inventory."),
                           *item->get_name());
        return false;
    }

    FInventoryEntry item_entry{item, item->get_quantity(), item->get_size(), *free_point};
    auto const item_type{item->get_item_type()};

    logger.log_display(TEXT("Adding %s [%s] to %s"),
                       *item->get_name(),
                       *UEnum::GetValueAsString(item_type),
                       *free_point->to_string());

    item_entries.Add(item_entry);

    switch (item_type) {
        using enum EItemType;
        case Weapon: {
            auto* weapon{item->get_weapon()};
            check(weapon);
            on_weapon_added.ExecuteIfBound(*weapon);
            break;
        }
        case Junk: {
            break;
        }
        case Ammo: {
            break;
        }
        default: {
            logger.log_warning(TEXT("%s"), *ml::make_unhandled_enum_case_warning(item_type));
            break;
        }
    }

    return true;
}
bool UInventoryComponent::move_item(FInventoryEntry const& moving_slot,
                                    FCoord item_offset,
                                    FCoord drop_location) {
    constexpr auto logger{NestedLogger<"move_item">()};

    auto const new_origin{drop_location - item_offset};

#if !UE_BUILD_SHIPPING

    logger.log_verbose(TEXT("\nOriginal location: %s"
                            "\nDrop location: %s"
                            "\nClick offset: %s"
                            "\nNew location: %s"),
                       *moving_slot.origin.to_string(),
                       *drop_location.to_string(),
                       *item_offset.to_string(),
                       *new_origin.to_string());
#endif

    for (auto& item_entry : item_entries) {
        if (&item_entry != &moving_slot) {
            continue;
        }

        if (!is_free(new_origin, moving_slot.dimensions(), &moving_slot)) {
            return false;
        }

        item_entry.origin = new_origin;

        return true;
    }

    return false;
}
auto UInventoryComponent::find_free_point(IInventoryItem const& item) const
    -> std::optional<FCoord> {
    auto const size{item.get_size()};
    auto const n_rows{get_n_rows()};
    auto const n_cols{get_n_columns()};

    for (int32 row{0}; row < n_rows; row++) {
        for (int32 col{0}; col < n_cols; col++) {
            FCoord coord{col, row};
            if (is_free(coord, size)) {
                return coord;
            }
        }
    }

    return {};
}

auto UInventoryComponent::get_first_weapon() -> AWeaponBase* {
    auto const n_rows{get_n_rows()};
    auto const n_cols{get_n_columns()};

    for (int32 row{0}; row < n_rows; ++row) {
        for (int32 col{0}; col < n_cols; ++col) {
            FCoord const coord{col, row};
            if (auto* item{get_entry_at_origin(coord)}) {
                if (auto* weapon{item->item->get_weapon()}) {
                    return weapon;
                }
            }
        }
    }

    return nullptr;
}
auto UInventoryComponent::get_last_weapon() -> AWeaponBase* {
    auto const n_rows{get_n_rows()};
    auto const n_cols{get_n_columns()};

    check(n_rows > 0);
    check(n_cols > 0);

    for (int32 row{n_rows - 1}; row >= 0; --row) {
        for (int32 col{n_cols - 1}; col >= 0; --col) {
            FCoord const coord{col, row};
            if (auto* item{get_entry_at_origin(coord)}) {
                if (auto* weapon{item->item->get_weapon()}) {
                    return weapon;
                }
            }
        }
    }

    return nullptr;
}
auto UInventoryComponent::get_random_weapon() -> AWeaponBase* {
    log_display(TEXT("Getting random weapon."));

    TArray<int32> indexes;
    auto const n{item_entries.Num()};
    indexes.Reserve(n);
    for (int32 i{0}; i < n; ++i) {
        indexes.Add(i);
    }
    Algo::RandomShuffle(indexes);

    for (int32 i : indexes) {
        if (auto* weapon{item_entries[i].item->get_weapon()}) {
            return weapon;
        }
    }

    return nullptr;
}
auto UInventoryComponent::get_weapon_after(AWeaponBase const& cur_weapon) -> AWeaponBase* {
    return get_weapon_adjacent<ml::increment_index>(cur_weapon);
}
auto UInventoryComponent::get_weapon_before(AWeaponBase const& cur_weapon) -> AWeaponBase* {
    return get_weapon_adjacent<ml::decrement_index>(cur_weapon);
}
auto UInventoryComponent::request_ammo(FAmmoData ammo_needed) -> FAmmoData {
    FAmmoData out{ammo_needed.type};

    auto ammo_entries{item_entries.FilterByPredicate(
        [type_needed = ammo_needed.type](FInventoryEntry const& entry) -> bool {
            if (entry.item_type() != EItemType::Ammo) {
                return false;
            }

            if (auto* ammo{Cast<AAmmoItem>(entry.item.GetObject())}) {
                if (ammo->ammo_type == type_needed) {
                    return true;
                }
            }

            return true;
        })};

    for (auto& item_entry : item_entries) {
        if (item_entry.item_type() != EItemType::Ammo) {
            continue;
        }

        auto* ammo{Cast<AAmmoItem>(item_entry.item.GetObject())};
        if ((!ammo) || (ammo->ammo_type != ammo_needed.type)) {
            continue;
        }

        if (is_continuous(out.type)) {
            auto const ammo_taken{std::min(static_cast<int32>(ammo_needed.continuous_amount),
                                           item_entry.stack_size.get_value())};

            item_entry.stack_size -= FStackSize{ammo_taken};

            ammo_needed.continuous_amount -= static_cast<float>(ammo_taken);
            out.continuous_amount += static_cast<float>(ammo_taken);
        } else {
            auto const ammo_taken{
                std::min(ammo_needed.discrete_amount, item_entry.stack_size.get_value())};

            item_entry.stack_size -= FStackSize{ammo_taken};

            ammo_needed.discrete_amount -= ammo_taken;
            out.discrete_amount += ammo_taken;
        }

        if (ammo_needed.is_empty()) {
            break;
        }
    }

    remove_empty_entries();

    return out;
}

bool UInventoryComponent::is_free(FCoord coord,
                                  FDimensions item_dimensions,
                                  FInventoryEntry const* to_ignore) const {
    constexpr auto logger{NestedLogger<"is_free">()};

    auto const item_width{item_dimensions.x()};
    auto const item_height{item_dimensions.y()};

    auto const ax0{coord.x()};
    auto const ax1{ax0 + item_width};

    auto const ay0{coord.y()};
    auto const ay1{ay0 + item_height};

    if (ax0 < 0) {
        logger.log_verbose(TEXT("ax0 is negative. (%d)"), ax0);
        return false;
    }
    if (ay0 < 0) {
        logger.log_verbose(TEXT("ay0 is negative. (%d)"), ay0);
        return false;
    }

    auto const inventory_width{get_width()};
    auto const inventory_height{get_height()};

    if (ax1 > inventory_width) {
        logger.log_verbose(TEXT("ax1 beyond inventory width. (%d vs %d)"), ax1, inventory_width);
        return false;
    }
    if (ay1 > inventory_height) {
        logger.log_verbose(TEXT("ay1 beyond inventory height. (%d vs %d)"), ay1, inventory_height);
        return false;
    }

    for (auto const& item_entry : item_entries) {
        if (&item_entry == to_ignore) {
            continue;
        }

        // AABB collision
        // Check if A's left edge is to the left of B's right edge
        // and if A's right edge is to the right of B's left edge
        //
        // Check if A's top is above B's bottom and A's bottom is below B's top
        auto const bx{item_entry.origin.x()};
        auto const by{item_entry.origin.y()};

        auto const bw{item_entry.width()};
        auto const bh{item_entry.height()};

        auto const intersects{
            (ax0 < (bx + bw))    // A's left < B's right
            && (ax1 > bx)        // A's right > B's left
            && (ay0 < (by + bh)) // A's bottom < B's top
            && (ay1 > by)        // A's top > B's bottom
        };
        if (intersects) {
            logger.log_verbose(TEXT("Intersected with item_entry item %s"), *item_entry.name());
            return false;
        }
    }

    return true;
}
auto UInventoryComponent::get_entry_at_origin(FCoord coord) -> FInventoryEntry* {
    for (auto& item_entry : item_entries) {
        if (item_entry.origin == coord) {
            return &item_entry;
        }
    }

    return nullptr;
}
auto UInventoryComponent::get_entry(AWeaponBase const& weapon) -> FInventoryEntry* {
    for (auto& item_entry : item_entries) {
        if (auto* slot_weapon{item_entry.item->get_weapon()}; slot_weapon == &weapon) {
            return &item_entry;
        }
    }

    return nullptr;
}

template <auto next_index_fn>
auto UInventoryComponent::get_weapon_adjacent(AWeaponBase const& cur_weapon) -> AWeaponBase* {
    INIT_PTR_OR_RETURN_VALUE(item_entry, get_entry(cur_weapon), nullptr);

    auto const grid_dimensions{get_dimensions()};
    auto const grid_area{grid_dimensions.area()};
    auto const initial_coordinate{ml::to_1d_index(grid_dimensions, item_entry->origin)};
    auto coord_1d{next_index_fn(grid_area, ml::to_1d_index(grid_dimensions, item_entry->origin))};

    while (coord_1d != initial_coordinate) {
        auto const coord_2d{ml::to_coord(grid_dimensions, coord_1d)};

        if (auto* item{get_entry_at_origin(coord_2d)}) {
            if (auto* weapon{item->item->get_weapon()}) {
                return weapon;
            }
        }

        coord_1d = next_index_fn(grid_area, coord_1d);
    }

    return item_entry->item->get_weapon();
}
void UInventoryComponent::remove_empty_entries() {
    item_entries.RemoveAllSwap(
        [](FInventoryEntry const& entry) -> bool { return entry.stack_size.get_value() == 0; });
}
