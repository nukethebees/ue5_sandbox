#include "../protocol.h"

#include <gtest/gtest.h>

namespace lispb::lsp {
TEST(LispbFileUris, RoundTripsReservedCharactersAndUnicode) {
#ifdef _WIN32
    fs::path const path{u8"C:/A B/値#%.lispb"};
    std::string const uri{"file:///C:/A%20B/%E5%80%A4%23%25.lispb"};
    EXPECT_EQ(file_path("file:///c:/A%20B/%E5%80%A4%23%25.lispb"), path);
#else
    fs::path const path{u8"/A B/値#%.lispb"};
    std::string const uri{"file:///A%20B/%E5%80%A4%23%25.lispb"};
#endif
    EXPECT_EQ(file_uri(path), uri);
    EXPECT_EQ(file_path(uri), path);
}
#ifdef _WIN32
TEST(LispbFileUris, RoundTripsUncAuthorityWithoutAccessingTheNetwork) {
    fs::path const path{LR"(\\server\share\A B.lispb)"};
    std::string const uri{"file://server/share/A%20B.lispb"};
    EXPECT_EQ(file_uri(path), uri);
    EXPECT_EQ(file_path(uri), path);
}
#endif
}
