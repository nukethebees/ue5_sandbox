#pragma once

#include <Engine/EngineTypes.h>

class AActor;

namespace ioj {
void SANDBOXGAMESHARED_API set_actor_component_mobility(AActor& actor,
                                                    EComponentMobility::Type mobility);
}
