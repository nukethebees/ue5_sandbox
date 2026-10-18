#pragma once

#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct RecordMemberAnalysis {
    std::string name{};
    lispb::schema::TypeId semantic_type{};
    std::uint64_t element_count{1};
    std::optional<TypeFacts> element_facts{};
    std::optional<std::uint64_t> offset_bytes{};
    std::optional<std::uint64_t> extent_bytes{};
    std::optional<std::uint64_t> padding_before_bytes{};
};

}
