#pragma once

#include <lispb/indices.h>
#include <lispb/strings.h>
#include <material_gen/MaterialIR.h>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace material_synth {

inline constexpr std::uint32_t compiled_material_version{6};

struct CompiledMaterial {
    CompiledMaterial();
    CompiledMaterial(CompiledMaterial&& other) noexcept;
    auto operator=(CompiledMaterial&& other) noexcept -> CompiledMaterial&;

    lispb::Strings strings;
    lispb::StringId source_path;
    lispb::StringId source_hash;
    MaterialIR material;
};

// The material must have passed semantic validation.
auto serialize(MaterialIR const& material,
               std::string_view source_path,
               std::string_view source_hash)
    -> std::expected<std::vector<std::uint8_t>, std::string>;
auto deserialize(std::span<std::uint8_t const> bytes)
    -> std::expected<CompiledMaterial, std::string>;

}
