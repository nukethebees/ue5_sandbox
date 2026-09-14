#include "jobserver/protocol.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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

TEST(JobserverProtocol, JsonObjectConstructionPreservesObjectShape) {
    auto value = nlohmann::json::object();
    value["type"] = "hello";
    EXPECT_TRUE(value.is_object());
    EXPECT_EQ(value.value("type", ""), "hello");
}

TEST(JobserverProtocol, Base64RoundTripsArbitraryBytes) {
    std::string const bytes{"text\0\xff", 6};
    auto const encoded{jobserver::protocol::encode_base64(bytes)};
    auto const decoded{jobserver::protocol::decode_base64(encoded)};
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, bytes);
}

TEST(JobserverProtocol, RejectsMalformedBase64Padding) {
    EXPECT_FALSE(jobserver::protocol::decode_base64("AA=A").has_value());
    EXPECT_FALSE(jobserver::protocol::decode_base64("AAAA=AAA").has_value());
    EXPECT_FALSE(jobserver::protocol::decode_base64("A===").has_value());
}

TEST(JobserverProtocol, FilesystemPathsRoundTripUtf8) {
    auto const utf8{std::string{"C:/worktree two \xe2\x98\x83/build"}};
    EXPECT_EQ(jobserver::path_to_utf8(jobserver::path_from_utf8(utf8)), utf8);
}
