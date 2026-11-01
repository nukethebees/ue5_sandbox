#pragma once

#include <ioj/levels/objective_name.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/levels/objective_wins_level.h>
#include <ioj/levels/team.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/missions/objective_membership_index.h>
#include <ioj/sim/missions/objective_roots.h>

#include <cassert>
#include <span>
#include <vector>

namespace ioj::sim {
class Objectives {
  public:
    Objectives() = default;
    explicit Objectives(std::size_t count);

    auto is_completed(ObjectiveId id) const -> bool;
    auto inactive() const -> std::span<ObjectiveId const> { return inactive_; }
    auto active() const -> std::span<ObjectiveId const> { return active_; }
    auto completed() const -> std::span<ObjectiveId const> { return completed_; }
    void activate(ObjectiveId id);
    void complete(ObjectiveId id);

    auto is_valid(ObjectiveId id) const -> bool { return id.is_valid() && id.index() < size(); }
    auto size() const -> std::size_t { return identifiers_.size(); }
    auto name(ObjectiveId id) -> levels::ObjectiveName& { return at(identifiers_, id); }
    auto name(ObjectiveId id) const -> levels::ObjectiveName const& { return at(identifiers_, id); }
    auto title(ObjectiveId id) -> std::string& { return at(display_names_, id); }
    auto title(ObjectiveId id) const -> std::string const& { return at(display_names_, id); }
    auto team(ObjectiveId id) -> Team& { return at(assigned_teams_, id); }
    auto team(ObjectiveId id) const -> Team { return at(assigned_teams_, id); }
    auto roots(ObjectiveId id) -> ObjectiveRoots& { return at(roots_, id); }
    auto roots(ObjectiveId id) const -> ObjectiveRoots const& { return at(roots_, id); }
    auto leaves(ObjectiveId id) -> IndexSpan& { return at(leaf_ranges_, id); }
    auto composites(ObjectiveId id) -> IndexSpan& { return at(composite_ranges_, id); }
    auto prerequisite_objectives(ObjectiveId id) const -> std::span<ObjectiveId const>;
    auto prerequisite_outcomes(ObjectiveId id) const -> std::span<ObjectiveOutcome const>;
    // Append each objective's prerequisites consecutively during compilation.
    void add_prerequisite(ObjectiveId id, ObjectiveId prerequisite, ObjectiveOutcome outcome);
    auto wins_level(ObjectiveId id) -> ObjectiveWinsLevel& { return at(wins_level_, id); }
  private:
    void remove_member(std::vector<ObjectiveId>& collection, ObjectiveId id);

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
    std::vector<ObjectiveId> prerequisite_objectives_;
    std::vector<ObjectiveOutcome> prerequisite_outcomes_;

    // Mutable
    std::vector<ObjectiveMembershipIndex> membership_positions_;

    std::vector<ObjectiveId> inactive_;
    std::vector<ObjectiveId> active_;
    std::vector<ObjectiveId> completed_;
};
}
