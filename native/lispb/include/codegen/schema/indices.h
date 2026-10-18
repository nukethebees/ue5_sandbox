#pragma once

#include <lispb/index.h>

namespace lispb::codegen {

struct EnumUnrealProjections;
struct EnumeratorSchemas;
struct FacadeMethodSchemas;
struct FixedSoaSchemas;
struct FunctionSchemas;
struct HomogeneousValueSchemas;
struct ModuleSettingsStore;
struct PackedNamedCodeSchemas;
struct ParameterSchemas;
struct RecordMemberSchemas;
class RegisteredTypes;
struct SemanticRelationSchemas;
struct SettingCategorySchemas;
struct SettingControlSchemas;
struct SettingSchemas;
struct SoaAllocators;
struct SoaMaskDimensionSchemas;
struct SoaMemberSchemas;
struct StaticTableColumnSchemas;
struct StaticTableGroupSchemas;
struct StaticTableRowSchemas;
struct TaggedUnionAlternativeSchemas;
struct TypeRefs;
struct UnionAlternativeSchemas;

using EnumUnrealProjectionId = Index<EnumUnrealProjections>;
using EnumeratorId = Index<EnumeratorSchemas>;
using FacadeMethodId = Index<FacadeMethodSchemas>;
using FixedSoaId = Index<FixedSoaSchemas>;
using FunctionSchemaId = Index<FunctionSchemas>;
using HomogeneousValueId = Index<HomogeneousValueSchemas>;
using ModuleSettingsId = Index<ModuleSettingsStore>;
using PackedNamedCodeId = Index<PackedNamedCodeSchemas>;
using ParameterId = Index<ParameterSchemas>;
using RecordMemberId = Index<RecordMemberSchemas>;
using RegisteredTypeId = Index<RegisteredTypes>;
using SemanticRelationId = Index<SemanticRelationSchemas>;
using SettingCategoryId = Index<SettingCategorySchemas>;
using SettingControlId = Index<SettingControlSchemas>;
using SettingId = Index<SettingSchemas>;
using SoaAllocatorVariantId = Index<SoaAllocators>;
using SoaMaskDimensionId = Index<SoaMaskDimensionSchemas>;
using SoaMemberId = Index<SoaMemberSchemas>;
using StaticTableColumnId = Index<StaticTableColumnSchemas>;
using StaticTableGroupId = Index<StaticTableGroupSchemas>;
using StaticTableRowId = Index<StaticTableRowSchemas>;
using TaggedUnionAlternativeId = Index<TaggedUnionAlternativeSchemas>;
using TypeRefId = Index<TypeRefs>;
using UnionAlternativeId = Index<UnionAlternativeSchemas>;

}
