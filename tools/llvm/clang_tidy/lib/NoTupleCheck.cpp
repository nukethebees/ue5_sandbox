#include "NoTupleCheck.hpp"

#include "ExplicitStdType.hpp"
#include "SimulationPolicy.hpp"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace clang::tidy::ioj {

void NoTupleCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    finder->addMatcher(ast_matchers::traverse(TK_IgnoreUnlessSpelledInSource,
                                              ast_matchers::templateArgumentLoc().bind("argument")),
                       this);
    finder->addMatcher(ast_matchers::traverse(TK_IgnoreUnlessSpelledInSource,
                                              ast_matchers::typeLoc().bind("type")),
                       this);
}
void NoTupleCheck::check(ast_matchers::MatchFinder::MatchResult const& result) {
    auto const* type{result.Nodes.getNodeAs<TypeLoc>("type")};
    auto const location{
        type ? explicit_std_type_location(*type, "tuple")
             : explicit_std_template_location(
                   *result.Nodes.getNodeAs<TemplateArgumentLoc>("argument"), "tuple")};
    if (is_policy_source(location, *result.SourceManager)) {
        diag(location, "avoid std::tuple in simulation code; use a named aggregate");
    }
}

} // namespace clang::tidy::ioj
