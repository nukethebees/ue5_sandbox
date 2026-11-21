#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

AS7LevelAuthoringDocument::AS7LevelAuthoringDocument() {
    SetActorHiddenInGame(true);
    SetCanBeDamaged(false);
}

void AS7LevelAuthoringDocument::set_needs_export(bool value) {
    if (needs_export != value) {
        needs_export = value;
        MarkPackageDirty();
    }
}
