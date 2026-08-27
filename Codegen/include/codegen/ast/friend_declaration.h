#pragma once

#include <string>

namespace codegen {

struct FriendDeclaration {
    std::string name;
    std::string kind{"class"};
};

} // namespace codegen
