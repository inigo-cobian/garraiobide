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

    /// Stop the server (thread-safe).
    void stop();

   private:
    void register_routes();
    void handle_list_layers(const httplib::Request& req,
                            httplib::Response& res);
    void handle_get_layer(const httplib::Request& req,
                          httplib::Response& res);
    void handle_query_features(const httplib::Request& req,
                               httplib::Response& res);

    app::LayerService& service_;
    httplib::Server server_;
};

}  // namespace garraiobide::adapters::http
