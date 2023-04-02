#pragma once

#include "blackbit/move.hpp"
#include "board.hpp"
#include "board_array.hpp"

namespace blackbit {

constexpr Score p(double pawns) { return Score::of_pawns(pawns); }

struct MoveScore {
 private:
  constexpr static PieceTypeArray<Score> cap_table{{
    p(0),
    p(1),
    p(3),
    p(2),
    p(5),
    p(9),
    p(2),
    p(0),
  }};

 public:
  Move m;
  Score score;
  MoveScore() : score(Score::zero()) {}
  void setScore(const Board& board, Score move_history_score)
  {
    score = cap_table[board[m.d].type] + move_history_score * 213 / 128;
  }
  inline bool operator<(const MoveScore& ms) const { return score > ms.score; }
};

struct MoveHistory {
 private:
  constexpr static Score high_pri_score = Score::of_pawns(10000);

  constexpr static Score memory_cap = Score::of_milli_pawns(512);

  struct ScoreDefaultZero : public Score {
    ScoreDefaultZero() : Score(Score::zero()) {}
  };

 public:
  MoveHistory();
  ~MoveHistory();

  MoveHistory(const MoveHistory&) = delete;
  MoveHistory& operator=(const MoveHistory&) = delete;

  void clear();

  inline void sort_moves(
    const Board& board, MoveVector& moves, Move high_pri_move)
  {
    MoveScore ms[moves.size()];
    for (int i = 0; i < int(moves.size()); ++i) {
      auto& m = moves[i];
      ms[i].m = m;
      ms[i].setScore(board, _table[board.ply()][m.o][m.d]);
      if (m == high_pri_move) { ms[i].score += high_pri_score; }
    }
    std::stable_sort(ms, ms + moves.size());
    for (int i = 0; i < int(moves.size()); ++i) { moves[i] = ms[i].m; }
  }

  inline void add(const Board& board, const Move& move)
  {
    auto& table = _table[board.ply()];
    table[move.o][move.d] += Score::of_milli_pawns(1);
    if ((table[move.o][move.d]) >= memory_cap) {
      for (auto& f : table) {
        for (auto& t : f) { t /= 2; }
      }
    }
  }

 private:
  std::array<BoardArray<BoardArray<ScoreDefaultZero>>, 1024> _table;
};

} // namespace blackbit
