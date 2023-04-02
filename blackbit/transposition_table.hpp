#pragma once

#include "board.hpp"
#include "score.hpp"

#include <algorithm>
#include <mutex>

namespace blackbit {

struct TranspositionTable {
 public:
  struct hash_slot {
    uint64_t hash_key;
    Score lower_bound = Score::min();
    Score upper_bound = Score::max();
    int32_t depth;
    Move move;
  };

 private:
  uint64_t mask = 0;
  static constexpr size_t BUCKET_SIZE = 4;
  struct hash_bucket {
    hash_slot slot[BUCKET_SIZE];
  };

  std::array<std::mutex, 256> _segment_locks;

  size_t hash_size;
  ColorArray<hash_bucket*> hash_table{{nullptr, nullptr}};

  uint64_t get_board_hash(const Board& board) const
  {
    return board.hash_key() ^ mask;
  }

  inline hash_bucket* get_bucket(const Board& board)
  {
    return &hash_table[board.turn][get_board_hash(board) % hash_size];
  }

  inline hash_slot* find_key(const Board& board, hash_bucket* bucket)
  {
    auto key = get_board_hash(board);
    for (uint32_t i = 0; i < BUCKET_SIZE; ++i) {
      if (bucket->slot[i].hash_key == key) {
        for (int j = i; j > 0; --j) {
          std::swap(bucket->slot[j - 1], bucket->slot[j]);
        }
        return &bucket->slot[0];
      }
    }
    return 0;
  }

  /* insert the given position in the hash */
  inline void _hash_insert(
    const Board& board,
    int depth,
    Score lower_bound,
    Score upper_bound,
    Move move)
  {
    hash_bucket* bucket = get_bucket(board);
    hash_slot* cand = find_key(board, bucket);

    if (cand == nullptr) {
      for (int i = BUCKET_SIZE - 1; i > 0; --i) {
        bucket->slot[i] = bucket->slot[i - 1];
      }
      cand = &bucket->slot[0];
    } else {
      if (cand->depth > depth) {
        return;
      } else if (cand->depth == depth) {
        lower_bound = std::max(lower_bound, cand->lower_bound);
        upper_bound = std::min(upper_bound, cand->upper_bound);
      }
    }

    cand->hash_key = get_board_hash(board);
    cand->lower_bound = lower_bound;
    cand->upper_bound = upper_bound;
    cand->depth = depth;
    cand->move = move;
  }

  std::mutex& _mutex_for(const Board& board)
  {
    auto key = get_board_hash(board);
    return _segment_locks[key % _segment_locks.size()];
  }

 public:
  TranspositionTable(size_t size);
  ~TranspositionTable();

  void set_size(size_t size);

  inline hash_slot* find(const Board& board)
  {
    std::lock_guard<std::mutex> _guard(_mutex_for(board));

    hash_bucket* bucket = get_bucket(board);
    return find_key(board, bucket);
  }

  inline void insert(
    const Board& board,
    int depth,
    Score lower_bound,
    Score upper_bound,
    Move move)
  {
    std::lock_guard<std::mutex> _guard(_mutex_for(board));
    _hash_insert(board, depth, lower_bound, upper_bound, move);
  }

  void clear();
};

} // namespace blackbit
