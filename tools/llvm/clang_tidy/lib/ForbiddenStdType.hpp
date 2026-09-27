#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace clang::tidy::ioj {

enum class ForbiddenStdType { Pair, Tuple };

bool contains_forbidden_std_type(QualType type, ForbiddenStdType forbidden);
void register_forbidden_std_type_matchers(ast_matchers::MatchFinder& finder,
                                          ast_matchers::MatchFinder::MatchCallback& callback);
SourceLocation forbidden_std_type_location(ast_matchers::MatchFinder::MatchResult const& result,
                                           ForbiddenStdType forbidden);

} // namespace clang::tidy::ioj
