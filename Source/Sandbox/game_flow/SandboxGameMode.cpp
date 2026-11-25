#include "SandboxGameMode.h"

namespace ioj {
ASandboxGameMode::ASandboxGameMode() {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

} // namespace ioj
