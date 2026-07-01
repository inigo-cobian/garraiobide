#pragma once
#include <expected>
#include <memory>
#include <stdexcept>

#include "config.hpp"

namespace core {
    using ConfigVariant = std::variant<RunConfig, IngestConfig, StatsConfig>;

    /**
     * @brief Parses command‑line arguments and returns the appropriate configuration.
     *
     * This class is a pure static factory that interprets argc/argv, validates the
     * input, and builds one of the concrete StartupConfig subclasses (RunConfig,
     * IngestConfig, or StatsConfig) based on the first argument.
     */
    class Args {
    public:
        /**
         * @brief Parse the command line.
         *
         * @param argc Number of arguments (as passed to main).
         * @param argv Array of argument strings.
         * @return std::expected<ConfigVariant, std::runtime_error> On success, a variant
         *         holding the correct config object; on failure, an error describing the
         *         problem (e.g. missing required options or unknown mode).
         */
        [[nodiscard]] static std::expected<ConfigVariant, std::runtime_error> parse_args(
            int argc, char *argv[]);
    };
}
