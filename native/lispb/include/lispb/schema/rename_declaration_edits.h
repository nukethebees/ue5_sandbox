#pragma once

#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <vector>

namespace lispb::schema {

struct RenameDeclarationEdits {
    std::vector<DeclarationId> declarations;
    std::vector<IdentifierId> names;
};

} // namespace lispb::schema
