"""
Strong typedef definitions for the Sandbox project.
Add new TypedefSpec entries to TYPEDEFS array to generate wrapper structs.
"""

from strong_typedefs import TypedefSpec, ConfigOptions


TYPEDEFS = [
    TypedefSpec(
        name="Ammo",
        underlying_type="float",
        ops=["comparison", "arithmetic"],
        members=[],
        config=ConfigOptions(),
    ),
    TypedefSpec(
        name="StackSize",
        underlying_type="int32",
        ops=["comparison", "arithmetic", "increment", "modulo"],
        members=[],
        config=ConfigOptions(),
    ),
]
