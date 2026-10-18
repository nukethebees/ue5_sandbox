#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct SettingCategorySchemas {
    [[nodiscard]] auto append() -> SettingCategoryId;
    [[nodiscard]] auto clone(SettingCategoryId id) -> SettingCategoryId;
    [[nodiscard]] auto index(SettingCategoryId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<StringId> label;
};

}
