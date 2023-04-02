#include "transposition_table.hpp"

#include <cstdlib>

namespace blackbit {

/* initialize the hash table */
namespace {

inline bool is_prime(size_t N)
{
  if (N == 2) return true;
  if (N % 2 == 0) return false;
  for (size_t i = 3; i * i <= N; i += 2) {
    if (N % i == 0) return false;
  }
  return true;
}

inline size_t next_prime(size_t N)
{
  while (not is_prime(N)) ++N;
  return N;
}

} // namespace

TranspositionTable::TranspositionTable(size_t size) { set_size(size); }
TranspositionTable::~TranspositionTable()
{
  for (auto& t : hash_table) { free(t); }
}

void TranspositionTable::set_size(size_t size)
{
  hash_size = next_prime(size / sizeof(hash_bucket) / 2);

  for (auto& t : hash_table) {
    if (t) free(t);
    t = (hash_bucket*)calloc(hash_size, sizeof(hash_bucket));
  }
  clear();
}

void TranspositionTable::clear() { mask++; }

} // namespace blackbit
