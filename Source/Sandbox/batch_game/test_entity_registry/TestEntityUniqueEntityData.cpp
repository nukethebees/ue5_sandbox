#include "TestEntityUniqueEntityData.h"

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

auto TestEntityUniqueEntityData::num() const -> int32 {
    return registry_handles.Num();
}
void TestEntityUniqueEntityData::reset() {
    ml::reset(registry_handles, kills, alive, killed_by);
}
void TestEntityUniqueEntityData::add_defaulted(int32 const count) {
    registry_handles.AddDefaulted(count);
    kills.AddDefaulted(count);
    alive.AddDefaulted(count);
    killed_by.AddDefaulted(count);
    death_reason.AddDefaulted(count);
}

void TestEntityUniqueEntityData::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(registry_handles),
        SANDBOX_NAMED_NUM(kills),
        SANDBOX_NAMED_NUM(alive),
        SANDBOX_NAMED_NUM(killed_by),
    });
}
