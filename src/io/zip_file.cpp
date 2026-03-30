#include "zip_file.hpp"

#include <zip.h>
#include <stdexcept>
#include <vector>
#include <spdlog/common.h>

namespace io {

ZipFile::ZipFile(const std::string &zip_path) {
    int err = 0;
    archive = zip_open(zip_path.c_str(), 0, &err);
    if (!archive) {
        throw std::runtime_error("Failed to open zip file");
    }
}

ZipFile::~ZipFile() {
    if (archive != nullptr) {
        zip_close(archive);
    }
}

ZipFile::ZipFile(ZipFile&& other) noexcept
    : archive(other.archive)
{
    other.archive = nullptr;
}

ZipFile& ZipFile::operator=(ZipFile&& other) noexcept {
    if (this != &other) {
        if (archive) {
            zip_close(archive);   // clean up current resource
        }
        archive = other.archive;
        other.archive = nullptr;
    }
    return *this;
}

std::string ZipFile::get_file_content(const std::string &filename) const {
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

}
