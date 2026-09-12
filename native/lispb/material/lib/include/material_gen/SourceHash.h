#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace material_synth {

auto sha256(std::span<std::uint8_t const> bytes) -> std::string;

}
