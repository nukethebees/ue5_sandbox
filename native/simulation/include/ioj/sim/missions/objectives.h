#pragma once

#include <ioj/levels/objective_name.h>
#include <ioj/levels/objective_wins_level.h>
#include <ioj/levels/team.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/missions/objective_membership_index.h>
#include <ioj/sim/missions/objective_prerequisite_count.h>
#include <ioj/sim/missions/objective_roots.h>
#include <ioj/sim/sim_tick.h>

#include <cassert>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
class Objectives {
  public:
    Objectives() = default;
    explicit Objectives(std::size_t count);

    auto is_active(ObjectiveId id) const -> bool;
    auto inactive() const -> std::span<ObjectiveId const> { return inactive_; }
    auto active() const -> std::span<ObjectiveId const> { return active_; }
    auto completed() const -> std::span<ObjectiveId const> { return completed_; }
    void activate(ObjectiveId id, SimTick tick);
    // Record an outcome during evaluation; commit its membership change afterward.
    void resolve(ObjectiveId id, ObjectiveOutcome outcome);
    void commit_completion(ObjectiveMembershipIndex position);

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
    auto leaves(ObjectiveId id) const -> IndexSpan { return at(leaf_ranges_, id); }
    auto composites(ObjectiveId id) -> IndexSpan& { return at(composite_ranges_, id); }
    auto composites(ObjectiveId id) const -> IndexSpan { return at(composite_ranges_, id); }
    auto prerequisites(ObjectiveId id) -> ObjectivePrerequisiteCount& {
        return at(prerequisites_, id);
    }
    auto prerequisites(ObjectiveId id) const -> ObjectivePrerequisiteCount {
        return at(prerequisites_, id);
    }
    auto dependents(ObjectiveId id) -> IndexSpan& { return at(dependent_ranges_, id); }
    auto dependents(ObjectiveId id) const -> IndexSpan { return at(dependent_ranges_, id); }
    auto wins_level(ObjectiveId id) -> ObjectiveWinsLevel& { return at(wins_level_, id); }
    auto wins_level(ObjectiveId id) const -> ObjectiveWinsLevel { return at(wins_level_, id); }
    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome> {
        return at(outcomes_, id);
    }
    auto activation_tick(ObjectiveId id) const -> SimTick { return at(activated_at_, id); }
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
    std::vector<IndexSpan> dependent_ranges_;

    // Mutable
    std::vector<ObjectivePrerequisiteCount> prerequisites_;
    std::vector<std::optional<ObjectiveOutcome>> outcomes_;
    std::vector<SimTick> activated_at_;
    std::vector<ObjectiveMembershipIndex> membership_positions_;

    std::vector<ObjectiveId> inactive_;
    std::vector<ObjectiveId> active_;
    std::vector<ObjectiveId> completed_;
};
}
