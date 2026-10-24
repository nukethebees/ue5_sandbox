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
    auto name(ObjectiveId id) -> levels::ObjectiveName& { return at(names_, id); }
    auto name(ObjectiveId id) const -> levels::ObjectiveName const& { return at(names_, id); }
    auto title(ObjectiveId id) -> std::string& { return at(titles_, id); }
    auto title(ObjectiveId id) const -> std::string const& { return at(titles_, id); }
    auto team(ObjectiveId id) -> Team& { return at(teams_, id); }
    auto team(ObjectiveId id) const -> Team { return at(teams_, id); }
    auto roots(ObjectiveId id) -> ObjectiveRoots& { return at(roots_, id); }
    auto roots(ObjectiveId id) const -> ObjectiveRoots const& { return at(roots_, id); }
    auto leaves(ObjectiveId id) -> IndexSpan& { return at(leaves_, id); }
    auto leaves(ObjectiveId id) const -> IndexSpan { return at(leaves_, id); }
    auto composites(ObjectiveId id) -> IndexSpan& { return at(composites_, id); }
    auto composites(ObjectiveId id) const -> IndexSpan { return at(composites_, id); }
    auto prerequisites(ObjectiveId id) -> std::uint32_t& { return at(prerequisites_, id); }
    auto prerequisites(ObjectiveId id) const -> std::uint32_t { return at(prerequisites_, id); }
    auto dependents(ObjectiveId id) -> IndexSpan& { return at(dependents_, id); }
    auto dependents(ObjectiveId id) const -> IndexSpan { return at(dependents_, id); }
    auto observers(ObjectiveId id) -> IndexSpan& { return at(observers_, id); }
    auto observers(ObjectiveId id) const -> IndexSpan { return at(observers_, id); }
    auto team_results(ObjectiveId id) -> IndexSpan& { return at(team_results_, id); }
    auto team_results(ObjectiveId id) const -> IndexSpan { return at(team_results_, id); }
    auto outcome(ObjectiveId id) -> std::optional<ObjectiveOutcome>& { return at(outcomes_, id); }
    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome> {
        return at(outcomes_, id);
    }
    auto activation_tick(ObjectiveId id) -> SimTick& { return at(activation_ticks_, id); }
    auto activation_tick(ObjectiveId id) const -> SimTick { return at(activation_ticks_, id); }
    auto membership_position(ObjectiveId id) -> std::uint32_t& {
        return at(membership_positions_, id);
    }
    auto membership_position(ObjectiveId id) const -> std::uint32_t {
        return at(membership_positions_, id);
    }
  private:
    template <typename Column>
    auto at(Column& column, ObjectiveId const id) const -> decltype(column[id.index()]) {
        assert(is_valid(id));
        return column[id.index()];
    }

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
