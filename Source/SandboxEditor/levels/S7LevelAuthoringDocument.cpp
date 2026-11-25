#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

#include "Misc/SecureHash.h"

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

void AS7LevelAuthoringDocument::PostLoad() {
    Super::PostLoad();
    if (!source_path.IsEmpty() && editor_source_hash.IsEmpty() && !needs_export) {
        remember_editor_source();
    }
}
void AS7LevelAuthoringDocument::remember_editor_source() {
    editor_source_hash =
        LexToString(FMD5Hash::HashFile(*ioj::editor::s7_editor_config_path(*this)));
}
bool AS7LevelAuthoringDocument::editor_source_matches() const {
    auto const hash{FMD5Hash::HashFile(*ioj::editor::s7_editor_config_path(*this))};
    return hash.IsValid() && LexToString(hash) == editor_source_hash;
}
