/// Bug condition exploration test for HTTP adapter server readiness.
/// This test was created as part of Task 1 to demonstrate the race condition
/// where a fixed 100ms sleep is insufficient when the server has a startup delay.
///
/// With the polling loop fix (Task 3.2), this test should now PASS.

#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <httplib.h>

#include "../src/adapters/http/http_adapter.h"
#include "../src/adapters/ui/mock_presentation_adapter.h"
#include "../src/app/layer_service.h"
#include "mocks/mock_persistence_adapter.h"
#include "mocks/mock_ingestion_adapter.h"

namespace garraiobide::tests {
namespace {

using namespace garraiobide::adapters;

constexpr std::uint16_t kReadinessTestPort = 18081;

/// Test fixture that introduces an artificial delay before listen()
/// to deterministically trigger the race condition.
class HttpAdapterReadinessTest : public ::testing::Test {
   protected:
    void SetUp() override {
        service_ = std::make_unique<app::LayerService>(
            ingestion_, persistence_, presentation_);
        adapter_ = std::make_unique<adapters::http::HttpAdapter>(*service_);

        // Start server with an artificial 200ms delay before listen().
        // This simulates CI load where the server thread doesn't get CPU time.
        server_thread_ = std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            adapter_->listen(kReadinessTestPort);
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
    }

    void TearDown() override {
        adapter_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    ingestion::MockIngestionAdapter ingestion_;
    persistence::MockPersistenceAdapter persistence_;
    ui::MockPresentationAdapter presentation_;
    std::unique_ptr<app::LayerService> service_;
    std::unique_ptr<adapters::http::HttpAdapter> adapter_;
    std::thread server_thread_;
};

TEST_F(HttpAdapterReadinessTest, ServerRespondsAfterDelayedStart) {
    httplib::Client client("localhost", kReadinessTestPort);
    auto res = client.Get("/api/layers");

    // With the polling loop, the server should be ready despite the 200ms delay.
    ASSERT_NE(res, nullptr)
        << "Server not responding — race condition still present";
    EXPECT_EQ(res->status, 200);
}

}  // namespace
}  // namespace garraiobide::tests
