#pragma once
#include "core/args.hpp"

namespace init {
    /**
     * @brief Entry point for the "run" (normal operation) mode.
     *
     * This function takes over the main thread and never returns.
     * @param config The configuration obtained from Args parsing.
     */
    [[noreturn]] void run(core::RunConfig &config);
}

