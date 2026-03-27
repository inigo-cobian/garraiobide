#pragma once
#include <string>

namespace io {

class CsvReader {
public:
    static void read(const std::string& file);
};

}