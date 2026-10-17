#pragma once

#include <codegen/ast/record_kind.h>

#include <string>

namespace codegen {

struct FriendDeclaration {
    std::string name;
    RecordKind kind{RecordKind::class_};
};

} // namespace codegen
