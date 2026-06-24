#include "args.hpp"

#include <args.hxx>
#include <memory>

namespace core {
    LaunchMode StartupConfig::getMode() {
        return mode;
    }

    LogLevel StartupConfig::getLogLevel() {
        return logLevel;
    }

    void StartupConfig::initializeLogger(LogLevel level) {
        this->logLevel = level;
    }

    RunConfig::RunConfig() : StartupConfig() {
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

    IngestConfig::IngestConfig() : StartupConfig() {
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

    StatsConfig::StatsConfig() : StartupConfig() {
        this->mode = Stats;
    }

    std::expected<ConfigVariant, std::runtime_error> Args::parse_args(int argc, char *argv[]) {
        args::ArgumentParser parser("Options for Garraiobide.", "Kontuz nasa eta trenaren arteko tartearekin.");
        args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

        args::Group globalOpts(parser, "global options", args::Group::Validators::DontCare, args::Options::Global);
        args::ValueFlag<std::string> logLevel(globalOpts, "level",
                                              "Set the logging level (error, warning, info, debug)",
                                              {'l', "log"}, "info");

        args::Group commands(parser, "commands", args::Group::Validators::AtMostOne);

        ConfigVariant result;

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

                RunConfig cfg;
                cfg.setMongo(args::get(mongoUser), args::get(mongoPass), args::get(mongoUrl));
                cfg.setPostgres(args::get(pgUser), args::get(pgPass), args::get(pgUrl));
                result = cfg;
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
        auto level = to_log_level(args::get(logLevel));
        if (!level) {
            return std::unexpected(level.error());
        }

        std::visit([&](auto &cfg) {
            cfg.initializeLogger(*level);
        }, result);

        return result;
    }
}
