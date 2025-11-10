import os

from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, NewType, Any, Generator

SkillId = NewType("SkillId", int)
CategoryIndex = NewType("CategoryIndex", int)

@dataclass
class SkillConfig:
    name: str
    max_level: int
    category: str
    display_name: Optional[str] = None

    ATTRIBUTE_MAX_LEVEL: int = 10
    ATTRIBUTE_CATEGORY: str = "Attributes"

    TECH_MAX_LEVEL: int = 10
    TECH_CATEGORY: str = "Tech"

    WEAPON_MAX_LEVEL: int = 10
    WEAPON_CATEGORY: str = "Weapon"

    PSI_MAX_LEVEL: int = 10
    PSI_CATEGORY: str = "Psi"

    @classmethod
    def attr(cls, name: str):
        return cls(name=name, 
                   max_level=cls.ATTRIBUTE_MAX_LEVEL, 
                   category=cls.ATTRIBUTE_CATEGORY)
    @classmethod
    def tech(cls, name: str):
        return cls(name=name, 
                   max_level=cls.TECH_MAX_LEVEL, 
                   category=cls.TECH_CATEGORY)
    @classmethod
    def weapon(cls, name: str):
        return cls(name=name, 
                   max_level=cls.WEAPON_MAX_LEVEL, 
                   category=cls.WEAPON_CATEGORY)
    @classmethod
    def psi(cls, name: str):
        return cls(name=name, 
                   max_level=cls.PSI_MAX_LEVEL, 
                   category=cls.PSI_CATEGORY)

    def get_display_name(self) -> str:
        if self.display_name is not None:
            return self.display_name
        
        return self.name.replace("_", " ").title()
    def get_member_name(self) -> str:
        return self.name.lower()
    def get_private_member_name(self) -> str:
        return self.get_member_name() + "_"

@dataclass
class GeneratorSkill:
    config: SkillConfig
    skill_index: SkillId
    category_index: CategoryIndex

@dataclass
class GeneratorSkillCategory:
    name: str
    skill_indexes: list[SkillId]
    first_skill_index: SkillId = 0
    last_skill_index: SkillId = 0

    def add_index(self, i: SkillId) -> None:
        self.skill_indexes.append(i)
        self.first_skill_index = min(self.first_skill_index, i)
        self.last_skill_index = max(self.last_skill_index, i)
    def lower_name(self) -> str:
        return self.name.lower()
 
class SkillGenerator:
    skills: list[GeneratorSkill]

    output_dir: Path
    generated_dir: Path
    output_file_path: Path
    
    categories: dict[str, GeneratorSkillCategory]
    output_file: str
    
    namespace: str
    player_skills_enum_name: str
    player_skills_enum_typename: str

    player_skills_struct_name: str
    player_skills_struct_typename: str

    file_name: str = "player_skills"

    skill_type: str = "int32"
    skill_base_level: int = 1

    uproperty_header: str = "UPROPERTY(EditAnywhere, Category=\"Player\")"

    def __init__(self, player_skills: list[SkillConfig], output_dir: Path):
        self.categories = {}

        self.skills = []
        self.init_skills(player_skills)

        self.output_dir = output_dir       
        self.generated_dir = self.output_dir / "generated"
        self.output_file_path = self.generated_dir / f"{self.file_name}.h"

        self.output_file = ""
        
        self.namespace = "ml"

        self.player_skills_enum_name = "PlayerSkillName"
        self.player_skills_enum_typename = "E" + self.player_skills_enum_name

        self.player_skills_struct_name = "PlayerSkills"
        self.player_skills_struct_typename = "F" + self.player_skills_struct_name

    def init_skills(self, player_skills: list[SkillConfig]) -> None:
        active_category:str = None

        for skill in player_skills:
            if skill.category not in self.categories:
                self.categories[skill.category] = GeneratorSkillCategory(skill.category, [])
                active_category = skill.category
            else:
                if (active_category != skill.category):
                    raise Exception("Category split up")

            skill_category = self.categories[skill.category]
            generator_skill = GeneratorSkill(
                skill, 
                len(self.skills), 
                len(skill_category.skill_indexes)
            )
            
            self.skills.append(generator_skill)
            skill_category.add_index(generator_skill.skill_index)

    def run(self) -> None:   
        os.makedirs(self.generated_dir, exist_ok=True)
        
        self.write_file_header()

        self.write_skills_enum()
        self.write_skills_struct()

        self.write_namespace_start()
        self.write_enum_functions()
        self.write_namespace_end()

        self.write_file_footer()
        self.write_output_file()
        self.write_include_file()

        print("Finished.")
        if False:
            for skill in self.skills:
                print(f"    {skill.config.name},\t{skill.config.category}")

    # Accessors
    def get_category_skills(self, cat:GeneratorSkillCategory) -> Generator[GeneratorSkill]:
        for skill_idx in cat.skill_indexes:
            yield self.skills[skill_idx]

    # File writing
    def write_output_file(self):
        with open(self.output_file_path, "w") as file:
            file.write(self.output_file)
    def write_include_file(self) -> None:
        """Create a file in the non-generated directory
        This is to avoid breaking include paths if we change to a non-generated version in future
        """
        

        file_path = self.output_dir / f"{self.file_name}.h"
        include_path = Path(*self.output_file_path.parts[1:]).as_posix()

        out: str = f"""#pragma once

#include "{include_path}"
"""

        with open(file_path, "w") as file:
            file.write(out)

    # Helpers
    def category_loop(self, fn:Callable[[SkillGenerator, GeneratorSkill], None]) -> None:
        active_category = None
        for skill in self.skills:
            if active_category != skill.config.category:
                if active_category is not None:
                    self.output_file += "\n"
                active_category = skill.config.category
                self.output_file += f"    // {active_category}\n"
            fn(self, skill)

    # File contents
    def write_file_header(self) -> None:
        self.output_file += f"""#pragma once

/*
Warning: This is an autogenerated file.
*/

// clang-format off
#include <utility>

#include "CoreMinimal.h"

#include "{self.file_name}.generated.h"

"""
    def write_file_footer(self) -> None:
        self.output_file += f"// clang-format on\n"
    def write_skills_enum(self) -> None:
        def add_line(self: SkillGenerator, skill: GeneratorSkill):
            self.output_file += f"    {skill.config.name} UMETA(DisplayName = \"{skill.config.get_display_name()}\"),\n"

        self.output_file += f"""UENUM(BlueprintType)
enum class {self.player_skills_enum_typename} : uint8 {{
"""
        self.category_loop(add_line)
        self.output_file += "};\n"

    def write_skills_struct(self) -> None:
        def add_accessors(self: SkillGenerator, skill: GeneratorSkill):
            self.output_file += f"""    auto get_{skill.config.get_member_name()}() const -> {self.skill_type} {{ return {skill.config.get_member_name()}_; }}
    void set_{skill.config.get_member_name()}({self.skill_type} value) {{ 
        {skill.config.get_member_name()}_ = std::min({skill.config.get_private_member_name()}, {skill.config.max_level});
    }}
"""
        def add_member(self: SkillGenerator, skill: GeneratorSkill):
            self.output_file += f"""    {self.uproperty_header}
    {self.skill_type} {skill.config.get_private_member_name()}{{{self.skill_base_level}}};
"""

        self.output_file += f"""
USTRUCT(BlueprintType)
struct {self.player_skills_struct_typename} {{
    GENERATED_BODY()
"""

        self.output_file += "  public:\n"
        self.category_loop(add_accessors)          
        self.output_file += "  private:\n"
        self.category_loop(add_member)

        self.output_file += "};\n"
    
    def create_constant(self, var_type:str, name:str, value:Any) -> str:
        return f"inline constexpr {var_type} {name}{{{value}}};"

    def write_enum_functions(self) -> None:
        # Create constant
        cc = self.create_constant
        t = "int32"

        for category in self.categories.values():
            lname = category.lower_name()

            self.output_file += f"""
// {category.name}
// ----------------------------------------------------------------------
{cc(t, f"num_{lname}_values", len(category.skill_indexes))}
// Index functions for looping
{cc(t, f"{lname}_start_index", category.first_skill_index)}
// The last index (inclusive)
{cc(t, f"{lname}_last_index", category.last_skill_index)}
// The index after the last index 
{cc(t, f"{lname}_end_index", category.last_skill_index + 1)}

inline constexpr auto is_{lname}({self.player_skills_enum_typename} value) -> bool {{
    auto const x{{std::to_underlying(value)}};

    return ((x >= {category.first_skill_index}) && (x <= {category.last_skill_index}));
}}

"""
            n = len(category.skill_indexes)
            self.output_file += f"inline constexpr TStaticArray<{self.skill_type}, {n}> {lname}_values{{{{\n"
            first = True
            for skill in self.get_category_skills(category):
                if not first:
                    self.output_file += ",\n"
                self.output_file += f"    {self.player_skills_enum_typename}::{skill.config.name}"
                first = False
            self.output_file += "\n}};\n\n"
                

        # String functions
        self.output_file += f"// String functions\n"
        self.write_get_display_fname()
    def write_get_display_fname(self) -> None:
        self.output_file += f"""inline auto get_display_fname({self.player_skills_enum_typename} value) -> FName {{
    switch (value) {{
"""
        for skill in self.skills:
            self.output_file += f"""        case {self.player_skills_enum_typename}::{skill.config.name}: {{
            static auto const name{{FName{{TEXT("{skill.config.get_display_name()}")}}}};
            return name;
        }}
"""
        

        self.output_file += """        default: {
            break;
        }
    }

    return FName{TEXT("UNHANDLED_CASE")};
}
"""

    def write_namespace_start(self) -> None:
        self.output_file += f"namespace {self.namespace} {{"
    def write_namespace_end(self) -> None:
        self.output_file += f"}} // namespace {self.namespace}\n"

