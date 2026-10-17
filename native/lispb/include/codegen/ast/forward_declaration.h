#pragma once

#include <codegen/ast/record_kind.h>

#include <string>

namespace codegen {

struct ForwardDeclaration {
    std::string name;
    RecordKind kind{RecordKind::struct_};
};

} // namespace codegen
