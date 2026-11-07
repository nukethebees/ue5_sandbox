#pragma once

#include <lispb/index.h>

namespace lispb::codegen {

struct EnumUnrealProjections;
struct EnumeratorSchemas;
struct FacadeMethodSchemas;
struct FunctionSchemas;
struct ModuleSettingsStore;
struct PackedNamedCodeSchemas;
struct ParameterSchemas;
struct ChildOccurrences;
class RegisteredTypes;
struct SemanticRelationSchemas;
struct SettingCategorySchemas;
struct SettingControlSchemas;
struct SettingSchemas;
struct SoaMaskDimensionSchemas;
struct SoaMemberSchemas;
struct StaticTableColumnSchemas;
struct StaticTableGroupSchemas;
struct StaticTableRowSchemas;
struct TypeRefs;

using EnumUnrealProjectionId = Index<EnumUnrealProjections>;
using EnumeratorId = Index<EnumeratorSchemas>;
using FacadeMethodId = Index<FacadeMethodSchemas>;
using FunctionSchemaId = Index<FunctionSchemas>;
using ModuleSettingsId = Index<ModuleSettingsStore>;
using PackedNamedCodeId = Index<PackedNamedCodeSchemas>;
using ParameterId = Index<ParameterSchemas>;
using ChildOccurrenceId = Index<ChildOccurrences>;
using RegisteredTypeId = Index<RegisteredTypes>;
using SemanticRelationId = Index<SemanticRelationSchemas>;
using SettingCategoryId = Index<SettingCategorySchemas>;
using SettingControlId = Index<SettingControlSchemas>;
using SettingId = Index<SettingSchemas>;
using SoaMaskDimensionId = Index<SoaMaskDimensionSchemas>;
using SoaMemberId = Index<SoaMemberSchemas>;
using StaticTableColumnId = Index<StaticTableColumnSchemas>;
using StaticTableGroupId = Index<StaticTableGroupSchemas>;
using StaticTableRowId = Index<StaticTableRowSchemas>;
using TypeRefId = Index<TypeRefs>;

}
