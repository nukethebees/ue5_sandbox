---
name: level-designer
description: Design, modify, review, or generate authored game levels in this repository, including S7 scripted levels and their mission setup.
---

# Level Designer

Use this skill for authored-level work: designing encounters, modifying level layouts or objectives, reviewing level gameplay, and creating level content. Do not use it for general gameplay-system or unrelated C++ changes.

## Workflow

1. Inspect the level and mission DSL before changing S7 scripts. The authoritative reader is `Plugins/SpaceGame/Source/SpaceGameS7/Private/LevelDefinitionReader.cpp`.
2. Inspect relevant existing levels in `LevelScripts/` and, when applicable, authored maps in `Content/Levels/`.
3. Understand the requested gameplay goal, including the intended player experience and success conditions.
4. Make the smallest coherent level change that achieves that goal.
5. Run applicable validation or headless tests. Use the repository CMake test presets; `debug-game-unit-tests` covers DSL and catalog behavior, while `debug-game-level-tests` is appropriate for runtime level-loading behavior.

For legacy or configuration migration context, consult `docs/level_config_migration_mapping.md`. Do not duplicate those sources in this skill.

## Level Design Rules

Keep this section deliberately small. Add rules only as the project accumulates evidence about what works.

- When enemies can attack the player at level start, spawn the player outside their weapon range with enough approach distance that they cannot be hit in the first one or two seconds.
- Keep each spawned entity's complete world AABB inside the collision grid configured by the level's `USpaceGameLevelConfig`; checking only the spawn position is insufficient.
