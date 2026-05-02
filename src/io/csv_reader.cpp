#include "csv_reader.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace io {
    auto split_lines(std::string_view text) {
        return text | std::views::split('\n') |
               std::views::transform([](auto &&line) {
                   return std::string_view(line.begin(), line.end());
               });
    }

    std::vector<std::string> split_fields(std::string_view line, char delimiter) {
        std::vector<std::string> fields;
        std::string current;
        bool in_quotes = false;
        size_t i = 0;
        const size_t n = line.size();

        auto push_field = [&] {
            fields.push_back(std::move(current));
            current.clear();
        };

        while (i < n) {
            char c = line[i];
            if (!in_quotes && c == delimiter) {
                // Delimiter outside quotes: end current field
                push_field();
                ++i;
            }
            // Quoted field
            else if (!in_quotes && c == '"') {
                // Start of quoted field
                in_quotes = true;
                ++i;
            }
            // Either closing quote or scaped quote
            else if (in_quotes && c == '"') {
                if (i + 1 < n && line[i + 1] == '"') {
                    // Escaped double quote: add one quote and skip the next char
                    current.push_back('"');
                    i += 2;
                } else {
                    // Closing quote
                    in_quotes = false;
                    ++i;
                }
            } else {
                // Regular character (including commas inside quotes)
                current.push_back(c);
                ++i;
            }
        }
        push_field();
        return fields;
    }

    std::unordered_map<std::string, size_t> build_header_map(const std::vector<std::string> &header_fields) {
        std::unordered_map<std::string, size_t> map;
        size_t idx = 0;
        for (const auto &field: header_fields) {
            map[field] = idx++;
        }
        return map;
    }

    std::vector<std::pair<int, std::string> > map_requested_columns(const std::vector<std::string> &columns,
                                                                    const std::unordered_map<std::string, size_t> &
                                                                    header_map) {
        std::vector<std::pair<int, std::string> > index_name_pairs;
        index_name_pairs.reserve(columns.size());
        for (const auto &col: columns) {
            auto it = header_map.find(col);
            int idx = (it != header_map.end()) ? static_cast<int>(it->second) : -1;
            index_name_pairs.emplace_back(idx, col);
        }
        return index_name_pairs;
    }

    std::unordered_map<std::string, std::string> select_fields(const std::vector<std::string> &row_fields,
                                                               const std::vector<std::pair<int, std::string> > &
                                                               column_info) {
        std::unordered_map<std::string, std::string> row_map;
        row_map.reserve(column_info.size());
        for (const auto &[idx, col_name]: column_info) {
            if (idx >= 0 && static_cast<size_t>(idx) < row_fields.size()) {
                row_map[col_name] = row_fields[static_cast<size_t>(idx)];
            } else {
                row_map[col_name] = ""; // empty string for missing column
            }
        }
        return row_map;
    }

    std::vector<std::unordered_map<std::string, std::string> > CsvReader::parse_file(
        std::string_view text, char delimiter,
        const std::vector<std::string> &columns) {
        auto lines = split_lines(text);

        auto it = lines.begin();
        if (it == lines.end()) {
            return {}; // empty file
        }

        std::string_view header_line = *it;
        auto header_fields = split_fields(header_line, delimiter);
        auto header_map = build_header_map(header_fields);
        auto column_info = map_requested_columns(columns, header_map);

        std::vector<std::unordered_map<std::string, std::string> > result;
        for (++it; it != lines.end(); ++it) {
            std::string_view line = *it;
            if (line.empty()) {
                continue; // skip empty lines
            }

            auto row_fields = split_fields(line, delimiter);
            auto selected_row = select_fields(row_fields, column_info);
            result.push_back(std::move(selected_row));
        }
        return result;
    }
}
