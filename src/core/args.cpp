#include "args.hpp"

#include <args.hxx>

namespace core {
    void runStartup(const std::string &mongoUser, const std::string &mongoPass,
                    const std::string &mongoUrl, const std::string &pgUser,
                    const std::string &pgPass, const std::string &pgUrl,
                    const std::string &logLevel) {
        std::cout << "Running app with log level: " << logLevel << std::endl;
        std::cout << "MongoDB: " << mongoUser << "@" << mongoUrl << std::endl;
        std::cout << "PostGIS: " << pgUser << "@" << pgUrl << std::endl;
    }

    void statsStartup(const std::string &logLevel) {
        std::cout << "Displaying statistics with log level: " << logLevel << std::endl;
    }

    void ingestStartup(const std::string &name, const std::string &type,
                       const std::string &url, const std::string &creds,
                       const std::string &logLevel) {
        std::cout << "Ingesting " << type << " resource: " << name << std::endl;
        std::cout << "URL: " << url << ", Credentials: " << creds << std::endl;
    }

    using commandtype = std::function<void(const std::string &, std::vector<std::string>::const_iterator,
                                           std::vector<std::string>::const_iterator)>;

    args::ArgumentParser parser("Options for Garraiobide.", "Kontuz nasa eta trenaren arteko tartearekin.");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<std::string> logLevel(parser, "level", "Set the logging level (error, warning, info, debug)",
                                          {'l', "log"}, "info");

    args::Group commands(parser, "commands", args::Group::Validators::AllOrNone);

    args::Command runCmd(
        commands,
        "run",
        "Run the main application",
        [](args::Subparser &s) {
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
            runStartup(args::get(mongoUser), args::get(mongoPass), args::get(mongoUrl),
                       args::get(pgUser), args::get(pgPass), args::get(pgUrl), level);
        }
    );

    args::Command statsCmd(
        commands,
        "stats",
        "Display statistics",
        [](args::Subparser &s) {
            s.Parse(); // No extra arguments needed
            statsStartup(args::get(logLevel));
        }
    );


    args::Command ingestCmd(
        commands,
        "ingest",
        "Ingest a resource",
        [](args::Subparser &s) {
            args::ValueFlag<std::string> resourceName(s, "name", "Resource name", {"name"});
            args::ValueFlag<std::string> resourceType(s, "type", "Resource type", {"type"});
            args::ValueFlag<std::string> resourceUrl(s, "url", "Resource URL", {"url"});
            args::ValueFlag<std::string> resourceCreds(s, "credentials", "Resource credentials", {"creds"});

            s.Parse();

            if (!resourceName || !resourceType || !resourceUrl || !resourceCreds) {
                std::cerr << "Error: Missing required arguments for 'ingest'." << std::endl;
                return;
            }

            ingestStartup(args::get(resourceName), args::get(resourceType),
                          args::get(resourceUrl), args::get(resourceCreds),
                          args::get(logLevel));
        }
    );

    int Args::parse_args(int argc, char *argv[]) {
        try {
            parser.ParseCLI(argc, argv);
        } catch (const args::Completion &e) {
            std::cout << e.what();
            return 1;
        } catch (const args::Help &) {
            std::cout << parser;
        } catch (const args::ParseError &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << parser;
            return 1;
        }
        //auto mongoUrl = "mongodb://" + mongoUser.Get() + ":" + mongoPass.Get() + "@" + mongoUrl.Get() + "/?authSource=admin&authMechanism=SCRAM-SHA-256";

        return 0;
    }
}
