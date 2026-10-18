#pragma once

#include <lispb/schema/indices.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ioj::layout {

// Durable declaration identities are resolved only when preparing an analysis.
struct VariantSettings {
    std::vector<lispb::schema::DeclarationId> packed_types;
    std::vector<std::string> packed_storage;
    std::vector<lispb::schema::DeclarationId> field_types;
    std::vector<std::string> field_names;
    std::vector<std::uint32_t> field_widths;
    std::vector<lispb::schema::DeclarationId> column_types;
    std::vector<std::string> column_names;
    std::vector<std::string> column_spelling;
    std::vector<lispb::schema::DeclarationId> capacity_types;
    std::vector<std::uint64_t> capacities;

    auto operator==(VariantSettings const&) const -> bool = default;
};

}
