#include "ActorLoggingConfig.h"

#include <utility>

FActorLoggingConfig::FActorLoggingConfig(EActorLoggingVerbosity verbosity, float cooldown)
    : verbosity{verbosity}
    , tick_cooldown{cooldown} {}
FActorLoggingConfig::FActorLoggingConfig(float cooldown)
    : tick_cooldown{cooldown} {}

void FActorLoggingConfig::reset() {
    tick_cooldown.finish();
}
void FActorLoggingConfig::tick(float dt) {
    tick_cooldown.tick(dt);
}
bool FActorLoggingConfig::on_tick_end() {
    return tick_cooldown.reset_if_done();
}

bool FActorLoggingConfig::can_log(EActorLoggingVerbosity msg_verbosity) const {
    return (msg_verbosity != EActorLoggingVerbosity::None) &&
           (std::to_underlying(verbosity) >= std::to_underlying(msg_verbosity));
}
bool FActorLoggingConfig::can_tick_log(EActorLoggingVerbosity msg_verbosity) const {
    return can_log(msg_verbosity) && tick_cooldown.is_finished();
}
