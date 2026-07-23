#include "http_adapter.h"

#include <charconv>
#include <string>

#include "geojson_serializer.h"

namespace garraiobide::adapters::http {

namespace {

/// Helper to build a JSON error response body.
std::string error_json(const std::string& message) {
    return R"({"error": ")" + message + R"("})";
}

/// Set common headers on all responses.
void set_common_headers(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
}

/// Try to parse a string as a double. Returns false on failure.
bool parse_double(const std::string& str, double& out) {
    const char* begin = str.data();
    const char* end = begin + str.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

}  // namespace

HttpAdapter::HttpAdapter(app::LayerService& service) : service_(service) {}

void HttpAdapter::listen(std::uint16_t port) {
    register_routes();
    server_.listen("0.0.0.0", port);
}

void HttpAdapter::stop() {
    server_.stop();
}

void HttpAdapter::register_routes() {
    server_.Get("/api/layers", [this](const httplib::Request& req,
                                      httplib::Response& res) {
        handle_list_layers(req, res);
    });

    server_.Get(R"(/api/layers/([^/]+))",
                [this](const httplib::Request& req, httplib::Response& res) {
                    handle_get_layer(req, res);
                });

    server_.Get("/api/query", [this](const httplib::Request& req,
                                     httplib::Response& res) {
        handle_query_features(req, res);
    });
}

void HttpAdapter::handle_list_layers(const httplib::Request& /*req*/,
                                     httplib::Response& res) {
    set_common_headers(res);

    auto result = service_.list_layers();
    if (!result) {
        res.status = 500;
        res.set_content(error_json("Internal server error"),
                        "application/json");
        return;
    }

    // Build JSON array of layer name strings.
    std::string body = "[";
    bool first = true;
    for (const auto& name : *result) {
        if (!first) {
            body += ",";
        }
        body += "\"" + name + "\"";
        first = false;
    }
    body += "]";

    res.status = 200;
    res.set_content(body, "application/json");
}

void HttpAdapter::handle_get_layer(const httplib::Request& req,
                                   httplib::Response& res) {
    set_common_headers(res);

    std::string name = req.matches[1];

    auto result = service_.show_layer(name);
    if (!result) {
        if (result.error() == app::LayerServiceError::LayerNotFound) {
            res.status = 404;
            res.set_content(error_json("Layer not found: " + name),
                            "application/json");
        } else {
            res.status = 500;
            res.set_content(error_json("Internal server error"),
                            "application/json");
        }
        return;
    }

    std::string body = GeoJsonSerializer::serialize_layer(*result);
    res.status = 200;
    res.set_content(body, "application/json");
}

void HttpAdapter::handle_query_features(const httplib::Request& req,
                                        httplib::Response& res) {
    set_common_headers(res);

    // Extract query parameters.
    std::string min_lat_str = req.get_param_value("min_lat");
    std::string min_lng_str = req.get_param_value("min_lng");
    std::string max_lat_str = req.get_param_value("max_lat");
    std::string max_lng_str = req.get_param_value("max_lng");

    // Validate all four parameters are present.
    if (min_lat_str.empty() || min_lng_str.empty() ||
        max_lat_str.empty() || max_lng_str.empty()) {
        res.status = 400;
        res.set_content(
            error_json(
                "Missing or invalid query parameters: "
                "min_lat, min_lng, max_lat, max_lng are required"),
            "application/json");
        return;
    }

    // Parse as doubles.
    double min_lat = 0, min_lng = 0, max_lat = 0, max_lng = 0;
    if (!parse_double(min_lat_str, min_lat) ||
        !parse_double(min_lng_str, min_lng) ||
        !parse_double(max_lat_str, max_lat) ||
        !parse_double(max_lng_str, max_lng)) {
        res.status = 400;
        res.set_content(
            error_json(
                "Missing or invalid query parameters: "
                "min_lat, min_lng, max_lat, max_lng must be numeric"),
            "application/json");
        return;
    }

    core::domain::BoundingBox bbox{
        .south_west = {.latitude = min_lat, .longitude = min_lng},
        .north_east = {.latitude = max_lat, .longitude = max_lng},
    };

    auto result = service_.query_features(bbox);
    if (!result) {
        res.status = 500;
        res.set_content(error_json("Internal server error"),
                        "application/json");
        return;
    }

    std::string body =
        GeoJsonSerializer::serialize_feature_collection(*result);
    res.status = 200;
    res.set_content(body, "application/json");
}

}  // namespace garraiobide::adapters::http
