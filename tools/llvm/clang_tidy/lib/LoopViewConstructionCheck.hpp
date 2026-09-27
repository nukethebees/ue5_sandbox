#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace clang::tidy::ioj {

class LoopViewConstructionCheck : public ClangTidyCheck {
  public:
    using ClangTidyCheck::ClangTidyCheck;
    void registerMatchers(ast_matchers::MatchFinder* finder) override;
    void check(ast_matchers::MatchFinder::MatchResult const& result) override;
};

} // namespace clang::tidy::ioj
