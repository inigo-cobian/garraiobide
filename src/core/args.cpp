#include "args.hpp"

#include <args.hxx>

namespace core {
    std::expected<ConfigVariant, std::runtime_error> Args::parse_args(int argc, char *argv[]) {
        args::ArgumentParser parser("Options for Garraiobide.", "Kontuz nasa eta trenaren arteko tartearekin.");
        args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

        args::Group globalOpts(parser, "global options", args::Group::Validators::DontCare, args::Options::Global);
        args::ValueFlag<std::string> logLevel(globalOpts, "level",
                                              "Set the logging level (error, warning, info, debug)",
                                              {'l', "log"}, "info");

        args::ValueFlag<std::string> mongoUser(globalOpts, "user", "MongoDB username", {"mongo-user"});
        args::ValueFlag<std::string> mongoPass(globalOpts, "password", "MongoDB password", {"mongo-pass"});
        args::ValueFlag<std::string> mongoUrl(globalOpts, "url", "MongoDB URL", {"mongo-url"});
        args::ValueFlag<std::string> pgHost(globalOpts, "host", "PostGIS host", {"pg-host"});
        args::ValueFlag<std::string> pgPort(globalOpts, "port", "PostGIS port", {"pg-port"});
        args::ValueFlag<std::string> pgUser(globalOpts, "user", "PostGIS username", {"pg-user"});
        args::ValueFlag<std::string> pgPass(globalOpts, "password", "PostGIS password", {"pg-pass"});

        args::Group commands(parser, "commands", args::Group::Validators::AtMostOne);

        ConfigVariant result;

        args::Command runCmd(
            commands,
            "run",
            "Run the main application",
            [&](args::Subparser &s) {
                //empty
                s.Parse();

                if (!mongoUser || !mongoPass || !mongoUrl || !pgUser || !pgPass || !pgHost || !pgPort) {
                    throw std::runtime_error("Missing required arguments for 'run'");
                }
            }
        );

        args::Command statsCmd(
            commands,
            "stats",
            "Display statistics",
            [&](args::Subparser &s) {
                s.Parse();
                result = StatsConfig{};
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
                    throw std::runtime_error("Error: Missing required arguments for 'ingest'.");
                }

                IngestConfig cfg;
                cfg.setName(args::get(resourceName));
                cfg.setType(args::get(resourceType));
                cfg.setUrl(args::get(resourceUrl));
                cfg.setCredentials(args::get(resourceCreds));
                result = cfg;
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

        if (!runCmd && !ingestCmd && !statsCmd) {
            return std::unexpected(std::runtime_error("No mode specified"));
        }
        LogLevel level = LogLevel::Info;
        if (!args::get(logLevel).empty()) {
            auto levelOrError = to_log_level(args::get(logLevel));
            if (!levelOrError.has_value()) {
                return std::unexpected(levelOrError.error());
            }
            level = levelOrError.value();
        }

        std::visit([&](auto &cfg) {
            cfg.initializeLogger(level);
            // TODO
            if (mongoUser) cfg.setMongo(args::get(mongoUser), args::get(mongoPass), args::get(mongoUrl));
            if (pgUser) cfg.setPostgres(args::get(pgHost), args::get(pgPort), args::get(pgUser), args::get(pgPass));
        }, result);
        return result;
    }
}
