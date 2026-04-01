#include "csv_reader.hpp"
#include <algorithm>
#include <iostream>
#include <map>
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
    std::map<int, std::string> pos_col;

    auto headers = rows.front() | std::views::split(delimiter) | std::views::transform([](auto header) {
        return std::string_view(header.begin(), header.end());
    });
    int header_pos = 0;
    for (const auto &header : headers) {
        if (std::ranges::contains(columns, header,
            [](const std::string& s) -> std::string_view {
                                           return s;
        })) {
            pos_col[header_pos] = header;
        }
        header_pos++;
    }

    for (auto line : rows) {
        if (line.empty()) continue;

        // Split the line into fields using the delimiter
        auto fields = line | std::views::split(delimiter) | std::views::transform([](auto part) {
            return std::string(part.begin(), part.end());
        });

        std::vector<std::string> field_vec;
        std::ranges::copy(fields, std::back_inserter(field_vec));

        std::vector<std::string> selected;
        for (int col : pos_col | std::views::keys) {
            if (col >= 0 && static_cast<size_t>(col) < field_vec.size()) {
                selected.push_back(std::move(field_vec[col]));
            } else {
                selected.emplace_back();
            }
        }
        result.push_back(std::move(selected));
    }
    result.erase(result.begin()); // Remove header
    return result;
}

}
