---
name: level-designer
description: Design, modify, review, or generate authored game levels in this repository, including S7 scripted levels and their mission setup.
---

# Level Designer

Use this skill for authored-level work: designing encounters, modifying level layouts or objectives, reviewing level gameplay, and creating level content. Do not use it for general gameplay-system or unrelated C++ changes.

## Workflow

1. Inspect the level and mission DSL before changing S7 scripts. Use the [mission guide](../../../LevelScripts/README.md) for examples and the [S7 input schema](../../../lispb/schema/level_inputs.lispb) for record fields.
2. Inspect relevant existing levels in `LevelScripts/` and, when applicable, authored maps in `Content/Levels/`.
3. Understand the requested gameplay goal, including the intended player experience and success conditions.
4. Make the smallest coherent level change that achieves that goal.
5. Use the `native-level-authoring` CTest label for DSL and catalog changes. Use `debug-game-level-tests` for Unreal level loading.

## Level Design Rules

Keep this section deliberately small. Add rules only as the project accumulates evidence about what works.

- When enemies can attack the player at level start, spawn the player outside their weapon range with enough approach distance that they cannot be hit in the first one or two seconds.
- Keep each spawned entity's full world AABB inside the mission's `:collision-grid`. Check the [base settings](../../../LevelScripts/Settings/base.scm) and any mission overrides.
