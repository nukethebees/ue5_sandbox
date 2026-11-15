#pragma once

#include "ioj/sim/collision/line_traces.h"
#include "ioj/vectors3f.h"

#include <cassert>

namespace ioj::sim {
// Describe input columns independently of their owners so queries can use existing arrays.
struct LineTraceBatch {
    LineTraceBatch(vectors3f::ConstView starts, vectors3f::ConstView ends)
        : starts{starts}
        , ends{ends} {}
    LineTraceBatch(LineTraces::CompactConstView traces)
        : starts{traces.view_starts().resolve()}
        , ends{traces.view_ends().resolve()} {}

    auto num() const noexcept -> std::uint32_t { return starts.num(); }
    void validate() const { assert(starts.num() == ends.num()); }

    vectors3f::ConstView starts;
    vectors3f::ConstView ends;
};
}
