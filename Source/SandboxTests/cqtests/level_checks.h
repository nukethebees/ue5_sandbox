#pragma once

#include <Containers/ArrayView.h>

namespace ml {
struct TestSimulationDriver;
struct FSoftTestAssertions;

void check_radii(TConstArrayView<float> radii, FSoftTestAssertions& checks, float threshold);
void check_radii(TestSimulationDriver const& driver, FSoftTestAssertions& checks, float threshold);
}
