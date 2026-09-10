#include <SandboxCore/frame_rotators.h>
#include <SandboxCore/frame_vectors.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <cstddef>
#include <memory_resource>
#include <type_traits>

TEST_CASE("SandboxCore frame SOA owners preserve fixed identity") {
    static_assert(!std::is_copy_constructible_v<FFrameVectors3f>);
    static_assert(!std::is_copy_assignable_v<FFrameVectors3f>);
    static_assert(!std::is_move_constructible_v<FFrameVectors3f>);
    static_assert(!std::is_move_assignable_v<FFrameVectors3f>);

    static_assert(!std::is_copy_constructible_v<FFrameRotatorsf>);
    static_assert(!std::is_copy_assignable_v<FFrameRotatorsf>);
    static_assert(!std::is_move_constructible_v<FFrameRotatorsf>);
    static_assert(!std::is_move_assignable_v<FFrameRotatorsf>);
}

TEST_CASE("SandboxCore frame SOA owners use a supplied resource and expose generated views") {
    alignas(std::max_align_t) std::byte storage[1024];
    std::pmr::monotonic_buffer_resource resource{storage, sizeof(storage), std::pmr::null_memory_resource()};

    FFrameVectors3f vectors{&resource};
    FFrameRotatorsf rotations{&resource};
    vectors.reserve(2);
    rotations.reserve(2);

    vectors.add(1.f, 2.f, 3.f);
    vectors.add(FVector3f{4.f, 5.f, 6.f});
    rotations.add(10.f, 20.f, 30.f);
    rotations.add(FRotator3f{40.f, 50.f, 60.f});

    vectors.validate_array_sizes();
    rotations.validate_array_sizes();
    CHECK(vectors.get_const_view()[0] == FVector3f{1.f, 2.f, 3.f});
    CHECK(vectors.get_const_view()[1] == FVector3f{4.f, 5.f, 6.f});
    CHECK(rotations.get_const_view()[0] == FRotator3f{10.f, 20.f, 30.f});
    CHECK(rotations.get_const_view()[1] == FRotator3f{40.f, 50.f, 60.f});

    vectors.clear();
    rotations.clear();
    CHECK(vectors.is_empty());
    CHECK(rotations.is_empty());
}
