#pragma once

#include <lispb/schema/declaration_id.h>

#include <string>

namespace lispb::schema {
struct RenameDeclaration {
    DeclarationId declaration;
    std::string new_name;
};
}
