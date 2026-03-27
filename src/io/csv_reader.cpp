#include "csv_reader.hpp"
#include <iostream>

namespace io {

void CsvReader::read(const std::string& file) {
    std::cout << "Reading CSV: " << file << std::endl;
}

}