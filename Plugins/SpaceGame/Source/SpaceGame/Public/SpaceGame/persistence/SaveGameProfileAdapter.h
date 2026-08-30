#pragma once

#include "SpaceGame/persistence/SaveGameBrowser.h"

#include <Containers/ArrayView.h>

struct FScoreRecord;

namespace ml::ioj::save_profile {
SPACEGAME_API auto make_summary(FString profile_id,
                                FString display_name,
                                TConstArrayView<FScoreRecord> records) -> FSaveProfileSummary;
SPACEGAME_API auto make_report(FString profile_id, TConstArrayView<FScoreRecord> records)
    -> FSaveProfileReport;
}
