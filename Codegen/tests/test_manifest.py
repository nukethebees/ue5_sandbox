import unittest
from typing import override
from Codegen.manifest import modules
from Codegen.cpp import (
    FunctionBody,
    FunctionParameter,
    Raw,
    RenderContext,
)


class ReturnParameterBody(FunctionBody):
    def __init__(self, parameter: FunctionParameter) -> None:
        self.parameter_to_return = parameter

    @override
    def render(self, context: RenderContext) -> str:
        parameter = self.function.parameter(self.parameter_to_return)
        return Raw(f"return {parameter.cpp_name};").render(context)


class ManifestTests(unittest.TestCase):
    def test_manifest_groups_multiple_soa_types_in_one_header(self) -> None:
        generated_modules = modules()
        module_names = {module.name for module in generated_modules}

        self.assertIn("test_capital_ships_soa", module_names)
        self.assertIn("test_lasers_soa", module_names)
        self.assertIn("test_laser_collision_data_soa", module_names)
        self.assertIn("collision_damage_events_soa", module_names)
        self.assertIn("test_capital_ship_fighter_order_queue", module_names)
        self.assertIn("test_entity_registry_data_soa", module_names)
        self.assertIn("registry_entity_handles_soa", module_names)
        self.assertIn("entity_death_info", module_names)
        self.assertIn("sandbox_core_soa_vectors", module_names)
        self.assertIn("soa_vectors_3f", module_names)
        self.assertIn("sandbox_core_soa_rotators", module_names)
        self.assertIn("test_capital_ship_fighters_command_interface", module_names)
        self.assertIn("test_space_ship_phase_interface", module_names)
        self.assertIn("test_lasers_phase_interface", module_names)
        self.assertIn("test_capital_ships_phase_interface", module_names)
        self.assertIn("test_capital_ship_fighters_phase_interface", module_names)
        self.assertIn("test_static_turrets_phase_interface", module_names)
        self.assertIn("test_tube_spinners_phase_interface", module_names)
        self.assertIn("fixed_soa_test_types", module_names)
        self.assertEqual(len(generated_modules), 33)
