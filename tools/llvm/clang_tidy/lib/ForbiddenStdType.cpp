#include "ForbiddenStdType.hpp"

#include "SimulationPolicy.hpp"

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/STLExtras.h>

namespace clang::tidy::ioj {

static bool is_standard_template(TemplateDecl const& declaration) {
    auto const* context{declaration.getDeclContext()};
    while (context->isInlineNamespace()) {
        context = context->getParent();
    }
    return context->isStdNamespace();
}

static bool is_forbidden_template(TemplateDecl const* declaration, ForbiddenStdType forbidden) {
    auto const name{forbidden == ForbiddenStdType::Pair ? "pair" : "tuple"};
    if (!declaration || !isa<ClassTemplateDecl>(declaration) || declaration->getName() != name) {
        return false;
    }
    return is_standard_template(*declaration);
}

static bool is_forbidden_value(QualType type, ForbiddenStdType forbidden) {
    if (type.isNull()) {
        return false;
    }
    auto const* canonical{type.getNonReferenceType().getCanonicalType().getTypePtr()};
    if (auto const* record{
            dyn_cast_or_null<ClassTemplateSpecializationDecl>(canonical->getAsCXXRecordDecl())}) {
        return is_forbidden_template(record->getSpecializedTemplate(), forbidden);
    }
    if (auto const* specialization{dyn_cast<TemplateSpecializationType>(canonical)}) {
        return is_forbidden_template(specialization->getTemplateName().getAsTemplateDecl(),
                                     forbidden);
    }
    return false;
}

static bool contains_type(QualType type,
                          ForbiddenStdType forbidden,
                          llvm::SmallPtrSetImpl<Type const*>& visited);

static bool is_std_implementation(TemplateDecl const& declaration) {
    auto const* identifier{declaration.getIdentifier()};
    if (!identifier || !isReservedInAllContexts(
                           identifier->isReserved(declaration.getASTContext().getLangOpts()))) {
        return false;
    }
    for (auto const* context{declaration.getDeclContext()}; context;
         context = context->getParent()) {
        if (context->isStdNamespace()) {
            return true;
        }
    }
    return false;
}

static bool contains_argument(TemplateArgument const& argument,
                              ForbiddenStdType forbidden,
                              llvm::SmallPtrSetImpl<Type const*>& visited) {
    if (argument.getKind() == TemplateArgument::Type) {
        return contains_type(argument.getAsType(), forbidden, visited);
    }
    if (argument.getKind() == TemplateArgument::Pack) {
        for (auto const& element : argument.pack_elements()) {
            if (contains_argument(element, forbidden, visited)) {
                return true;
            }
        }
    }
    return false;
}

static bool contains_type(QualType type,
                          ForbiddenStdType forbidden,
                          llvm::SmallPtrSetImpl<Type const*>& visited) {
    if (type.isNull()) {
        return false;
    }
    auto const* canonical{type.getCanonicalType().getTypePtr()};
    if (!visited.insert(canonical).second) {
        return false;
    }

    if (auto const pointee{canonical->getPointeeType()}; !pointee.isNull()) {
        return contains_type(pointee, forbidden, visited);
    }
    if (auto const* member{dyn_cast<MemberPointerType>(canonical)}) {
        return contains_type(member->getPointeeType(), forbidden, visited);
    }
    if (auto const* array{dyn_cast<ArrayType>(canonical)}) {
        return contains_type(array->getElementType(), forbidden, visited);
    }
    if (auto const* function{dyn_cast<FunctionType>(canonical)}) {
        if (contains_type(function->getReturnType(), forbidden, visited)) {
            return true;
        }
        if (auto const* prototype{dyn_cast<FunctionProtoType>(function)}) {
            for (auto parameter : prototype->param_types()) {
                if (contains_type(parameter, forbidden, visited)) {
                    return true;
                }
            }
        }
    }

    TemplateDecl const* declaration{};
    llvm::ArrayRef<TemplateArgument> arguments;
    if (auto const* record{
            dyn_cast_or_null<ClassTemplateSpecializationDecl>(canonical->getAsCXXRecordDecl())}) {
        declaration = record->getSpecializedTemplate();
        arguments = record->getTemplateArgs().asArray();
    } else if (auto const* specialization{dyn_cast<TemplateSpecializationType>(canonical)}) {
        declaration = specialization->getTemplateName().getAsTemplateDecl();
        arguments = specialization->template_arguments();
    }
    if (is_forbidden_template(declaration, forbidden)) {
        return true;
    }
    if (declaration && is_std_implementation(*declaration)) {
        return false;
    }
    auto const argument_count{arguments.size()};
    for (std::size_t index{}; index < argument_count; ++index) {
        // Defaulted standard policy parameters (e.g. allocators) do not expose
        // additional value types. Use the parameter declaration, not whichever
        // spelling first instantiated this canonical specialization.
        if (declaration && is_standard_template(*declaration)) {
            auto const* parameters{declaration->getTemplateParameters()};
            if (index < parameters->size()) {
                auto const* parameter{dyn_cast<TemplateTypeParmDecl>(parameters->getParam(index))};
                if (parameter && parameter->hasDefaultArgument()) {
                    continue;
                }
            }
        }
        if (contains_argument(arguments[index], forbidden, visited)) {
            return true;
        }
    }
    return false;
}

bool contains_forbidden_std_type(QualType type, ForbiddenStdType forbidden) {
    llvm::SmallPtrSet<Type const*, 16> visited;
    return contains_type(type, forbidden, visited);
}

static QualType declaration_type(Decl const& declaration) {
    if (auto const* variable{dyn_cast<VarDecl>(&declaration)}) {
        return variable->getType();
    }
    if (auto const* field{dyn_cast<FieldDecl>(&declaration)}) {
        return field->getType();
    }
    if (auto const* alias{dyn_cast<TypedefNameDecl>(&declaration)}) {
        return alias->getUnderlyingType();
    }
    if (auto const* function{dyn_cast<FunctionDecl>(&declaration)}) {
        if (auto const* method{dyn_cast<CXXMethodDecl>(function)};
            method && method->getParent()->isLambda()) {
            return {};
        }
        return function->getReturnType();
    }
    return {};
}

static bool owns_declaration(Decl const& declaration,
                             ForbiddenStdType forbidden,
                             SourceManager const& sources) {
    return !declaration.isImplicit() && is_policy_source(declaration.getLocation(), sources) &&
           contains_forbidden_std_type(declaration_type(declaration), forbidden);
}

static bool is_value_expression(Expr const& expression) {
    if (auto const* reference{dyn_cast<DeclRefExpr>(&expression)}) {
        return isa<VarDecl>(reference->getDecl());
    }
    if (auto const* member{dyn_cast<MemberExpr>(&expression)}) {
        return isa<FieldDecl, VarDecl>(member->getMemberDecl());
    }
    return isa<CallExpr,
               CXXConstructExpr,
               CXXUnresolvedConstructExpr,
               ExplicitCastExpr,
               InitListExpr>(&expression);
}

static bool is_consumed(Expr const& expression, ASTContext& context) {
    for (auto const& parent : context.getParents(expression)) {
        auto const* outer{parent.get<Expr>()};
        if (!outer) {
            if (parent.get<VarDecl>() || parent.get<FieldDecl>() || parent.get<ReturnStmt>()) {
                return true;
            }
            continue;
        }
        if (auto const* cast{dyn_cast<ExplicitCastExpr>(outer)};
            cast && cast->getType()->isVoidType()) {
            continue;
        }
        if (auto const* binary{dyn_cast<BinaryOperator>(outer)};
            binary && binary->getOpcode() == BO_Comma) {
            if (binary->getRHS() == &expression && is_consumed(*outer, context)) {
                return true;
            }
            continue;
        }
        bool const transparent{isa<ParenExpr,
                                   ImplicitCastExpr,
                                   ExprWithCleanups,
                                   MaterializeTemporaryExpr,
                                   CXXBindTemporaryExpr>(outer) ||
                               (isa<ConditionalOperator>(outer) &&
                                cast<ConditionalOperator>(outer)->getCond() != &expression)};
        if (!transparent || is_consumed(*outer, context)) {
            return true;
        }
    }
    return false;
}

static bool
    owns_expression(Expr const& expression, ForbiddenStdType forbidden, ASTContext& context) {
    if (!is_value_expression(expression) ||
        !is_policy_source(expression.getBeginLoc(), context.getSourceManager()) ||
        !is_forbidden_value(expression.getType(), forbidden)) {
        return false;
    }
    if (isa<CallExpr, DeclRefExpr, MemberExpr>(expression) && !is_consumed(expression, context)) {
        return false;
    }
    // Clang can locate an omitted aggregate field's implicit constructor at '}'.
    Token token;
    return !isa<CXXConstructExpr>(expression) ||
           Lexer::getRawToken(expression.getBeginLoc(),
                              token,
                              context.getSourceManager(),
                              context.getLangOpts()) ||
           !token.is(tok::r_brace);
}

static bool has_owner(DynTypedNode const& node,
                      ForbiddenStdType forbidden,
                      ASTContext& context,
                      bool returning = false) {
    for (auto const& parent : context.getParents(node)) {
        if (auto const* function{parent.get<FunctionDecl>()}) {
            // Parameters nested in a return type's function prototype belong to
            // that return type. The function's own parameters remain independent.
            if ((returning || node.get<TypeLoc>()) &&
                owns_declaration(*function, forbidden, context.getSourceManager())) {
                return true;
            }
            continue;
        }
        if (parent.get<CXXRecordDecl>()) {
            continue;
        }
        if (auto const* lambda{parent.get<LambdaExpr>()};
            lambda && node.get<Stmt>() == lambda->getBody()) {
            continue;
        }
        if (auto const* declaration{parent.get<Decl>()};
            declaration && owns_declaration(*declaration, forbidden, context.getSourceManager())) {
            return true;
        }
        if (auto const* expression{parent.get<Expr>()};
            expression && owns_expression(*expression, forbidden, context)) {
            return true;
        }
        if (has_owner(parent, forbidden, context, returning || parent.get<ReturnStmt>())) {
            return true;
        }
    }
    return false;
}

void register_forbidden_std_type_matchers(ast_matchers::MatchFinder& finder,
                                          ast_matchers::MatchFinder::MatchCallback& callback) {
    using namespace ast_matchers;
    finder.addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 namedDecl(anyOf(varDecl(), fieldDecl(), functionDecl(), typedefNameDecl()))
                     .bind("declaration")),
        &callback);
    finder.addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                               expr(anyOf(callExpr(),
                                          cxxConstructExpr(),
                                          cxxUnresolvedConstructExpr(),
                                          explicitCastExpr(),
                                          initListExpr(),
                                          declRefExpr(),
                                          memberExpr()))
                                   .bind("expression")),
                      &callback);
}

SourceLocation forbidden_std_type_location(ast_matchers::MatchFinder::MatchResult const& result,
                                           ForbiddenStdType forbidden) {
    if (auto const* declaration{result.Nodes.getNodeAs<NamedDecl>("declaration")}) {
        bool nested_parameter{};
        if (auto const* parameter{dyn_cast<ParmVarDecl>(declaration)}) {
            auto const* function{dyn_cast<FunctionDecl>(parameter->getDeclContext())};
            nested_parameter = !function || !llvm::is_contained(function->parameters(), parameter);
        }
        if (owns_declaration(*declaration, forbidden, *result.SourceManager) &&
            !(nested_parameter &&
              has_owner(DynTypedNode::create(*declaration), forbidden, *result.Context))) {
            return declaration->getLocation();
        }
        return {};
    }
    auto const* expression{result.Nodes.getNodeAs<Expr>("expression")};
    if (owns_expression(*expression, forbidden, *result.Context) &&
        !has_owner(DynTypedNode::create(*expression), forbidden, *result.Context)) {
        return expression->getExprLoc();
    }
    return {};
}

} // namespace clang::tidy::ioj
