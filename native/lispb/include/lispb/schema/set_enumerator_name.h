#pragma once

#include <lispb/schema/declaration_id.h>

#include <string>

namespace lispb::schema {
struct SetEnumeratorName {
    DeclarationId enum_declaration;
    std::string current_name;
    std::string new_name;
};
}
