#pragma once

#include <ioj/s7/ast_result.h>

namespace ioj::s7 {
// Borrow a node index; retain the original AST until the node is decoded.
struct RawNode {
    NodeIndex index{};
};
}
