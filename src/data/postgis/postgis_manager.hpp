#pragma once

#include <pqxx/pqxx>
#include <string>

namespace core {
    class Stop;
    class Line;
    class StopInLine;
}

namespace data {
    class PostgisManager {
    public:
        explicit PostgisManager(const std::string &connection_string);

        void insertStop(const core::Stop &stop);

        void insertLine(const core::Line &line);

        void insertStopInLine(const core::StopInLine &stopInLine);

        [[nodiscard]] bool stopExists(const std::string &stop_id, const std::string &source) const;

        [[nodiscard]] bool lineExists(const std::string &line_id, const std::string &source) const;

    private:
        mutable pqxx::connection conn_;

        void createTables();

        void enablePostGis();
    };
}
