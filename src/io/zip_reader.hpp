#pragma once
#include <string>
#include <zip.h>

namespace io {

// TODO rename it as Zip file and rethink it
class ZipReader {
public:
    void open_file(const std::string &zip_path);
    [[nodiscard]] std::string get_file(const std::string &filename);
    ~ZipReader();
private:
    zip *archive = nullptr;
};

}
