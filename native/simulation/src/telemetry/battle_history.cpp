#include <ioj/sim/telemetry/battle_history.h>

#include <cassert>
#include <limits>
#include <type_traits>

namespace ioj::sim::telemetry {
namespace battle_history_detail {
template <typename Value>
void append_value(Value const value, LinkedVector<Value>& column, bool& changed) {
    changed = column.empty() || value != column.back();
    if (changed) {
        column.emplace_back(value);
    }
}

template <typename Values, typename Columns, typename Mask>
void append_group(Values const& values, Columns& columns, Mask& mask) {
    static_assert(std::tuple_size_v<Columns> <= std::numeric_limits<Mask>::digits);
    std::size_t index{};
    for (auto const value : values) {
        bool changed{};
        append_value(value, columns[index], changed);
        if (changed) {
            mask |= Mask{1} << index;
        }
        ++index;
    }
}

template <typename T, std::size_t Count>
auto cursors(std::array<LinkedVector<T>, Count> const& columns) {
    std::array<typename LinkedVector<T>::const_iterator, Count> result;
    for (std::size_t index{}; index < Count; ++index) {
        result[index] = columns[index].begin();
    }
    return result;
}

template <typename Values, typename Cursors, typename Mask>
void read_group(Values& values, Cursors& cursors, Mask const mask) {
    std::size_t index{};
    for (auto& value : values) {
        if ((mask & (Mask{1} << index)) != 0) {
            value = *cursors[index]++;
        }
        ++index;
    }
}
}

BattleHistory::BattleHistory(std::pmr::memory_resource& memory) {
    each_column([&]<typename T>(LinkedVector<T>& column) { column = LinkedVector<T>{memory}; });
}
void BattleHistory::append(BattleSample const& sample) {
    using namespace battle_history_detail;
    if (!ticks_.empty()) {
        assert(sample.completed_tick >= ticks_.back());
        if (sample.completed_tick == ticks_.back()) {
            return;
        }
    }

    BattleChangeMask mask;
    auto const& combat{sample.combat};
    append_group(sample.alive, alive_, mask.alive);
    append_group(combat.spawned, spawned_, mask.spawned);
    append_group(combat.destroyed, destroyed_, mask.destroyed);
    append_group(combat.shots, shots_, mask.shots);
    append_group(combat.hits, hits_, mask.hits);
    append_group(combat.damage_dealt, damage_dealt_, mask.damage_dealt);
    append_group(combat.kills, kills_, mask.kills);
    append_value(sample.active_lasers, active_lasers_, mask.active_lasers);
    append_value(sample.lasers_fired, lasers_fired_, mask.lasers_fired);
    masks_.emplace_back(mask);
    ticks_.emplace_back(sample.completed_tick);
}
void BattleHistory::reset() {
    each_column([](auto& column) { column.clear(); });
}
auto BattleHistory::materialize() const -> std::vector<BattleSample> {
    using namespace battle_history_detail;
    auto alive{cursors(alive_)};
    auto spawned{cursors(spawned_)};
    auto destroyed{cursors(destroyed_)};
    auto shots{cursors(shots_)};
    auto hits{cursors(hits_)};
    auto damage_dealt{cursors(damage_dealt_)};
    auto kills{cursors(kills_)};
    auto active_lasers{active_lasers_.begin()};
    auto lasers_fired{lasers_fired_.begin()};
    auto masks{masks_.begin()};
    std::vector<BattleSample> result;
    result.reserve(size());
    BattleSample sample;
    for (auto const tick : ticks_) {
        auto const mask{*masks++};
        read_group(sample.alive, alive, mask.alive);
        read_group(sample.combat.spawned, spawned, mask.spawned);
        read_group(sample.combat.destroyed, destroyed, mask.destroyed);
        read_group(sample.combat.shots, shots, mask.shots);
        read_group(sample.combat.hits, hits, mask.hits);
        read_group(sample.combat.damage_dealt, damage_dealt, mask.damage_dealt);
        read_group(sample.combat.kills, kills, mask.kills);
        if (mask.active_lasers) {
            sample.active_lasers = *active_lasers++;
        }
        if (mask.lasers_fired) {
            sample.lasers_fired = *lasers_fired++;
        }
        sample.completed_tick = tick;
        result.push_back(sample);
    }
    return result;
}
}
