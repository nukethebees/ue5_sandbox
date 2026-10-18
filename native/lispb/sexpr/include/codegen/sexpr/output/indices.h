#pragma once
#include <lispb/index.h>

namespace lispb::codegen::sexpr::output_detail {
struct Texts;
struct Lists;
using TextId = Index<Texts>;
using ListId = Index<Lists>;
}
