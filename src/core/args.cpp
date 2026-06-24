#include "args.hpp"

#include <args.hxx>

namespace core {
    LaunchMode StartupConfig::getMode() {
        return mode;
    }

    std::string StartupConfig::getLogLevel() {
        return logLevel;
    }

    void StartupConfig::initializeLogger(std::string level) {
        this->logLevel = level;
    }

    RunConfig::RunConfig() {
        this->mode = Run;
    }

    std::string RunConfig::getMongoUri() {
        return "mongodb://" + this->mongoUser + ":" + this->mongoPass + "@" + this->mongoUrl +
               "/?authSource=admin&authMechanism=SCRAM-SHA-256";
    }

    std::string RunConfig::getPostgresUri() {
        // TODO manage credentials (?)
        return this->pgUrl;
    }

    void RunConfig::setMongo(std::string user, std::string pass, std::string url) {
        this->mongoUser = user;
        this->mongoPass = pass;
        this->mongoUrl = url;
    }

    void RunConfig::setPostgres(std::string user, std::string pass, std::string url) {
        this->pgUser = user;
        this->pgPass = pass;
        this->pgUrl = url;
    }

    IngestConfig::IngestConfig() {
        this->mode = Ingest;
    }

    std::string IngestConfig::getName() {
        return name;
    }

    std::string IngestConfig::getType() {
        return type;
    }

    std::string IngestConfig::getUrl() {
        return url;
    }

    std::string IngestConfig::getCredentials() {
        return credentials;
    }

    void IngestConfig::setName(std::string name) {
        this->name = name;
    }

    void IngestConfig::setType(std::string type) {
        this->type = type;
    }

    void IngestConfig::setUrl(std::string url) {
        this->url = url;
    }

    void IngestConfig::setCredentials(std::string credentials) {
        this->credentials = credentials;
    }

    StatsConfig::StatsConfig() {
        this->mode = Stats;
    }

    std::expected<StartupConfig, std::runtime_error> Args::parse_args(int argc, char *argv[]) {
        args::ArgumentParser parser("Options for Garraiobide.", "Kontuz nasa eta trenaren arteko tartearekin.");
        args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

        args::Group globalOpts(parser, "global options", args::Group::Validators::DontCare, args::Options::Global);
        args::ValueFlag<std::string> logLevel(globalOpts, "level",
                                              "Set the logging level (error, warning, info, debug)",
                                              {'l', "log"}, "info");

        args::Group commands(parser, "commands", args::Group::Validators::AtMostOne);

        RunConfig run_config;
        IngestConfig ingest_config;
        StatsConfig stats_config;

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
                    throw std::runtime_error("Missing required arguments for 'run'");
                }

                run_config.setMongo(args::get(mongoUser), args::get(mongoPass), args::get(mongoUrl));
                run_config.setPostgres(args::get(mongoUser), args::get(mongoPass), args::get(mongoUrl));
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
                    throw std::runtime_error("Error: Missing required arguments for 'ingest'.");
                }

                ingest_config.setName(args::get(resourceName));
                ingest_config.setType(args::get(resourceType));
                ingest_config.setUrl(args::get(resourceUrl));
                ingest_config.setCredentials(args::get(resourceCreds));
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

        if (runCmd) {
            run_config.initializeLogger(args::get(logLevel));
            return run_config;
        }

        if (statsCmd) {
            stats_config.initializeLogger(args::get(logLevel));
            return stats_config;
        }

        if (ingestCmd) {
            ingest_config.initializeLogger(args::get(logLevel));
            return ingest_config;
        }
        throw std::runtime_error("No mode found");
    }
}
