from pathlib import Path

from generate_skills import SkillConfig, SkillGenerator
import string_constants_generator as scg

import project_constants as pc


def generate_skills() -> None:
    player_skills: list[SkillConfig] = [
        SkillConfig.attr("Strength"),
        SkillConfig.attr("Endurance"),
        SkillConfig.attr("Agility"),
        SkillConfig.attr("Cyber"),
        SkillConfig.attr("Psi"),
        SkillConfig.tech("hacking"),
        SkillConfig.weapon("small_guns"),
    ]

    output_dir = pc.Paths.sandbox_source / "players"
    ed_output_dir = Path("./Source/SandboxEditor/slate/")
    generator = SkillGenerator(player_skills, output_dir, ed_output_dir)
    generator.run()


def generate_string_constants() -> None:
    CI = scg.ConstantInput

    inputs: list[CI] = [
        CI(*vals)
        for vals in (
            ["acceptable_radius"],
            ["attack_radius"],
            ["default_ai_state"],
            ["ai_state"],
            ["mob_attack_mode"],
            ["target_actor"],
            ["last_known_location"],
            ["defend_actor"],
            ["defend_position"],
        )
    ]
    name = "TestEnemyBlackboardConstants"
    file_path = pc.Paths.sandbox_source / f"players/{name}.h"
    gen = scg.ConstantGenerator(name, inputs)

    with open(file_path, "w") as file:
        file.write(gen.generate_file())


def main() -> None:
    generate_skills()
    generate_string_constants()


if __name__ == "__main__":
    main()
