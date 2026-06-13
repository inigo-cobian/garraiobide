#pragma once
#include <string>

namespace core {
    class StopInLine {
        int order;
        std::string stopId;
        std::string lineId;
        std::string source;

    public:
        [[nodiscard]] StopInLine(int order, const std::string &stop_id, const std::string &line_id,
                                 const std::string &source);

        [[nodiscard]] int get_order() const;

        [[nodiscard]] std::string get_stop_id() const;

        [[nodiscard]] std::string get_line_id() const;

        [[nodiscard]] std::string get_source() const;
    };
}
