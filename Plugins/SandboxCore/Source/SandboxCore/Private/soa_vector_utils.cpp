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

}
