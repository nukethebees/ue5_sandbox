#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct ModuleSettingsStore {
    [[nodiscard]] auto append() -> ModuleSettingsId;
    [[nodiscard]] auto clone(ModuleSettingsId id) -> ModuleSettingsId;
    [[nodiscard]] auto index(ModuleSettingsId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<StringId> header;
    std::vector<std::optional<StringId>> source;
    std::vector<std::optional<StringId>> header_include;
    std::vector<std::optional<StringId>> namespace_name;
    std::vector<SchemaRange<StringId>> include_order;
    std::vector<SchemaRange<StringId>> prelude_lines;
};

}
