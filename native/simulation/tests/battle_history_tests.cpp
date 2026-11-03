#include <ioj/memory/bump_resource.h>
#include <ioj/sim/telemetry/analysis.h>
#include <ioj/sim/telemetry/battle_history.h>

#include <gtest/gtest.h>

namespace ioj::sim::telemetry::tests {
TEST(BattleHistory, ReconstructsIndependentlyChangedFieldsAcrossSegments) {
    memory::BumpResource memory{128 * 1024};
    BattleHistory history{memory};
    BattleSample sample;
    history.append(sample);
    for (std::uint64_t tick{1}; tick <= 100; ++tick) {
        sample.completed_tick = SimTick{tick};
        sample.alive[Team::Red][EntityType::Fighter] = static_cast<std::int32_t>(tick);
        if (tick % 7 == 0) {
            sample.combat.shots[Team::Blue][EntityType::Turret] += 2;
        }
        if (tick % 9 == 0) {
            sample.combat.damage_dealt[Team::Blue][EntityType::Turret] += 0.5;
        }
        sample.combat.kills[Team::Yellow][EntityType::Fighter] = tick / 5;
        history.append(sample);
    }
    auto const samples{history.materialize()};
    auto const sample_count{samples.size()};
    ASSERT_EQ(sample_count, 101);
    for (std::size_t index{}; index < sample_count; ++index) {
        auto const& row{samples[index]};
        EXPECT_EQ(row.completed_tick.value(), index);
        EXPECT_EQ(row.alive[Team::Red][EntityType::Fighter], index);
        EXPECT_EQ(row.alive[Team::Blue][EntityType::Fighter], 0);
        EXPECT_EQ(row.combat.shots[Team::Blue][EntityType::Turret], index / 7 * 2);
        EXPECT_EQ(row.combat.damage_dealt[Team::Blue][EntityType::Turret],
                  static_cast<double>(index / 9) * 0.5);
        EXPECT_EQ(row.combat.kills[Team::Yellow][EntityType::Fighter], index / 5);
    }
    EXPECT_LT(memory.used_bytes(), samples.size() * sizeof(BattleSample));
    auto const analysis{analyze({.battle_samples = samples})};
    ASSERT_EQ(analysis.battle_simulated_seconds.size(), samples.size());
    EXPECT_FLOAT_EQ(analysis.battle_simulated_seconds.back(), 100.0f / 60.0f);
    EXPECT_FLOAT_EQ(analysis.battle_shots.back(), 28.0f);
}

class HistoryUpstream final : public std::pmr::memory_resource {
  public:
    std::size_t allocations{};
    std::size_t deallocations{};
  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        ++allocations;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* data, std::size_t bytes, std::size_t alignment) override {
        ++deallocations;
        std::pmr::new_delete_resource()->deallocate(data, bytes, alignment);
    }
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }
};

TEST(BattleHistory, GrowsUpstreamAndReusesAllSegmentsAfterReset) {
    HistoryUpstream upstream;
    {
        memory::BumpResource memory{1024, upstream};
        {
            BattleHistory history{memory};
            ASSERT_EQ(upstream.allocations, 1);
            std::size_t first_run_allocations{};
            for (int repeat{}; repeat < 2; ++repeat) {
                BattleSample sample;
                for (std::uint64_t tick{}; tick < 100; ++tick) {
                    sample.completed_tick = SimTick{tick};
                    sample.alive[Team::Red][EntityType::Fighter] = static_cast<std::int32_t>(tick);
                    history.append(sample);
                }
                auto const samples{history.materialize()};
                ASSERT_EQ(samples.size(), 100);
                EXPECT_EQ(samples.back().alive[Team::Red][EntityType::Fighter], 99);
                EXPECT_GT(upstream.allocations, 1);
                if (repeat == 0) {
                    first_run_allocations = upstream.allocations;
                }
                EXPECT_EQ(upstream.allocations, first_run_allocations);
                history.reset();
                EXPECT_TRUE(history.materialize().empty());
            }
        }
        EXPECT_EQ(upstream.deallocations + 1, upstream.allocations);
    }
    EXPECT_EQ(upstream.deallocations, upstream.allocations);
}

TEST(BattleHistory, UnchangedSamplesUseOnlyTickAndMaskStorage) {
    memory::BumpResource memory{64 * 1024};
    BattleHistory history{memory};
    BattleSample sample;
    history.append(sample);
    auto const initial_bytes{memory.used_bytes()};
    for (std::uint64_t tick{1}; tick < 64; ++tick) {
        sample.completed_tick = SimTick{tick};
        history.append(sample);
    }
    EXPECT_LT(memory.used_bytes() - initial_bytes,
              64 * (sizeof(BattleChangeMask) + sizeof(SimTick) + 16));
    auto const before_duplicate{memory.used_bytes()};
    history.append(sample);
    EXPECT_EQ(history.size(), 64);
    EXPECT_EQ(memory.used_bytes(), before_duplicate);
}
}
