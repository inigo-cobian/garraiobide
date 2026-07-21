#include <cstdint>
#include <iostream>

#include <args.hxx>

#include "src/adapters/http/http_adapter.h"
#include "src/adapters/ingestion/mock_ingestion_adapter.h"
#include "src/adapters/persistence/file_persistence_adapter.h"
#include "src/adapters/ui/mock_presentation_adapter.h"
#include "src/app/layer_service.h"

int main(int argc, char* argv[]) {
    args::ArgumentParser parser("API Server",
                                "Garraiobide map API server");
    args::HelpFlag help(parser, "help", "Display this help menu",
                        {'h', "help"});
    args::ValueFlag<std::uint16_t> port_flag(
        parser, "PORT", "Port to listen on", {"port"}, 8080);

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::cout << parser;
        return 0;
    } catch (const args::ParseError& e) {
        std::cerr << e.what() << "\n";
        std::cerr << parser;
        return 1;
    }

    const std::uint16_t port = args::get(port_flag);

    // Wire adapters.
    garraiobide::adapters::persistence::FilePersistenceAdapter persistence(
        "data/");
    garraiobide::adapters::ingestion::MockIngestionAdapter ingestion;
    garraiobide::adapters::ui::MockPresentationAdapter presentation;

    // Create application service.
    garraiobide::app::LayerService layer_service(
        ingestion, persistence, presentation);

    // Create HTTP adapter and start listening.
    garraiobide::adapters::http::HttpAdapter http_adapter(layer_service);

    std::cout << "Listening on http://localhost:" << port << "\n";
    http_adapter.listen(port);

    return 0;
}
