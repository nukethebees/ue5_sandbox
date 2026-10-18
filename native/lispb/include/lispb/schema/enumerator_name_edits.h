#pragma once

#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <vector>

namespace lispb::schema {

struct EnumeratorNameEdits {
    std::vector<DeclarationId> declarations;
    std::vector<IdentifierId> current_names;
    std::vector<IdentifierId> new_names;
};

} // namespace lispb::schema
