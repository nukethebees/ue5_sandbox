#pragma once

#include <lispb/index.h>

namespace lispb {

class Strings;
class Diagnostics;

using StringId = Index<Strings>;
using DiagnosticId = Index<Diagnostics>;

}
