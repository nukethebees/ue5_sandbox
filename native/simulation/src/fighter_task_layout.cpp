#include "ioj/sim/fighter_task_layout.h"

namespace ioj::sim::fighters {
TaskLayout::TaskLayout(TaskCounts const& counts) {
    std::int32_t offset{};
    for (std::size_t group{}; group < task_type_count; ++group) {
        auto const count{counts[group]};
        assert(count >= 0);
        write_indexes_[group] = offset;
        spans_[group] = {offset, count};
        offset += count;
    }
}

auto count_tasks(std::span<FighterTask const> const tasks) -> TaskCounts {
    TaskCounts counts{};
    for (auto const task : tasks) {
        auto const group{static_cast<std::size_t>(task)};
        assert(group < task_type_count);
        ++counts[group];
    }
    return counts;
}

auto tasks_are_contiguous(std::span<FighterTask const> const tasks, TaskSpans const& spans) noexcept
    -> bool {
    auto current_task{FighterTask::Standby};
    for (auto const task : tasks) {
        if (task < current_task || task >= FighterTask::COUNT) {
            return false;
        }
        current_task = task;
    }

    return TaskLayout{count_tasks(tasks)}.spans() == spans;
}
}
