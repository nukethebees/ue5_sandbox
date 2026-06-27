#include <SandboxCore/array_checks.h>

#include <initializer_list>

namespace ml {
void fatal_if_nums_not_equal(std::initializer_list<NamedNum> const values) {
    if (values.size() <= 1) { return; }

    auto const expected{values.begin()->num};
    auto const* expected_name{values.begin()->name};

    FString message{};

    for (auto const& value : values) {
        if (value.num == expected) { continue; }

        message += FString::Printf(TEXT("%s.Num() == %d, "), value.name, value.num);
    }

    if (message.IsEmpty()) { return; }

    UE_LOG(LogTemp,
           Fatal,
           TEXT("Array size invariant failed. Expected %s.Num() == %d. Mismatches: %s"),
           expected_name,
           expected,
           *message);
}
void fatal_if_nums_not_equal(std::initializer_list<int32> const values) {
    if (values.size() <= 1) { return; }

    auto const expected{*values.begin()};

    FString message{};

    int32 i{0};
    for (auto const& value : values) {
        if (value == expected) { continue; }

        message += FString::Printf(TEXT("array[%d].Num() == %d, "), i, value);
        ++i;
    }

    if (message.IsEmpty()) { return; }

    UE_LOG(LogTemp,
           Fatal,
           TEXT("Array size invariant failed. Expected arrays.Num() == %d. Mismatches: %s"),
           expected,
           *message);
}
}
