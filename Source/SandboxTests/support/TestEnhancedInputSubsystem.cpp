#include <SandboxTests/support/TestEnhancedInputSubsystem.h>

#include <EnhancedPlayerInput.h>

void USandboxTestEnhancedInputSubsystem::initialise() {
    player_input = NewObject<UEnhancedPlayerInput>(this);
}
