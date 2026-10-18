#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct ParameterSchemas {
    [[nodiscard]] auto append() -> ParameterId;
    [[nodiscard]] auto clone(ParameterId id) -> ParameterId;
    [[nodiscard]] auto index(ParameterId id) const -> std::uint32_t;
    void clear();

    std::vector<TypeRefId> type;
    std::vector<IdentifierId> name;
    std::vector<std::optional<StringId>> default_value;
};

}
