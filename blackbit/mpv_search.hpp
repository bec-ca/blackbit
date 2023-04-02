#pragma once

#include "blackbit/transposition_table.hpp"
#include "board.hpp"
#include "eval.hpp"
#include "move_history.hpp"
#include "search_result_info.hpp"
#include "transposition_table.hpp"

#include <atomic>
#include <optional>

namespace blackbit {

struct MpvSearch {
  static bee::OrError<std::vector<SearchResultInfo::ptr>> search(
    std::unique_ptr<Board>&& board,
    int max_depth,
    int max_pvs,
    std::optional<int> num_workers,
    const std::shared_ptr<TranspositionTable>& hash_table,
    const std::shared_ptr<MoveHistory>& move_history,
    const std::shared_ptr<std::atomic_bool>& should_stop,
    const Experiment& experiment,
    const EvalParameters& eval_params,
    std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update);
};

} // namespace blackbit
