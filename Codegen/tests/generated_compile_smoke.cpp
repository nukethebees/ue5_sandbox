#include "Generated.h"

auto main() -> int {
    using namespace codegen_compile_fixture;

    FValuesf values;
    values.add(1.0f, 2.0f);
    values.add(FVector2f{3.0f, 4.0f});
    values.add(FPoint2f{5.0f, 6.0f});
    values.validate_array_sizes();

    auto const value{values.at(1)};
    auto const view_value{values.get_const_view().at(2)};
    if (value.X != 3.0f || value.Y != 4.0f ||
        view_value.X != 5.0f || view_value.Y != 6.0f) {
        return 1;
    }

    TArray<int32> scratch;
    scratch.AddUninitialized(values.num());
    values.sort([](auto const& rows, int32 const lhs, int32 const rhs) {
        return rows.xs[lhs] < rows.xs[rhs];
    }, scratch);
    return 0;
}
