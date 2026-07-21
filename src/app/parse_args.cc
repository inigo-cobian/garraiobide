#include "src/app/parse_args.h"

#include <args.hxx>

namespace garraiobide::app {

std::expected<AppConfig, ParseResult> parse_args(int argc, char* argv[]) {
    args::ArgumentParser parser(
        "garraiobide - geographic feature management service");

    args::HelpFlag help(parser, "help", "Display this help menu",
                        {'h', "help"});

    args::ValueFlag<std::string> config_flag(
        parser, "PATH", "Path to JSON configuration file",
        {'c', "config"}, "config.json");

    args::ValueFlag<std::string> log_level_flag(
        parser, "LEVEL", "Log level (trace, debug, info, warn, error)",
        {'l', "log-level"}, "info");

    args::ValueFlag<std::string> mongo_host_flag(
        parser, "HOST", "MongoDB host",
        {'H', "mongo-host"}, "localhost");

    args::ValueFlag<std::uint16_t> mongo_port_flag(
        parser, "PORT", "MongoDB port",
        {'P', "mongo-port"}, 27017);

    args::ValueFlag<std::string> mongo_user_flag(
        parser, "USER", "MongoDB username",
        {'u', "mongo-user"});

    args::ValueFlag<std::string> mongo_pass_flag(
        parser, "PASS", "MongoDB password",
        {'p', "mongo-pass"});

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        return std::unexpected(ParseResult::kHelpRequested);
    } catch (const args::ParseError&) {
        return std::unexpected(ParseResult::kError);
    }

    return AppConfig{
        .config_path = args::get(config_flag),
        .log_level = args::get(log_level_flag),
        .mongo_host = args::get(mongo_host_flag),
        .mongo_port = args::get(mongo_port_flag),
        .mongo_user = mongo_user_flag ? args::get(mongo_user_flag) : "",
        .mongo_pass = mongo_pass_flag ? args::get(mongo_pass_flag) : "",
    };
}

}  // namespace garraiobide::app
