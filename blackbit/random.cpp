#include "random.hpp"

#include <random>

using std::mt19937_64;
using std::random_device;
using std::uniform_int_distribution;
using std::uniform_real_distribution;
using std::unique_ptr;

namespace blackbit {
namespace {

struct RandomImpl : public Random {
  mt19937_64 rng;
  uniform_int_distribution<uint32_t> uint32_interval;
  uniform_int_distribution<uint64_t> uint64_interval;
  uniform_real_distribution<double> double_interval;

  RandomImpl()
      : uint32_interval(0, 0xffffffffu),
        uint64_interval(0, 0xffffffffffffffffllu),
        double_interval(0.0, 1.0)
  {}

  virtual ~RandomImpl() {}

  virtual uint32_t rand32() { return uint32_interval(rng); }

  virtual uint64_t rand64() { return uint64_interval(rng); }

  virtual double rand_double() { return double_interval(rng); }

  static Random& global()
  {
    static RandomImpl global;
    return global;
  }

  virtual void seed(uint64_t seed) { rng.seed(seed); }

  virtual mt19937_64& get_rng() { return rng; }
};

} // namespace

uint32_t rand32() { return RandomImpl::global().rand32(); }

uint64_t rand64() { return RandomImpl::global().rand64(); }

void seed(uint32_t seed) { RandomImpl::global().seed(seed); }

void randomize_seed() { seed(random_device()()); }

Random::~Random() {}

Random::ptr Random::create(uint64_t seed)
{
  auto out = unique_ptr<Random>(new RandomImpl());
  out->seed(seed);
  return out;
}

} // namespace blackbit
