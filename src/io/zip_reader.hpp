#pragma once
#include <string>

namespace io {

class ZipReader {
public:
    static void extract(const std::string& zip_path, const std::string& dest);
};

}