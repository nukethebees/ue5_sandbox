#include "NoPairCheck.hpp"

#include "ExplicitStdType.hpp"
#include "SimulationPolicy.hpp"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace clang::tidy::ioj {

void NoPairCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    finder->addMatcher(ast_matchers::traverse(TK_IgnoreUnlessSpelledInSource,
                                              ast_matchers::typeLoc().bind("type")),
                       this);
}
void NoPairCheck::check(ast_matchers::MatchFinder::MatchResult const& result) {
    auto const location{
        explicit_std_type_location(*result.Nodes.getNodeAs<TypeLoc>("type"), "pair")};
    if (is_policy_source(location, *result.SourceManager)) {
        diag(location, "avoid std::pair in simulation code; use a named aggregate");
    }
}

} // namespace clang::tidy::ioj
