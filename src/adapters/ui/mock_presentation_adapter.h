#pragma once

#include <string>
#include <vector>

#include "src/core/domain/layer.h"
#include "src/core/ports/presentation_port.h"

namespace garraiobide::adapters::ui {

/// Mock presentation adapter that records calls for test verification.
class MockPresentationAdapter final : public core::ports::PresentationPort {
   public:
    void render_layer(const core::domain::Layer& layer) override;
    void show_message(const std::string& message) override;
    void show_error(const std::string& error) override;
    void present_layer_list(const std::vector<std::string>& names) override;

    // Test inspection methods.
    [[nodiscard]] const std::vector<std::string>& messages() const {
        return messages_;
    }
    [[nodiscard]] const std::vector<std::string>& errors() const {
        return errors_;
    }
    [[nodiscard]] const std::vector<core::domain::Layer>& rendered_layers() const {
        return rendered_layers_;
    }
    [[nodiscard]] const std::vector<std::vector<std::string>>& presented_lists() const {
        return presented_lists_;
    }

    void clear();

   private:
    std::vector<std::string> messages_;
    std::vector<std::string> errors_;
    std::vector<core::domain::Layer> rendered_layers_;
    std::vector<std::vector<std::string>> presented_lists_;
};

}  // namespace garraiobide::adapters::ui
