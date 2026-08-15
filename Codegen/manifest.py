from __future__ import annotations

from pathlib import Path

from Codegen.nodes import CppFile, Include, Module, Namespace, NewLines
from Codegen.soa import SoAStruct, soa_member, tarray_member


PROJECT_ROOT = Path(__file__).resolve().parent.parent


def fighter_soa_module() -> Module:
    members = (
        tarray_member("entity_handles", "FRegistryEntityHandle"),
        tarray_member("integral_biases", "uint32"),
        tarray_member("float_biases", "float"),
        tarray_member("tasks", "ETestCapitalShipFightersTask"),
        soa_member("locations", "FVectors3f"),
        soa_member("desired_move_locations", "FVectors3f"),
        soa_member("aim_directions", "FVectors3f"),
        soa_member("desired_aiming_directions", "FVectors3f"),
        soa_member("movement_directions", "FVectors3f"),
        soa_member("velocities", "FVectors3f"),
        tarray_member("move_distances", "float"),
        tarray_member("speeds", "float"),
        tarray_member("teams", "ETestTeam"),
        tarray_member("healths", "int32"),
        soa_member("awareness_scan_countdowns", "FTickCountdown8"),
        soa_member("attack_reposition_countdowns", "FTickCountdown16"),
        soa_member("attack_cooldowns", "FTickCountdown16"),
        tarray_member("target_handles", "FRegistryEntityHandle"),
        soa_member("target_locations", "FVectors3f"),
        soa_member("target_velocities", "FVectors3f"),
        soa_member("target_directions", "FVectors3f"),
        tarray_member("intercept_times", "float"),
        tarray_member("target_distance_sq", "float"),
        tarray_member("target_distances", "float"),
        tarray_member("target_radii", "float"),
    )
    return Module(
        name="test_capital_ship_fighters_soa",
        header=CppFile(
            path=PROJECT_ROOT
            / "Source"
            / "Sandbox"
            / "batch_game"
            / "TestCapitalShipFightersSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/TestCapitalShipFightersTask.h"),
                Include("Sandbox/batch_game/TestTeam.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                Include("SandboxCore/soa_vectors.h"),
                Include("SandboxCore/tick_countdown.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("Containers/ArrayView.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(
                Namespace(
                    "ml::test_capital_ship_fighters",
                    (
                        SoAStruct(
                            "EntityData",
                            "EntityDataView",
                            "EntityDataConstView",
                            members,
                        ),
                    ),
                ),
            ),
        ),
    )


def modules() -> tuple[Module, ...]:
    return (fighter_soa_module(),)
