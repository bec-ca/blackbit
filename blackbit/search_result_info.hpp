#pragma once

#include "board.hpp"
#include "move.hpp"
#include "score.hpp"

#include <memory>

namespace blackbit {

struct PV {
 public:
  using ptr = std::unique_ptr<PV>;

  PV(Move move, std::unique_ptr<PV>&& next) : move(move), next(std::move(next))
  {}

  ~PV();

  PV(PV&& other) = default;
  PV& operator=(PV&& other) = default;

  PV(const PV& other) = delete;

  ptr clone() const;

  std::vector<Move> to_vector() const;

  static ptr of_vector(const std::vector<Move>& moves);

  Move move;
  std::unique_ptr<PV> next;

  bool operator==(const PV& other) const;

  std::string to_string() const;
};

struct SearchResultInfo {
 public:
  using ptr = std::unique_ptr<SearchResultInfo>;
  Move best_move;
  Score eval;
  std::vector<Move> pv;
  int depth;
  bee::Span think_time;
  uint64_t nodes;

  std::vector<std::string> make_pretty_moves(const Board& board) const;

  ptr clone() const;

  void flip(Color color);

  static SearchResultInfo::ptr create(
    Move m,
    std::vector<Move>&& pv,
    Score eval,
    uint64_t nodes,
    int depth,
    bee::Span ellapsed);

  bool operator==(const SearchResultInfo& other) const;

  std::string to_string() const;
};

} // namespace blackbit
