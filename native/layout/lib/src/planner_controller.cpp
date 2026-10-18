#include <ioj/layout/planner_controller.hpp>

#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analyzer.hpp>
#include <ioj/layout/analysis_settings.hpp>
#include <ioj/layout/editor_session.hpp>

#include <lispb/strings.h>

#include <array>
#include <cassert>
#include <utility>

namespace ioj::layout {
PlannerController::PlannerController(EditorSession& editor)
    : editor_{editor} {}

PlannerController::PlannerController(EditorSession& editor, Document document)
    : editor_{editor} {
    assert(&document.session() == &editor);
    adopt(std::move(document));
}

auto PlannerController::document() -> Document* {
    return document_ ? &*document_ : nullptr;
}

auto PlannerController::document() const -> Document const* {
    return document_ ? &*document_ : nullptr;
}

auto PlannerController::analysis() const -> AnalysisStorage const& {
    return analysis_;
}

auto PlannerController::analysis_count() const -> std::uint64_t {
    return analysis_count_;
}

auto PlannerController::last_edit_succeeded() const -> std::optional<bool> {
    return last_edit_succeeded_;
}

auto PlannerController::selected_type() const -> std::optional<lispb::schema::TypeId> {
    return document_ && document_->valid() ? document_->find_type(module_, declaration_)
                                           : std::nullopt;
}

auto PlannerController::selected_module() const -> std::string_view {
    return module_;
}
auto PlannerController::selected_declaration() const -> std::string_view {
    return declaration_;
}
auto PlannerController::active_variant() const -> VariantId {
    return active_variant_;
}
auto PlannerController::comparison_variant(bool first) const -> VariantId {
    return first ? comparison_a_ : comparison_b_;
}

void PlannerController::submit(Transaction transaction) {
    pending_.emplace_back([this, transaction = std::move(transaction)] {
        assert(document_ && document_->valid());
        last_edit_succeeded_ = document_->commit(transaction);
        analysis_needed_ = true;
        if (*last_edit_succeeded_) {
            reconcile();
        }
    });
}

void PlannerController::select_declaration(std::string module, std::string name) {
    pending_.emplace_back([this, module = std::move(module), name = std::move(name)]() mutable {
        select_now(std::move(module), std::move(name));
    });
}

void PlannerController::select_variant(VariantId variant) {
    pending_.emplace_back([this, variant] {
        assert(document_ && document_->variants().contains(variant));
        if (active_variant_ != variant) {
            active_variant_ = variant;
            if (comparison_b_follows_active_) {
                comparison_b_ = variant;
            }
            analysis_needed_ = true;
        }
    });
}

void PlannerController::select_created_declaration(std::string module) {
    pending_.emplace_back([this, module = std::move(module)] {
        if (last_edit_succeeded_.value_or(false)) {
            auto const declarations{document_->query().module(module).declarations()};
            assert(!declarations.empty());
            select_now(module, std::string{declarations.back().name()});
        }
    });
}

void PlannerController::select_created_variant() {
    pending_.emplace_back([this] {
        if (last_edit_succeeded_.value_or(false)) {
            active_variant_ = document_->variants().ids().back();
            if (comparison_b_follows_active_) {
                comparison_b_ = active_variant_;
            }
            analysis_needed_ = true;
        }
    });
}

void PlannerController::select_comparison_variant(bool first, VariantId variant) {
    pending_.emplace_back([this, first, variant] {
        assert(document_ && document_->variants().contains(variant));
        auto& selected{first ? comparison_a_ : comparison_b_};
        if (selected != variant) {
            selected = variant;
            analysis_needed_ = true;
        }
        if (!first) {
            comparison_b_follows_active_ = false;
        }
    });
}

void PlannerController::undo() {
    pending_.emplace_back([this] {
        assert(document_);
        if (document_->undo()) {
            reconcile();
            analysis_needed_ = true;
        }
    });
}

void PlannerController::redo() {
    pending_.emplace_back([this] {
        assert(document_);
        if (document_->redo()) {
            reconcile();
            analysis_needed_ = true;
        }
    });
}

void PlannerController::save() {
    pending_.emplace_back([this] {
        assert(document_);
        document_->save();
    });
}

void PlannerController::save_as(std::filesystem::path path, std::string target) {
    pending_.emplace_back([this, path = std::move(path), target = std::move(target)] {
        assert(document_);
        document_->save_as(path, target);
    });
}

void PlannerController::open(std::filesystem::path path, std::string target) {
    pending_.emplace_back([this, path = std::move(path), target = std::move(target)] {
        if (document_ && document_->dirty()) {
            editor_.record_error("Save or undo the current edits before opening another project.");
            return;
        }
        adopt(editor_.load(path, target));
    });
}

void PlannerController::new_document() {
    pending_.emplace_back([this] {
        if (document_ && document_->dirty()) {
            editor_.record_error("Save or undo the current edits before creating another project.");
            return;
        }
        adopt(editor_.from_source(R"((module game :header "Game.h" :backend standard-library))"));
    });
}

auto PlannerController::drain() -> bool {
    last_edit_succeeded_.reset();
    if (pending_.empty() && !analysis_needed_) {
        return false;
    }
    for (auto& action : pending_) {
        action();
    }
    pending_.clear();
    if (std::exchange(analysis_needed_, false)) {
        analyze();
    }
    return true;
}

void PlannerController::select_now(std::string module, std::string name) {
    if (!name.empty() && (!document_ || !document_->valid() ||
                          !document_->query().module(module).declaration(name).exists())) {
        return;
    }
    if (module_ == module && declaration_ == name) {
        return;
    }
    module_ = std::move(module);
    declaration_ = std::move(name);
    declaration_id_.reset();
    if (auto const type{selected_type()}) {
        declaration_id_ = document_->find_declaration(document_->types().identity(*type));
    }
    analysis_needed_ = true;
}

void PlannerController::adopt(Document document) {
    document_ = std::move(document);
    module_.clear();
    declaration_.clear();
    declaration_id_.reset();
    active_variant_ = {};
    comparison_a_ = {};
    comparison_b_ = {};
    comparison_b_follows_active_ = true;
    analysis_needed_ = true;
    if (document_->valid()) {
        for (auto const& module : document_->query().modules()) {
            auto const declarations{module.declarations()};
            if (!declarations.empty()) {
                select_now(std::string{module.name()}, std::string{declarations.front().name()});
                break;
            }
        }
    }
}

void PlannerController::reconcile() {
    if (declaration_id_) {
        if (auto const type{document_->declaration_type(*declaration_id_)}) {
            auto const& graph{document_->types()};
            module_ = graph.strings().view(graph.module_name(*type));
            declaration_ = graph.strings().view(graph.name(*type));
        }
    }
    auto const& variants{document_->variants()};
    if (!variants.contains(active_variant_)) {
        active_variant_ = {};
    }
    if (!variants.contains(comparison_a_)) {
        comparison_a_ = {};
    }
    if (!variants.contains(comparison_b_)) {
        comparison_b_ = {};
    }
    if (comparison_b_follows_active_) {
        comparison_b_ = active_variant_;
    }
}

void PlannerController::analyze() {
    analysis_.clear();
    auto const type{selected_type()};
    if (!type) {
        return;
    }

    auto const& settings{document_->analysis_settings()};
    AnalysisRequest request{};
    request.type = *type;
    request.abi = &settings.abi;
    request.variant = active_variant_;
    request.element_count = settings.element_count;
    request.default_capacity = settings.default_soa_capacity;
    request.multiplicity = settings.access_multiplicity;
    request.allocation_strategy = settings.soa_allocation_strategy;
    if (declaration_id_) {
        if (auto const found{settings.access_sets.find(*declaration_id_)};
            found != settings.access_sets.end()) {
            request.accesses = &found->second;
        }
        if (auto const found{settings.varint_distributions.find(*declaration_id_)};
            found != settings.varint_distributions.end()) {
            request.varint_distribution = found->second;
        }
        if (auto const found{settings.union_distributions.find(*declaration_id_)};
            found != settings.union_distributions.end()) {
            request.union_distribution = found->second;
        }
        if (auto const found{settings.tagged_union_distributions.find(*declaration_id_)};
            found != settings.tagged_union_distributions.end()) {
            request.tagged_union_distribution = found->second;
        }
    }

    std::array requests{request, request, request, request};
    requests[1].abi = settings.comparison_abi ? &*settings.comparison_abi : &settings.abi;
    requests[2].variant = comparison_a_;
    requests[3].variant = comparison_b_;
    AnalysisContext context{*document_, analysis_};
    Analyzer::analyze(context, requests);
    ++analysis_count_;
}
}
