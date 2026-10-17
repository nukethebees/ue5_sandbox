#pragma once

#include <lispb/schema/create_declaration.h>
#include <lispb/schema/create_enum.h>
#include <lispb/schema/create_fixed_point.h>
#include <lispb/schema/create_integer_scalar.h>
#include <lispb/schema/create_integer_varint.h>
#include <lispb/schema/create_linear_quantized.h>
#include <lispb/schema/create_mini_float.h>
#include <lispb/schema/create_module.h>
#include <lispb/schema/create_optional_presence_bit.h>
#include <lispb/schema/create_optional_sentinel.h>
#include <lispb/schema/create_packed_value.h>
#include <lispb/schema/create_record.h>
#include <lispb/schema/create_soa.h>
#include <lispb/schema/create_tagged_union.h>
#include <lispb/schema/create_union.h>
#include <lispb/schema/delete_declaration.h>
#include <lispb/schema/delete_enum.h>
#include <lispb/schema/delete_fixed_point.h>
#include <lispb/schema/delete_integer_scalar.h>
#include <lispb/schema/delete_integer_varint.h>
#include <lispb/schema/delete_linear_quantized.h>
#include <lispb/schema/delete_mini_float.h>
#include <lispb/schema/delete_module.h>
#include <lispb/schema/delete_optional_presence_bit.h>
#include <lispb/schema/delete_optional_sentinel.h>
#include <lispb/schema/delete_packed_value.h>
#include <lispb/schema/delete_record.h>
#include <lispb/schema/delete_soa.h>
#include <lispb/schema/delete_tagged_union.h>
#include <lispb/schema/delete_union.h>
#include <lispb/schema/move_declaration.h>
#include <lispb/schema/rename_declaration.h>
#include <lispb/schema/replace_declaration.h>
#include <lispb/schema/replace_enum.h>
#include <lispb/schema/replace_fixed_point.h>
#include <lispb/schema/replace_integer_scalar.h>
#include <lispb/schema/replace_integer_varint.h>
#include <lispb/schema/replace_linear_quantized.h>
#include <lispb/schema/replace_mini_float.h>
#include <lispb/schema/replace_optional_presence_bit.h>
#include <lispb/schema/replace_optional_sentinel.h>
#include <lispb/schema/replace_packed_value.h>
#include <lispb/schema/replace_record.h>
#include <lispb/schema/replace_soa.h>
#include <lispb/schema/replace_tagged_union.h>
#include <lispb/schema/replace_union.h>
#include <lispb/schema/restore_module.h>
#include <lispb/schema/set_enumerator_display_name.h>
#include <lispb/schema/set_enumerator_name.h>

#include <variant>

namespace lispb::schema {
using SchemaEditCommand = std::variant<SetEnumeratorDisplayName,
                                       SetEnumeratorName,
                                       CreateModule,
                                       DeleteModule,
                                       RestoreModule,
                                       MoveDeclaration,
                                       CreateDeclaration,
                                       ReplaceDeclaration,
                                       DeleteDeclaration,
                                       CreateEnum,
                                       ReplaceEnum,
                                       DeleteEnum,
                                       CreatePackedValue,
                                       ReplacePackedValue,
                                       DeletePackedValue,
                                       CreateIntegerScalar,
                                       ReplaceIntegerScalar,
                                       RenameDeclaration,
                                       DeleteIntegerScalar,
                                       CreateLinearQuantized,
                                       ReplaceLinearQuantized,
                                       DeleteLinearQuantized,
                                       CreateIntegerVarint,
                                       ReplaceIntegerVarint,
                                       DeleteIntegerVarint,
                                       CreateFixedPoint,
                                       ReplaceFixedPoint,
                                       DeleteFixedPoint,
                                       CreateOptionalSentinel,
                                       ReplaceOptionalSentinel,
                                       DeleteOptionalSentinel,
                                       CreateOptionalPresenceBit,
                                       ReplaceOptionalPresenceBit,
                                       DeleteOptionalPresenceBit,
                                       CreateMiniFloat,
                                       ReplaceMiniFloat,
                                       DeleteMiniFloat,
                                       CreateRecord,
                                       ReplaceRecord,
                                       DeleteRecord,
                                       CreateUnion,
                                       ReplaceUnion,
                                       DeleteUnion,
                                       CreateTaggedUnion,
                                       ReplaceTaggedUnion,
                                       DeleteTaggedUnion,
                                       CreateSoa,
                                       ReplaceSoa,
                                       DeleteSoa>;
}
