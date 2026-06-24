#include "core/args.hpp"
#include "gtfs/gtfs_manager.hpp"
#include "init/run.hpp"
#include "init/run_ingest.hpp"
#include "init/run_stats.hpp"


int main(int argc, char *argv[]) {
    auto configVariant = core::Args::parse_args(argc, argv);

    if (!configVariant) {
        return 1;
    }
    std::visit([](auto &&config) {
        using T = std::decay_t<decltype(config)>;
        if constexpr (std::is_same_v<T, core::RunConfig>) {
            init::run(config);
        } else if constexpr (std::is_same_v<T, core::IngestConfig>) {
            init::run_ingest(config);
        } else if constexpr (std::is_same_v<T, core::StatsConfig>) {
            init::run_stats(config);
        } else {
            static_assert(false, "Unhandled configuration type");
        }
    }, configVariant.value());

    return 0;
}
