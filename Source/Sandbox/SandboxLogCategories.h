#pragma once

#include "Logging/LogMacros.h"

// Core logging category for general Sandbox project systems
DECLARE_LOG_CATEGORY_EXTERN(LogSandboxCore, Log, All);

// UI and widget logging category
DECLARE_LOG_CATEGORY_EXTERN(LogSandboxUI, Log, All);

// Mass entity system logging category
DECLARE_LOG_CATEGORY_EXTERN(LogSandboxMassEntity, Log, All);

// Gun and weapon system logging category
DECLARE_LOG_CATEGORY_EXTERN(LogSandboxGuns, Log, All);
