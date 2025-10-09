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
    typedefs = [
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
    typedefs.append(point)
    point.name = "Dimensions"
    typedefs.append(point)

    typedefs.append(
        TypedefSpec(
            name="ItemPtr",
            underlying_type="TScriptInterface<IInventoryItem>",
            ops=["dereference"],
            members=[],
            includes=["Sandbox/interfaces/inventory/InventoryItem.h"],
            config=ConfigOptions(),
        )
    )

    

    return typedefs;

TYPEDEFS = make_typedefs()
