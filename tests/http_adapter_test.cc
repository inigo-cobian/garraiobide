#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../src/adapters/http/http_adapter.h"
#include "../src/adapters/ui/mock_presentation_adapter.h"
#include "../src/app/layer_service.h"
#include "../src/core/domain/geo_feature.h"
#include "../src/core/domain/geometry.h"
#include "../src/core/domain/layer.h"
#include "mocks/mock_persistence_adapter.h"
#include "mocks/mock_ingestion_adapter.h"

namespace garraiobide::tests {
namespace {

using namespace garraiobide::core::domain;
using namespace garraiobide::adapters;
using json = nlohmann::json;

constexpr const char* kTestHost = "localhost";

/// Test fixture that sets up the HTTP adapter with mock dependencies.
class HttpAdapterTest : public ::testing::Test {
   protected:
    void SetUp() override {
        service_ = std::make_unique<app::LayerService>(
            ingestion_, persistence_, presentation_);
        adapter_ = std::make_unique<adapters::http::HttpAdapter>(*service_);

        // Start server in background thread with OS-assigned port.
        server_thread_ = std::thread([this]() {
            adapter_->listen_on_any_port();
        });

        // Poll until server is accepting connections.
        constexpr auto kPollInterval = std::chrono::milliseconds(5);
        constexpr auto kTimeout = std::chrono::seconds(5);
        auto start = std::chrono::steady_clock::now();
        while (!adapter_->is_running()) {
            if (std::chrono::steady_clock::now() - start > kTimeout) {
                FAIL() << "HTTP server did not start within 5s timeout";
            }
            std::this_thread::sleep_for(kPollInterval);
        }

        port_ = adapter_->assigned_port();
    }

    void TearDown() override {
        adapter_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    /// Create a test layer with a single point feature.
    Layer make_test_layer(const std::string& name) {
        GeoFeature feature;
        feature.id = "stop_001";
        feature.geometry = Point{{.latitude = 43.2630, .longitude = -2.9350}};
        feature.properties = {
            {"name", std::string("Plaza Moyua")},
            {"lines", int64_t{5}},
            {"accessible", true},
        };

        Layer layer;
        layer.name = name;
        layer.scale = SpatialScale::Urban;
        layer.features.push_back(feature);
        return layer;
    }

    /// Pre-populate the mock persistence with a layer.
    void seed_layer(const std::string& name) {
        auto layer = make_test_layer(name);
        auto result = persistence_.save_layer(layer);
        ASSERT_TRUE(result.has_value());
    }

    ingestion::MockIngestionAdapter ingestion_;
    persistence::MockPersistenceAdapter persistence_;
    ui::MockPresentationAdapter presentation_;
    std::unique_ptr<app::LayerService> service_;
    std::unique_ptr<adapters::http::HttpAdapter> adapter_;
    std::thread server_thread_;
    int port_ = 0;
};

// ---------- GET /api/layers ----------

TEST_F(HttpAdapterTest, ListLayersReturnsJsonArray) {
    seed_layer("bilbao_stops");
    seed_layer("donostia_routes");

    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.is_array());
    EXPECT_EQ(body.size(), 2u);

    // Both layer names should be present (order not guaranteed).
    std::vector<std::string> names = body.get<std::vector<std::string>>();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "bilbao_stops") !=
                names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "donostia_routes") !=
                names.end());
}

TEST_F(HttpAdapterTest, ListLayersEmptyReturnsEmptyArray) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.is_array());
    EXPECT_TRUE(body.empty());
}

// ---------- GET /api/layers/{name} ----------


TEST_F(HttpAdapterTest, GetLayerReturnsGeoJsonFeatureCollection) {
    seed_layer("bilbao_stops");

    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers/bilbao_stops");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    EXPECT_EQ(body["type"], "FeatureCollection");
    ASSERT_TRUE(body.contains("features"));
    ASSERT_TRUE(body["features"].is_array());
    EXPECT_EQ(body["features"].size(), 1u);

    // Check the first feature structure.
    auto& feature = body["features"][0];
    EXPECT_EQ(feature["type"], "Feature");
    EXPECT_EQ(feature["id"], "stop_001");
    EXPECT_EQ(feature["geometry"]["type"], "Point");

    // Coordinates should be [longitude, latitude] per RFC 7946.
    auto& coords = feature["geometry"]["coordinates"];
    EXPECT_DOUBLE_EQ(coords[0].get<double>(), -2.9350);
    EXPECT_DOUBLE_EQ(coords[1].get<double>(), 43.2630);

    // Properties.
    auto& props = feature["properties"];
    EXPECT_EQ(props["name"], "Plaza Moyua");
    EXPECT_EQ(props["lines"], 5);
    EXPECT_EQ(props["accessible"], true);
}

TEST_F(HttpAdapterTest, GetNonExistentLayerReturns404) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers/nonexistent_layer");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
    EXPECT_TRUE(body["error"].get<std::string>().find("nonexistent_layer") !=
                std::string::npos);
}

// ---------- GET /api/query ----------

TEST_F(HttpAdapterTest, QueryWithValidParamsReturnsFeatures) {
    seed_layer("bilbao_stops");

    httplib::Client client(kTestHost, port_);
    // The test point is at lat=43.2630, lng=-2.9350.
    // Use a bounding box that contains it.
    auto res = client.Get(
        "/api/query?min_lat=43.00&min_lng=-3.00&max_lat=44.00&max_lng=-2.00");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    EXPECT_EQ(body["type"], "FeatureCollection");
    ASSERT_TRUE(body["features"].is_array());
    EXPECT_EQ(body["features"].size(), 1u);

    auto& feature = body["features"][0];
    EXPECT_EQ(feature["id"], "stop_001");
}

TEST_F(HttpAdapterTest, QueryWithBboxOutsideReturnsEmptyCollection) {
    seed_layer("bilbao_stops");

    httplib::Client client(kTestHost, port_);
    // Bounding box that does NOT contain the test point (lat 43.26, lng -2.93).
    auto res = client.Get(
        "/api/query?min_lat=10.00&min_lng=10.00&max_lat=11.00&max_lng=11.00");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    EXPECT_EQ(body["type"], "FeatureCollection");
    ASSERT_TRUE(body["features"].is_array());
    EXPECT_TRUE(body["features"].empty());
}

TEST_F(HttpAdapterTest, QueryMissingParamsReturns400) {
    httplib::Client client(kTestHost, port_);

    // Missing all parameters.
    auto res = client.Get("/api/query");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
}

TEST_F(HttpAdapterTest, QueryPartialParamsReturns400) {
    httplib::Client client(kTestHost, port_);

    // Only min_lat provided.
    auto res = client.Get("/api/query?min_lat=43.00");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
}

TEST_F(HttpAdapterTest, QueryNonNumericParamsReturns400) {
    httplib::Client client(kTestHost, port_);

    auto res = client.Get(
        "/api/query?min_lat=abc&min_lng=-3.00&max_lat=44.00&max_lng=-2.00");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
}

// ---------- Headers ----------

TEST_F(HttpAdapterTest, ResponseHasContentTypeJson) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers");

    ASSERT_NE(res, nullptr);
    EXPECT_TRUE(res->has_header("Content-Type"));
    EXPECT_TRUE(res->get_header_value("Content-Type").find("application/json") !=
                std::string::npos);
}

TEST_F(HttpAdapterTest, ResponseHasCorsHeader) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers");

    ASSERT_NE(res, nullptr);
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Origin"));
    EXPECT_EQ(res->get_header_value("Access-Control-Allow-Origin"), "*");
}

TEST_F(HttpAdapterTest, GetLayerResponseHasCorsHeader) {
    seed_layer("test_layer");

    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers/test_layer");

    ASSERT_NE(res, nullptr);
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Origin"));
    EXPECT_EQ(res->get_header_value("Access-Control-Allow-Origin"), "*");
}

TEST_F(HttpAdapterTest, QueryResponseHasCorsHeader) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Get(
        "/api/query?min_lat=43.00&min_lng=-3.00&max_lat=44.00&max_lng=-2.00");

    ASSERT_NE(res, nullptr);
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Origin"));
    EXPECT_EQ(res->get_header_value("Access-Control-Allow-Origin"), "*");
}

TEST_F(HttpAdapterTest, ErrorResponseHasCorsAndContentType) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Get("/api/layers/nonexistent");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Origin"));
    EXPECT_EQ(res->get_header_value("Access-Control-Allow-Origin"), "*");
    EXPECT_TRUE(res->get_header_value("Content-Type").find("application/json") !=
                std::string::npos);
}

// ---------- POST /api/ingest/gtfs ----------

TEST_F(HttpAdapterTest, IngestGtfsSuccessReturnsLayerNames) {
    // Configure the mock ingestion adapter to return a stop and a route.
    std::vector<core::domain::GeoFeature> features;
    {
        GeoFeature stop;
        stop.id = "stop_1";
        stop.geometry = Point{{.latitude = 43.26, .longitude = -2.93}};
        stop.properties = {{"name", std::string("Abando")}};
        features.push_back(stop);
    }
    {
        GeoFeature route;
        route.id = "route_1";
        route.geometry = LineString{{
            {.latitude = 43.26, .longitude = -2.93},
            {.latitude = 43.27, .longitude = -2.94},
        }};
        route.properties = {{"name", std::string("L1")}};
        features.push_back(route);
    }
    ingestion_.set_features(std::move(features));

    httplib::Client client(kTestHost, port_);
    httplib::UploadFormDataItems items = {
        {"file", "fake zip content", "bilbao_transit.zip", "application/zip"},
    };
    auto res = client.Post("/api/ingest/gtfs", items);

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    EXPECT_EQ(body["status"], "ok");
    ASSERT_TRUE(body.contains("layers"));
    ASSERT_TRUE(body["layers"].is_array());
    EXPECT_EQ(body["layers"].size(), 2u);

    // Layer names should be derived from the filename.
    auto layer_names = body["layers"].get<std::vector<std::string>>();
    EXPECT_TRUE(std::find(layer_names.begin(), layer_names.end(),
                          "bilbao_transit_routes") != layer_names.end());
    EXPECT_TRUE(std::find(layer_names.begin(), layer_names.end(),
                          "bilbao_transit_stops") != layer_names.end());
}

TEST_F(HttpAdapterTest, IngestGtfsNoFileReturns400) {
    httplib::Client client(kTestHost, port_);
    // Post with no file field at all.
    httplib::UploadFormDataItems items = {
        {"not_file", "data", "something.txt", "text/plain"},
    };
    auto res = client.Post("/api/ingest/gtfs", items);

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
    auto err_msg = body["error"].get<std::string>();
    EXPECT_NE(err_msg.find("No file"), std::string::npos);
}

TEST_F(HttpAdapterTest, IngestGtfsEmptyFileReturns400) {
    httplib::Client client(kTestHost, port_);
    httplib::UploadFormDataItems items = {
        {"file", "", "empty.zip", "application/zip"},
    };
    auto res = client.Post("/api/ingest/gtfs", items);

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
    auto err_msg = body["error"].get<std::string>();
    EXPECT_NE(err_msg.find("empty"), std::string::npos);
}

TEST_F(HttpAdapterTest, IngestGtfsIngestionFailureReturns422) {
    // Configure mock to fail ingestion.
    ingestion_.set_error(core::ports::IngestionError::ParseError);

    httplib::Client client(kTestHost, port_);
    httplib::UploadFormDataItems items = {
        {"file", "fake zip content", "bad_data.zip", "application/zip"},
    };
    auto res = client.Post("/api/ingest/gtfs", items);

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 422);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
    auto err_msg = body["error"].get<std::string>();
    EXPECT_NE(err_msg.find("ingestion failed"), std::string::npos);
}

TEST_F(HttpAdapterTest, IngestGtfsDuplicateLayerReturns409) {
    // Set up features so ingestion succeeds.
    std::vector<core::domain::GeoFeature> features;
    GeoFeature stop;
    stop.id = "s1";
    stop.geometry = Point{{.latitude = 43.0, .longitude = -2.0}};
    features.push_back(stop);
    ingestion_.set_features(features);

    // Pre-seed a layer that will collide with the derived name.
    Layer routes_layer;
    routes_layer.name = "duptest_routes";
    routes_layer.scale = SpatialScale::Urban;
    (void)persistence_.save_layer(routes_layer);

    httplib::Client client(kTestHost, port_);
    httplib::UploadFormDataItems items = {
        {"file", "content", "duptest.zip", "application/zip"},
    };
    auto res = client.Post("/api/ingest/gtfs", items);

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 409);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
    auto err_msg = body["error"].get<std::string>();
    EXPECT_NE(err_msg.find("already exists"), std::string::npos);
}

TEST_F(HttpAdapterTest, IngestGtfsResponseHasCorsHeader) {
    ingestion_.set_features({});

    httplib::Client client(kTestHost, port_);
    httplib::UploadFormDataItems items = {
        {"file", "content", "test.zip", "application/zip"},
    };
    auto res = client.Post("/api/ingest/gtfs", items);

    ASSERT_NE(res, nullptr);
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Origin"));
    EXPECT_EQ(res->get_header_value("Access-Control-Allow-Origin"), "*");
}

// ---------- OPTIONS /api/ingest/gtfs (CORS preflight) ----------

TEST_F(HttpAdapterTest, IngestGtfsOptionsReturns204) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Options("/api/ingest/gtfs");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 204);
}

TEST_F(HttpAdapterTest, IngestGtfsOptionsHasCorsHeaders) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Options("/api/ingest/gtfs");

    ASSERT_NE(res, nullptr);
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Origin"));
    EXPECT_EQ(res->get_header_value("Access-Control-Allow-Origin"), "*");
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Methods"));
    EXPECT_TRUE(res->has_header("Access-Control-Allow-Headers"));
}

// ---------- Method Not Allowed on /api/ingest/gtfs ----------

TEST_F(HttpAdapterTest, IngestGtfsPutReturns405) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Put("/api/ingest/gtfs", "", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 405);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
    auto err_msg = body["error"].get<std::string>();
    EXPECT_NE(err_msg.find("not allowed"), std::string::npos);
}

TEST_F(HttpAdapterTest, IngestGtfsDeleteReturns405) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Delete("/api/ingest/gtfs");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 405);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
}

TEST_F(HttpAdapterTest, IngestGtfsPatchReturns405) {
    httplib::Client client(kTestHost, port_);
    auto res = client.Patch("/api/ingest/gtfs", "", "application/json");

    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 405);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("error"));
}

}  // namespace
}  // namespace garraiobide::tests
