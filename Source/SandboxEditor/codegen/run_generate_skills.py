from pathlib import Path

from generate_skills import SkillConfig, PlayerSkills, SkillGenerator

def main() -> None:
    skills: list[SkillConfig] = [
        SkillConfig.attr("Strength"),
        SkillConfig.attr("Endurance"),
        SkillConfig.attr("Agility"),
        SkillConfig.attr("Cyber"),
        SkillConfig.attr("Psi"),
        SkillConfig.tech("hacking"),
        SkillConfig.weapon("small_guns")

    ]
    player_skills = PlayerSkills(skills=skills)

    output_dir = Path("./Source/Sandbox/players/playable/data")
    generator = SkillGenerator(player_skills, output_dir)
    generator.run()
    

if __name__ == "__main__":
    main()
