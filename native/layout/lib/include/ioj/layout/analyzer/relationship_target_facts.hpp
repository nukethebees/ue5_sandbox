#pragma once

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct RelationshipTargetFacts {
    lispb::schema::TypeId target{};
    std::optional<std::uint64_t> element_capacity{};
    std::optional<std::uint64_t> byte_extent{};
};

}
