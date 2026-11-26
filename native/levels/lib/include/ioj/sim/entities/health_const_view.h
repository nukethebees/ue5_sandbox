#pragma once

#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/health.h>

#include <cassert>
#include <cstddef>
#include <span>

namespace ioj::sim {
namespace health_storage {
[[nodiscard]] inline auto index(EntityId const id,
                                [[maybe_unused]] EntityType const type,
                                [[maybe_unused]] std::size_t const capacity) noexcept
    -> std::size_t {
    assert(id.is_valid() && id.entity_type() == type && id.index() < capacity);
    return id.index();
}
}

class HealthConstView {
  public:
    HealthConstView() = default;
    explicit HealthConstView(EntityType const type, std::span<Health const> const values) noexcept
        : type_{type}
        , values_{values} {}

    [[nodiscard]] auto is_empty() const noexcept -> bool { return values_.empty(); }
    [[nodiscard]] auto values() const noexcept -> std::span<Health const> { return values_; }
    [[nodiscard]] auto health(EntityId const id) const -> Health {
        return values_[health_storage::index(id, type_, values_.size())];
    }
    void copy_to(std::span<EntityId const> const ids, std::span<Health> const output) const {
        assert(output.size() == ids.size());
        auto const count{ids.size()};
        for (std::size_t index{}; index < count; ++index) {
            output[index] = health(ids[index]);
        }
    }
  private:
    EntityType type_{};
    std::span<Health const> values_;
};

}
