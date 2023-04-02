#pragma once

#include <memory>
#include <random>

namespace blackbit {

struct Random {
 public:
  using ptr = std::shared_ptr<Random>;

  virtual ~Random();

  virtual void seed(uint64_t seed) = 0;

  virtual uint32_t rand32() = 0;

  virtual uint64_t rand64() = 0;

  virtual double rand_double() = 0;

  static ptr create(uint64_t seed);

  virtual std::mt19937_64& get_rng() = 0;
};

uint32_t rand32();

uint64_t rand64();

void seed(uint32_t seed);

void randomize_seed();

} // namespace blackbit
