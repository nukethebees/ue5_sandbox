#include "ExplicitStdType.hpp"

#include <clang/AST/DeclTemplate.h>

namespace clang::tidy::ioj {

SourceLocation explicit_std_type_location(TypeLoc location, llvm::StringRef name) {
    location = location.getUnqualifiedLoc();
    TemplateDecl const* declaration{};
    SourceLocation name_location;
    if (auto const specialization{location.getAs<TemplateSpecializationTypeLoc>()}) {
        declaration = specialization.getTypePtr()->getTemplateName().getAsTemplateDecl();
        name_location = specialization.getTemplateNameLoc();
    } else if (auto const deduced{location.getAs<DeducedTemplateSpecializationTypeLoc>()}) {
        declaration = deduced.getTypePtr()->getTemplateName().getAsTemplateDecl();
        name_location = deduced.getTemplateNameLoc();
    }

    if (!declaration || declaration->getName() != name || !isa<ClassTemplateDecl>(declaration)) {
        return {};
    }
    auto const* context{declaration->getDeclContext()};
    while (context->isInlineNamespace()) {
        context = context->getParent();
    }
    return context->isStdNamespace() ? name_location : SourceLocation{};
}

} // namespace clang::tidy::ioj
