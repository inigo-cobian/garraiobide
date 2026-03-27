#pragma once
#include <string>

namespace io {

class GeoJsonReader {
public:
    static void read(const std::string& file);
};

}