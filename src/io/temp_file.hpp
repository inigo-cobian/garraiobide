#pragma once
#include <filesystem>
#include <fstream>

namespace io {
    class TempFile {
    public:
        TempFile(const std::string &path, const std::string &extension);

        ~TempFile();

        std::filesystem::path getPath();

    private:
        std::filesystem::path path_;
    };
}
