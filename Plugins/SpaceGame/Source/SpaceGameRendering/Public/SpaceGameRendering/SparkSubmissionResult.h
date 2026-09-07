#pragma once

#include <CoreMinimal.h>

struct SPACEGAMERENDERING_API FSparkSubmissionResult {
    int64 requested{0};
    int32 admitted{0};
    int32 replaced_slots{0};
};
