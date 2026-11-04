#pragma once

namespace ioj::sim {
class Mission;
}

namespace ioj::sim::tasks {
struct TaskPlanningContext {
    Mission const& mission;
};

}
