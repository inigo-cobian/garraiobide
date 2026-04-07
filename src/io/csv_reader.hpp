#pragma once
#include <string>
#include <vector>

namespace io {

class CsvReader {
public:
    static std::vector<std::vector<std::string>> parse_file(std::string_view text, char delimiter, const std::vector<std::string>& columns);
private:
    static auto split_lines(std::string_view text);
};

}
