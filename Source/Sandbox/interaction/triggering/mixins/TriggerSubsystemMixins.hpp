#include "Sandbox/macros/create_forwarding_fn.hpp"

class UTriggerSubsystemMixins {
  public:
    FORWARDING_FN(core_, register_triggerable);
    FORWARDING_FN(core_, trigger);
    FORWARDING_FN(core_, deregister_triggerable);
    FORWARDING_FN(core_, get_triggerable_ids);
    FORWARDING_FN(core_, get_or_create_actor_id);
    FORWARDING_FN(core_, get_actor_id);
};

#include "Sandbox/macros/create_forwarding_fn_undef.hpp"
