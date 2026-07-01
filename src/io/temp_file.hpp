#pragma once
#include <filesystem>
#include <fstream>

namespace io {
    /**
     * @brief RAII wrapper for a temporary file.
     *
     * The file is created with a unique name and a given extension.
     * It is automatically deleted when the TempFile object goes out of scope.
     */
    class TempFile {
    public:
        /**
         * @brief Create a new temporary file.
         * @param extension File extension (including the dot, e.g. ".zip").
         */
        TempFile(const std::string &extension);

        ~TempFile();

        /**
         * @brief Get the filesystem path of the temporary file.
         * @return std::filesystem::path The path.
         */
        std::filesystem::path getPath();

    private:
        std::filesystem::path path_;
    };
}
