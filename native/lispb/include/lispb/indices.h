#pragma once

#include <lispb/index.h>

namespace lispb {

class Strings;
class Diagnostics;
struct IdentifierTag;

using StringId = Index<Strings>;
using IdentifierId = Index<IdentifierTag>;
using DiagnosticId = Index<Diagnostics>;

[[nodiscard]] constexpr auto string_id(IdentifierId const id) -> StringId {
    return StringId{id.value};
}

}
