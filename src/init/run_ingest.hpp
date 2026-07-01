#pragma once
#include "core/args.hpp"

namespace init {
    /**
     * @brief Entry point for the ingest mode.
     *
     * Downloads the feed from the configured URL, processes it, and stores
     * the data in the databases. Does not return.
     * @param config The ingestion configuration.
     */
    [[noreturn]] void run_ingest(core::IngestConfig &config);
}
