#pragma once
#include "core/args.hpp"

namespace init {
    /**
     * @brief Entry point for the statistics mode.
     *
     * Computes and outputs statistical information about the data.
     * Does not return.
     * @param config The statistics configuration.
     */
    [[noreturn]] void run_stats(core::StatsConfig &config);
}
