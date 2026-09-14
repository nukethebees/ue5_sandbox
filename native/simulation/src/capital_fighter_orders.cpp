#include "ioj/sim/capital_fighter_orders.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ioj::sim {
void build_fighter_orders(std::span<RegistryEntityHandle const> const capital_targets,
                          std::span<IndexSpan const> const fighter_spans,
                          std::span<RegistryEntityHandle const> const owned_fighters,
                          std::span<RegistryEntityHandle const> const fighter_handles,
                          std::span<RegistryEntityHandle const> const fighter_targets,
                          EntityRegistryQueryView const registry,
                          FighterOrderQueue& orders) {
    assert(capital_targets.size() == fighter_spans.size());
    assert(fighter_handles.size() == fighter_targets.size());

    orders.reset();
    auto const capital_count{capital_targets.size()};
    for (std::size_t capital_index{}; capital_index < capital_count; ++capital_index) {
        auto const capital_target{capital_targets[capital_index]};
        auto const span{fighter_spans[capital_index]};
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0);
        assert(static_cast<std::size_t>(end) <= owned_fighters.size());

        for (auto index{span.start()}; index < end; ++index) {
            auto const fighter{owned_fighters[static_cast<std::size_t>(index)]};
            if (capital_target.is_null()) {
                orders.add(fighter,
                           FighterOrder{.task = 1, .target = 1},
                           FighterTask::Standby,
                           capital_target);
                continue;
            }

            auto const found{std::ranges::find(fighter_handles, fighter)};
            assert(found != fighter_handles.end());
            auto const fighter_index{static_cast<std::size_t>(found - fighter_handles.begin())};
            auto const target{fighter_targets[fighter_index]};
            auto const target_is_dead{analyse_handle(registry, target) ==
                                          RegistryHandleState::Active &&
                                      registry.alive[static_cast<std::size_t>(target.index)] == 0};
            if (target.is_null() || target_is_dead) {
                orders.add(fighter, FighterOrder{.task = 0, .target = 1}, {}, capital_target);
            }
        }
    }
}
}
