#include "stop.hpp"

namespace gtfs {

Stop::Stop(std::string id, std::string name, float latitude, float longitude)
: id(id), name(name), latitude(latitude), longitude(longitude) {}

}