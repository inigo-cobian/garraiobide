#pragma once

#include <string>
#include <vector>

namespace garraiobide::core::ports {

/// Represents a parsed CLI command with its arguments.
struct CliCommand {
    std::string name;
    std::vector<std::string> args;
};

/// Driving port: the CLI adapter parses user input and translates it into
/// domain commands. This port defines the output channel for CLI responses.
class CliPort {
   public:
    virtual ~CliPort() = default;

    /// Write a line of output to the CLI.
    virtual void write_line(const std::string& line) = 0;

    /// Write an error line.
    virtual void write_error(const std::string& error) = 0;

    /// Prompt the user for input and return the response.
    [[nodiscard]] virtual std::string prompt(const std::string& message) = 0;
};

}  // namespace garraiobide::core::ports
