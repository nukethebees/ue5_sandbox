import os

from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, NewType, Any, Generator, Sequence

from unreal_type import UnrealType
from file_io import FileBuffer

SkillId = NewType("SkillId", int)
CategoryIndex = NewType("CategoryIndex", int)

indent: str = "    "
default_level: int = 1

@dataclass
class SkillConfig:
    name: str
    max_level: int
    category: str
    display_name: Optional[str] = None
    enum_typename: str = ""

    ATTRIBUTE_MAX_LEVEL: int = 10
    ATTRIBUTE_CATEGORY: str = "Attribute"

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
    def get_getter_name(self) -> str:
        return f"get_{self.get_member_name()}"
    def get_setter_name(self) -> str:
        return f"set_{self.get_member_name()}"
    def get_view_name(self) -> str:
        return f"get_{self.get_member_name()}_view"
    def get_max_variable_name(self) -> str:
        return f"max_{self.get_member_name()}"
    def get_full_enum_value(self) -> str:
        if not self.enum_typename:
            raise ValueError("enum_type is missing")
        return f"{self.enum_typename}::{self.name}"

@dataclass
class GeneratorSkill:
    config: SkillConfig
    skill_index: SkillId
    category_index: CategoryIndex

@dataclass
class GeneratorSkillCategory:
    name: str
    skill_indexes: list[SkillId]
    first_skill_index: SkillId = None
    last_skill_index: SkillId = None

    def __len__(self) -> int:
        return len(self.skill_indexes)

    def add_index(self, i: SkillId) -> None:
        self.skill_indexes.append(i)
        if self.first_skill_index is None:
            self.first_skill_index = i
        else:
            self.first_skill_index = min(self.first_skill_index, i)
        if self.last_skill_index is None:
            self.last_skill_index = i
        else:
            self.last_skill_index = max(self.last_skill_index, i)
    def lower_name(self) -> str:
        return self.name.lower()
    def first_skill(self, skills: list[GeneratorSkill]) -> GeneratorSkill:
        return skills[self.first_skill_index]
    def last_skill(self, skills: list[GeneratorSkill]) -> GeneratorSkill:
        return skills[self.last_skill_index]
    def skills(self, skills: list[GeneratorSkill]) -> Generator[GeneratorSkill]:
        for i in self.skill_indexes:
            yield skills[i]

@dataclass
class CppVar:
    var_type: str
    name: str

class SkillGenerator:
    skills: list[GeneratorSkill]

    output_dir: Path
    generated_dir: Path
    
    categories: dict[str, GeneratorSkillCategory]
    header: FileBuffer
    source: FileBuffer
    ed_header: FileBuffer
    ed_source: FileBuffer
    header_include_path: Path
    
    namespace: str = "ml"

    player_skills_enum = UnrealType("PlayerSkillName", UnrealType.Tag.ENUM)
    player_skills_struct = UnrealType("PlayerSkills", UnrealType.Tag.STRUCT)
    skill_struct = UnrealType("PlayerSkill", UnrealType.Tag.STRUCT)
    details_struct_name = UnrealType(f"{player_skills_struct.rawname()}PropDisplay", UnrealType.Tag.STRUCT)

    file_name: str = player_skills_struct.rawname()

    index_typename: str = "int32"
    skill_value_typename: str = "uint8"
    skill_base_level: int = 1

    uproperty_header: str = "UPROPERTY(EditAnywhere, Category=\"Player\")"

    skill_view_typename: str = "SkillView"

    max_value_varname: str = "max_"
    skill_value_varname: str = "skill_"

    module_api: str = "SANDBOX_API"

    class SkillMembers:
        get_skill = "get_skill"
        set_skill = "set_skill"
        inc_skill = "inc_skill"
        get_max = "get_max"
        set_max = "set_max"

    class SkillsMembers:
        get_skill = "get_skill"
        get_value = "get_value"
        set_value = "set_value"


    def __init__(self, 
                 player_skills: list[SkillConfig], 
                 output_dir: Path,
                 editor_output_dir: Path):
        self.categories = {}

        self.skills = []
        self.init_skills(player_skills)

        self.output_dir = output_dir       
        self.generated_dir = self.output_dir / "generated"

        self.header = self.init_generated_file(f"{self.file_name}.h")
        self.source = self.init_generated_file(f"{self.file_name}.cpp")
        self.header_include_path = Path(*self.header.path.parts[1:]).as_posix()

        self.ed_header = FileBuffer(editor_output_dir / f"{self.details_struct_name.rawname()}.h", "", self.namespace)
        self.ed_source = FileBuffer(editor_output_dir / f"{self.details_struct_name.rawname()}.cpp", "", self.namespace)
        self.ed_header_include_path = Path(*self.ed_header.path.parts[1:]).as_posix()

    def init_generated_file(self, name:str) -> FileBuffer:
        return FileBuffer(self.generated_dir / name, "", self.namespace)

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
            skill.enum_typename = str(self.player_skills_enum)
            generator_skill = GeneratorSkill(
                skill, 
                len(self.skills), 
                len(skill_category.skill_indexes)
            )
            
            self.skills.append(generator_skill)
            skill_category.add_index(generator_skill.skill_index)

    def run(self) -> None:   
        os.makedirs(self.generated_dir, exist_ok=True)
        
        self.write_header_start()
        self.write_source_start()

        self.write_skills_enum()

        self.header.namespace_start()
        self.source.namespace_start()
        self.write_enum_functions()
        self.header.namespace_end()
        self.source.namespace_end()

        self.write_skill_struct()

        self.write_player_skills_struct()        

        self.write_detail_customisation()

        self.write_header_end()

        # File writing
        self.header.write()
        self.source.write()
        self.write_include_file()

        self.ed_header.write()
        self.ed_source.write()

        print("Finished.")
        if False:
            for skill in self.skills:
                print(f"    {skill.config.name},\t{skill.config.category}")

    # Accessors
    def get_category_skills(self, cat:GeneratorSkillCategory) -> Generator[GeneratorSkill]:
        for skill_idx in cat.skill_indexes:
            yield self.skills[skill_idx]

    # File writing
    def write_include_file(self) -> None:
        """Create a file in the non-generated directory
        This is to avoid breaking include paths if we change to a non-generated version in future
        """

        out = FileBuffer(self.output_dir / f"{self.file_name}.h")

        out += f"""#pragma once

#include "{self.header_include_path}"
"""

        out.write()

    # Helpers
    def category_loop(self, 
                      fn:Callable[[SkillGenerator, GeneratorSkill], None], 
                      is_method:bool = False) -> None:
        active_category = None
        for skill in self.skills:
            if active_category != skill.config.category:
                if active_category is not None:
                    self.header += "\n"
                active_category = skill.config.category
                self.header += f"    // {active_category}"
            if is_method:
                fn(skill)
            else:
                fn(self, skill)
    def create_constant(self, var_type:str, name:str, value:Any) -> str:
        return f"inline constexpr {var_type} {name}{{{value}}};"
    def newline(self):
        self.header += "\n"
    def index_skill(self, index_value:str) -> str:
        return f"skills_[std::to_underlying({index_value})]"

    # General file contents
    def write_header_start(self) -> None:
        self.header += f"""#pragma once

/*
Warning: This is an autogenerated file.
*/

// clang-format off
#include <array>
#include <functional>
#include <utility>

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

#include "{self.file_name}.generated.h"
"""

        self.ed_header += f"""#pragma once

/*
Warning: This is an autogenerated file.
*/

// clang-format off
#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class FDetailWidgetRow;
class IPropertyTypeCustomizationUtils;
class IDetailChildrenBuilder;"""
        

    def write_header_end(self) -> None:
        self.header += f"// clang-format on\n"
        self.ed_header += f"// clang-format on\n"
    def write_source_start(self) -> None:
        self.source += f'''#include "{self.header_include_path}"'''
        self.ed_source += f'''#include "{self.ed_header_include_path}"

#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#include "{self.header_include_path}"'''
   
    # Enum generation
    def write_skills_enum(self) -> None:
        def add_line(self: SkillGenerator, skill: GeneratorSkill):
            self.header += f"\n    {skill.config.name} UMETA(DisplayName = \"{skill.config.get_display_name()}\"),"

        self.header += f"""
UENUM(BlueprintType)
enum class {self.player_skills_enum} : uint8 {{
"""
        self.category_loop(add_line)
        self.header += "\n};"

    def write_enum_functions(self) -> None:
        arr_start = f"inline constexpr std::array<{self.player_skills_enum},"

        n = len(self.skills)
        self.header += f"\n{arr_start} {n}> {self.player_skills_enum}_values{{{{\n"
        first = True
        for skill in self.skills:
            if not first:
                self.header += ",\n"
            self.header += f"    {skill.config.get_full_enum_value()}"
            first = False
        self.header += "\n}};"

        for category in self.categories.values():
            lname = category.lower_name()

            n = len(category.skill_indexes)
            self.header += f"\n{arr_start} {n}> {lname}_values{{{{\n"
            first = True
            for skill in self.get_category_skills(category):
                if not first:
                    self.header += ",\n"
                self.header += f"    {skill.config.get_full_enum_value()}"
                first = False
            self.header += "\n}};"
                

        # String functions
        self.header += f"\n\n// String functions"
        self.write_get_display_name()
        self.write_get_display_string()
        self.write_get_display_string_view()
        self.write_get_display_text()

    def write_get_display_name(self) -> None:
        sig = f"\n{self.module_api} auto get_display_name({self.player_skills_enum} value) -> FName"

        self.header += f"{sig};"
        self.source += f"""{sig} {{
    switch (value) {{
"""
        for skill in self.skills:
            self.source += f"""        case {skill.config.get_full_enum_value()}: {{
            static FName const name{{TEXT("{skill.config.get_display_name()}")}};
            return name;
        }}
"""
        self.source += """        default: {
            break;
        }
    }

    static auto const unhandled_name{FName{TEXT("UNHANDLED_CASE")}};
    return unhandled_name;
}"""
    def write_get_display_string(self) -> None:
        sig = f"\n{self.module_api} auto get_display_string({self.player_skills_enum} value) -> FString const&"

        self.header += f"{sig};"
        self.source += f"""{sig} {{
    switch (value) {{
"""
        for skill in self.skills:
            self.source += f"""        case {skill.config.get_full_enum_value()}: {{
            static FString const name{{get_display_name(value).ToString()}};
            return name;
        }}
"""
        self.source += """        default: {
            break;
        }
    }

    static FString const unhandled_name{TEXT("UNHANDLED_CASE")};
    return unhandled_name;
}"""
    def write_get_display_string_view(self) -> None:
        sig = f"\n{self.module_api} auto get_display_string_view({self.player_skills_enum} value) -> TStringView<TCHAR>"

        self.header += f"{sig};"
        self.source += f"""{sig} {{
    return get_display_string(value);
}}"""
    def write_get_display_text(self) -> None:
        sig = f"\n{self.module_api} auto get_display_text({self.player_skills_enum} value) -> FText"

        self.header += f"{sig};"
        self.source += f"""{sig} {{
    return FText::FromStringView(get_display_string_view(value));
}}"""

    # Struct generation
    def write_struct_skill_enum_accessors(self) -> None:
        key = "skill_type"
        input = "value"

        indexed = self.index_skill(key)
        m = self.SkillsMembers

        self.header += f"""
    template <{self.player_skills_enum} {key}, typename Self>
    constexpr auto {m.get_value}(this Self const& self) {{
        return self.{indexed}.{m.get_skill}();
    }}
    template <typename Self>
    constexpr auto {m.get_value}(this Self const& self, {self.player_skills_enum} {key}) {{
        return self.{indexed}.{m.get_skill}();
    }}
    template <{self.player_skills_enum} {key}, typename Self>
    constexpr auto {m.get_skill}(this Self&& self) -> auto&& {{
        return std::forward_like<Self>(self.{indexed});
    }}
    template <typename Self>
    constexpr auto {m.get_skill}(this Self const& self, {self.player_skills_enum} {key}) -> auto&& {{
        return std::forward_like<Self>(self.{indexed});
    }}
    template <{self.player_skills_enum} {key}, typename Self>
    auto get_view(this Self& self) {{
        return SkillView{{
            {key},
            ml::get_display_string({key}),
            self.template {m.get_skill}<{key}>()
        }};
    }}
    template <{self.player_skills_enum} {key}, typename Self>
    constexpr void {m.set_value}(this Self& self, {self.skill_value_typename} {input}) {{
        self.template {m.get_skill}<{key}>.set({input});    
    }}
    constexpr void {m.set_value}(this auto& self, {self.player_skills_enum} {key}, {self.skill_value_typename} {input}) {{
        self.template {m.get_skill}<{key}>.set({input});
    }}"""
    def add_struct_accessor(self, skill: GeneratorSkill):
        e = skill.config.get_full_enum_value()
        indexed = self.index_skill(e)

        self.header += f"""
    auto {skill.config.get_view_name()}() {{ return get_view<{skill.config.get_full_enum_value()}>(); }}"""
    def add_struct_max_value_variable(self, skill:GeneratorSkill) -> None:
        self.header += f"""    static constexpr {self.skill_value_typename} {skill.config.get_max_variable_name()}{{{skill.config.max_level}}};
"""
    def add_category_view_functions(self) -> None:
        self.header += "    // Category views"
        
        def arr_type(n:int) -> str:
            return f"std::array<{self.skill_view_typename}, {n}>"
        def fn_sig(name:str, array:str) -> str:
            return f"auto get_{name}_views(this auto& self) -> {array} {{"

        n = len(self.skills)
        self.header += f'''
    {fn_sig("all", arr_type(n))}
        return {{{{'''
        first = True
        for skill in self.skills:
            if not first:
                self.header += ","
            self.header += f"\n{" "*12}self.{skill.config.get_view_name()}()"
            first = False
        self.header += '''    
        }};
    }'''


        for cat in self.categories.values():
            array = arr_type(len(cat))
            self.header += f"""
    {fn_sig(cat.lower_name(), array)}
        return {{{{"""
            first = True
            for skill in cat.skills(self.skills):
                if not first:
                    self.header += ","
                self.header += f"\n            self.{skill.config.get_view_name()}()"
                first = False
            self.header += "\n        }};\n    }"
    def write_private_variables(self) -> None:
        array = f"std::array<{self.skill_struct}, {len(self.skills)}>"
        self.header += f"""
    {array} skills_{{{{"""
        first = True
        for skill in self.skills:
            if not first:
                self.header += ","
            self.header += f"""
        {{{self.skill_value_typename}{{{default_level}}}, {self.skill_value_typename}{{{skill.config.max_level}}}}}"""
            first = False
        self.header += "\n    }};"
    def write_player_skills_struct(self) -> None:
        self.header += f"""
USTRUCT(BlueprintType)
struct {self.player_skills_struct} {{
    GENERATED_BODY()
"""

        self.header += "  public:"
        self.header += f"""
    static constexpr int32 N_SKILLS{{{len(self.skills)}}};

    struct {self.skill_view_typename} {{
        {self.skill_struct}& skill;
        FString const& name;
        {self.player_skills_enum} enum_key;

        {self.skill_view_typename}() = delete;
        {self.skill_view_typename}({self.player_skills_enum} enum_key, 
                  FString const& name, 
                  {self.skill_struct}& skill)
            : skill(skill)
            , name(name)
            , enum_key(enum_key)
        {{}};
    }};"""
        self.write_struct_skill_enum_accessors()
        self.newline()
        self.category_loop(self.add_struct_accessor, True)
        self.newline()
        self.add_category_view_functions()
        self.newline()  
        self.header += "  private:"
        self.write_private_variables()

        self.header += "\n};\n"
    
    def write_skill_struct(self) -> None:
        m = self.SkillMembers

        self.header += f"""

USTRUCT(BlueprintType)
struct {self.skill_struct} {{
    GENERATED_BODY()
  public:
    {self.skill_struct}() = default;
    {self.skill_struct}({self.skill_value_typename} {self.skill_value_varname}, {self.skill_value_typename} {self.max_value_varname})
        : {self.skill_value_varname}({self.skill_value_varname})
        , {self.max_value_varname}({self.max_value_varname})
    {{}}

    auto {m.get_skill}(this auto const& self) {{
        return self.{self.skill_value_varname};
    }}
    auto {m.set_skill}(this auto& self, {self.skill_value_typename} input) {{
        self.{self.skill_value_varname} = std::min(input, self.{self.max_value_varname});
    }}
    auto {m.inc_skill}(this auto& self) {{
        self.{m.set_skill}(self.{self.skill_value_varname} + {self.skill_value_typename}{{1}});
    }}
    auto {m.get_max}(this auto const& self) {{
        return self.{self.max_value_varname};
    }}
  private:
    {self.uproperty_header}
    {self.skill_value_typename} {self.skill_value_varname}{{1}};
    {self.uproperty_header}
    {self.skill_value_typename} {self.max_value_varname}{{1}};
}};
"""
        
    # Display generation
    def write_detail_customisation(self) -> None:
        struct_name = self.details_struct_name

        ph = "PropertyHandle"
        cb = "ChildBuilder"
        cu = "CustomizationUtils"
        hr = "HeaderRow"

        self.ed_header += f"""
struct {struct_name} : public IPropertyTypeCustomization {{
  public:
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> {ph}, 
                                 FDetailWidgetRow& {hr}, 
                                 IPropertyTypeCustomizationUtils& {cu}) override;
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> {ph}, 
                                   IDetailChildrenBuilder& {cb}, 
                                   IPropertyTypeCustomizationUtils& {cu}) override;
    
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();
}};
"""
        prefix = f"{struct_name}::"       

        SM = self.SkillMembers
        self.ed_source += f"""

void {prefix}CustomizeHeader(
        TSharedRef<IPropertyHandle> {ph}, 
        FDetailWidgetRow& {hr}, 
        IPropertyTypeCustomizationUtils& {cu}) {{
                                    
}}
void {prefix}CustomizeChildren(
        TSharedRef<IPropertyHandle> {ph}, 
        IDetailChildrenBuilder& {cb}, 
        IPropertyTypeCustomizationUtils& {cu}) {{
	if (!{ph}->IsValidHandle())
	{{
	    return;
	}}
	 
    void* raw_data{{nullptr}};
    if (auto result{{{ph}->GetValueData(raw_data)}}; result != FPropertyAccess::Result::Success) {{
        return;
    }}
    auto* data{{reinterpret_cast<FPlayerSkills*>(raw_data)}};

	for (auto value : ml::EPlayerSkillName_values) {{
        auto const& name{{ml::get_display_string_view(value)}};
        auto skill{{data->{self.SkillsMembers.get_skill}(value)}};

	    {cb}.AddCustomRow(FText::FromStringView(name))
	        .NameContent()
	        [
	            SNew(STextBlock)
	            .Text(FText::FromStringView(name))
	            .Font(IDetailLayoutBuilder::GetDetailFont())
	        ]
            .ValueContent()
            [
                SNew(STextBlock).Text(FText::AsNumber(skill.{SM.get_skill}()))
            ];
    }}
}}
    
TSharedRef<IPropertyTypeCustomization> {prefix}MakeInstance() {{
    return MakeShareable(new {struct_name});
}}
"""