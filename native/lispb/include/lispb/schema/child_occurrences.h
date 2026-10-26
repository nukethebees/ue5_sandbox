#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct ChildOccurrences {
    [[nodiscard]] auto append() -> ChildOccurrenceId;
    [[nodiscard]] auto index(ChildOccurrenceId id) const -> std::uint32_t;

    std::vector<codegen::ChildOccurrenceId> source;
    std::vector<StringId> name;
    std::vector<ResolvedOccurrenceId> semantic_type;
    std::vector<std::optional<std::uint64_t>> count;
    std::vector<RelationshipId> relationship;
    std::vector<std::optional<StringId>> initializer;
    std::vector<std::optional<std::uint32_t>> bits;
    std::vector<std::optional<StringId>> case_label;
    std::vector<std::optional<StringId>> tag;
    std::vector<std::optional<codegen::PackedIntegerValue>> default_value;
    std::vector<std::uint8_t> reserved;
};

}
