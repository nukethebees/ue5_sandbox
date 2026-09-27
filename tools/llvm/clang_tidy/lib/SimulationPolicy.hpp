#pragma once

#include <clang/AST/ASTContext.h>
#include <clang/Basic/SourceManager.h>

namespace clang::tidy::ioj {

bool is_policy_source(SourceLocation location, SourceManager const& sources);
bool is_view_type(QualType type);
bool is_in_loop(Stmt const& statement, ASTContext& context);

} // namespace clang::tidy::ioj
