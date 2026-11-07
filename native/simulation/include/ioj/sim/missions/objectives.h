#pragma once

#include <ioj/levels/objective_name.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/levels/objective_wins_level.h>
#include <ioj/levels/team.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/event_handle.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/missions/objective_membership_index.h>
#include <ioj/sim/missions/objective_prerequisite.h>
#include <ioj/sim/missions/objective_roots.h>

#include <cassert>
#include <span>
#include <vector>

namespace ioj::sim {
class Objectives {
  public:
    struct CompilationView {
        std::span<levels::ObjectiveName> identifiers;
        std::span<std::string> display_names;
        std::span<Team> assigned_teams;
        std::span<ObjectiveWinsLevel> wins_level;
        std::span<ObjectiveRoots> roots;
        std::span<IndexSpan> leaf_ranges;
        std::span<IndexSpan> composite_ranges;
    };

    Objectives() = default;
    explicit Objectives(std::size_t count);

    void reset_progress();
    auto is_completed(ObjectiveId id) const -> bool;
    auto inactive() const -> std::span<ObjectiveId const> { return inactive_; }
    auto active() const -> std::span<ObjectiveId const> { return active_; }
    auto completed() const -> std::span<ObjectiveId const> { return completed_; }
    // Positions must be unique and ascending. Returned spans borrow the destination list.
    auto activate(std::span<ObjectiveMembershipIndex const> positions)
        -> std::span<ObjectiveId const>;
    auto complete(std::span<ObjectiveMembershipIndex const> positions)
        -> std::span<ObjectiveId const>;

    auto compilation_view() -> CompilationView;
    auto size() const -> std::size_t { return identifiers_.size(); }
    auto name(ObjectiveId id) const -> levels::ObjectiveName const& { return at(identifiers_, id); }
    auto title(ObjectiveId id) const -> std::string const& { return at(display_names_, id); }
    auto team(ObjectiveId id) const -> Team { return at(assigned_teams_, id); }
    auto roots(ObjectiveId id) const -> ObjectiveRoots const& { return at(roots_, id); }
    auto leaves(ObjectiveId id) const -> IndexSpan { return at(leaf_ranges_, id); }
    auto composites(ObjectiveId id) const -> IndexSpan { return at(composite_ranges_, id); }
    auto prerequisites(ObjectiveId id) const -> std::span<ObjectivePrerequisite const>;
    void add_prerequisites(ObjectiveId id, std::span<ObjectivePrerequisite const> prerequisites);
    auto activation_events(ObjectiveId id) const -> std::span<EventHandle const>;
    void add_activation_events(ObjectiveId id, std::span<EventHandle const> events);
    auto wins_level(ObjectiveId id) const -> ObjectiveWinsLevel { return at(wins_level_, id); }
  private:
    auto is_valid(ObjectiveId id) const -> bool { return id.is_valid() && id.index() < size(); }

    template <typename Column>
    auto at(Column& column, ObjectiveId const id) const -> decltype(column[id.index()]) {
        assert(is_valid(id));
        return column[id.index()];
    }

    // Immutable
    std::vector<levels::ObjectiveName> identifiers_;
    std::vector<std::string> display_names_;
    std::vector<Team> assigned_teams_;
    std::vector<ObjectiveWinsLevel> wins_level_;
    std::vector<ObjectiveRoots> roots_;
    std::vector<IndexSpan> leaf_ranges_;
    std::vector<IndexSpan> composite_ranges_;
    std::vector<IndexSpan> prerequisite_ranges_;
    std::vector<ObjectivePrerequisite> prerequisites_;
    std::vector<IndexSpan> activation_event_ranges_;
    std::vector<EventHandle> activation_events_;

    // Mutable
    std::vector<ObjectiveId> inactive_;
    std::vector<ObjectiveId> active_;
    std::vector<ObjectiveId> completed_;
};
}
