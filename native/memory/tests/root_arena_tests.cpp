#include "native/memory/root_arena.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <utility>

namespace ml::memory::tests {
auto expect(bool const condition, std::string_view const message) -> bool {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

auto reuses_returned_blocks() -> bool {
    alignas(64) std::array<std::byte, 512> backing{};
    RootArena arena{backing.data(), backing.size()};

    std::byte* first_address{};
    {
        auto block{arena.try_acquire_block(128, 64)};
        if (!expect(static_cast<bool>(block), "initial block acquisition succeeds")) {
            return false;
        }
        first_address = block.data();
        if (!expect(arena.get_statistics().live_block_bytes == 128,
                    "live block bytes include the allocation")) {
            return false;
        }
    }

    auto reused{arena.try_acquire_block(128, 64)};
    return expect(static_cast<bool>(reused), "replacement block acquisition succeeds") &&
           expect(reused.data() == first_address, "returned block address is reused") &&
           expect(arena.get_statistics().claimed_bytes == 128,
                  "reuse does not claim additional root bytes");
}

auto reports_exact_exhaustion_diagnostics() -> bool {
    alignas(64) std::array<std::byte, 512> backing{};
    RootArena arena{backing.data(), backing.size()};
    auto live_block{arena.try_acquire_block(128, 64)};
    if (!expect(static_cast<bool>(live_block), "diagnostic setup allocation succeeds")) {
        return false;
    }

    auto exhaustion{arena.try_acquire_block(1024, 32)};
    if (!expect(!static_cast<bool>(exhaustion), "oversized block acquisition fails")) {
        return false;
    }
    auto const failure{arena.get_last_allocation_failure()};
    return expect(failure != nullptr, "allocation failure is recorded") &&
           expect(failure->requested_bytes == 1024, "requested bytes are exact") &&
           expect(failure->alignment == 32, "requested alignment is exact") &&
           expect(failure->claimed_bytes == 128, "claimed bytes are exact") &&
           expect(failure->total_capacity_bytes == 512, "root capacity is exact");
}

auto moving_a_block_transfers_its_range() -> bool {
    alignas(64) std::array<std::byte, 512> backing{};
    RootArena arena{backing.data(), backing.size()};
    auto source{arena.try_acquire_block(128, 64)};
    if (!expect(static_cast<bool>(source), "move setup allocation succeeds")) {
        return false;
    }
    auto* const address{source.data()};

    Block destination{std::move(source)};
    if (!expect(!static_cast<bool>(source), "moved-from block is empty") ||
        !expect(destination.data() == address, "move preserves the block address") ||
        !expect(arena.get_statistics().live_block_count == 1,
                "move does not alter the live block count")) {
        return false;
    }

    destination = {};
    if (!expect(arena.get_statistics().live_block_count == 0, "reset releases the moved block")) {
        return false;
    }
    auto reused{arena.try_acquire_block(128, 64)};
    return expect(static_cast<bool>(reused), "released moved block can be reacquired") &&
           expect(reused.data() == address, "reacquired moved block preserves its address");
}

auto pmr_deallocation_returns_the_range() -> bool {
    alignas(64) std::array<std::byte, 512> backing{};
    RootArena arena{backing.data(), backing.size()};
    auto& resource{arena.memory_resource()};

    auto* const first{resource.allocate(128, 64)};
    resource.deallocate(first, 128, 64);
    auto* const second{resource.allocate(128, 64)};

    auto const passed{expect(second == first, "PMR deallocation returns its range")};
    resource.deallocate(second, 128, 64);
    return passed;
}
}

auto main() -> int {
    using namespace ml::memory::tests;
    auto const passed{reuses_returned_blocks() && reports_exact_exhaustion_diagnostics() &&
                      moving_a_block_transfers_its_range() && pmr_deallocation_returns_the_range()};
    return passed ? 0 : 1;
}
