#include <iostream>

#include "src/adapters/ingestion/mock_ingestion_adapter.h"
#include "src/adapters/persistence/mock_persistence_adapter.h"
#include "src/adapters/ui/mock_presentation_adapter.h"
#include "src/app/layer_service.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"

using namespace garraiobide;

int main() {
    // Wire up mock adapters.
    adapters::ingestion::MockIngestionAdapter ingestion;
    adapters::persistence::MockPersistenceAdapter persistence;
    adapters::ui::MockPresentationAdapter presentation;

    app::LayerService service{ingestion, persistence, presentation};

    // Seed mock ingestion with sample Bilbao points of interest.
    ingestion.set_features({
        core::domain::GeoFeature{
            .id = "plaza_moyua",
            .geometry = core::domain::Point{{43.2630, -2.9350}},
            .properties = {{"name", std::string{"Plaza Moyua"}}},
        },
        core::domain::GeoFeature{
            .id = "guggenheim",
            .geometry = core::domain::Point{{43.2687, -2.9340}},
            .properties = {{"name", std::string{"Guggenheim Museum"}}},
        },
        core::domain::GeoFeature{
            .id = "casco_viejo",
            .geometry = core::domain::Point{{43.2580, -2.9230}},
            .properties = {{"name", std::string{"Casco Viejo"}}},
        },
    });

    // Import a layer.
    std::cout << "=== Importing layer 'bilbao_poi' ===\n";
    auto import_result = service.import_layer(
        "bilbao_poi", "bilbao_pois.geojson", core::domain::SpatialScale::Urban);
    if (!import_result) {
        std::cerr << "Import failed.\n";
        return 1;
    }

    // List layers.
    std::cout << "\n=== Listing layers ===\n";
    auto layers = service.list_layers();
    if (layers) {
        for (const auto& name : *layers) {
            std::cout << "  - " << name << "\n";
        }
    }

    // Show the layer and its envelope.
    std::cout << "\n=== Showing layer 'bilbao_poi' ===\n";
    auto layer = service.show_layer("bilbao_poi");
    if (layer) {
        std::cout << "  Features: " << layer->features.size() << "\n";
        if (auto env = layer->envelope()) {
            std::cout << "  Envelope: ["
                      << env->south_west.latitude << ", "
                      << env->south_west.longitude << "] -> ["
                      << env->north_east.latitude << ", "
                      << env->north_east.longitude << "]\n";
        }
    }

    // Query features in a tight bounding box.
    std::cout << "\n=== Querying features near Guggenheim ===\n";
    core::domain::BoundingBox query_box{
        .south_west = {43.267, -2.935},
        .north_east = {43.270, -2.933},
    };
    auto features = service.query_features(query_box);
    if (features) {
        std::cout << "  Found " << features->size() << " feature(s)\n";
        for (const auto& f : *features) {
            if (f.id) {
                std::cout << "    - " << *f.id << "\n";
            }
        }
    }

    // Print presentation adapter log.
    std::cout << "\n=== Presentation log ===\n";
    for (const auto& msg : presentation.messages()) {
        std::cout << "  [info] " << msg << "\n";
    }
    for (const auto& err : presentation.errors()) {
        std::cout << "  [error] " << err << "\n";
    }

    return 0;
}
