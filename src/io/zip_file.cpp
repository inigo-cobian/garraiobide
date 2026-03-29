#include "zip_file.hpp"

#include <iostream>
#include <zip.h>
#include <stdexcept>
#include <vector>

namespace io {

void ZipFile::open_file(const std::string &zip_path) {
    // TODO validate file state
    int err = 0;
    archive = zip_open(zip_path.c_str(), 0, &err);
    if (!archive) {
        throw std::runtime_error("Failed to open zip file");
    }

}

std::string ZipFile::get_file(const std::string &filename) {
    if (!archive) {
        throw std::runtime_error("Should open the zip file before working with it");
    }

    zip_int64_t num_entries = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < num_entries; ++i)
    {
        const char* name = zip_get_name(archive, i, 0);

        zip_file_t* file = zip_fopen_index(archive, i, 0);
        if (!file || filename == name) {
            continue;
        }

        std::vector<char> file_buffer(4096);
        zip_int64_t bytes_read;

        std::string result;
        while ((bytes_read =
                zip_fread(file,
                          file_buffer.data(),
                          file_buffer.size())) > 0)
        {
            result.append(file_buffer.data(), static_cast<size_t>(bytes_read));
        }
        zip_fclose(file);
        return result;
    }
    throw std::runtime_error("Cannot find zip file");
}

ZipFile::~ZipFile() {
    if (archive != nullptr) {
        zip_close(archive);
    }
}

}
