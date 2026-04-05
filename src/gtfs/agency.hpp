#pragma once
#include <string>

namespace gtfs {

class Agency {
private:
  std::string id;
  std::string name;
public:
  explicit Agency(std::string id, std::string name);
  [[no_discard]] std::string get_id() const;
  [[no_discard]] std::string get_name() const;
};

}
