#pragma once
#include <string>

namespace io {
    /**
     * @brief Simple HTTP client for downloading resources.
     *
     * All methods are static; the class is not meant to be instantiated.
     */
    class HttpClient {
    public:
        /**
         * @brief Download the content from the given URL.
         * @param url The HTTP/HTTPS URL.
         * @return std::string The response body as a string.
         * @throws std::runtime_error on network or HTTP errors.
         */
        static std::string download(const std::string &url);
    };
}
