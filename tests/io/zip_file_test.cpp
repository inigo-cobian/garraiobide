#include <gtest/gtest.h>
#include "io/zip_file.hpp"

#include <zip.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;

// Helper: generate a unique temporary directory path
static fs::path create_temp_dir() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    std::string dir_name = "zip_test_" + std::to_string(dis(gen));
    fs::path temp_dir = fs::temp_directory_path() / dir_name;
    fs::create_directories(temp_dir);
    return temp_dir;
}

// Helper: create a zip file containing given files (map filename -> content)
static fs::path create_test_zip(const std::unordered_map<std::string, std::string>& files) {
    fs::path temp_dir = create_temp_dir();
    fs::path zip_path = temp_dir / "test.zip";

    int err = 0;
    zip* archive = zip_open(zip_path.c_str(), ZIP_CREATE, &err);
    if (!archive) {
        throw std::runtime_error("Failed to create test zip");
    }

    for (const auto& [name, content] : files) {
        zip_source* src = zip_source_buffer(archive, content.data(), content.size(), 0);
        if (!src) {
            zip_close(archive);
            throw std::runtime_error("Failed to create zip source for " + name);
        }
        zip_int64_t index = zip_file_add(archive, name.c_str(), src, ZIP_FL_OVERWRITE);
        if (index < 0) {
            zip_source_free(src);
            zip_close(archive);
            throw std::runtime_error("Failed to add file " + name + " to zip");
        }
    }

    zip_close(archive);
    return zip_path;
}

// Helper: clean up temporary directory
static void cleanup_temp_dir(const fs::path& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
}

class ZipFileTest : public ::testing::Test {
protected:
    void TearDown() override {
        if (!temp_dir_.empty()) {
            cleanup_temp_dir(temp_dir_);
        }
    }

    fs::path temp_dir_;
};

// ----------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------

TEST_F(ZipFileTest, ConstructorThrowsOnNonExistentFile) {
    EXPECT_THROW({ io::ZipFile zip("/non/existent/path.zip"); }, std::runtime_error);
}

TEST_F(ZipFileTest, GetExistingFileContent) {
    // Create a zip with one file
    std::unordered_map<std::string, std::string> files = {
        {"hello.txt", "Hello, world!"}
    };
    fs::path zip_path = create_test_zip(files);
    temp_dir_ = zip_path.parent_path(); // for cleanup

    io::ZipFile zip(zip_path.string());
    auto result = zip.get_file_content("hello.txt");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Hello, world!");
}

TEST_F(ZipFileTest, GetFileFromZipWithMultipleFiles) {
    std::unordered_map<std::string, std::string> files = {
        {"a.txt", "AAA"},
        {"b.txt", "BBB"},
        {"sub/c.txt", "CCC"}
    };
    fs::path zip_path = create_test_zip(files);
    temp_dir_ = zip_path.parent_path();

    io::ZipFile zip(zip_path.string());

    auto content_a = zip.get_file_content("a.txt");
    ASSERT_TRUE(content_a.has_value());
    EXPECT_EQ(content_a.value(), "AAA");

    auto content_b = zip.get_file_content("b.txt");
    ASSERT_TRUE(content_b.has_value());
    EXPECT_EQ(content_b.value(), "BBB");

    auto content_c = zip.get_file_content("sub/c.txt");
    ASSERT_TRUE(content_c.has_value());
    EXPECT_EQ(content_c.value(), "CCC");
}

TEST_F(ZipFileTest, GetNonExistentFileReturnsFileNotFound) {
    std::unordered_map<std::string, std::string> files = {
        {"existing.txt", "content"}
    };
    fs::path zip_path = create_test_zip(files);
    temp_dir_ = zip_path.parent_path();

    io::ZipFile zip(zip_path.string());
    auto result = zip.get_file_content("missing.txt");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), io::ExtractError::FileNotFound);
}

TEST_F(ZipFileTest, EmptyFileInZipWorks) {
    std::unordered_map<std::string, std::string> files = {
        {"empty.txt", ""}
    };
    fs::path zip_path = create_test_zip(files);
    temp_dir_ = zip_path.parent_path();

    io::ZipFile zip(zip_path.string());
    auto result = zip.get_file_content("empty.txt");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(ZipFileTest, BinaryContentIsPreserved) {
    std::vector<char> binary_data = {0x00, 0x01, 0x4F, 0x7E, 0x40};
    std::string binary_str(binary_data.begin(), binary_data.end());

    std::unordered_map<std::string, std::string> files = {
        {"binary.bin", binary_str}
    };
    fs::path zip_path = create_test_zip(files);
    temp_dir_ = zip_path.parent_path();

    io::ZipFile zip(zip_path.string());
    auto result = zip.get_file_content("binary.bin");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), binary_str);
}

// Move semantics test (optional)
TEST_F(ZipFileTest, MoveConstructorTransfersOwnership) {
    std::unordered_map<std::string, std::string> files = {
        {"data.txt", "move test"}
    };
    fs::path zip_path = create_test_zip(files);
    temp_dir_ = zip_path.parent_path();

    io::ZipFile original(zip_path.string());
    auto content_before = original.get_file_content("data.txt");
    ASSERT_TRUE(content_before.has_value());

    io::ZipFile moved(std::move(original));
    auto content_after = moved.get_file_content("data.txt");
    ASSERT_TRUE(content_after.has_value());
    EXPECT_EQ(content_after.value(), "move test");

    // original is now in a valid but unspecified state; calling get_file_content on it
    // may return FileNotOpen (since archive was moved). This is acceptable.
    auto result_from_original = original.get_file_content("data.txt");
    EXPECT_FALSE(result_from_original.has_value());
    EXPECT_EQ(result_from_original.error(), io::ExtractError::FileNotOpen);
}

// If you want to test the move assignment as well:
TEST_F(ZipFileTest, MoveAssignmentTransfersOwnership) {
    std::unordered_map<std::string, std::string> files1 = {{"a.txt", "AAA"}};
    std::unordered_map<std::string, std::string> files2 = {{"b.txt", "BBB"}};

    fs::path zip_path1 = create_test_zip(files1);
    fs::path zip_path2 = create_test_zip(files2);

    // We need to keep both directories for cleanup
    temp_dir_ = zip_path1.parent_path(); // will clean one
    // Keep the second directory separately to clean later
    fs::path temp_dir2 = zip_path2.parent_path();

    io::ZipFile zip1(zip_path1.string());
    io::ZipFile zip2(zip_path2.string());

    // Move assignment: zip1 = std::move(zip2)
    zip1 = std::move(zip2);

    // Now zip1 should point to the zip containing b.txt
    auto content = zip1.get_file_content("b.txt");
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content.value(), "BBB");

    // zip2 is now moved‑from; accessing it should return FileNotOpen
    auto result_from_moved = zip2.get_file_content("b.txt");
    EXPECT_FALSE(result_from_moved.has_value());
    EXPECT_EQ(result_from_moved.error(), io::ExtractError::FileNotOpen);

    // Clean up the second temporary directory
    cleanup_temp_dir(temp_dir2);
}