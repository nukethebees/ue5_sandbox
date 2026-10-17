#pragma once

#include <codegen/schema/declaration_schema.h>
#include <lispb/schema/resolved_type_ref.h>
#include <lispb/schema/type_identity.h>

#include <optional>
#include <string>

namespace lispb::schema {
struct TypeUse {
    std::string module_name;
    std::optional<TypeIdentity> declaration;
    std::string role;
    ResolvedTypeRef target;
    codegen::TypeReferenceKind kind{codegen::TypeReferenceKind::ordinary};
};
}
