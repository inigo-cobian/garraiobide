#include "csv_reader.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <bits/ranges_algobase.h>

namespace io {
auto split_lines(std::string_view text) {
    return text | std::views::split('\n') |
           std::views::transform([](auto &&line) {
               return std::string_view(line.begin(), line.end());
           });
}

std::vector<std::string> split_fields(std::string_view line, char delimiter) {
    auto fields_view = line | std::views::split(delimiter) |
                       std::views::transform([](auto&& part) {
                           return std::string(part.begin(), part.end());
                       });
    std::vector<std::string> fields;
    fields.reserve(static_cast<size_t>(std::ranges::distance(fields_view)));
    std::ranges::copy(fields_view, std::back_inserter(fields));
    return fields;
}

std::unordered_map<std::string, size_t> build_header_map(const std::vector<std::string>& header_fields) {
    std::unordered_map<std::string, size_t> map;
    size_t idx = 0;
    for (const auto& field : header_fields) {
        map[field] = idx++;
    }
    return map;
}

std::vector<int> map_requested_columns(const std::vector<std::string>& columns,
                                       const std::unordered_map<std::string, size_t>& header_map) {
    std::vector<int> indices;
    indices.reserve(columns.size());
    for (const auto& col : columns) {
        auto it = header_map.find(col);
        indices.push_back(it != header_map.end() ? static_cast<int>(it->second) : -1);
    }
    return indices;
}

std::vector<std::string> select_fields(const std::vector<std::string>& row_fields,
                                       const std::vector<int>& column_indices) {
    std::vector<std::string> selected;
    selected.reserve(column_indices.size());
    for (int idx : column_indices) {
        if (idx >= 0 && static_cast<size_t>(idx) < row_fields.size()) {
            selected.push_back(row_fields[static_cast<size_t>(idx)]);
        } else {
            selected.emplace_back(); // empty string for missing column
        }
    }
    return selected;
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
    auto header_fields = split_fields(header_line, delimiter);
    auto header_map = build_header_map(header_fields);
    auto column_indices = map_requested_columns(columns, header_map);

    std::vector<std::vector<std::string>> result;
    for (++it; it != lines.end(); ++it) {
        std::string_view line = *it;
        if (line.empty()) {
            continue; // skip empty lines
        }

        auto row_fields = split_fields(line, delimiter);
        auto selected_row = select_fields(row_fields, column_indices);
        result.push_back(std::move(selected_row));
    }
    return result;}

}
