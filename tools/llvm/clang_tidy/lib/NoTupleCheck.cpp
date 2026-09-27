#include "NoTupleCheck.hpp"

#include "ForbiddenStdType.hpp"

namespace clang::tidy::ioj {

void NoTupleCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    register_forbidden_std_type_matchers(*finder, *this);
}
void NoTupleCheck::check(ast_matchers::MatchFinder::MatchResult const& result) {
    auto const location{forbidden_std_type_location(result, ForbiddenStdType::Tuple)};
    if (location.isValid()) {
        diag(location, "avoid std::tuple in simulation code; use a named aggregate");
    }
}

} // namespace clang::tidy::ioj
