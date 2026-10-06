#include <native/s7/detail/script_loader.h>

#include <native/s7/interpreter.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

namespace ml::s7::detail {
ScriptLoader::ScriptLoader(InterpreterOptions const& options)
    : max_file_bytes_{options.max_loaded_file_bytes}
    , max_total_bytes_{options.max_total_loaded_bytes}
    , max_files_{options.max_loaded_files}
    , max_depth_{options.max_load_depth} {}

auto ScriptLoader::begin_load(std::string key, std::size_t const file_size) -> LoadAdmission {
    if (loaded_files_.contains(key)) {
        return LoadAdmission{std::in_place, LoadStatus::already_loaded};
    }

    auto const active{std::ranges::find(active_loads_, key, &ActiveLoad::key)};
    if (active != active_loads_.end()) {
        std::string cycle{"Recursive load-script cycle: "};
        for (auto iterator{active}; iterator != active_loads_.end(); ++iterator) {
            if (iterator != active) {
                cycle += " -> ";
            }
            cycle += iterator->key;
        }
        cycle += " -> " + key;
        return LoadAdmission{std::unexpect, std::move(cycle)};
    }
    if (active_loads_.size() >= max_depth_) {
        return LoadAdmission{std::unexpect, "The load-script nesting limit was exceeded."};
    }
    if (loaded_files_.size() >= max_files_ ||
        active_loads_.size() >= max_files_ - loaded_files_.size()) {
        return LoadAdmission{std::unexpect, "The load-script file count limit was exceeded."};
    }

    assert(loaded_bytes_ <= max_total_bytes_);
    assert(reserved_bytes_ <= max_total_bytes_ - loaded_bytes_);
    if (file_size > max_file_bytes_ ||
        file_size > max_total_bytes_ - loaded_bytes_ - reserved_bytes_) {
        return LoadAdmission{std::unexpect, "The load-script source size limit was exceeded."};
    }
    if (next_token_.value == std::numeric_limits<std::int64_t>::max()) {
        return LoadAdmission{std::unexpect, "The load-script identity limit was exceeded."};
    }

    auto const token{next_token_};
    active_loads_.push_back({.token = token, .key = std::move(key), .bytes = file_size});
    ++next_token_.value;
    reserved_bytes_ += file_size;
    return LoadAdmission{std::in_place, LoadStatus::admitted, token};
}
void ScriptLoader::complete_load(LoadToken const token) {
    auto const active{std::ranges::find(active_loads_, token, &ActiveLoad::token)};
    assert(active != active_loads_.end());

    loaded_files_.insert(active->key);
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
