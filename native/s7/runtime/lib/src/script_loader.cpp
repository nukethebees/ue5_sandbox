#include "script_loader.h"

#include <ioj/s7/interpreter_options.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

namespace ioj::s7::detail {
ScriptLoader::ScriptLoader(InterpreterOptions const& options)
    : max_file_bytes_{options.max_loaded_file_bytes}
    , max_total_bytes_{options.max_total_loaded_bytes}
    , max_files_{options.max_loaded_files}
    , max_depth_{options.max_load_depth} {}

auto ScriptLoader::begin_load(std::string key, std::size_t const file_size) -> LoadAdmission {
    auto const active{std::ranges::find(active_loads_, key, &ActiveLoad::key)};
    if (active != active_loads_.end()) {
        std::string cycle{"Recursive import-module cycle: "};
        for (auto iterator{active}; iterator != active_loads_.end(); ++iterator) {
            if (iterator != active) {
                cycle += " -> ";
            }
            cycle += iterator->key;
        }
        cycle += " -> " + key;
        return LoadAdmission{std::unexpect, std::move(cycle)};
    }
    auto const active_count{active_loads_.size()};
    auto const loaded_count{loaded_files_};
    if (active_count >= max_depth_) {
        return LoadAdmission{std::unexpect, "The import-module nesting limit was exceeded."};
    }
    if (loaded_count >= max_files_ || active_count >= max_files_ - loaded_count) {
        return LoadAdmission{std::unexpect, "The import-module file count limit was exceeded."};
    }

    assert(loaded_bytes_ <= max_total_bytes_);
    assert(reserved_bytes_ <= max_total_bytes_ - loaded_bytes_);
    if (file_size > max_file_bytes_ ||
        file_size > max_total_bytes_ - loaded_bytes_ - reserved_bytes_) {
        return LoadAdmission{std::unexpect, "The import-module source size limit was exceeded."};
    }
    if (next_token_.value == std::numeric_limits<std::int64_t>::max()) {
        return LoadAdmission{std::unexpect, "The import-module identity limit was exceeded."};
    }

    auto const token{next_token_};
    active_loads_.emplace_back(token, std::move(key), file_size);
    ++next_token_.value;
    reserved_bytes_ += file_size;
    return LoadAdmission{token};
}
void ScriptLoader::complete_load(LoadToken const token) {
    auto const active{std::ranges::find(active_loads_, token, &ActiveLoad::token)};
    assert(active != active_loads_.end());

    ++loaded_files_;
    loaded_bytes_ += active->bytes;
    reserved_bytes_ -= active->bytes;
    active_loads_.erase(active);
}
void ScriptLoader::abort_load(LoadToken const token) {
    auto const active{std::ranges::find(active_loads_, token, &ActiveLoad::token)};
    assert(active != active_loads_.end());

    reserved_bytes_ -= active->bytes;
    active_loads_.erase(active);
}
void ScriptLoader::evaluation_ended() {
    // Keep recovery here for host failures that bypass the Scheme wind handlers.
    while (!active_loads_.empty()) {
        abort_load(active_loads_.back().token);
    }
    assert(reserved_bytes_ == 0);
}
}
