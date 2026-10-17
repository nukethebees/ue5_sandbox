#pragma once

namespace codegen {

struct AccessSpecifier {
    enum class Access { public_, protected_, private_ };
    enum class Indentation {
        outdented,
        normal,
    };

    Access access{Access::public_};
    Indentation indentation{Indentation::outdented};
};

} // namespace codegen
