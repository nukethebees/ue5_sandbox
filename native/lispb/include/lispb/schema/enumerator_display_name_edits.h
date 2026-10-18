#pragma once

#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <optional>
#include <vector>

namespace lispb::schema {

struct EnumeratorDisplayNameEdits {
    std::vector<DeclarationId> declarations;
    std::vector<IdentifierId> names;
    std::vector<std::optional<StringId>> display_names;
};

} // namespace lispb::schema
