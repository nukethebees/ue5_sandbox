#pragma once

#include <codegen/schema/declaration_handle.h>
#include <lispb/schema/indices.h>

#include <vector>

namespace lispb::schema {

struct ReplaceDeclarationEdits {
    std::vector<DeclarationId> declarations;
    std::vector<codegen::DeclarationHandle> versions;
};

} // namespace lispb::schema
