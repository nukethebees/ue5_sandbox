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
class Objectives {
  public:
    Objectives() = default;
    explicit Objectives(std::size_t count);

    auto is_valid(ObjectiveId id) const -> bool { return id.is_valid() && id.index() < size(); }
    auto size() const -> std::size_t { return names_.size(); }
    auto name(ObjectiveId id) -> levels::ObjectiveName& {
        assert(is_valid(id));
        return names_[id.index()];
    }
    auto name(ObjectiveId id) const -> levels::ObjectiveName const& {
        assert(is_valid(id));
        return names_[id.index()];
    }
    auto title(ObjectiveId id) -> std::string& {
        assert(is_valid(id));
        return titles_[id.index()];
    }
    auto title(ObjectiveId id) const -> std::string const& {
        assert(is_valid(id));
        return titles_[id.index()];
    }
    auto team(ObjectiveId id) -> Team& {
        assert(is_valid(id));
        return teams_[id.index()];
    }
    auto team(ObjectiveId id) const -> Team {
        assert(is_valid(id));
        return teams_[id.index()];
    }
    auto roots(ObjectiveId id) -> ObjectiveRoots& {
        assert(is_valid(id));
        return roots_[id.index()];
    }
    auto roots(ObjectiveId id) const -> ObjectiveRoots const& {
        assert(is_valid(id));
        return roots_[id.index()];
    }
    auto leaves(ObjectiveId id) -> IndexSpan& {
        assert(is_valid(id));
        return leaves_[id.index()];
    }
    auto leaves(ObjectiveId id) const -> IndexSpan {
        assert(is_valid(id));
        return leaves_[id.index()];
    }
    auto composites(ObjectiveId id) -> IndexSpan& {
        assert(is_valid(id));
        return composites_[id.index()];
    }
    auto composites(ObjectiveId id) const -> IndexSpan {
        assert(is_valid(id));
        return composites_[id.index()];
    }
    auto prerequisites(ObjectiveId id) -> std::uint32_t& {
        assert(is_valid(id));
        return prerequisites_[id.index()];
    }
    auto prerequisites(ObjectiveId id) const -> std::uint32_t {
        assert(is_valid(id));
        return prerequisites_[id.index()];
    }
    auto dependents(ObjectiveId id) -> IndexSpan& {
        assert(is_valid(id));
        return dependents_[id.index()];
    }
    auto dependents(ObjectiveId id) const -> IndexSpan {
        assert(is_valid(id));
        return dependents_[id.index()];
    }
    auto observers(ObjectiveId id) -> IndexSpan& {
        assert(is_valid(id));
        return observers_[id.index()];
    }
    auto observers(ObjectiveId id) const -> IndexSpan {
        assert(is_valid(id));
        return observers_[id.index()];
    }
    auto team_results(ObjectiveId id) -> IndexSpan& {
        assert(is_valid(id));
        return team_results_[id.index()];
    }
    auto team_results(ObjectiveId id) const -> IndexSpan {
        assert(is_valid(id));
        return team_results_[id.index()];
    }
    auto outcome(ObjectiveId id) -> std::optional<ObjectiveOutcome>& {
        assert(is_valid(id));
        return outcomes_[id.index()];
    }
    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome> {
        assert(is_valid(id));
        return outcomes_[id.index()];
    }
    auto activation_tick(ObjectiveId id) -> SimTick& {
        assert(is_valid(id));
        return activation_ticks_[id.index()];
    }
    auto activation_tick(ObjectiveId id) const -> SimTick {
        assert(is_valid(id));
        return activation_ticks_[id.index()];
    }
    auto membership_position(ObjectiveId id) -> std::uint32_t& {
        assert(is_valid(id));
        return membership_positions_[id.index()];
    }
    auto membership_position(ObjectiveId id) const -> std::uint32_t {
        assert(is_valid(id));
        return membership_positions_[id.index()];
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
};
}
