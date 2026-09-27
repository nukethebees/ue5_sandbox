#include "LoopViewConstructionCheck.hpp"

#include "SimulationPolicy.hpp"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Lex/Lexer.h>

namespace clang::ast_matchers {
AST_MATCHER(Expr, is_paren_list_init) {
    return isa<CXXParenListInitExpr>(Node);
}
} // namespace clang::ast_matchers

namespace clang::tidy::ioj {

void LoopViewConstructionCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    using namespace ast_matchers;
    finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 expr(anyOf(cxxConstructExpr(), initListExpr(), is_paren_list_init()))
                     .bind("construction")),
        this);
    // Source-only traversal skips the implicit initializer of a range-for variable.
    finder->addMatcher(
        traverse(TK_AsIs,
                 cxxForRangeStmt(hasLoopVariable(varDecl(hasInitializer(
                     ignoringImplicit(cxxConstructExpr().bind("construction"))))))),
        this);
}
void LoopViewConstructionCheck::check(ast_matchers::MatchFinder::MatchResult const& result) {
    auto const* expression{result.Nodes.getNodeAs<Expr>("construction")};
    if (!is_policy_source(expression->getBeginLoc(), *result.SourceManager) ||
        !is_view_type(expression->getType())) {
        return;
    }
    if (auto const* list{dyn_cast<InitListExpr>(expression)}) {
        auto const* record{list->getType()->getAsCXXRecordDecl()};
        if (!record || !record->isAggregate()) {
            return;
        }
        if (list->getNumInits() == 1 &&
            result.Context->hasSameUnqualifiedType(list->getType(), list->getInit(0)->getType())) {
            return;
        }
    }
    if (auto const* construction{dyn_cast<CXXConstructExpr>(expression)}) {
        // Passing/copying an existing view does not resolve it again. Calls returning
        // a view belong to the accessor check, including optional copy elision.
        if (construction->getConstructor()->isCopyOrMoveConstructor()) {
            return;
        }
        // Omitted aggregate fields can have implicit default constructors located
        // at the closing brace even under IgnoreUnlessSpelledInSource traversal.
        Token token;
        if (!Lexer::getRawToken(expression->getBeginLoc(),
                                token,
                                *result.SourceManager,
                                result.Context->getLangOpts()) &&
            token.is(tok::r_brace)) {
            return;
        }
    }
    if (is_in_loop(*expression, *result.Context)) {
        diag(expression->getBeginLoc(),
             "view is constructed inside a loop; resolve the view before entering the loop");
    }
}

} // namespace clang::tidy::ioj
