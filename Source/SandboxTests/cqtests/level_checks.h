#pragma once

namespace ml {
struct TestSimulationDriver;
struct FSoftTestAssertions;

void check_radii(TestSimulationDriver const& driver, FSoftTestAssertions& checks, float threshold);
}
