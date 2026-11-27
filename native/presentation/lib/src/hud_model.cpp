#include <ioj/presentation/hud_model.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ioj::presentation {
namespace hud_detail {
template <typename T>
auto replace(T& current, T const& next) -> bool {
    auto const changed{current != next};
    current = next;
    return changed;
}
}

void HudModel::initialise(sim::EntityCounts const& counts,
                          std::optional<PlayerFrame> const& player) {
    deactivate();
    tick_loop_.initialise();
    std::array const periods{settings_.player_status_update_period,
                             settings_.entity_count_update_period};
    for (auto const period : periods) {
        auto const ticks{std::ceil(period.get() * tick_loop_.tick_rate.hz())};
        if (ticks < 1 || ticks > std::numeric_limits<PeriodicTickCountdown8::counter_type>::max()) {
            throw std::invalid_argument{"HUD update period is outside the countdown range"};
        }
        update_timers_.add_started(PeriodicTickCountdown8::period_type::make(
            static_cast<PeriodicTickCountdown8::counter_type>(ticks)));
    }
    active_ = true;
    force_sample(counts, player);
}
void HudModel::deactivate() {
    active_ = false;
    update_timers_.reset();
    entity_count_data_ = {};
    player_data_.reset();
}
auto HudModel::advance(double const delta_seconds,
                       sim::EntityCounts const& counts,
                       std::optional<PlayerFrame> const& player) -> HudChanges {
    HudChanges changes;
    if (!active()) {
        return changes;
    }
    tick_loop_.add_time(delta_seconds);
    while (tick_loop_.try_tick()) {
        auto const next{tick(1, counts, player)};
        changes.entity_counts |= next.entity_counts;
        changes.player_status |= next.player_status;
        changes.player_flight |= next.player_flight;
        changes.sampled_speed |= next.sampled_speed;
    }
    return changes;
}
auto HudModel::tick(PeriodicTickCountdown8::counter_type const num_ticks,
                    sim::EntityCounts const& counts,
                    std::optional<PlayerFrame> const& player) -> HudChanges {
    assert(num_ticks >= 0);
    HudChanges changes;
    if (!active()) {
        return changes;
    }
    assert(player_data_.has_value() == player.has_value());
    update_timers_.tick(num_ticks);
    auto const sample_status{update_timers_.try_consume(0)};
    if (player) {
        changes.player_flight = hud_detail::replace(player_data_->flight, player->flight);
        changes.sampled_speed = num_ticks > 0;
        if (sample_status) {
            changes.player_status = hud_detail::replace(player_data_->status, player->status);
        }
    }
    if (update_timers_.try_consume(1)) {
        changes.entity_counts = hud_detail::replace(entity_count_data_, counts);
    }
    return changes;
}
auto HudModel::force_sample(sim::EntityCounts const& counts,
                            std::optional<PlayerFrame> const& player) -> HudChanges {
    if (!active()) {
        return {};
    }
    HudChanges changes{.entity_counts = hud_detail::replace(entity_count_data_, counts)};
    if (player) {
        if (!player_data_) {
            player_data_.emplace(player->status, player->flight);
            changes.player_status = true;
            changes.player_flight = true;
        } else {
            changes.player_status = hud_detail::replace(player_data_->status, player->status);
            changes.player_flight = hud_detail::replace(player_data_->flight, player->flight);
        }
        changes.sampled_speed = true;
    }
    return changes;
}
auto HudModel::seconds_per_tick() const -> float {
    return static_cast<float>(tick_loop_.get_tick_period());
}

}
