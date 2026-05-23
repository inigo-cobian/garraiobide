#include "config.hpp"

#include <stdexcept>

#ifdef __linux__
#include <unistd.h>
#include <pwd.h>
#endif


namespace core {
    void Config::init() {
        if (isInit) {
            throw std::runtime_error("Config already initialized");
        }
        isInit = true;
        const char *homedir = getpwuid(getuid())->pw_dir;
        installation_dir = std::string(homedir) + "/.garraiobide";
    }

    void Config::init(const std::string &dir) {
        if (isInit) {
            throw std::runtime_error("Config already initialized");
        }
        isInit = true;
        installation_dir = dir;
    }

    const std::string &Config::get_installation_dir() {
        if(!isInit) {
            throw std::runtime_error("Config hasn't been initialized");
        }
        return installation_dir;
    }
}
