from pathlib import Path

from generate_skills import SkillConfig, SkillGenerator

def generate_skills() -> None:
    player_skills: list[SkillConfig] = [
        SkillConfig.attr("Strength"),
        SkillConfig.attr("Endurance"),
        SkillConfig.attr("Agility"),
        SkillConfig.attr("Cyber"),
        SkillConfig.attr("Psi"),
        SkillConfig.tech("hacking"),
        SkillConfig.weapon("small_guns")

    ]

    output_dir = Path("./Source/Sandbox/players/playable/data")
    ed_output_dir = Path("./Source/SandboxEditor/slate/")
    generator = SkillGenerator(player_skills, output_dir, ed_output_dir)
    generator.run()

def main() -> None:
    generate_skills()

if __name__ == "__main__":
    main()
