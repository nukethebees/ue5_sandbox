#pragma once

#include "ioj/sim/line_traces.h"
#include "ioj/sim/vectors3f.h"

#include <cassert>

namespace ioj::sim {
// Describe input columns independently of their owners so queries can use existing arrays.
struct LineTraceBatch {
    LineTraceBatch(Vectors3fConstView starts, Vectors3fConstView ends)
        : starts{starts}
        , ends{ends} {}
    LineTraceBatch(LineTraces::ConstView traces)
        : LineTraceBatch{
              {traces.view_starts().xs(), traces.view_starts().ys(), traces.view_starts().zs()},
              {traces.view_ends().xs(), traces.view_ends().ys(), traces.view_ends().zs()}} {}

    auto num() const noexcept -> std::uint32_t { return starts.num(); }
    void validate() const {
        starts.validate_array_sizes();
        ends.validate_array_sizes();
        assert(starts.num() == ends.num());
    }

    Vectors3fConstView starts;
    Vectors3fConstView ends;
};
}
