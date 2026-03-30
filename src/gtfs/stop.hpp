#pragma once
#include <string>

namespace gtfs {

class Stop {
private:
    std::string id;
    std::string name;
    float latitude, longitude;
public:
    explicit Stop(std::string id, std::string name, float latitude, float longitude);
};

}