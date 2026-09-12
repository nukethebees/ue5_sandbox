#include <SandboxCore/array_math_kernels.h>

#include <sandbox/core/generated/array_math_kernels.h>

#include <span>

namespace {
template <typename T>
auto as_span(TArrayView<T> const values) -> std::span<T> {
    return {values.GetData(), static_cast<std::size_t>(values.Num())};
}

template <typename T>
auto as_span(TConstArrayView<T> const values) -> std::span<T const> {
    return {values.GetData(), static_cast<std::size_t>(values.Num())};
}
}

namespace ml {
#define SANDBOX_DEFINE_SCALAR_IN_PLACE(Type, Function)                      \
    void Function(TArrayView<Type> const data, Type const value) noexcept { \
        ml::Function(as_span(data), value);                                 \
    }                                                                       \
    void Function(TArray<Type>& data, Type const value) noexcept {          \
        Function(TArrayView<Type>{data}, value);                            \
    }

SANDBOX_DEFINE_SCALAR_IN_PLACE(int32, add_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(float, add_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(double, add_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(int32, subtract_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(float, subtract_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(double, subtract_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(int32, divide_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(float, divide_in_place)
SANDBOX_DEFINE_SCALAR_IN_PLACE(double, divide_in_place)

#undef SANDBOX_DEFINE_SCALAR_IN_PLACE

#define SANDBOX_DEFINE_MULTIPLY(Type)                                                              \
    void multiply(TConstArrayView<Type> const lhs,                                                 \
                  TConstArrayView<Type> const rhs,                                                 \
                  TArrayView<Type> const out) noexcept {                                           \
        ml::multiply(as_span(lhs), as_span(rhs), as_span(out));                                    \
    }                                                                                              \
    void multiply(                                                                                 \
        TConstArrayView<Type> const lhs, Type const rhs, TArrayView<Type> const out) noexcept {    \
        ml::multiply(as_span(lhs), rhs, as_span(out));                                             \
    }                                                                                              \
    void multiply_in_place(TArrayView<Type> const lhs, TConstArrayView<Type> const rhs) noexcept { \
        ml::multiply_in_place(as_span(lhs), as_span(rhs));                                         \
    }                                                                                              \
    void multiply_in_place(TArray<Type>& lhs, TConstArrayView<Type> const rhs) noexcept {          \
        multiply_in_place(TArrayView<Type>{lhs}, rhs);                                             \
    }                                                                                              \
    void multiply_in_place(TArrayView<Type> const lhs, Type const rhs) noexcept {                  \
        ml::multiply_in_place(as_span(lhs), rhs);                                                  \
    }                                                                                              \
    void multiply_in_place(TArray<Type>& lhs, Type const rhs) noexcept {                           \
        multiply_in_place(TArrayView<Type>{lhs}, rhs);                                             \
    }

SANDBOX_DEFINE_MULTIPLY(int32)
SANDBOX_DEFINE_MULTIPLY(float)
SANDBOX_DEFINE_MULTIPLY(double)

#undef SANDBOX_DEFINE_MULTIPLY
}
