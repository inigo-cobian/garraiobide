#pragma once
#include <string>

namespace core {
    class Args {
    public:
        static int init(int argc, char *argv[]);
        inline static std::string mongdbUri;
    };
}
