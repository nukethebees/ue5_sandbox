#pragma once

namespace ioj::profiles {
struct RecordSaveResult {
    explicit operator bool() const { return persisted; }

    bool persisted{};
    bool new_best_time{};
};
}
