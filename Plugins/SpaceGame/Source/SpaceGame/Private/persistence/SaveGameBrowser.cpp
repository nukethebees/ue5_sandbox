#include "SpaceGame/persistence/SaveGameBrowser.h"

#include <Templates/UnrealTemplate.h>

namespace ml::ioj {
FSaveGameBrowser::FSaveGameBrowser(FSummarySource summary_source, FReportSource report_source)
    : summary_source_{MoveTemp(summary_source)}
    , report_source_{MoveTemp(report_source)} {}

void FSaveGameBrowser::refresh() {
    loaded_report_.Reset();
    summaries_ = summary_source_ ? summary_source_() : TArray<FSaveProfileSummary>{};
    summaries_.Sort([](FSaveProfileSummary const& lhs, FSaveProfileSummary const& rhs) {
        if (lhs.last_played_at != rhs.last_played_at) {
            return lhs.last_played_at > rhs.last_played_at;
        }

        return lhs.profile_id < rhs.profile_id;
    });
}

bool FSaveGameBrowser::load_profile_report(FString const& profile_id) {
    if (loaded_report_.IsSet() && loaded_report_->profile_id == profile_id) {
        return true;
    }

    loaded_report_.Reset();
    if (!report_source_) {
        return false;
    }

    auto report{report_source_(profile_id)};
    if (!report.IsSet() || report->profile_id != profile_id) {
        return false;
    }

    loaded_report_ = MoveTemp(report);
    return true;
}

auto FSaveGameBrowser::get_summaries() const -> TConstArrayView<FSaveProfileSummary> {
    return summaries_;
}

auto FSaveGameBrowser::get_loaded_profile_report() const -> FSaveProfileReport const* {
    return loaded_report_.IsSet() ? &loaded_report_.GetValue() : nullptr;
}
}
