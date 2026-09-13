#pragma once

#include <sandbox/simulation/frame_collision_scratch.h>
#include <sandbox/simulation/frame_direct_damage_events.h>
#include <sandbox/simulation/frame_hit_details.h>
#include <sandbox/simulation/frame_laser_spawn_requests.h>

namespace ml::test_lasers {
using FrameCollisionScratch = simulation::lasers::FrameCollisionScratch;
using FrameDirectDamageEvents = simulation::lasers::FrameDirectDamageEvents;
using FrameHitDetails = simulation::lasers::FrameHitDetails;
}
