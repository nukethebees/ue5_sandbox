#include "SandboxCore/multi_buffer.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.MultiBuffer.Initially aliases previous and current") {
    ml::MultiBuffer<int32, 3> buffer{{10, 20, 30}};

    CHECK(buffer.previous() == 10);
    CHECK(buffer.current() == 10);
    CHECK(buffer.next() == 20);
}

TEST_CASE("SandboxCore.MultiBuffer.MultiBuffer cycles through its buffers") {
    ml::MultiBuffer<int32, 3> buffer{{10, 20, 30}};

    buffer.cycle();

    CHECK(buffer.previous() == 10);
    CHECK(buffer.current() == 20);
    CHECK(buffer.next() == 30);

    buffer.cycle();

    CHECK(buffer.previous() == 20);
    CHECK(buffer.current() == 30);
    CHECK(buffer.next() == 10);
}

TEST_CASE("SandboxCore.MultiBuffer.Double MultiBuffer wraps correctly") {
    ml::MultiBuffer<int32, 2> buffer{{10, 20}};

    buffer.cycle();

    CHECK(buffer.previous() == 10);
    CHECK(buffer.current() == 20);
    CHECK(buffer.next() == 10);
}
