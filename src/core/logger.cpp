#include "logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace core {

void Logger::init() {
    auto logger = spdlog::stdout_color_mt("gtfs");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
}

}