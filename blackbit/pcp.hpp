#pragma once

#include "generated_game_record.hpp"
#include "search_result_info.hpp"

#include "stone/stone_reader.hpp"

#include <unordered_map>

namespace blackbit {

namespace gr = generated_game_record;

struct PCP {
 public:
  using ptr = std::shared_ptr<PCP>;

  using entry = gr::PCPEntry;

  virtual ~PCP();

  bee::OrError<std::optional<SearchResultInfo::ptr>> lookup(
    const std::string& fen);

  virtual bee::OrError<std::optional<entry>> lookup_raw(
    const std::string& fen) = 0;

  virtual bee::OrError<std::unordered_map<std::string, entry>> read_all() = 0;

  static bee::OrError<ptr> open_on_disk(const bee::FilePath& filename);

  static bee::OrError<ptr> open_in_memory(const bee::FilePath& filename);

 private:
};

} // namespace blackbit
