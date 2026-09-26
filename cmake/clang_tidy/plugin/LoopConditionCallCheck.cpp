#include "LoopConditionCallCheck.hpp"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace clang::tidy::ioj {

static bool contains_call(Stmt const* statement) {
    if (!statement) {
        return false;
    }
    if (isa<CallExpr>(statement)) {
        return true;
    }
    for (auto const* child : statement->children()) {
        if (contains_call(child)) {
            return true;
        }
    }
    return false;
}

void LoopConditionCallCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    finder->addMatcher(ast_matchers::forStmt().bind("loop"), this);
}
void LoopConditionCallCheck::check(ast_matchers::MatchFinder::MatchResult const& result) {
    auto const* loop{result.Nodes.getNodeAs<ForStmt>("loop")};
    if (contains_call(loop->getCond()) || contains_call(loop->getConditionVariableDeclStmt())) {
        diag(loop->getForLoc(), "function calls do not belong in C-style for-loop conditions");
    }
}

} // namespace clang::tidy::ioj
