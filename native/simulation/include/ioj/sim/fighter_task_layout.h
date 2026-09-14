#pragma once

#include "ioj/sim/fighter_types.h"
#include "ioj/sim/index_span.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <span>

namespace ioj::sim::fighters {
inline constexpr auto task_type_count{static_cast<std::size_t>(FighterTask::COUNT)};
using TaskCounts = std::array<std::int32_t, task_type_count>;
using TaskSpans = std::array<IndexSpan, task_type_count>;

class TaskLayout {
  public:
    explicit TaskLayout(TaskCounts const& counts);

    [[nodiscard]] auto spans() const noexcept -> TaskSpans const& { return spans_; }

    [[nodiscard]] auto next_index(FighterTask const task) noexcept -> std::int32_t {
        auto const group{static_cast<std::size_t>(task)};
        assert(group < task_type_count);
        assert(write_indexes_[group] < spans_[group].end());
        return write_indexes_[group]++;
    }
  private:
    TaskSpans spans_{};
    TaskCounts write_indexes_{};
};

[[nodiscard]] auto count_tasks(std::span<FighterTask const> tasks) -> TaskCounts;
[[nodiscard]] auto tasks_are_contiguous(std::span<FighterTask const> tasks,
                                        TaskSpans const& spans) noexcept -> bool;
}
