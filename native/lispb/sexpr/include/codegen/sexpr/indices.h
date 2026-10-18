#pragma once

#include <lispb/index.h>

namespace lispb::codegen::sexpr {

class Sources;
struct TokenTag;
class Syntax;
struct ExpansionTag;

using SourceFileId = Index<Sources>;
using TokenId = Index<TokenTag>;
using FormId = Index<Syntax>;
using ExpansionId = Index<ExpansionTag>;

}
