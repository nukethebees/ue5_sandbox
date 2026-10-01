#pragma once

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <concepts>

namespace ml {

// Process disjoint [begin, end) chunks and wait for every callback to finish.
template <std::integral Index, typename Function>
void parallel_for(Index const begin, Index const end, Function const& function) {
    oneapi::tbb::parallel_for(oneapi::tbb::blocked_range<Index>{begin, end},
                              [&function](oneapi::tbb::blocked_range<Index> const& range) {
                                  function(range.begin(), range.end());
                              });
}

template <std::integral Index, typename Function>
void parallel_for(Index const end, Function const& function) {
    ml::parallel_for(Index{0}, end, function);
}

} // namespace ml
