#include "temp_file.hpp"

namespace io {
TempFile::TempFile(const std::string& path, const std::string& extension) {
    path_ = std::filesystem::temp_directory_path() /
           ("temp_" + std::to_string(std::rand()) + extension);
}

TempFile::~TempFile() {
    std::filesystem::remove(path_);
}

std::filesystem::path TempFile::getPath() {
    return path_;
}

}