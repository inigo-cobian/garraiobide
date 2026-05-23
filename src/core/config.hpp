#pragma once
#include <string>

namespace core {
    class Config {
        static inline bool isInit = false;
        static inline std::string installation_dir;

    public:
        static void init();

        static void init(const std::string &dir);

        [[nodiscard]] static const std::string &get_installation_dir();
    };
}
