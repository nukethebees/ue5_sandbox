#pragma once

#include <ioj/constrained_numbers.h>

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_arena.h>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>

namespace ioj {

namespace tbb = oneapi::tbb;

using ThreadIndex = std::uint32_t;

[[nodiscard]] inline auto task_arena_concurrency(Positive<std::uint32_t> const work_count,
                                                 Positive<std::uint32_t> const grain_size) noexcept
    -> int {
    auto const chunk_count{(work_count.get() - 1) / grain_size.get() + 1};
    auto const max_concurrency{static_cast<std::uint32_t>(tbb::this_task_arena::max_concurrency())};

    return static_cast<int>(std::min(chunk_count, max_concurrency));
}

// Call from inside a TBB task arena to get the current arena's thread index.
[[nodiscard]] inline auto current_thread_index() noexcept -> ThreadIndex {
    auto const thread_index{tbb::this_task_arena::current_thread_index()};
    assert(thread_index >= 0);

    return static_cast<ThreadIndex>(thread_index);
}

// Process disjoint [begin, end) chunks and wait for every callback to finish.
template <std::integral Index, typename Function>
void parallel_for(Index const begin, Index const end, Function const& function) {
    tbb::parallel_for(tbb::blocked_range<Index>{begin, end},
                      [&function](tbb::blocked_range<Index> const& range) {
                          function(range.begin(), range.end());
                      });
}

template <std::integral Index, typename Function>
void parallel_for(Index const end, Function const& function) {
    ioj::parallel_for(Index{0}, end, function);
}

} // namespace ioj
