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
    LineTraceBatch(LineTraces::ConstView traces) {
        auto const start_columns{traces.view_starts()};
        auto const end_columns{traces.view_ends()};
        starts = {start_columns.xs(), start_columns.ys(), start_columns.zs()};
        ends = {end_columns.xs(), end_columns.ys(), end_columns.zs()};
    }

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
