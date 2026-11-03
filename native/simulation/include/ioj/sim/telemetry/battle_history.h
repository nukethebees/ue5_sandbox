#pragma once

#include <ioj/linked_vector.h>
#include <ioj/sim/telemetry/battle_change_mask.h>
#include <ioj/sim/telemetry/run_data.h>

#include <array>
#include <vector>

namespace ioj::sim::telemetry {
class BattleHistory {
    template <typename T, std::size_t Count = team_count * entity_type_count>
    using Columns = std::array<LinkedVector<T>, Count>;

  public:
    explicit BattleHistory(std::pmr::memory_resource& memory);
    BattleHistory(BattleHistory const&) = delete;
    BattleHistory(BattleHistory&&) = delete;
    auto operator=(BattleHistory const&) -> BattleHistory& = delete;
    auto operator=(BattleHistory&&) -> BattleHistory& = delete;

    // Require increasing ticks; a repeated tick is already sampled.
    void append(BattleSample const& sample);
    void reset();
    auto size() const noexcept -> std::size_t { return ticks_.size(); }
    auto materialize() const -> std::vector<BattleSample>;

  private:
    template <typename Func>
    void each_column(Func&& func) {
        auto const group{[&](auto& columns) {
            for (auto& column : columns) {
                func(column);
            }
        }};
        group(alive_);
        group(spawned_);
        group(destroyed_);
        group(shots_);
        group(hits_);
        group(damage_dealt_);
        group(kills_);
        func(active_lasers_);
        func(lasers_fired_);
        func(ticks_);
        func(masks_);
    }

    Columns<std::int32_t> alive_;
    Columns<std::uint64_t> spawned_;
    Columns<std::uint64_t> destroyed_;
    Columns<std::uint64_t> shots_;
    Columns<std::uint64_t> hits_;
    Columns<double> damage_dealt_;
    Columns<std::uint64_t> kills_;
    LinkedVector<std::int32_t> active_lasers_;
    LinkedVector<std::int32_t> lasers_fired_;
    LinkedVector<SimTick> ticks_;
    LinkedVector<BattleChangeMask> masks_;
};
}
