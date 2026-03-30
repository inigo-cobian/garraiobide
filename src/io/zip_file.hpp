#pragma once
#include <string>
#include <zip.h>

namespace io {

class ZipFile {
public:
    ZipFile(const std::string &zip_path);
    ~ZipFile();

    ZipFile(ZipFile&& other) noexcept;
    ZipFile& operator=(ZipFile&& other) noexcept;

    ZipFile(const ZipFile&) = delete;
    ZipFile& operator=(const ZipFile&) = delete;

    [[nodiscard]] std::string get_file_content(const std::string &filename) const;
private:
    zip *archive = nullptr;
};

}
