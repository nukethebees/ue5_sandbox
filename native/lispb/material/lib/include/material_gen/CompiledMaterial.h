#pragma once

#include <material_gen/MaterialIR.h>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace material_synth {

inline constexpr std::uint32_t compiled_material_version{3};

struct CompiledMaterial {
    std::string source_path;
    std::string source_hash;
    MaterialIR material;
};

auto serialize(CompiledMaterial const& compiled)
    -> std::expected<std::vector<std::uint8_t>, std::string>;
auto deserialize(std::span<std::uint8_t const> bytes)
    -> std::expected<CompiledMaterial, std::string>;

}
