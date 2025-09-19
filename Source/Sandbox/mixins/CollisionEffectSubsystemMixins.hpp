#pragma once

#include "Sandbox/macros/create_forwarding_fn.hpp"

class UCollisionEffectSubsystemMixins {
  public:
    FORWARDING_FN(add_payload, &self);
    FORWARDING_FN(register_actor, &self)
  protected:
    FORWARDING_FN(handle_collision_event_)
};
#undef FORWARDING_FN
