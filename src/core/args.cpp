#include "args.hpp"

#include <args.hxx>

namespace core {
    using commandtype = std::function<void(const std::string &, std::vector<std::string>::const_iterator, std::vector<std::string>::const_iterator)>;

    int Args::init(int argc, char *argv[]) {
        args::ArgumentParser parser("Options for Garraiobide.", "Kontuz nasa eta trenaren arteko tartearekin.");
        //args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
        //args::CompletionFlag completion(parser, {"complete"});

        args::Group commands(parser, "commands");
        args::Command add(commands, "add", "add a single resource");
        args::Command run(commands, "run", "runs the main program");
        args::Group arguments(parser, "arguments", args::Group::Validators::DontCare, args::Options::Global);
        args::ValueFlag<std::string> resourceType(arguments, "type", "resource type. Allowed: gtfs", {"type"});
        args::ValueFlag<std::string> resourcePath(arguments, "path", "path or URL of the resource", {"url"});
        args::ValueFlag<std::string> resourceName(arguments, "name", "name of the resource", {"name"});
        args::HelpFlag h(arguments, "help", "help", {'h', "help"});
        //args::PositionalList<std::string> pathsList(arguments, "paths", "files to import");

        try {
            parser.ParseCLI(argc, argv);
        } catch (const args::Completion &e) {
            std::cout << e.what();
            return 0;
        }
        catch (const args::Help &) {
            std::cout << parser;
            return 0;
        }
        catch (const args::ParseError &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << parser;
            return 1;
        }
        return 0;
    }
}
