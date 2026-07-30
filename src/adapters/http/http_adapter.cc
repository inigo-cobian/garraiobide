#include "http_adapter.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <string>

#include "geojson_serializer.h"

namespace garraiobide::adapters::http {

namespace {

/// RAII guard that deletes a temporary file when it goes out of scope.
struct TempFileGuard {
    std::string path;
    ~TempFileGuard() {
        if (!path.empty()) {
            std::filesystem::remove(path);
        }
    }
};

/// Maximum upload size: 50 MB.
constexpr std::size_t kMaxUploadSize = 50ULL * 1024 * 1024;

/// Helper to build a JSON error response body.
std::string error_json(const std::string& message) {
    return R"({"error": ")" + message + R"("})";
}

/// Derive a layer prefix from a filename.
/// Strips the .zip extension, converts to lowercase, and replaces
/// non-alphanumeric characters with underscores. Returns "gtfs" if
/// the result is empty.
std::string derive_layer_prefix(const std::string& filename) {
    std::string name = filename;

    // Strip .zip extension (case-insensitive).
    if (name.size() >= 4) {
        std::string suffix = name.substr(name.size() - 4);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (suffix == ".zip") {
            name = name.substr(0, name.size() - 4);
        }
    }

    // Normalize: lowercase, non-alphanumeric → underscore.
    std::string result;
    result.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            result += static_cast<char>(std::tolower(c));
        } else {
            result += '_';
        }
    }

    // Trim leading/trailing underscores and collapse consecutive underscores.
    std::string cleaned;
    bool prev_underscore = true;  // suppress leading underscores
    for (char c : result) {
        if (c == '_') {
            if (!prev_underscore) {
                cleaned += c;
            }
            prev_underscore = true;
        } else {
            cleaned += c;
            prev_underscore = false;
        }
    }
    // Remove trailing underscore.
    if (!cleaned.empty() && cleaned.back() == '_') {
        cleaned.pop_back();
    }

    return cleaned.empty() ? "gtfs" : cleaned;
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
    // Default headers are applied to every response (including 404s) before
    // routing, so the browser always sees the CORS allow-origin header.
    server_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
    });

    register_routes();
    server_.listen("0.0.0.0", port);
}

int HttpAdapter::listen_on_any_port() {
    server_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
    });

    register_routes();

    int port = server_.bind_to_any_port("0.0.0.0");
    if (port < 0) {
        return -1;
    }
    assigned_port_ = port;
    server_.listen_after_bind();
    return port;
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

    server_.Post("/api/ingest/gtfs",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_ingest_gtfs(req, res);
                 });

    server_.Options("/api/ingest/gtfs",
                    [this](const httplib::Request& req, httplib::Response& res) {
                        handle_ingest_gtfs_options(req, res);
                    });

    // Method-not-allowed handlers for /api/ingest/gtfs.
    auto method_not_allowed = [](const httplib::Request& /*req*/,
                                 httplib::Response& res) {
        res.status = 405;
        res.set_content(R"({"error": "Method not allowed"})",
                        "application/json");
    };

    server_.Get("/api/ingest/gtfs", method_not_allowed);
    server_.Put("/api/ingest/gtfs", method_not_allowed);
    server_.Delete("/api/ingest/gtfs", method_not_allowed);
    server_.Patch("/api/ingest/gtfs", method_not_allowed);
}

void HttpAdapter::handle_list_layers(const httplib::Request& /*req*/,
                                     httplib::Response& res) {
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

void HttpAdapter::handle_ingest_gtfs(const httplib::Request& req,
                                     httplib::Response& res) {
    // Check file field presence.
    if (!req.form.has_file("file")) {
        res.status = 400;
        res.set_content(error_json("No file provided"), "application/json");
        return;
    }

    const auto file = req.form.get_file("file");

    // Validate content is not empty.
    if (file.content.empty()) {
        res.status = 400;
        res.set_content(error_json("Uploaded file is empty"),
                        "application/json");
        return;
    }

    // Validate file size does not exceed 50 MB.
    if (file.content.size() > kMaxUploadSize) {
        res.status = 400;
        res.set_content(
            error_json("File exceeds the maximum allowed size of 50 MB"),
            "application/json");
        return;
    }

    // Write content to a temporary file with a unique name.
    std::string temp_path =
        (std::filesystem::temp_directory_path() /
         ("garraiobide_upload_" +
          std::to_string(reinterpret_cast<std::uintptr_t>(&req)) + ".zip"))
            .string();

    {
        std::ofstream ofs(temp_path, std::ios::binary);
        if (!ofs) {
            res.status = 500;
            res.set_content(error_json("Failed to write temporary file"),
                            "application/json");
            return;
        }
        ofs.write(file.content.data(),
                  static_cast<std::streamsize>(file.content.size()));
        if (!ofs) {
            res.status = 500;
            res.set_content(error_json("Failed to write temporary file"),
                            "application/json");
            return;
        }
    }

    // RAII guard ensures temp file is removed in all paths.
    TempFileGuard guard{temp_path};

    // Derive layer prefix from uploaded filename.
    std::string layer_prefix = derive_layer_prefix(file.filename);

    // Call the service to import GTFS data.
    auto result = service_.import_gtfs(temp_path, layer_prefix);
    if (!result) {
        switch (result.error()) {
            case app::LayerServiceError::IngestionFailed:
                res.status = 422;
                res.set_content(error_json("GTFS ingestion failed"),
                                "application/json");
                return;
            case app::LayerServiceError::PersistenceFailed:
                res.status = 500;
                res.set_content(
                    error_json("Failed to persist ingested layers"),
                    "application/json");
                return;
            case app::LayerServiceError::DuplicateLayer:
                res.status = 409;
                res.set_content(
                    error_json("Layer already exists: " + layer_prefix),
                    "application/json");
                return;
            default:
                res.status = 500;
                res.set_content(error_json("Internal server error"),
                                "application/json");
                return;
        }
    }

    // Build success response.
    std::string body = R"({"status":"ok","layers":[)";
    bool first = true;
    for (const auto& name : *result) {
        if (!first) {
            body += ",";
        }
        body += "\"" + name + "\"";
        first = false;
    }
    body += "]}";

    res.status = 200;
    res.set_content(body, "application/json");
}

void HttpAdapter::handle_ingest_gtfs_options(const httplib::Request& /*req*/,
                                             httplib::Response& res) {
    // CORS headers are applied globally via set_default_headers.
    res.status = 204;
}

}  // namespace garraiobide::adapters::http
