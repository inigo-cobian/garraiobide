#include "args.hpp"

#include <args.hxx>

namespace core {
    void runStartup(const RunConfig& run_config) {
        std::cout << "Running app with log level: " << run_config.logLevel << std::endl;
        std::cout << "MongoDB: " << run_config.mongoUser << "@" << run_config.mongoUrl << std::endl;
        std::cout << "PostGIS: " << run_config.pgUser << "@" << run_config.pgUrl << std::endl;
    }

    void statsStartup(const StartupConfig& startup_config) {
        std::cout << "Displaying statistics with log level: " << startup_config.logLevel << std::endl;
    }

    void ingestStartup(const IngestConfig& ingest_config) {
        std::cout << "Ingesting " << ingest_config.type << " resource: " << ingest_config.name << std::endl;
        std::cout << "URL: " << ingest_config.url << ", Credentials: " << ingest_config.credentials << std::endl;
    }

    std::expected<StartupConfig, std::runtime_error> Args::parse_args(int argc, char *argv[]) {
        args::ArgumentParser parser("Options for Garraiobide.", "Kontuz nasa eta trenaren arteko tartearekin.");
        args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
        args::ValueFlag<std::string> logLevel(parser, "level", "Set the logging level (error, warning, info, debug)",
                                              {'l', "log"}, "info");

        args::Group commands(parser, "commands", args::Group::Validators::AtMostOne);

        args::Command runCmd(
            commands,
            "run",
            "Run the main application",
            [&](args::Subparser &s) {
                args::ValueFlag<std::string> mongoUser(s, "user", "MongoDB username", {"mongo-user"});
                args::ValueFlag<std::string> mongoPass(s, "password", "MongoDB password", {"mongo-pass"});
                args::ValueFlag<std::string> mongoUrl(s, "url", "MongoDB URL", {"mongo-url"});
                args::ValueFlag<std::string> pgUser(s, "user", "PostGIS username", {"pg-user"});
                args::ValueFlag<std::string> pgPass(s, "password", "PostGIS password", {"pg-pass"});
                args::ValueFlag<std::string> pgUrl(s, "url", "PostGIS URL", {"pg-url"});

                s.Parse();

                if (!mongoUser || !mongoPass || !mongoUrl || !pgUser || !pgPass || !pgUrl) {
                    std::cerr << "Error: Missing required arguments for 'run'." << std::endl;
                    return;
                }

                std::string level = args::get(logLevel);
            }
        );

        args::Command statsCmd(
            commands,
            "stats",
            "Display statistics",
            [&](args::Subparser &s) {
                s.Parse();
            }
        );


        args::Command ingestCmd(
            commands,
            "ingest",
            "Ingest a resource",
            [&](args::Subparser &s) {
                args::ValueFlag<std::string> resourceName(s, "name", "Resource name", {"name"});
                args::ValueFlag<std::string> resourceType(s, "type", "Resource type", {"type"});
                args::ValueFlag<std::string> resourceUrl(s, "url", "Resource URL", {"url"});
                args::ValueFlag<std::string> resourceCreds(s, "credentials", "Resource credentials", {"creds"});

                s.Parse();

                if (!resourceName || !resourceType || !resourceUrl || !resourceCreds) {
                    std::cerr << "Error: Missing required arguments for 'ingest'." << std::endl;
                }
            }
        );
        try {
            parser.ParseCLI(argc, argv);
        } catch (const args::Completion &e) {
            throw std::runtime_error(e.what());
        } catch (const args::Help &) {
            std::cout << parser;
        } catch (const args::ParseError &e) {
            throw std::runtime_error(e.what() + parser);
        }
        //auto mongoUrl = "mongodb://" + mongoUser.Get() + ":" + mongoPass.Get() + "@" + mongoUrl.Get() + "/?authSource=admin&authMechanism=SCRAM-SHA-256";
        if (runCmd) {
            RunConfig run_config{};
            runStartup(run_config);
        }

        if (statsCmd) {
            StatsConfig stats_config{};
            statsStartup(stats_config);
        }

        if (ingestCmd) {
            IngestConfig ingest_config{};
            ingestStartup(ingest_config);
        }
        throw std::runtime_error("No mode found");
    }
}
