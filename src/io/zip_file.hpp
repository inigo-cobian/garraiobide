#pragma once
#include <string>
#include <zip.h>

namespace io {

class ZipFile {
public:
    ZipFile(const std::string &zip_path);
    ~ZipFile();
    [[nodiscard]] std::string get_file_content(const std::string &filename) const;
private:
    zip *archive = nullptr;
};

}
