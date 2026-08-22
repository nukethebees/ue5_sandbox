#include "SandboxCore/multi_buffer.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <type_traits>

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

TEST_CASE("SandboxCore.MultiBuffer.ForEachVisitsAndMutatesEveryBuffer") {
    ml::MultiBuffer<int32, 3> buffer{{10, 20, 30}};
    int32 visit_count{0};

    buffer.for_each([&visit_count](auto&& value) {
        static_assert(std::is_same_v<decltype(value), int32&>);
        value += 1;
        ++visit_count;
    });

    CHECK(visit_count == 3);
    CHECK(buffer.current() == 11);
    CHECK(buffer.next() == 21);

    buffer.cycle();
    buffer.cycle();
    CHECK(buffer.current() == 31);
}

TEST_CASE("SandboxCore.MultiBuffer.ConstForEachVisitsEveryBufferAsConst") {
    ml::MultiBuffer<int32, 3> const buffer{{10, 20, 30}};
    int32 visit_count{0};
    int32 sum{0};

    buffer.for_each([&visit_count, &sum](auto&& value) {
        static_assert(std::is_same_v<decltype(value), int32 const&>);
        ++visit_count;
        sum += value;
    });

    CHECK(visit_count == 3);
    CHECK(sum == 60);
}
