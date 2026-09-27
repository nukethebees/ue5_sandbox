#include "SimulationPolicy.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ExprConcepts.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/ParentMapContext.h>
#include <llvm/ADT/StringSwitch.h>

namespace clang::tidy::ioj {

bool is_policy_source(SourceLocation location, SourceManager const& sources) {
    if (location.isInvalid() || location.isMacroID() || sources.isInSystemHeader(location)) {
        return false;
    }

    // Match the existing translation-unit diagnostic scope. Included implementation
    // details (including generated SoA bodies) are never policy use sites.
    return sources.isWrittenInMainFile(location);
}

static bool is_view_record(CXXRecordDecl const* record) {
    if (!record) {
        return false;
    }

    auto const* specialization{dyn_cast<ClassTemplateSpecializationDecl>(record)};
    auto const* declaration{
        specialization ? specialization->getSpecializedTemplate()->getTemplatedDecl() : record};
    auto const name{declaration->getQualifiedNameAsString()};
    // This is the intentional extension point for view families without a common
    // base. Names identify declarations, never substrings of source spelling.
    if (llvm::StringSwitch<bool>(name)
            .Cases({"std::span", "ml::Vector3SoAView"}, true)
            .Cases({"ml::TickCountdownView", "ml::PeriodicTickCountdownView"}, true)
            .Cases(
                {"ml::soa_storage_detail::CompactViewState", "ml::soa_storage_detail::VectorView"},
                true)
            .Cases({"ioj::sim::HealthView", "ioj::sim::HealthConstView"}, true)
            .Cases({"ioj::sim::DirectDamageEventsView", "ioj::sim::DirectDamageEventsConstView"},
                   true)
            .Cases({"ioj::sim::EntityDeathInfoView", "ioj::sim::EntityDeathInfoConstView"}, true)
            .Cases(
                {"ioj::sim::EntityEntityOverlapsView", "ioj::sim::EntityEntityOverlapsConstView"},
                true)
            .Cases(
                {"ioj::sim::EntityStaticOverlapsView", "ioj::sim::EntityStaticOverlapsConstView"},
                true)
            .Cases({"ioj::sim::FighterOrderQueueView", "ioj::sim::FighterOrderQueueConstView"},
                   true)
            .Cases({"ioj::sim::LineTracesView", "ioj::sim::LineTracesConstView"}, true)
            .Cases({"ioj::sim::Rotators3fView", "ioj::sim::Rotators3fConstView"}, true)
            .Cases({"ioj::sim::TraceHitsView", "ioj::sim::TraceHitsConstView"}, true)
            .Cases({"ioj::sim::LevelSpawnGroupsView", "ioj::sim::LevelSpawnGroupsConstView"}, true)
            .Cases({"ioj::sim::LevelMissionEventGroupsView",
                    "ioj::sim::LevelMissionEventGroupsConstView"},
                   true)
            .Cases({"ioj::sim::PlayerAgentView", "ioj::sim::AgentTargetView"}, true)
            .Cases({"ioj::sim::CapitalReadView",
                    "ioj::sim::FighterReadView",
                    "ioj::sim::TurretReadView"},
                   true)
            .Cases(
                {"ioj::sim::SpinnerReadView", "ioj::sim::LaserReadView", "ioj::sim::LevelReadView"},
                true)
            .Cases({"ioj::sim::collision::DetectedOverlapsView",
                    "ioj::sim::collision::AABBOverlapEventBatchView",
                    "ioj::sim::collision::AABBOverlapEventsView"},
                   true)
            .Default(false)) {
        return true;
    }

    auto const* definition{record->getDefinition()};
    if (definition) {
        for (auto const& base : definition->bases()) {
            if (is_view_record(base.getType()->getAsCXXRecordDecl())) {
                return true;
            }
        }
    }
    return false;
}

bool is_view_type(QualType type) {
    return !type.isNull() && is_view_record(type.getNonReferenceType()->getAsCXXRecordDecl());
}

static bool node_in_loop(DynTypedNode const& node, ASTContext& context) {
    for (auto const& parent : context.getParents(node)) {
        auto const* child{node.get<Stmt>()};
        if (parent.get<FunctionDecl>() || parent.get<CXXRecordDecl>()) {
            continue;
        }
        if (auto const* lambda{parent.get<LambdaExpr>()}; lambda && child == lambda->getBody()) {
            continue;
        }
        if (parent.get<UnaryExprOrTypeTraitExpr>() || parent.get<CXXNoexceptExpr>() ||
            parent.get<RequiresExpr>()) {
            continue;
        }
        if (auto const* loop{parent.get<ForStmt>()}) {
            if (child == loop->getBody() || child == loop->getCond() || child == loop->getInc() ||
                child == loop->getConditionVariableDeclStmt()) {
                return true;
            }
        }
        if (auto const* loop{parent.get<CXXForRangeStmt>()}; loop && child == loop->getBody()) {
            return true;
        }
        if (parent.get<WhileStmt>() || parent.get<DoStmt>()) {
            return true;
        }
        if (node_in_loop(parent, context)) {
            return true;
        }
    }
    return false;
}

bool is_in_loop(Stmt const& statement, ASTContext& context) {
    return node_in_loop(DynTypedNode::create(statement), context);
}

} // namespace clang::tidy::ioj
