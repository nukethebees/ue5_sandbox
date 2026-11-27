#pragma once

#include <ioj/host/level_startup.h>

namespace ioj {
class ATestBatchOrchestrator;
void attach_level_world(ATestBatchOrchestrator& actor);
void detach_level_world(ATestBatchOrchestrator& actor);
void report_level_startup(LevelStartup status);

}
