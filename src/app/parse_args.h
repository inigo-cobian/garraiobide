#pragma once

#include <expected>

#include "../app/app_config.h"

namespace garraiobide::app {

enum class ParseResult {
    kHelpRequested,
    kError,
};

// Parses command-line arguments into an AppConfig.
// Returns the populated AppConfig on success, or a ParseResult indicating
// either that help was requested or a parse error occurred.
[[nodiscard]] std::expected<AppConfig, ParseResult> parse_args(int argc,
                                                               char* argv[]);

}  // namespace garraiobide::app

