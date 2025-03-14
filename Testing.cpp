#include <gtest/gtest.h>
#include "Archive.hpp"

// Simple test case for Archive
TEST(ArchiveTest, CanCreateArchive) {
    ArchiveStatus<std::shared_ptr<Archive>> archive = Archive::createArchive("test");
    EXPECT_TRUE(archive.isOK());  // Check if archive creation succeeds
}

// Run all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}