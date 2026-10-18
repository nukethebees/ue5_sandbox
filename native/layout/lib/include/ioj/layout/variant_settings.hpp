#pragma once

#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ioj::layout {

// Keep durable declaration identities and interned member names until analysis resolution.
struct VariantSettings {
    auto operator==(VariantSettings const&) const -> bool = default;

    std::vector<lispb::schema::DeclarationId> packed_types;
    std::vector<std::string> packed_storage;
    std::vector<lispb::schema::DeclarationId> field_types;
    std::vector<lispb::IdentifierId> field_names;
    std::vector<std::uint32_t> field_widths;
    std::vector<lispb::schema::DeclarationId> column_types;
    std::vector<lispb::IdentifierId> column_names;
    std::vector<std::string> column_spelling;
    std::vector<lispb::schema::DeclarationId> capacity_types;
    std::vector<std::uint64_t> capacities;
};

}
