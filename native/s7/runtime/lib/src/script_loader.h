#pragma once

#include "active_load.h"
#include "load_decision.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <unordered_set>
#include <vector>

namespace ioj::s7 {
struct InterpreterOptions;
}

namespace ioj::s7::detail {
using LoadAdmission = std::expected<LoadDecision, std::string>;

class ScriptLoader {
  public:
    explicit ScriptLoader(InterpreterOptions const& options);

    // Pass a canonical, normalized key and the validated source byte count.
    [[nodiscard]] auto begin_load(std::string key, std::size_t file_size) -> LoadAdmission;
    // Each admitted token must be completed or aborted exactly once.
    void complete_load(LoadToken token);
    void abort_load(LoadToken token);
    // Recover only exceptional leftovers at the outer evaluation boundary.
    void evaluation_ended();
  private:
    std::size_t max_file_bytes_;
    std::size_t max_total_bytes_;
    std::size_t max_files_;
    std::size_t max_depth_;
    std::unordered_set<std::string> loaded_files_;
    std::vector<ActiveLoad> active_loads_;
    std::size_t loaded_bytes_{};
    std::size_t reserved_bytes_{};
    LoadToken next_token_{1};
};
}
