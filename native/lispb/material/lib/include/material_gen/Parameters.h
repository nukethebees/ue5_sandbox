#pragma once

#include <lispb/indices.h>
#include <material_gen/Indices.h>
#include <material_gen/SourceSpan.h>
#include <material_gen/TextureSamplerType.h>
#include <material_gen/ValueType.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <vector>

namespace material_synth {
struct Parameters {
    auto append(lispb::StringId name,
                ValueType type,
                lispb::StringId texture_path,
                TextureSamplerType texture_sampler_type,
                NodeHandle node,
                SourceSpan span,
                std::array<double, 4> const& values = {}) -> ParameterId;
    auto size() const -> std::size_t { return name.size(); }
    auto handles() const {
        return std::views::iota(std::uint32_t{}, static_cast<std::uint32_t>(size())) |
               std::views::transform([](std::uint32_t index) { return ParameterId{index}; });
    }
    std::vector<lispb::StringId> name;
    std::vector<ValueType> type;
    std::vector<lispb::StringId> texture_path;
    std::vector<TextureSamplerType> texture_sampler_type;
    std::vector<NodeHandle> node;
    std::vector<SourceSpan> span;
    std::array<std::vector<double>, 4> default_values;
};
}
