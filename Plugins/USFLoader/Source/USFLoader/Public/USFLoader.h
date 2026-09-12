// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

USFLOADER_API DECLARE_LOG_CATEGORY_EXTERN(LogUSFLoader, Log, All);

class FUSFLoaderModule : public IModuleInterface {
  public:
    virtual void StartupModule() override;
};
