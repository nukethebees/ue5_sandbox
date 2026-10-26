#pragma once

#include <lispb/schema/indices.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::schema {

struct BitGuard {
    std::string path;
    std::uint32_t offset{};
    std::uint32_t bits{};
    std::uint64_t value{};
    TypeId enumeration;
};

struct BitPlacement {
    ChildOccurrenceId child;
    TypeId type;
    std::string path;
    std::string source_path;
    std::optional<std::uint32_t> parent;
    std::uint32_t offset{};
    std::uint32_t bits{};
    std::uint32_t extent{};
    bool reserved{};
    bool discriminator{};
    bool alternative{};
    std::vector<BitGuard> guards;
};

struct BitLayout {
    std::optional<std::uint32_t> encoded_bits;
    std::uint32_t storage_bits{};
    std::optional<std::uint64_t> default_raw_value;
    std::vector<BitPlacement> placements;
};

struct BitLayoutOverrides {
    std::map<std::string, std::uint32_t> field_bits;
    std::optional<std::uint32_t> storage_bits;
};

auto resolve_bit_layout(TypeGraph const& graph,
                        TypeId type,
                        BitLayoutOverrides const& overrides = {}) -> BitLayout;
auto is_packed_projection(TypeGraph const& graph, TypeId type) -> bool;
auto packed_storage_bits(std::string_view spelling) -> std::optional<std::uint32_t>;

}
