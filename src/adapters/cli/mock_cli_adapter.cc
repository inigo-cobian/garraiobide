#include "mock_cli_adapter.h"

namespace garraiobide::adapters::cli {

void MockCliAdapter::write_line(const std::string& line) {
    output_lines_.push_back(line);
}

void MockCliAdapter::write_error(const std::string& error) {
    error_lines_.push_back(error);
}

std::string MockCliAdapter::prompt(const std::string& /*message*/) {
    if (input_queue_.empty()) {
        return "";
    }
    auto front = input_queue_.front();
    input_queue_.erase(input_queue_.begin());
    return front;
}

void MockCliAdapter::queue_input(const std::string& input) {
    input_queue_.push_back(input);
}

void MockCliAdapter::clear() {
    output_lines_.clear();
    error_lines_.clear();
    input_queue_.clear();
}

}  // namespace garraiobide::adapters::cli
