#pragma once

#include <lispb/index.h>

#include <cstdint>
#include <limits>

namespace material_synth {

struct CustomInputs;
struct NamedNodes;
struct Parameters;
class Nodes;

inline constexpr std::uint32_t invalid_node_index{std::numeric_limits<std::uint32_t>::max()};

using CustomInputId = lispb::Index<CustomInputs>;
using NamedNodeId = lispb::Index<NamedNodes>;
using ParameterId = lispb::Index<Parameters>;
using NodeHandle = lispb::Index<Nodes, invalid_node_index>;

}
