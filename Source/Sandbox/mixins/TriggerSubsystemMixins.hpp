#include "Sandbox/macros/create_forwarding_fn.hpp"

class UTriggerSubsystemMixins {
  public:
    FORWARDING_FN(data_, register_triggerable, &self);
    FORWARDING_FN(data_, trigger);
    FORWARDING_FN(data_, deregister_triggerable);
    FORWARDING_FN(data_, get_triggerable_ids);
    FORWARDING_FN(data_, get_or_create_actor_id);
    FORWARDING_FN(data_, get_actor_id);
    FORWARDING_FN(data_, trigger_actor);
};

#include "Sandbox/macros/create_forwarding_fn_undef.hpp"
