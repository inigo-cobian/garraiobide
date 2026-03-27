#include "http_client.hpp"
#include <cpr/cpr.h>

namespace io {

std::string HttpClient::download(const std::string& url) {
    auto response = cpr::Get(cpr::Url{url});
    return response.text;
}

}