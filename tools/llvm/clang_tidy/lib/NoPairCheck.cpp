#include "NoPairCheck.hpp"

#include "ForbiddenStdType.hpp"

namespace clang::tidy::ioj {

void NoPairCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    register_forbidden_std_type_matchers(*finder, *this);
}
void NoPairCheck::check(ast_matchers::MatchFinder::MatchResult const& result) {
    auto const location{forbidden_std_type_location(result, ForbiddenStdType::Pair)};
    if (location.isValid()) {
        diag(location, "avoid std::pair in simulation code; use a named aggregate");
    }
}

} // namespace clang::tidy::ioj
