#include "USFLoaderEditor.h"

#define LOCTEXT_NAMESPACE "FUSFLoaderEditorModule"

void FUSFLoaderEditorModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file
}

void FUSFLoaderEditorModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUSFLoaderEditorModule, USFLoaderEditor)