#pragma once
#include <expected>
#include <memory>
#include <stdexcept>

#include "config.hpp"

namespace core {
    using ConfigVariant = std::variant<RunConfig, IngestConfig, StatsConfig>;

    class Args {
    public:
        [[nodiscard]] static std::expected<ConfigVariant, std::runtime_error> parse_args(
            int argc, char *argv[]);
    };
}
