#pragma once

#include "Records.h"

namespace codegen_compile_fixture {
struct SettingsTarget {
    ActorSettings first;
    ActorSettings second;
    int external_value{};
};
}
