#pragma once

#include <ioj/sim/entity_identity_layout.h>
#include <sandbox/core/diagnostics.h>

#include <format>
#include <vector>

namespace ioj::sim {
class EntityIdAllocator {
  public:
    [[nodiscard]] auto allocate(EntityType const type, std::int32_t const history_row)
        -> EntityUniqueId {
        if (type >= EntityType::COUNT) {
            ml::fatal_error("Cannot allocate an entity ID with an invalid type");
        }
        auto& count{issued_counts_[type]};
        if (count == entity_lifetime_capacities[type]) {
            ml::fatal_error(
                std::format("Entity type {} exhausted its level lifetime ID budget ({})",
                            static_cast<unsigned>(type),
                            count));
        }
        auto const id{EntityUniqueId::make(entity_identity_offset(type, count), type)};
        history_rows_[type].push_back(history_row);
        ++count;
        return id;
    }

    [[nodiscard]] auto history_index(EntityUniqueId const id) const noexcept -> std::int32_t {
        if (!is_entity_identity_offset(id)) {
            return -1;
        }
        auto const type{id.entity_type()};
        auto const ordinal{id.index() - entity_identity_offsets[type]};
        return ordinal < issued_counts_[type] ? history_rows_[type][ordinal] : -1;
    }

    [[nodiscard]] auto issued_counts() const noexcept -> EntityTypeSizes const& {
        return issued_counts_;
    }

    void reset() noexcept {
        issued_counts_ = {};
        for (std::size_t i{}; i < EntityTypeSizes::size(); ++i) {
            history_rows_[static_cast<EntityType>(i)].clear();
        }
    }
  private:
    EntityTypeSizes issued_counts_{};
    ml::EnumArray<EntityType, std::vector<std::int32_t>, EntityTypeSizes::size()> history_rows_;
};
}
