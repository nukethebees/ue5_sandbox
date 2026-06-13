#include <SandboxCore/soa_vector_utils.h>

#include <SandboxCore/array_math.h>
#include <SandboxCore/soa_vectors.h>

namespace ml {
void assign_from(FVectors3f& dst, FVectors3f const& src) {
    dst.xs = src.xs;
    dst.ys = src.ys;
    dst.zs = src.zs;
}
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

void add_scaled_in_place(FVectors3f& dst, FVectors3f const& src, float const scale_factor) {
    auto const n{ml::num(dst)};
    check(ml::num(src) == n);

    return ml::kernel::add_scaled_in_place<float>(dst.xs.GetData(),
                                                  dst.ys.GetData(),
                                                  dst.zs.GetData(),
                                                  src.xs.GetData(),
                                                  src.ys.GetData(),
                                                  src.zs.GetData(),
                                                  scale_factor,
                                                  n);
}

}
