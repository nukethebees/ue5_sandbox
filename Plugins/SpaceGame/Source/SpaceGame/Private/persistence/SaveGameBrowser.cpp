#include "SpaceGame/persistence/SaveGameBrowser.h"

#include <Templates/UnrealTemplate.h>

namespace ml::ioj {
FSaveGameBrowser::FSaveGameBrowser(FSummarySource summary_source)
    : summary_source_{MoveTemp(summary_source)} {}

void FSaveGameBrowser::refresh() {
    summaries_ = summary_source_ ? summary_source_() : TArray<FSaveGameSummary>{};
    summaries_.Sort([](FSaveGameSummary const& lhs, FSaveGameSummary const& rhs) {
        if (lhs.timestamp != rhs.timestamp) {
            return lhs.timestamp > rhs.timestamp;
        }

        return lhs.save_id < rhs.save_id;
    });
}

auto FSaveGameBrowser::get_summaries() const -> TConstArrayView<FSaveGameSummary> {
    return summaries_;
}
}
