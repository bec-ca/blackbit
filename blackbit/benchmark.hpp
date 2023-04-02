#pragma once

#include "command/cmd.hpp"

namespace blackbit {

struct Benchmark {
 public:
  static command::Cmd command();
  static command::Cmd command_mpv();
};

} // namespace blackbit
