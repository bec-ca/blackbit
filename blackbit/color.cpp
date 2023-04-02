#include "color.hpp"

namespace blackbit {}

namespace bee {

std::string to_string<blackbit::Color>::convert(const blackbit::Color p)
{
  switch (p) {
  case blackbit::Color::White:
    return "White";
  case blackbit::Color::Black:
    return "Black";
  case blackbit::Color::None:
    return "None";
  }
  assert(false && "This should not happen");
}

} // namespace bee
