#pragma once

#include <codegen/ast/ir/range.h>

#include <span>
#include <vector>

namespace lispb::codegen::ir {

// Consume values immediately; they must not alias storage.
template <typename T>
auto save_range(std::vector<T>& storage, std::span<T const> values) -> Range {
    Range const range{static_cast<std::uint32_t>(storage.size()),
                      static_cast<std::uint32_t>(values.size())};
    storage.insert(storage.end(), values.begin(), values.end());
    return range;
}

template <typename T>
auto range_values(std::vector<T> const& storage, Range range) -> std::span<T const> {
    return std::span{storage}.subspan(range.begin, range.count);
}

}
