#pragma once

#include <lispb/indices.h>
#include <material_gen/Indices.h>
#include <material_gen/SourceSpan.h>

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <vector>

namespace material_synth {
struct NamedNodes {
    auto append(lispb::StringId name, NodeHandle node, SourceSpan span) -> NamedNodeId;
    auto size() const -> std::size_t { return name.size(); }
    auto handles() const {
        return std::views::iota(std::uint32_t{}, static_cast<std::uint32_t>(size())) |
               std::views::transform([](std::uint32_t index) { return NamedNodeId{index}; });
    }
    std::vector<lispb::StringId> name;
    std::vector<NodeHandle> node;
    std::vector<SourceSpan> span;
};
}
