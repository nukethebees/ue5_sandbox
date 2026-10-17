#pragma once

#include <lispb/schema/declaration_id.h>

#include <optional>
#include <string>

namespace lispb::schema {
struct SetEnumeratorDisplayName {
    DeclarationId enum_declaration;
    std::string enumerator_name;
    std::optional<std::string> display_name;
};
}
