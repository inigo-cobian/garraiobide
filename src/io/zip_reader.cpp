#include "zip_reader.hpp"
#include <zip.h>
#include <stdexcept>

namespace io {

void ZipReader::extract(const std::string& zip_path, const std::string& dest) {
    int err = 0;
    zip* archive = zip_open(zip_path.c_str(), 0, &err);
    if (!archive) {
        throw std::runtime_error("Failed to open zip file");
    }

    zip_close(archive);
}

}