#pragma once
#include <expected>
#include <optional>
#include <string>
#include <zip.h>

namespace io {
    enum class ExtractError { FileNotFound, ParseError, FileNotOpen };

    class ZipFile {
    public:
        ZipFile(const std::string &zip_path);

        ~ZipFile();

        ZipFile(ZipFile &&other) noexcept;

        ZipFile &operator=(ZipFile &&other) noexcept;

        ZipFile(const ZipFile &) = delete;

        ZipFile &operator=(const ZipFile &) = delete;

        [[nodiscard]] std::expected<std::string, ExtractError> get_file_content(const std::string_view &filename) const;
    private:
        zip *archive = nullptr;
    };
}
