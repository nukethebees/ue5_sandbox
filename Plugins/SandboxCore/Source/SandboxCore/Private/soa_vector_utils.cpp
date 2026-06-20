#include <SandboxCore/soa_vector_utils.h>

#include <SandboxCore/array_math.h>
#include <SandboxCore/log_categories.h>
#include <SandboxCore/soa_vectors.h>
#include <SandboxCore/vector_math.h>

#include <Logging/LogMacros.h>

namespace ml {
/* ---------------------------------------------------------------------------------------------- */
// Checks
/* ---------------------------------------------------------------------------------------------- */
void check_is_consistent(FVectors3f const& vec) {
    auto const nx{vec.xs.Num()};
    auto const ny{vec.ys.Num()};
    auto const nz{vec.zs.Num()};

    if ((nx != ny) || (nx != nz)) {
        UE_LOG(LogSandboxCore, Fatal, TEXT("Array sizes inconsistent (%d, %d, %d)"), nx, ny, nz);
    }
}

/* ---------------------------------------------------------------------------------------------- */
// Assignment
/* ---------------------------------------------------------------------------------------------- */
void assign_from(FVectors3f& dst, FVectors3f const& src) {
    dst.xs = src.xs;
    dst.ys = src.ys;
    dst.zs = src.zs;
}
void assign_from_scaled(FVectors3f& dst,
                        FVectors3f const& src,
                        TConstArrayView<float> const scale_factors) {
    assign_from_scaled(dst, src.get_const_view(), scale_factors);
}
void assign_from_scaled(FVectors3f& dst,
                        FVectors3f::ConstView src,
                        TConstArrayView<float> const scale_factors) {
    auto const n{ml::num(dst)};
    check(n == ml::num(src));
    check(n == ml::num(scale_factors));

    check(dst.xs.GetData() != src.xs.GetData());
    check(dst.ys.GetData() != src.ys.GetData());
    check(dst.zs.GetData() != src.zs.GetData());

    ml::kernel::scale_vector3<float>(dst.xs.GetData(),
                                     dst.ys.GetData(),
                                     dst.zs.GetData(),
                                     src.xs.GetData(),
                                     src.ys.GetData(),
                                     src.zs.GetData(),
                                     scale_factors.GetData(),
                                     n);
}

/* ---------------------------------------------------------------------------------------------- */
// Multiplication
/* ---------------------------------------------------------------------------------------------- */
void multiply_in_place(FVectors3f& dst, float const value) {
    auto const n{dst.num()};

    ml::kernel::multiply_in_place(dst.xs.GetData(), value, n);
    ml::kernel::multiply_in_place(dst.ys.GetData(), value, n);
    ml::kernel::multiply_in_place(dst.zs.GetData(), value, n);
}
void multiply_in_place(FVectors3f& dst, TConstArrayView<float> const values) {
    auto const n{dst.num()};

    check(n == values.Num());

    ml::kernel::multiply_in_place(dst.xs.GetData(), values.GetData(), n);
    ml::kernel::multiply_in_place(dst.ys.GetData(), values.GetData(), n);
    ml::kernel::multiply_in_place(dst.zs.GetData(), values.GetData(), n);
}

/* ---------------------------------------------------------------------------------------------- */
// Addition
/* ---------------------------------------------------------------------------------------------- */
void add_scaled_in_place(FVectors3f& dst, FVectors3f const& src, float const scale_factor) {
    ml::check_is_consistent(dst);
    ml::check_is_consistent(src);

    auto const n{ml::num(dst)};
    check(ml::num(src) == n);

    check(&dst != &src);

    return ml::kernel::add_scaled_in_place<float>(dst.xs.GetData(),
                                                  dst.ys.GetData(),
                                                  dst.zs.GetData(),
                                                  src.xs.GetData(),
                                                  src.ys.GetData(),
                                                  src.zs.GetData(),
                                                  scale_factor,
                                                  n);
}
void add_scaled_in_place(FVectors3f& dst, FVectors3f const& a, FVectors3f const& b, float const c) {
    ml::check_is_consistent(dst);
    ml::check_is_consistent(a);
    ml::check_is_consistent(b);

    auto const n{ml::num(dst)};
    check(ml::num(a) == n);
    check(ml::num(b) == n);

    check(&dst != &a);
    check(&dst != &b);
    check(&a != &b);

    return ml::kernel::add_scaled_in_place<float>(dst.xs.GetData(),
                                                  dst.ys.GetData(),
                                                  dst.zs.GetData(),
                                                  a.xs.GetData(),
                                                  a.ys.GetData(),
                                                  a.zs.GetData(),
                                                  b.xs.GetData(),
                                                  b.ys.GetData(),
                                                  b.zs.GetData(),
                                                  c,
                                                  n);
}
void add_scaled_in_place(FVectors3f& dst,
                         FVectors3f const& a,
                         TConstArrayView<float> const b,
                         float const c) {
    auto const n{ml::num(dst)};
    check(ml::num(a) == n);
    check(ml::num(b) == n);

    check(&dst != &a);

    return ml::kernel::add_scaled_in_place<float>(dst.xs.GetData(),
                                                  dst.ys.GetData(),
                                                  dst.zs.GetData(),
                                                  a.xs.GetData(),
                                                  a.ys.GetData(),
                                                  a.zs.GetData(),
                                                  b.GetData(),
                                                  c,
                                                  n);
}

}
