#pragma once

#include <lispb/schema/indices.h>

#include <cstdint>
#include <span>
#include <vector>

namespace lispb::codegen::detail {

struct CompleteDependencies {
    std::vector<std::uint32_t> offsets;
    std::vector<schema::TypeId> targets;

    auto of(schema::TypeId type) const -> std::span<schema::TypeId const>;
};

}
