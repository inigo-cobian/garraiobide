#include "csv_reader.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <bits/ranges_algobase.h>

namespace io {
auto CsvReader::split_lines(std::string_view text) {
    return text | std::views::split('\n') |
           std::views::transform([](auto &&line) {
               return std::string_view(line.begin(), line.end());
           });
}

std::vector<std::vector<std::string>> CsvReader::parse_file(std::string_view text, char delimiter,
                                                                const std::vector<std::string> &columns)
{
    auto lines = split_lines(text);

    auto it = lines.begin();
    if (it == lines.end()) {
        return {}; // empty file
    }

    std::string_view header_line = *it;
    auto header_fields = header_line | std::views::split(delimiter) |
                     std::views::transform([](auto&& part) {
                         return std::string(part.begin(), part.end());
                     });

    std::unordered_map<std::string, size_t> header_index;
    size_t idx = 0;
    for (auto&& field : header_fields) {
        header_index[std::move(field)] = idx++;
    }

    std::vector<int> column_indices;
    column_indices.reserve(columns.size());
    for (const auto& col : columns) {
        auto found = header_index.find(col);
        if (found != header_index.end()) {
            column_indices.push_back(static_cast<int>(found->second));
        } else {
            column_indices.push_back(-1); // missing column
        }
    }

    std::vector<std::vector<std::string>> result;
    for (++it; it != lines.end(); ++it) {
        std::string_view line = *it;
        if (line.empty()) {
            continue; // skip empty lines
        }

        auto fields = line | std::views::split(delimiter) |
               std::views::transform([](auto &&part) {
                   return std::string(part.begin(), part.end());
               });

        std::vector<std::string> field_vec;
        field_vec.reserve(static_cast<size_t>(std::ranges::distance(fields)));
        std::ranges::copy(fields, std::back_inserter(field_vec));

        std::vector<std::string> selected_row;
        selected_row.reserve(columns.size());
        for (int col_idx : column_indices) {
            if (col_idx >= 0 && static_cast<size_t>(col_idx) < field_vec.size()) {
                selected_row.push_back(std::move(field_vec[static_cast<size_t>(col_idx)]));
            } else {
                selected_row.emplace_back(); // empty string for missing column
            }
        }
        result.push_back(std::move(selected_row));
    }

    return result;
}

}
