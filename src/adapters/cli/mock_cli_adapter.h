#pragma once

#include <string>
#include <vector>

#include "../../core/ports/cli_port.h"

namespace garraiobide::adapters::cli {

/// Mock CLI adapter that records output and provides canned input.
class MockCliAdapter final : public core::ports::CliPort {
   public:
    void write_line(const std::string& line) override;
    void write_error(const std::string& error) override;
    [[nodiscard]] std::string prompt(const std::string& message) override;

    /// Queue a response that will be returned by the next prompt() call.
    void queue_input(const std::string& input);

    // Test inspection.
    [[nodiscard]] const std::vector<std::string>& output_lines() const {
        return output_lines_;
    }
    [[nodiscard]] const std::vector<std::string>& error_lines() const {
        return error_lines_;
    }

    void clear();

   private:
    std::vector<std::string> output_lines_;
    std::vector<std::string> error_lines_;
    std::vector<std::string> input_queue_;
};

}  // namespace garraiobide::adapters::cli
