#include "csv_reader.hpp"
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>
#include <bits/ranges_algobase.h>

namespace io {

std::vector<std::vector<std::string>> CsvReader::parse_file(std::string_view text, char delimiter,
                                                        const std::vector<std::string> &columns)
{
    auto rows = text | std::views::split('\n') | std::views::transform([](auto line) {
        return std::string_view(line.begin(), line.end());
    });

    std::vector<std::vector<std::string>> result;

    // TODO take the first line and make it a filter. Maybe CSV as a matrix and apply t(csv)?
    for (auto line : rows) {
        if (line.empty()) continue; // skip empty lines

        // Split the line into fields using the delimiter
        auto fields = line | std::views::split(delimiter) | std::views::transform([](auto part) {
            return std::string(part.begin(), part.end());
        });

        std::vector<std::string> field_vec;
        std::ranges::copy(fields, std::back_inserter(field_vec));

        std::vector<std::string> selected; // Try to get the selected ones via name
        std::vector<int> meme = {1, 2};
        for (int col : meme) {
            if (col >= 0 && static_cast<size_t>(col) < field_vec.size()) {
                selected.push_back(std::move(field_vec[col]));
            } else {
                selected.emplace_back();
            }
        }
        result.push_back(std::move(selected));
    }
    return result;
}

}
