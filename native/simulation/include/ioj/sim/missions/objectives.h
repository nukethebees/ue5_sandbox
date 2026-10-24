#pragma once

#include <ioj/levels/objective_name.h>
#include <ioj/levels/team.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/missions/objective_roots.h>
#include <ioj/sim/sim_tick.h>

#include <cassert>
#include <optional>
#include <vector>

namespace ioj::sim {
// Every accessor requires an ObjectiveId belonging to this collection.
class Objectives {
  public:
    void resize(std::size_t count);
    auto size() const -> std::size_t { return names_.size(); }
    auto name(ObjectiveId id) -> levels::ObjectiveName& {
        assert(id.index() < size());
        return names_[id.index()];
    }
    auto name(ObjectiveId id) const -> levels::ObjectiveName const& {
        assert(id.index() < size());
        return names_[id.index()];
    }
    auto title(ObjectiveId id) -> std::string& {
        assert(id.index() < size());
        return titles_[id.index()];
    }
    auto title(ObjectiveId id) const -> std::string const& {
        assert(id.index() < size());
        return titles_[id.index()];
    }
    auto team(ObjectiveId id) -> Team& {
        assert(id.index() < size());
        return teams_[id.index()];
    }
    auto team(ObjectiveId id) const -> Team {
        assert(id.index() < size());
        return teams_[id.index()];
    }
    auto roots(ObjectiveId id) -> ObjectiveRoots& {
        assert(id.index() < size());
        return roots_[id.index()];
    }
    auto roots(ObjectiveId id) const -> ObjectiveRoots const& {
        assert(id.index() < size());
        return roots_[id.index()];
    }
    auto leaves(ObjectiveId id) -> IndexSpan& {
        assert(id.index() < size());
        return leaves_[id.index()];
    }
    auto leaves(ObjectiveId id) const -> IndexSpan {
        assert(id.index() < size());
        return leaves_[id.index()];
    }
    auto composites(ObjectiveId id) -> IndexSpan& {
        assert(id.index() < size());
        return composites_[id.index()];
    }
    auto composites(ObjectiveId id) const -> IndexSpan {
        assert(id.index() < size());
        return composites_[id.index()];
    }
    auto prerequisites(ObjectiveId id) -> std::uint32_t& {
        assert(id.index() < size());
        return prerequisites_[id.index()];
    }
    auto prerequisites(ObjectiveId id) const -> std::uint32_t {
        assert(id.index() < size());
        return prerequisites_[id.index()];
    }
    auto dependents(ObjectiveId id) -> IndexSpan& {
        assert(id.index() < size());
        return dependents_[id.index()];
    }
    auto dependents(ObjectiveId id) const -> IndexSpan {
        assert(id.index() < size());
        return dependents_[id.index()];
    }
    auto observers(ObjectiveId id) -> IndexSpan& {
        assert(id.index() < size());
        return observers_[id.index()];
    }
    auto observers(ObjectiveId id) const -> IndexSpan {
        assert(id.index() < size());
        return observers_[id.index()];
    }
    auto team_results(ObjectiveId id) -> IndexSpan& {
        assert(id.index() < size());
        return team_results_[id.index()];
    }
    auto team_results(ObjectiveId id) const -> IndexSpan {
        assert(id.index() < size());
        return team_results_[id.index()];
    }
    auto outcome(ObjectiveId id) -> std::optional<ObjectiveOutcome>& {
        assert(id.index() < size());
        return outcomes_[id.index()];
    }
    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome> {
        assert(id.index() < size());
        return outcomes_[id.index()];
    }
    auto activation_tick(ObjectiveId id) -> SimTick& {
        assert(id.index() < size());
        return activation_ticks_[id.index()];
    }
    auto activation_tick(ObjectiveId id) const -> SimTick {
        assert(id.index() < size());
        return activation_ticks_[id.index()];
    }
    auto membership_position(ObjectiveId id) -> std::uint32_t& {
        assert(id.index() < size());
        return membership_positions_[id.index()];
    }
    auto membership_position(ObjectiveId id) const -> std::uint32_t {
        assert(id.index() < size());
        return membership_positions_[id.index()];
    }
    auto dirty(ObjectiveId id) -> std::uint8_t& {
        assert(id.index() < size());
        return dirty_flags_[id.index()];
    }
    auto dirty(ObjectiveId id) const -> std::uint8_t {
        assert(id.index() < size());
        return dirty_flags_[id.index()];
    }
  private:
    std::vector<levels::ObjectiveName> names_;
    std::vector<std::string> titles_;
    std::vector<Team> teams_;
    std::vector<ObjectiveRoots> roots_;
    std::vector<IndexSpan> leaves_;
    std::vector<IndexSpan> composites_;
    std::vector<std::uint32_t> prerequisites_;
    std::vector<IndexSpan> dependents_;
    std::vector<IndexSpan> observers_;
    std::vector<IndexSpan> team_results_;
    std::vector<std::optional<ObjectiveOutcome>> outcomes_;
    std::vector<SimTick> activation_ticks_;
    std::vector<std::uint32_t> membership_positions_;
    std::vector<std::uint8_t> dirty_flags_;
};
}
