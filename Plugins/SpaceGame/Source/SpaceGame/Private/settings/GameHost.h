#pragma once

#include <ioj/host/mission_startup.h>

namespace ioj {
class ATestBatchOrchestrator;
void attach_level_world(ATestBatchOrchestrator& actor);
void detach_level_world(ATestBatchOrchestrator& actor);
void report_mission_startup(MissionStartup status);

}
