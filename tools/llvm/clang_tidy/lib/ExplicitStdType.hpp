#pragma once

#include <clang/AST/TypeLoc.h>
#include <llvm/ADT/StringRef.h>

namespace clang::tidy::ioj {

SourceLocation explicit_std_type_location(TypeLoc location, llvm::StringRef name);
SourceLocation explicit_std_template_location(TemplateArgumentLoc const& argument,
                                              llvm::StringRef name);

} // namespace clang::tidy::ioj
