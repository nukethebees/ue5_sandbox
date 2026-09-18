#pragma once

#include <ioj/sim/line_traces.h>

#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

namespace ml {
using FLineTraces = ::ioj::sim::LineTraces;
using FLineTracesView = ::ioj::sim::LineTracesView;
using FLineTracesConstView = ::ioj::sim::LineTracesConstView;

inline auto make_line_traces_const_view(::ioj::sim::Vectors3fConstView const starts,
                                        ::ioj::sim::Vectors3fConstView const ends)
    -> FLineTracesConstView {
    return {starts, ends};
}
}
