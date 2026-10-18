#pragma once

#include <cstdint>
namespace lispb {

enum class TargetKind : std::uint8_t { none, cpp_schema, slate, kernel, material, group };

}
