#include <sbx/memory.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <thread>

namespace {
auto allocations_are_aligned_and_owned() -> bool {
    for (auto const alignment : {std::size_t{16}, std::size_t{64}, std::size_t{256}}) {
        auto* const allocation{sbx::memory::allocate_aligned(1024, alignment)};
        if (allocation == nullptr ||
            reinterpret_cast<std::uintptr_t>(allocation) % alignment != 0 ||
            !sbx::memory::owns(allocation)) {
            return false;
        }
        sbx::memory::free(allocation);
    }

    auto* const foreign{::operator new(64)};
    auto const foreign_is_owned{sbx::memory::owns(foreign)};
    ::operator delete(foreign);
    sbx::memory::free(nullptr);
    return !foreign_is_owned;
}

auto reallocation_preserves_data_and_alignment() -> bool {
    auto* allocation{static_cast<std::byte*>(sbx::memory::allocate_aligned(64, 256))};
    if (allocation == nullptr) {
        return false;
    }
    for (std::size_t index{}; index < 64; ++index) {
        allocation[index] = static_cast<std::byte>(index);
    }

    allocation = static_cast<std::byte*>(sbx::memory::reallocate_aligned(allocation, 4096, 256));
    if (allocation == nullptr || reinterpret_cast<std::uintptr_t>(allocation) % 256 != 0) {
        return false;
    }
    for (std::size_t index{}; index < 64; ++index) {
        if (allocation[index] != static_cast<std::byte>(index)) {
            return false;
        }
    }
    sbx::memory::free(allocation);
    return true;
}

auto reallocation_edge_cases_preserve_allocator_semantics() -> bool {
    auto* const zero_allocation{sbx::memory::allocate_aligned(0, 64)};
    if (zero_allocation == nullptr || !sbx::memory::owns(zero_allocation)) {
        return false;
    }

    auto* const zero_reallocation{sbx::memory::reallocate_aligned(zero_allocation, 0, 64)};
    if (zero_reallocation == nullptr || !sbx::memory::owns(zero_reallocation)) {
        return false;
    }
    sbx::memory::free(zero_reallocation);

    auto* const from_null{sbx::memory::reallocate_aligned(nullptr, 128, 64)};
    if (from_null == nullptr || !sbx::memory::owns(from_null)) {
        return false;
    }

    auto* const failed{
        sbx::memory::reallocate_aligned(from_null, std::numeric_limits<std::size_t>::max(), 64)};
    if (failed != nullptr || !sbx::memory::owns(from_null)) {
        return false;
    }
    sbx::memory::free(from_null);
    return true;
}

auto independent_worker_threads_succeed() -> bool {
    std::array<std::thread, 8> threads;
    std::atomic_bool succeeded{true};
    for (auto& thread : threads) {
        thread = std::thread{[&succeeded] {
            for (std::size_t index{}; index < 1000; ++index) {
                auto* const allocation{sbx::memory::allocate_aligned(128 + index % 31, 64)};
                if (allocation == nullptr || !sbx::memory::owns(allocation)) {
                    succeeded.store(false, std::memory_order_relaxed);
                }
                sbx::memory::free(allocation);
            }
        }};
    }
    for (auto& thread : threads) {
        thread.join();
    }
    return succeeded.load(std::memory_order_relaxed);
}
}

auto main() -> int {
    if (!allocations_are_aligned_and_owned()) {
        return 1;
    }
    if (!reallocation_preserves_data_and_alignment()) {
        return 2;
    }
    if (!reallocation_edge_cases_preserve_allocator_semantics()) {
        return 3;
    }
    if (!independent_worker_threads_succeed()) {
        return 4;
    }
    return sbx::memory::version() > 0 ? 0 : 5;
}
