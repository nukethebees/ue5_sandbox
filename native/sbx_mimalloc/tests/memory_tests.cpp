#include <sbx/memory.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
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
    if (!independent_worker_threads_succeed()) {
        return 3;
    }
    return sbx::memory::version() > 0 ? 0 : 4;
}
