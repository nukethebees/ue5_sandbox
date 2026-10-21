// Mesh bounds and socket transforms captured from the Unreal presentation assets.
#include <ioj/sim/levels/reference_level_simulation_data.h>

namespace ioj::sim {
auto make_reference_level_simulation_data() -> ReferenceLevelSimulationData {
    ReferenceLevelSimulationData fixture;
    auto& data{fixture.data};
    auto& player{fixture.player};
    data.capital_ships.fighter_spawn_slots = 6;
    data.fighter_fire_point_distance = 1.807294556e+03f;
    data.turrets.fire_point_offset = {{0.000000000e+00f, 0.000000000e+00f, 1.560000000e+03f}};
    player.transform = {
        {0.00000000000000000e+00,
         0.00000000000000000e+00,
         0.00000000000000000e+00,
         1.00000000000000000e+00},
        {0.00000000000000000e+00, 0.00000000000000000e+00, 0.00000000000000000e+00},
        {1.00000000000000000e+00, 1.00000000000000000e+00, 1.00000000000000000e+00}};
    player.body_transform = {
        {0.00000000000000000e+00,
         0.00000000000000000e+00,
         0.00000000000000000e+00,
         1.00000000000000000e+00},
        {0.00000000000000000e+00, 0.00000000000000000e+00, 0.00000000000000000e+00},
        {1.00000000000000000e+00, 1.00000000000000000e+00, 1.00000000000000000e+00}};
    player.left_socket = {
        {3.07504227942209978e-15,
         -3.77489497438432342e-08,
         8.14603398069378503e-08,
         9.99999999999995781e-01},
        {2.00000000000000000e+02, -1.29999999999998295e+02, 0.00000000000000000e+00},
        {9.99999977648258320e-01, 9.99999977648258653e-01, 9.99999977648258542e-01}};
    player.right_socket = {
        {3.07504227942209978e-15,
         -3.77489497438432342e-08,
         8.14603398069378503e-08,
         9.99999999999995781e-01},
        {2.00000000000000000e+02, 1.39999999999998153e+02, 0.00000000000000000e+00},
        {9.99999977648258320e-01, 9.99999977648258653e-01, 9.99999977648258542e-01}};
    player.middle_socket = {
        {3.07504227942209978e-15,
         -3.77489497438432342e-08,
         8.14603398069378503e-08,
         9.99999999999995781e-01},
        {2.00000000000000000e+02, 0.00000000000000000e+00, 0.00000000000000000e+00},
        {9.99999977648258320e-01, 9.99999977648258653e-01, 9.99999977648258542e-01}};
    data.capital_ships.fighter_spawn_slots_relative_transforms.resize(6);
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location.x() =
        -3.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location.y() =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location.z() =
        -2.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.x() =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.y() =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.z() =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.w() =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].scale.x() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].scale.y() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].scale.z() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].location.x() =
        3.30000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].location.y() =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].location.z() =
        -2.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.x() =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.y() =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.z() =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.w() =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].scale.x() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].scale.y() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].scale.z() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].location.x() =
        6.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].location.y() =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].location.z() =
        -2.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.x() =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.y() =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.z() =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.w() =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].scale.x() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].scale.y() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].scale.z() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].location.x() =
        -3.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].location.y() =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].location.z() =
        7.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.x() =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.y() =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.z() =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.w() =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].scale.x() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].scale.y() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].scale.z() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].location.x() =
        3.30000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].location.y() =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].location.z() =
        7.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.x() =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.y() =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.z() =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.w() =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].scale.x() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].scale.y() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].scale.z() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].location.x() =
        6.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].location.y() =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].location.z() =
        7.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.x() =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.y() =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.z() =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.w() =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].scale.x() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].scale.y() =
        1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].scale.z() =
        1.00000000000000000e+00;
    data.spinners.fire_point_offsets.resize(9);
    data.spinners.fire_point_offsets[0].location.x() = 1.000000000e+02f;
    data.spinners.fire_point_offsets[0].location.y() = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].location.z() = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].rotation.yaw = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[1].location.x() = 5.000000000e+01f;
    data.spinners.fire_point_offsets[1].location.y() = 7.000000000e+01f;
    data.spinners.fire_point_offsets[1].location.z() = 1.500000000e+02f;
    data.spinners.fire_point_offsets[1].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[1].rotation.yaw = 5.446232224e+01f;
    data.spinners.fire_point_offsets[1].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[2].location.x() = -5.000000000e+01f;
    data.spinners.fire_point_offsets[2].location.y() = 8.000000000e+01f;
    data.spinners.fire_point_offsets[2].location.z() = 2.400000000e+02f;
    data.spinners.fire_point_offsets[2].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[2].rotation.yaw = 1.220053864e+02f;
    data.spinners.fire_point_offsets[2].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[3].location.x() = -1.000000000e+02f;
    data.spinners.fire_point_offsets[3].location.y() = 0.000000000e+00f;
    data.spinners.fire_point_offsets[3].location.z() = 3.300000000e+02f;
    data.spinners.fire_point_offsets[3].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[3].rotation.yaw = 1.800000000e+02f;
    data.spinners.fire_point_offsets[3].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[4].location.x() = -7.000000000e+01f;
    data.spinners.fire_point_offsets[4].location.y() = -8.000000000e+01f;
    data.spinners.fire_point_offsets[4].location.z() = 4.200000000e+02f;
    data.spinners.fire_point_offsets[4].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[4].rotation.yaw = -1.311859283e+02f;
    data.spinners.fire_point_offsets[4].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[5].location.x() = 0.000000000e+00f;
    data.spinners.fire_point_offsets[5].location.y() = -8.000000000e+01f;
    data.spinners.fire_point_offsets[5].location.z() = 4.800000000e+02f;
    data.spinners.fire_point_offsets[5].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[5].rotation.yaw = -9.000000000e+01f;
    data.spinners.fire_point_offsets[5].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[6].location.x() = 8.000000000e+01f;
    data.spinners.fire_point_offsets[6].location.y() = -8.000000000e+01f;
    data.spinners.fire_point_offsets[6].location.z() = 4.200000000e+02f;
    data.spinners.fire_point_offsets[6].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[6].rotation.yaw = -4.500000000e+01f;
    data.spinners.fire_point_offsets[6].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[7].location.x() = 1.000000000e+02f;
    data.spinners.fire_point_offsets[7].location.y() = -5.000000000e+01f;
    data.spinners.fire_point_offsets[7].location.z() = 3.400000000e+02f;
    data.spinners.fire_point_offsets[7].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[7].rotation.yaw = -2.656505203e+01f;
    data.spinners.fire_point_offsets[7].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[8].location.x() = 1.100000000e+02f;
    data.spinners.fire_point_offsets[8].location.y() = 4.000000000e+01f;
    data.spinners.fire_point_offsets[8].location.z() = 2.900000000e+02f;
    data.spinners.fire_point_offsets[8].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[8].rotation.yaw = 1.998310661e+01f;
    data.spinners.fire_point_offsets[8].rotation.roll = -0.000000000e+00f;
    data.collision.entity_bounds.set_centre(
        EntityType::PlayerShip, {{0.000000000e+00f, 0.000000000e+00f, 0.000000000e+00f}});
    data.collision.entity_bounds.set_half_extents(
        EntityType::PlayerShip, {{0.000000000e+00f, 0.000000000e+00f, 0.000000000e+00f}});
    data.collision.entity_bounds.set_centre(
        EntityType::Turret, {{0.000000000e+00f, 0.000000000e+00f, 5.675825806e+02f}});
    data.collision.entity_bounds.set_half_extents(
        EntityType::Turret, {{3.356489258e+02f, 3.356489258e+02f, 6.843348389e+02f}});
    data.collision.entity_bounds.set_centre(
        EntityType::CapitalShip, {{4.786312500e+03f, 2.441406250e-04f, -1.687307007e+03f}});
    data.collision.entity_bounds.set_half_extents(
        EntityType::CapitalShip, {{8.918416016e+03f, 5.000000488e+03f, 5.077661133e+03f}});
    data.collision.entity_bounds.set_centre(
        EntityType::Fighter, {{0.000000000e+00f, 0.000000000e+00f, 0.000000000e+00f}});
    data.collision.entity_bounds.set_half_extents(
        EntityType::Fighter, {{1.720659668e+03f, 1.021299377e+03f, 4.383157349e+02f}});
    data.collision.entity_bounds.set_centre(
        EntityType::TubeSpinner, {{0.000000000e+00f, 0.000000000e+00f, 2.500000000e+02f}});
    data.collision.entity_bounds.set_half_extents(
        EntityType::TubeSpinner, {{5.000000000e+01f, 5.000000000e+01f, 2.500000000e+02f}});
    return fixture;
}
}
