#pragma once

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::spinners {
struct ActionInputs {
    lasers::Sim& laser_simulation;
};
} // namespace ioj::sim::spinners
