#pragma once

namespace codegen {

struct FunctionFormatting {
    enum class BodyLayout {
        expanded,
        compact,
    };

    enum class RequiresPlacement {
        trailing_next_line,
        trailing_same_line,
        before_signature,
    };

    enum class TemplatePlacement {
        separate_line,
        same_line,
    };

    enum class OpeningBracePlacement {
        same_line,
        separate_line,
    };

    enum class TrailingReturnPlacement {
        same_line,
        next_line,
    };

    enum class DispositionPlacement {
        same_line,
        next_line,
    };

    BodyLayout body_layout{BodyLayout::expanded};
    RequiresPlacement requires_placement{RequiresPlacement::trailing_next_line};
    TemplatePlacement template_placement{TemplatePlacement::separate_line};
    OpeningBracePlacement opening_brace_placement{OpeningBracePlacement::same_line};
    TrailingReturnPlacement trailing_return_placement{TrailingReturnPlacement::same_line};
    DispositionPlacement disposition_placement{DispositionPlacement::same_line};
};

} // namespace codegen
