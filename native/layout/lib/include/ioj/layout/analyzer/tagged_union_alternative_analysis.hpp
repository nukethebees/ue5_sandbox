#pragma once

#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct TaggedUnionAlternativeAnalysis {
    std::string name{};
    std::string tag{};
    lispb::schema::TypeId semantic_type{};
    std::uint64_t element_count{1};
    std::optional<TypeFacts> element_facts{};
    std::optional<std::uint64_t> extent_bytes{};
    std::optional<std::uint64_t> payload_slack_bytes{};
    std::optional<std::uint64_t> total_payload_slack_bytes{};
};

}
