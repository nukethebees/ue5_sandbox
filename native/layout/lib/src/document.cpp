#include <ioj/layout/document.hpp>

#include <ioj/layout/editor_session.hpp>

#include <codegen/sexpr/source_error.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace ioj::layout {
namespace {
void validate_variant_paths(DocumentState const& state) {
    auto const& types{state.types};
    auto const& strings{state.storage().strings};
    for (auto const variant : state.variants.ids()) {
        auto const& settings{state.variants.settings(variant)};
        auto const count{settings.field_types.size()};
        for (std::size_t row{}; row < count; ++row) {
            auto const path{strings.view(settings.field_names[row])};
            bool found{};
            for (auto const type : types.types()) {
                auto const declaration{state.declaration_ids.find(types.source_declaration(type))};
                if (declaration == state.declaration_ids.end() ||
                    declaration->second != settings.field_types[row]) {
                    continue;
                }
                auto const& placements{types.bit_layout(type).placements};
                found = std::ranges::any_of(placements, [&](auto const& placement) {
                    return !placement.reserved && placement.source_path == path;
                });
                break;
            }
            if (!found) {
                throw std::invalid_argument{"Unknown packed member path '" + std::string{path} +
                                            "'"};
            }
        }
    }
}
}
Document::Document(EditorSession& session, DocumentId id)
    : session_{&session}
    , id_{id} {}
Document::~Document() {
    if (id_.value != 0) {
        session_->release_document(id_);
    }
}
Document::Document(Document&& other) noexcept
    : session_{other.session_}
    , id_{std::exchange(other.id_, DocumentId{})} {}
auto Document::operator=(Document&& other) noexcept -> Document& {
    if (this != &other) {
        if (id_.value != 0) {
            session_->release_document(id_);
        }
        session_ = other.session_;
        id_ = std::exchange(other.id_, DocumentId{});
    }
    return *this;
}
auto Document::state() const -> DocumentState& {
    return session_->state(id_);
}
auto Document::valid() const -> bool {
    return id_.value != 0 && state().valid;
}
auto Document::id() const -> DocumentId {
    return id_;
}
auto Document::query() const -> lispb::schema::DocumentQuery {
    assert(valid());
    auto const& current{state()};
    return {current.storage().declarations, current.modules};
}
auto Document::types() const -> lispb::schema::TypeGraph const& {
    assert(valid());
    return state().types;
}
auto Document::variants() const -> Variants const& {
    return state().variants;
}
auto Document::analysis_settings() const -> AnalysisSettings const& {
    return state().analysis_settings;
}
auto Document::find_type(std::string_view module, std::string_view name) const
    -> std::optional<lispb::schema::TypeId> {
    auto const& graph{types()};
    auto const& strings{state().storage().strings};
    for (auto const type : graph.types()) {
        if (strings.view(graph.module_name(type)) == module &&
            strings.view(graph.name(type)) == name) {
            return type;
        }
    }
    return {};
}
auto Document::find_declaration(lispb::schema::TypeIdentity const& identity) const
    -> std::optional<lispb::schema::DeclarationId> {
    auto const type{types().find(identity)};
    if (!type) {
        return {};
    }
    auto const& declarations{state().declaration_ids};
    auto const found{declarations.find(types().source_declaration(*type))};
    return found == declarations.end() ? std::nullopt : std::optional{found->second};
}
auto Document::declaration_type(lispb::schema::DeclarationId declaration) const
    -> std::optional<lispb::schema::TypeId> {
    auto const& graph{types()};
    auto const& identities{state().declaration_ids};
    for (auto const type : graph.types()) {
        auto const found{identities.find(graph.source_declaration(type))};
        if (found != identities.end() && found->second == declaration) {
            return type;
        }
    }
    return {};
}
auto Document::session() const -> EditorSession& {
    return *session_;
}
auto Document::transaction() const -> Transaction {
    assert(valid());
    return {*session_, id_};
}
auto Document::revision() const -> Revision {
    return state().revision;
}
auto Document::can_undo() const -> bool {
    return state().position != 0;
}
auto Document::can_redo() const -> bool {
    return state().position < state().transaction_ends.size();
}

auto Document::reconstruct(std::size_t position) -> lispb::codegen::Manifest {
    auto& current{state()};
    current.reset_workspace();
    session_->set_instruction(std::nullopt);
    current.restore_baseline();
    auto candidate{current.manifest()};
    if (position != 0) {
        current.history.apply(current, candidate, current.transaction_ends[position - 1]);
    }
    return candidate;
}
auto Document::replay(std::size_t position) -> bool {
    auto& current{state()};
    try {
        auto candidate{reconstruct(position)};
        current.types = lispb::schema::resolve_type_graph(candidate);
        validate_variant_paths(current);
        current.modules = std::move(candidate.modules);
        current.position = position;
        current.valid = true;
        session_->capture_diagnostics(id_);
        session_->set_instruction(std::nullopt);
        return true;
    } catch (lispb::codegen::sexpr::SourceError const&) {
        session_->capture_diagnostics(id_);
    } catch (std::invalid_argument const& error) {
        session_->capture_diagnostics(id_);
        session_->record_error(error.what());
    }
    current.valid = false;
    session_->set_instruction(std::nullopt);
    return false;
}
auto Document::commit(Transaction const& transaction) -> bool {
    assert(valid());
    assert(transaction.session_ == session_ && transaction.owner_ == id_);
    session_->begin_operation(id_);
    if (transaction.kinds_.empty()) {
        return true;
    }

    auto& current{state()};
    auto const position{current.position};
    try {
        auto candidate{reconstruct(position)};
        auto const previous_modules{candidate.modules};
        auto const previous_sources{current.source_order};
        current.changed = false;
        transaction.apply(current, candidate, transaction.kinds_.size());
        auto const changed{current.changed || candidate.modules != previous_modules ||
                           current.source_order != previous_sources};
        current.types = lispb::schema::resolve_type_graph(candidate);
        validate_variant_paths(current);
        current.modules = std::move(candidate.modules);
        current.valid = true;
        session_->capture_diagnostics(id_);
        session_->set_instruction(std::nullopt);
        if (!changed) {
            return true;
        }

        auto const retained{position == 0 ? 0 : current.transaction_ends[position - 1]};
        current.history.truncate(retained);
        current.transaction_ends.resize(position);
        current.history.append(transaction);
        current.transaction_ends.push_back(current.history.kinds_.size());
        ++current.position;
        ++current.revision;
        return true;
    } catch (lispb::codegen::sexpr::SourceError const&) {
        session_->capture_diagnostics(id_);
    } catch (std::invalid_argument const& error) {
        session_->capture_diagnostics(id_);
        session_->record_error(error.what());
    }

    // Rebuild the accepted prefix; failed instructions never enter the history.
    [[maybe_unused]] auto const restored{replay(position)};
    assert(restored);
    return false;
}
auto Document::undo() -> bool {
    if (!can_undo()) {
        return false;
    }
    session_->begin_operation(id_);
    auto const result{replay(state().position - 1)};
    if (result) {
        ++state().revision;
    }
    return result;
}
auto Document::redo() -> bool {
    if (!can_redo()) {
        return false;
    }
    session_->begin_operation(id_);
    auto const result{replay(state().position + 1)};
    if (result) {
        ++state().revision;
    }
    return result;
}
}
