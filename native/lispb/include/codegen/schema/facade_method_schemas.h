#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct FacadeMethodSchemas {
    [[nodiscard]] auto append() -> FacadeMethodId;
    [[nodiscard]] auto clone(FacadeMethodId id) -> FacadeMethodId;
    [[nodiscard]] auto index(FacadeMethodId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> return_type;
    std::vector<SchemaRange<ParameterId>> parameters;
    std::vector<std::uint8_t> is_const;
    std::vector<std::uint8_t> is_noexcept;
    std::vector<std::optional<IdentifierId>> target_name;
};

}
