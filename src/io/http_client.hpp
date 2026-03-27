#pragma once
#include <string>

namespace io {

class HttpClient {
public:
    static std::string download(const std::string& url);
};

}