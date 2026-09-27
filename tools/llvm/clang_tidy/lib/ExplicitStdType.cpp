#include "ExplicitStdType.hpp"

#include <clang/AST/DeclTemplate.h>

namespace clang::tidy::ioj {

static bool is_std_template(TemplateDecl const* declaration, llvm::StringRef name) {
    if (!declaration || declaration->getName() != name || !isa<ClassTemplateDecl>(declaration)) {
        return false;
    }
    auto const* context{declaration->getDeclContext()};
    while (context->isInlineNamespace()) {
        context = context->getParent();
    }
    return context->isStdNamespace();
}

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

    return is_std_template(declaration, name) ? name_location : SourceLocation{};
}

SourceLocation explicit_std_template_location(TemplateArgumentLoc const& argument,
                                              llvm::StringRef name) {
    if (argument.getArgument().getKind() != TemplateArgument::Template) {
        return {};
    }
    auto const* declaration{argument.getArgument().getAsTemplate().getAsTemplateDecl()};
    return is_std_template(declaration, name) ? argument.getTemplateNameLoc() : SourceLocation{};
}

} // namespace clang::tidy::ioj
