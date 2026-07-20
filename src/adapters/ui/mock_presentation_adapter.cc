#include "src/adapters/ui/mock_presentation_adapter.h"

namespace garraiobide::adapters::ui {

void MockPresentationAdapter::render_layer(const core::domain::Layer& layer) {
    rendered_layers_.push_back(layer);
}

void MockPresentationAdapter::show_message(const std::string& message) {
    messages_.push_back(message);
}

void MockPresentationAdapter::show_error(const std::string& error) {
    errors_.push_back(error);
}

void MockPresentationAdapter::present_layer_list(
    const std::vector<std::string>& names) {
    presented_lists_.push_back(names);
}

void MockPresentationAdapter::clear() {
    messages_.clear();
    errors_.clear();
    rendered_layers_.clear();
    presented_lists_.clear();
}

}  // namespace garraiobide::adapters::ui
