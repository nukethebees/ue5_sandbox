#include "LoopViewAccessorCallCheck.hpp"

#include "SimulationPolicy.hpp"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

namespace clang::tidy::ioj {

void LoopViewAccessorCallCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    finder->addMatcher(ast_matchers::traverse(TK_IgnoreUnlessSpelledInSource,
                                            ast_matchers::callExpr().bind("call")), this);
}
void LoopViewAccessorCallCheck::check(ast_matchers::MatchFinder::MatchResult const& result) {
    auto const* call{result.Nodes.getNodeAs<CallExpr>("call")};
    if (is_policy_source(call->getBeginLoc(), *result.SourceManager) &&
        is_view_type(call->getType()) && is_in_loop(*call, *result.Context)) {
        diag(call->getBeginLoc(),
             "view accessor is called inside a loop; resolve the view before entering the loop");
    }
}

} // namespace clang::tidy::ioj
