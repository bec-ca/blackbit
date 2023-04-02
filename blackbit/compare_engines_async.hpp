#pragma once

#include "async/async.hpp"
#include "bee/error.hpp"
#include "self_play_async.hpp"

#include <functional>
#include <string>

namespace blackbit {

struct CompareEnginesAsync {
  static async::Deferred<bee::OrError<bee::Unit>> compare(
    const std::string& positions_file,
    int num_rounds,
    int num_workers,
    const std::string& result_filename,
    const EngineFactory& base_engine_factory,
    const EngineFactory& test_engine_factory,
    bee::Span time_per_move);
};

} // namespace blackbit
