#include "Commandlets/GenerateSandboxImagesCommandlet.h"

#include "Generation/LabImageWriter.h"

UGenerateSandboxImagesCommandlet::UGenerateSandboxImagesCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxImagesCommandlet::Main(FString const&) {
    return SandboxImages::GenLab::regenerate_all() ? 0 : 1;
}
