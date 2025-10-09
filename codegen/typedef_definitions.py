"""
Strong typedef definitions for the Sandbox project.
Add new TypedefSpec entries to TYPEDEFS array to generate wrapper structs.
"""

from strong_typedefs import TypedefSpec, ConfigOptions

wrappers = {}
wrappers["FIntPoint"] = TypedefSpec(
    name="",
    underlying_type="FIntPoint",
    ops=[],
    members=[],
    config=ConfigOptions(),
)

def make_typedefs():
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
        )
    ]

    point = wrappers["FIntPoint"]

    point.name = "Coord"
    TYPEDEFS.append(point)
    point.name = "Coord"
    TYPEDEFS.append(point)

    return TYPEDEFS;

TYPEDEFS = make_typedefs()
