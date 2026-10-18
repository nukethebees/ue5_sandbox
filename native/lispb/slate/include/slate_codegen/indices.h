#pragma once

#include <lispb/index.h>

namespace slate_codegen::detail {

struct Declarations;
struct Functions;
struct Macros;

using DeclarationId = lispb::Index<Declarations>;
using FunctionId = lispb::Index<Functions>;
using MacroId = lispb::Index<Macros>;

}
