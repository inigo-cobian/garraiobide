#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace garraiobide::adapters::ingestion::gtfs {

/// A single CSV row: column_name → value
using CsvRow = std::unordered_map<std::string, std::string>;

/// Parse a CSV string into a vector of row maps.
/// First line is treated as the header row.
/// Handles quoted fields (with commas, newlines, escaped quotes), CRLF/LF.
[[nodiscard]] inline std::vector<CsvRow> parse_csv(const std::string& content) {
    std::vector<CsvRow> rows;

    if (content.empty()) {
        return rows;
    }

    // Parse all fields respecting quoting rules.
    // A "record" is a logical CSV line (may span multiple physical lines if quoted).
    std::vector<std::vector<std::string>> records;
    std::vector<std::string> current_record;
    std::string current_field;
    bool in_quotes = false;

    const std::size_t len = content.size();
    std::size_t i = 0;

    while (i < len) {
        char c = content[i];

        if (in_quotes) {
            if (c == '"') {
                // Check for escaped quote ""
                if (i + 1 < len && content[i + 1] == '"') {
                    current_field += '"';
                    i += 2;
                } else {
                    // End of quoted field
                    in_quotes = false;
                    ++i;
                }
            } else {
                // Inside quotes: accept any character including commas and newlines
                current_field += c;
                ++i;
            }
        } else {
            if (c == '"') {
                // Start of quoted field (should be at beginning of field)
                in_quotes = true;
                ++i;
            } else if (c == ',') {
                // Field separator
                current_record.push_back(current_field);
                current_field.clear();
                ++i;
            } else if (c == '\r') {
                // CRLF or bare CR — treat as record separator
                current_record.push_back(current_field);
                current_field.clear();
                records.push_back(current_record);
                current_record.clear();
                ++i;
                if (i < len && content[i] == '\n') {
                    ++i;  // consume the LF in CRLF
                }
            } else if (c == '\n') {
                // LF line ending — record separator
                current_record.push_back(current_field);
                current_field.clear();
                records.push_back(current_record);
                current_record.clear();
                ++i;
            } else {
                current_field += c;
                ++i;
            }
        }
    }

    // Handle last field/record if file doesn't end with newline
    if (!current_field.empty() || !current_record.empty()) {
        current_record.push_back(current_field);
        records.push_back(current_record);
    }

    if (records.empty()) {
        return rows;
    }

    // First record is the header
    const auto& header = records[0];

    // Process data records
    for (std::size_t r = 1; r < records.size(); ++r) {
        const auto& record = records[r];

        // Skip empty trailing lines (a record with a single empty field)
        if (record.size() == 1 && record[0].empty()) {
            continue;
        }

        CsvRow row;
        for (std::size_t col = 0; col < header.size() && col < record.size(); ++col) {
            row[header[col]] = record[col];
        }
        rows.push_back(std::move(row));
    }

    return rows;
}

}  // namespace garraiobide::adapters::ingestion::gtfs
