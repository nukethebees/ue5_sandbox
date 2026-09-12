#pragma once

#include <sandbox/simulation/line_traces.h>

#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

namespace ml {
using FLineTraces = simulation::LineTraces;
using FLineTracesView = simulation::LineTracesView;
using FLineTracesConstView = simulation::LineTracesConstView;

inline auto make_line_traces_const_view(FVectors3f::ConstView const starts,
                                        FVectors3f::ConstView const ends) -> FLineTracesConstView {
    return {to_native(starts), to_native(ends)};
}
}
