#pragma once

#include <ioj/sim/entity_identity_layout.h>

#include <sandbox/core/diagnostics.h>

#include <format>
#include <limits>
#include <vector>

namespace ioj::sim {
class EntityIdAllocator {
  public:
    inline static constexpr std::uint32_t invalid_index{std::numeric_limits<std::uint32_t>::max()};
    [[nodiscard]] auto allocate(EntityType const type, std::uint32_t const history_row)
        -> EntityUniqueId {
        if (std::to_underlying(type) >= ml::enum_count<EntityType>()) {
            ml::fatal_error("Cannot allocate an entity ID with an invalid type");
        }
        auto& count{issued_counts_[type]};
        if (count == entity_lifetime_capacities[type]) {
            ml::fatal_error(
                std::format("Entity type {} exhausted its level lifetime ID budget ({})",
                            static_cast<unsigned>(type),
                            count));
        }
        auto const id{EntityUniqueId(count, type)};
        history_rows_[type].push_back(history_row);
        ++count;
        return id;
    }

    [[nodiscard]] auto history_index(EntityUniqueId const id) const noexcept -> std::uint32_t {
        if (!is_entity_identity_offset(id)) {
            return invalid_index;
        }
        auto const type{id.entity_type()};
        auto const ordinal{id.index()};
        return ordinal < issued_counts_[type] ? history_rows_[type][ordinal] : invalid_index;
    }

    [[nodiscard]] auto issued_counts() const noexcept -> EntityTypeSizes const& {
        return issued_counts_;
    }

    void reset() noexcept {
        issued_counts_ = {};
        auto const entity_type_count{EntityTypeSizes::size()};
        for (std::size_t i{}; i < entity_type_count; ++i) {
            history_rows_[static_cast<EntityType>(i)].clear();
        }
    }
  private:
    EntityTypeSizes issued_counts_{};
    ml::EnumArray<EntityType, std::vector<std::uint32_t>> history_rows_;
};
}
