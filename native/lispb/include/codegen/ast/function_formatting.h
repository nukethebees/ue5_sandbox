#pragma once

#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_disposition_placement.h>
#include <codegen/ast/function_opening_brace_placement.h>
#include <codegen/ast/function_requires_placement.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/function_trailing_return_placement.h>

namespace lispb::codegen {

struct FunctionFormatting {

    FunctionBodyLayout body_layout{FunctionBodyLayout::expanded};
    FunctionRequiresPlacement requires_placement{FunctionRequiresPlacement::trailing_next_line};
    FunctionTemplatePlacement template_placement{FunctionTemplatePlacement::separate_line};
    FunctionOpeningBracePlacement opening_brace_placement{FunctionOpeningBracePlacement::same_line};
    FunctionTrailingReturnPlacement trailing_return_placement{
        FunctionTrailingReturnPlacement::same_line};
    FunctionDispositionPlacement disposition_placement{FunctionDispositionPlacement::same_line};
    bool export_after_return_type{false};
};

} // namespace lispb::codegen
