#pragma once

#include <ioj/vector.h>

#include <vector>

namespace ioj::sim::tasks {
class InstructionData {
  public:
    std::vector<Vector3f> locations;
};
}
