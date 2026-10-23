#pragma once

#include "ioj/sim/collision/line_traces.h"
#include "ioj/vectors3f.h"

#include <cassert>

namespace ioj::sim {
// Describe input columns independently of their owners so queries can use existing arrays.
struct LineTraceBatch {
    LineTraceBatch(vectors3f::ResolvedConstView starts, vectors3f::ResolvedConstView ends)
        : starts{starts}
        , ends{ends} {}
    LineTraceBatch(LineTraces::ConstView traces)
        : starts{traces.view_starts().resolve()}
        , ends{traces.view_ends().resolve()} {}

    auto num() const noexcept -> std::uint32_t { return starts.num(); }
    void validate() const { assert(starts.num() == ends.num()); }

    vectors3f::ResolvedConstView starts;
    vectors3f::ResolvedConstView ends;
};
}
