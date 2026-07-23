#pragma once

#include <string>
#include <vector>

#include "../domain/layer.h"

namespace garraiobide::core::ports {

/// Driving port: the UI adapter calls the application through use cases,
/// but the application notifies the UI through this port for rendering updates.
class PresentationPort {
   public:
    virtual ~PresentationPort() = default;

    /// Display a layer to the user.
    virtual void render_layer(const domain::Layer& layer) = 0;

    /// Show an informational message.
    virtual void show_message(const std::string& message) = 0;

    /// Show an error message.
    virtual void show_error(const std::string& error) = 0;

    /// Report available layer names (e.g. for a layer picker).
    virtual void present_layer_list(const std::vector<std::string>& names) = 0;
};

}  // namespace garraiobide::core::ports
