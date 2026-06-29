#pragma once
#include <unordered_map>
#include <expected>
#include <stdexcept>

enum class LogLevel {
    Debug, Info, Warn, Error
};

inline std::expected<LogLevel, std::runtime_error> to_log_level(const std::string &s) {
    static const std::unordered_map<std::string, LogLevel> map = {
        {"debug", LogLevel::Debug},
        {"info", LogLevel::Info},
        {"warn", LogLevel::Warn},
        {"error", LogLevel::Error}
    };
    if (const auto it = map.find(s); it != map.end()) {
        return it->second;
    }
    throw std::runtime_error("Unknown log level \"" + s + "\"");
}
