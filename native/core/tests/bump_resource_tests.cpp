#include <ioj/memory/bump_resource.h>

#include <gtest/gtest.h>

#include <cstdint>

namespace ioj::memory::tests {
class CountingResource final : public std::pmr::memory_resource {
  public:
    std::size_t live_bytes{};
    std::size_t last_alignment{};
  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        auto* allocation{std::pmr::new_delete_resource()->allocate(bytes, alignment)};
        live_bytes += bytes;
        last_alignment = alignment;
        return allocation;
    }
    void do_deallocate(void* allocation, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(allocation, bytes, alignment);
        live_bytes -= bytes;
        last_alignment = alignment;
    }
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }
};

class BumpResourceTest : public ::testing::Test {
  protected:
    CountingResource upstream_;
    BumpResource resource_{512, upstream_};
};

TEST_F(BumpResourceTest, ReusesBufferAfterReset) {
    auto* const first{resource_.allocate(1, 1)};
    auto* const aligned{resource_.allocate(128, 256)};
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) % 256, 0u);
    resource_.deallocate(aligned, 128, 256);
    resource_.deallocate(first, 1, 1);
    EXPECT_GT(resource_.used_bytes(), 128u);
    resource_.reset();
    EXPECT_EQ(resource_.used_bytes(), 0u);
    auto* const reused{resource_.allocate(1, 1)};
    EXPECT_EQ(reused, first);
    resource_.deallocate(reused, 1, 1);
}

TEST_F(BumpResourceTest, ForwardsOverflowAndItsDeallocation) {
    auto* const local{resource_.allocate(512, 64)};
    {
        auto* const overflow{resource_.allocate(1024, 256)};
        EXPECT_EQ(upstream_.live_bytes, 1536u);
        EXPECT_EQ(upstream_.last_alignment, 256u);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(overflow) % 256, 0u);
        resource_.deallocate(overflow, 1024, 256);
    }
    EXPECT_EQ(upstream_.live_bytes, 512u);
    EXPECT_EQ(upstream_.last_alignment, 256u);
    EXPECT_EQ(resource_.used_bytes(), 512u);
    resource_.deallocate(local, 512, 64);
}

TEST_F(BumpResourceTest, ReleasesBackingAndForwardsWithoutBacking) {
    {
        BumpResource forwarding{0, upstream_};
        auto* const allocation{forwarding.allocate(128, 64)};
        EXPECT_EQ(upstream_.live_bytes, 640u);
        forwarding.deallocate(allocation, 128, 64);
        EXPECT_EQ(upstream_.live_bytes, 512u);
    }
    {
        BumpResource nested{128, upstream_};
        EXPECT_EQ(upstream_.live_bytes, 640u);
    }
    EXPECT_EQ(upstream_.live_bytes, 512u);
}

TEST_F(BumpResourceTest, DeallocationDoesNotRewindTheBuffer) {
    auto* const first{resource_.allocate(128, 64)};
    resource_.deallocate(first, 128, 64);
    auto* const second{resource_.allocate(128, 64)};
    EXPECT_NE(second, first);
    EXPECT_EQ(resource_.used_bytes(), 256u);
    resource_.deallocate(second, 128, 64);
    EXPECT_EQ(upstream_.live_bytes, 512u);
}
}
