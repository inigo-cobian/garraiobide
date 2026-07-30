#pragma once

#include <cstdint>

#include <httplib.h>

#include "../../app/layer_service.h"

namespace garraiobide::adapters::http {

/// HTTP REST adapter — driving adapter that exposes LayerService over HTTP.
class HttpAdapter {
   public:
    explicit HttpAdapter(app::LayerService& service);

    /// Start listening on the given port. Blocks until the server is stopped.
    void listen(std::uint16_t port);

    /// Bind to an OS-assigned port and start listening. Blocks until stopped.
    /// Returns the assigned port, or -1 on bind failure.
    int listen_on_any_port();

    /// Returns the port assigned by listen_on_any_port(), or 0 if not yet bound.
    int assigned_port() const { return assigned_port_; }

    /// Stop the server (thread-safe).
    void stop();

    /// Returns true once the server is accepting connections (thread-safe).
    bool is_running() const { return server_.is_running(); }

   private:
    void register_routes();
    void handle_list_layers(const httplib::Request& req,
                            httplib::Response& res);
    void handle_get_layer(const httplib::Request& req,
                          httplib::Response& res);
    void handle_query_features(const httplib::Request& req,
                               httplib::Response& res);
    void handle_ingest_gtfs(const httplib::Request& req,
                            httplib::Response& res);
    void handle_ingest_gtfs_options(const httplib::Request& req,
                                    httplib::Response& res);

    app::LayerService& service_;
    httplib::Server server_;
    int assigned_port_ = 0;
};

}  // namespace garraiobide::adapters::http
