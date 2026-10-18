#include <ioj/layout/document.hpp>

#include <ioj/layout/editor_session.hpp>

#include <codegen/sexpr/source_error.h>

#include <cassert>
#include <stdexcept>

namespace ioj::layout {
Document::Document(EditorSession& session)
    : state_{session, session.allocate_document_id()} {}
Document::~Document() = default;
Document::Document(Document&&) noexcept = default;
auto Document::operator=(Document&&) noexcept -> Document& = default;
auto Document::valid() const -> bool {
    return state_.valid;
}
auto Document::query() const -> lispb::schema::DocumentQuery {
    assert(valid());
    return lispb::schema::DocumentQuery{state_.storage().declarations, state_.modules};
}
auto Document::types() const -> lispb::schema::TypeGraph const& {
    assert(valid());
    return state_.types;
}
auto Document::variants() const -> Variants const& {
    return state_.variants;
}
auto Document::find_type(std::string_view module, std::string_view name) const
    -> std::optional<lispb::schema::TypeId> {
    auto const& graph{types()};
    auto const& strings{state_.storage().strings};
    for (auto const type : graph.types()) {
        if (strings.view(graph.module_name(type)) == module &&
            strings.view(graph.name(type)) == name) {
            return type;
        }
    }
    return std::nullopt;
}
auto Document::find_declaration(lispb::schema::TypeIdentity const& identity) const
    -> std::optional<lispb::schema::DeclarationId> {
    auto const type{types().find(identity)};
    if (!type) {
        return std::nullopt;
    }
    auto const found{state_.declaration_ids.find(types().source_declaration(*type))};
    if (found == state_.declaration_ids.end()) {
        return std::nullopt;
    }
    return found->second;
}
auto Document::declaration_type(lispb::schema::DeclarationId declaration) const
    -> std::optional<lispb::schema::TypeId> {
    auto const& graph{types()};
    for (auto const type : graph.types()) {
        auto const found{state_.declaration_ids.find(graph.source_declaration(type))};
        if (found != state_.declaration_ids.end() && found->second == declaration) {
            return type;
        }
    }
    return std::nullopt;
}
auto Document::session() const -> EditorSession& {
    return *state_.session;
}
auto Document::transaction() const -> Transaction {
    assert(valid());
    return Transaction{session(), state_.id, state_.revision};
}
auto Document::revision() const -> Revision {
    return state_.revision;
}
auto Document::can_undo() const -> bool {
    return state_.position != 0;
}
auto Document::can_redo() const -> bool {
    return state_.position < state_.starts.size();
}

auto Document::commit(Transaction const& transaction) -> bool {
    assert(valid());
    assert(transaction.session_ == state_.session && transaction.owner_ == state_.id);
    auto& state{state_};
    state.session->begin_operation();
    if (transaction.revision_ != state.revision) {
        state.session->record_error("Transaction belongs to an obsolete document revision");
        return false;
    }

    auto candidate{state.manifest()};
    auto previous_variants{state.variants};
    auto previous_sources{state.source_order};
    try {
        transaction.apply(state, candidate);
        auto const schema_changed{candidate.modules != state.modules};
        auto const variants_changed{previous_variants != state.variants};
        auto const sources_changed{previous_sources != state.source_order};
        if (!schema_changed && !variants_changed && !sources_changed) {
            return true;
        }

        std::optional<lispb::schema::TypeGraph> resolved;
        if (schema_changed) {
            resolved.emplace(lispb::schema::resolve_type_graph(candidate));
        }
        auto const& before{state.modules};
        auto const& after{candidate.modules};
        std::size_t first{};
        while (first < before.size() && first < after.size() && before[first] == after[first]) {
            ++first;
        }
        auto old_end{before.size()};
        auto new_end{after.size()};
        while (old_end > first && new_end > first && before[old_end - 1] == after[new_end - 1]) {
            --old_end;
            --new_end;
        }

        if (state.position < state.starts.size()) {
            state.versions.resize(state.offsets[state.position]);
            state.starts.resize(state.position);
            state.removed.resize(state.position);
            state.offsets.resize(state.position);
            state.counts.resize(state.position);
            state.variant_versions.resize(state.position);
            state.source_versions.resize(state.position);
        }
        state.starts.push_back(first);
        state.removed.push_back(old_end - first);
        state.offsets.push_back(state.versions.size());
        state.counts.push_back(new_end - first);
        state.versions.insert(state.versions.end(),
                              after.begin() + static_cast<std::ptrdiff_t>(first),
                              after.begin() + static_cast<std::ptrdiff_t>(new_end));
        state.variant_versions.push_back(
            variants_changed ? std::make_shared<Variants const>(state.variants) : nullptr);
        state.source_versions.push_back(
            sources_changed
                ? std::make_shared<std::vector<lispb::codegen::sexpr::SourceFileId> const>(
                      state.source_order)
                : nullptr);
        state.modules = std::move(candidate.modules);
        if (resolved) {
            state.types = std::move(*resolved);
        }
        ++state.position;
        ++state.revision;
        return true;
    } catch (lispb::codegen::sexpr::SourceError const&) {
        // The parser already appended its diagnostic.
    } catch (std::invalid_argument const& error) {
        state.session->record_error(error.what());
    }

    state.variants = std::move(previous_variants);
    state.source_order = std::move(previous_sources);
    return false;
}
auto Document::replay(std::size_t position) -> bool {
    auto& state{state_};
    state.session->begin_operation();
    auto candidate{state.manifest()};
    candidate.modules = state.baseline;
    Variants const* variants{};
    std::vector<lispb::codegen::sexpr::SourceFileId> const* sources{&state.baseline_sources};
    for (std::size_t index{}; index < position; ++index) {
        auto const start{candidate.modules.begin() +
                         static_cast<std::ptrdiff_t>(state.starts[index])};
        candidate.modules.erase(start, start + static_cast<std::ptrdiff_t>(state.removed[index]));
        auto const versions{
            std::span{state.versions}.subspan(state.offsets[index], state.counts[index])};
        candidate.modules.insert(candidate.modules.begin() +
                                     static_cast<std::ptrdiff_t>(state.starts[index]),
                                 versions.begin(),
                                 versions.end());
        if (state.variant_versions[index]) {
            variants = state.variant_versions[index].get();
        }
        if (state.source_versions[index]) {
            sources = state.source_versions[index].get();
        }
    }
    if (candidate.modules != state.modules) {
        state.types = lispb::schema::resolve_type_graph(candidate);
    }
    state.modules = std::move(candidate.modules);
    state.variants = variants ? *variants : Variants{};
    state.source_order = *sources;
    state.position = position;
    ++state.revision;
    return true;
}
auto Document::undo() -> bool {
    return can_undo() && replay(state_.position - 1);
}
auto Document::redo() -> bool {
    return can_redo() && replay(state_.position + 1);
}
}
