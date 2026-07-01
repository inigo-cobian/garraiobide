#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace io {
    /**
     * @brief Utility class for parsing CSV text.
     *
     * All methods are static; the class is not meant to be instantiated.
     */
    class CsvReader {
    public:
        /**
         * @brief Parse a CSV string into a vector of row maps.
         *
         * @param text The CSV content as a string.
         * @param delimiter The field delimiter (e.g. ',' or ';').
         * @param columns A list of column names; must match the number of fields per row.
         * @return std::vector<std::unordered_map<std::string, std::string>>
         *         Each row is a map from column name to field value.
         */
        static std::vector<std::unordered_map<std::string, std::string> > parse_file(
            std::string_view text, char delimiter, const std::vector<std::string> &columns);
    };
}
