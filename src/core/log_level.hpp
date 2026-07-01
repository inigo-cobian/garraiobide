#pragma once
#include <unordered_map>
#include <expected>
#include <stdexcept>

/**
 * @brief Severity levels used by the logging subsystem.
 */
enum class LogLevel {
    Debug, ///< Detailed debugging information.
    Info, ///< General informational messages.
    Warn, ///< Warnings that are not fatal.
    Error ///< Recoverable errors.
};

/**
 * @brief Convert a string to its corresponding LogLevel.
 *
 * @param s String representation (case‑insensitive: "debug", "info", "warn", "error").
 * @return std::expected<LogLevel, std::runtime_error> On success the log level,
 *         otherwise an error containing an explanatory message.
 */
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
