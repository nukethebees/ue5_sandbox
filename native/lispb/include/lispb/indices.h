#pragma once

#include <lispb/index.h>

namespace lispb {

class Strings;
struct IdentifierTag;

using StringId = Index<Strings>;
using IdentifierId = Index<IdentifierTag>;

[[nodiscard]] constexpr auto string_id(IdentifierId const id) -> StringId {
    return StringId{id.value};
}

}
