#pragma once

#include "bee/error.hpp"
#include "self_play.hpp"

#include <functional>
#include <string>

namespace blackbit {

struct CompareEngines {
  static bee::OrError<bee::Unit> compare(
    const std::string& positions_file,
    double seconds_per_move,
    const int num_rounds,
    const int num_workers,
    const int repeat_position,
    const int max_depth,
    const std::string& result_filename,
    const std::function<EngineParams()>& create_base_params,
    const std::function<EngineParams()>& create_test_params);
};

} // namespace blackbit
