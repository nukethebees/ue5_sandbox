#include "jobserver/protocol.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>

TEST(JobserverProtocol, FramesPayloadWithLittleEndianSize) {
    auto const frame{jobserver::protocol::encode_frame("abc")};
    ASSERT_TRUE(frame.has_value());
    ASSERT_EQ(frame->size(), 7U);
    EXPECT_EQ(std::to_integer<unsigned>((*frame)[0]), 3U);
    EXPECT_EQ(std::to_integer<unsigned>((*frame)[1]), 0U);
    EXPECT_EQ(static_cast<char>((*frame)[4]), 'a');
}

TEST(JobserverProtocol, RejectsOversizedPayload) {
    auto const frame{jobserver::protocol::encode_frame(
        std::string(jobserver::protocol::maximum_payload_size + 1U, 'x'))};
    ASSERT_FALSE(frame.has_value());
    EXPECT_EQ(frame.error().code, "payload_too_large");
}

TEST(JobserverProtocol, RejectsOversizedHeaderBeforeAllocation) {
    std::array<std::byte, 4> const header{std::byte{1}, std::byte{0}, std::byte{16}, std::byte{0}};
    auto const size{jobserver::protocol::decode_header(header)};
    EXPECT_FALSE(size.has_value());
}
