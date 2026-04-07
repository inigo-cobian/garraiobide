#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace io {
    class CsvReader {
    public:
        static std::vector<std::unordered_map<std::string, std::string> > parse_file(
            std::string_view text, char delimiter, const std::vector<std::string> &columns);
    };
}
