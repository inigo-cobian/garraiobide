#pragma once
#include <expected>
#include <optional>
#include <string>
#include <zip.h>

namespace io {
    /**
     * @brief Errors that can occur when extracting from a ZIP archive.
     */
    enum class ExtractError { FileNotFound, ParseError, FileNotOpen };

    /**
     * @brief RAII wrapper around a libzip archive.
     *
     * Provides safe opening, closing, and extraction of individual files.
     */
    class ZipFile {
    public:
        /**
         * @brief Open a ZIP archive from a file path.
         * @param zip_path Path to the ZIP file.
         */
        ZipFile(const std::string &zip_path);

        ~ZipFile();

        ZipFile(ZipFile &&other) noexcept;

        ZipFile &operator=(ZipFile &&other) noexcept;

        ZipFile(const ZipFile &) = delete;

        ZipFile &operator=(const ZipFile &) = delete;

        /**
         * @brief Extract the content of a file inside the archive.
         *
         * @param filename Name of the file in the archive.
         * @return std::expected<std::string, ExtractError> On success, the file content;
         *         on failure, an error code.
         */
        [[nodiscard]] std::expected<std::string, ExtractError> get_file_content(const std::string_view &filename) const;

    private:
        zip *archive = nullptr;
    };
}
