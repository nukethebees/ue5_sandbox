#pragma once

#include <ioj/levels/team.h>

#include <span>

namespace ioj::sim::tasks {
struct TaskPlanningContext {
    std::span<Team const> active_teams;
};
}
