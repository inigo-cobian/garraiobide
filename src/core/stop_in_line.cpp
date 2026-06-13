#include "stop_in_line.hpp"

namespace core {
    StopInLine::StopInLine(const int order, const std::string &stop_id, const std::string &line_id,
                           const std::string &source)
        : order(order),
          stopId(stop_id),
          lineId(line_id),
          source(source) {
    }

    int StopInLine::get_order() const {
        return order;
    }

    std::string StopInLine::get_stop_id() const {
        return stopId;
    }

    std::string StopInLine::get_line_id() const {
        return lineId;
    }

    std::string StopInLine::get_source() const {
        return source;
    }
}
