#include <SandboxCore/array_checks.h>
#include <SandboxCore/projectile_intercept.h>

namespace {
auto make_native_view(FVectors3f::ConstView const values) -> ml::NativeVector3fSoAView {
    return {values.xs.GetData(), values.ys.GetData(), values.zs.GetData()};
}

template <typename Solve>
void solve_intercept_times_adapter(Solve const solve,
                                   TArrayView<float> const out_intercept_times,
                                   FVectors3f::ConstView const shooter_positions,
                                   FVectors3f::ConstView const target_positions,
                                   FVectors3f::ConstView const target_velocities,
                                   float const projectile_speed) {
    check(ml::all_num_equal_and_pointers_not_equal(
        out_intercept_times, shooter_positions, target_positions, target_velocities));

    solve(out_intercept_times.GetData(),
          make_native_view(shooter_positions),
          make_native_view(target_positions),
          make_native_view(target_velocities),
          projectile_speed,
          out_intercept_times.Num());
}
}

namespace ml {
auto solve_intercept_time(FVector3f const& shooter_pos,
                          FVector3f const& target_pos,
                          FVector3f const& target_vel,
                          float const projectile_speed) -> float {
    return ml::solve_intercept_time(NativeVector3f{shooter_pos.X, shooter_pos.Y, shooter_pos.Z},
                                    NativeVector3f{target_pos.X, target_pos.Y, target_pos.Z},
                                    NativeVector3f{target_vel.X, target_vel.Y, target_vel.Z},
                                    projectile_speed);
}
}

namespace ml::detail::solve_intercept_times_aos {
void solve_intercept_times(TArrayView<float> const out_intercept_times,
                           FVectors3f::ConstView const shooter_positions,
                           FVectors3f::ConstView const target_positions,
                           FVectors3f::ConstView const target_velocities,
                           float const projectile_speed) {
    solve_intercept_times_adapter(
        static_cast<void (*)(float*,
                             NativeVector3fSoAView,
                             NativeVector3fSoAView,
                             NativeVector3fSoAView,
                             float,
                             std::int32_t) noexcept>(
            &ml::detail::solve_intercept_times_aos::solve_intercept_times),
        out_intercept_times,
        shooter_positions,
        target_positions,
        target_velocities,
        projectile_speed);
}
}

namespace ml::detail::solve_intercept_times_struct_loop {
void solve_intercept_times(TArrayView<float> const out_intercept_times,
                           FVectors3f::ConstView const shooter_positions,
                           FVectors3f::ConstView const target_positions,
                           FVectors3f::ConstView const target_velocities,
                           float const projectile_speed) {
    solve_intercept_times_adapter(
        static_cast<void (*)(float*,
                             NativeVector3fSoAView,
                             NativeVector3fSoAView,
                             NativeVector3fSoAView,
                             float,
                             std::int32_t) noexcept>(
            &ml::detail::solve_intercept_times_struct_loop::solve_intercept_times),
        out_intercept_times,
        shooter_positions,
        target_positions,
        target_velocities,
        projectile_speed);
}
}

namespace ml::detail::solve_intercept_times_soa_loop {
void solve_intercept_times(TArrayView<float> const out_intercept_times,
                           FVectors3f::ConstView const shooter_positions,
                           FVectors3f::ConstView const target_positions,
                           FVectors3f::ConstView const target_velocities,
                           float const projectile_speed) {
    solve_intercept_times_adapter(
        static_cast<void (*)(float*,
                             NativeVector3fSoAView,
                             NativeVector3fSoAView,
                             NativeVector3fSoAView,
                             float,
                             std::int32_t) noexcept>(
            &ml::detail::solve_intercept_times_soa_loop::solve_intercept_times),
        out_intercept_times,
        shooter_positions,
        target_positions,
        target_velocities,
        projectile_speed);
}
}
