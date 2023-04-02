#include "experiment_framework.hpp"

#include "random.hpp"

#include <cassert>
#include <map>
#include <memory>
#include <random>
#include <vector>

using std::map;
using std::shared_ptr;
using std::string;
using std::uniform_int_distribution;
using std::vector;

namespace blackbit {
namespace {

struct IntFlagInfo {
 public:
  using ptr = shared_ptr<IntFlagInfo>;

  const string name;

  const int base_value;

  uniform_int_distribution<int> test_distribution;

  const int flag_id;

  int test_value(Random& random) { return test_distribution(random.get_rng()); }
};

struct FlagRegister {
 public:
  static FlagRegister& singleton()
  {
    static FlagRegister flag_register;
    return flag_register;
  }

  int register_flag(
    const std::string& name, int min_value, int max_value, int base_value)
  {
    int flag_id = _flags.size();
    auto info = IntFlagInfo::ptr(new IntFlagInfo{
      .name = name,
      .base_value = base_value,
      .test_distribution = uniform_int_distribution<int>(min_value, max_value),
      .flag_id = flag_id,
    });

    for (const auto& flag : _flags) {
      if (flag->name == name) {
        assert(false && "There are two experiment flags with the same name");
      }
    }

    _flags.push_back(info);

    return flag_id;
  }

  vector<int> values_for_base()
  {
    vector<int> out;
    for (const auto& flag : _flags) { out.push_back(flag->base_value); }
    return out;
  }

  vector<int> values_for_test(Random& random)
  {
    vector<int> out;
    for (const auto& flag : _flags) { out.push_back(flag->test_value(random)); }
    return out;
  }

  const vector<IntFlagInfo::ptr> flags() const { return _flags; }

 private:
  vector<IntFlagInfo::ptr> _flags;
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// ExperimentFlag
//

ExperimentFlag::ExperimentFlag(int flag_id) : _flag_id(flag_id) {}

int ExperimentFlag::flag_id() const { return _flag_id; }

ExperimentFlag ExperimentFlag::register_flag(
  const string& name, int min_value, int max_value, int base_value)
{
  int flag_id = FlagRegister::singleton().register_flag(
    name, min_value, max_value, base_value);
  return ExperimentFlag(flag_id);
}

int ExperimentFlag::value(const Experiment& exp) const
{
  return exp.flag_value(_flag_id);
}

vector<string> ExperimentFlag::all_flags()
{
  vector<string> out;
  for (const auto& flag : FlagRegister::singleton().flags()) {
    out.push_back(flag->name);
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// Experiment
//

Experiment Experiment::base()
{
  return Experiment(Side::Base, FlagRegister::singleton().values_for_base());
}

Experiment Experiment::test(Random& random)
{
  return Experiment(
    Side::Test, FlagRegister::singleton().values_for_test(random));
}

Experiment Experiment::test_with_seed(uint64_t seed)
{
  auto random = Random::create(seed);
  return Experiment(
    Side::Test, FlagRegister::singleton().values_for_test(*random));
}

bool Experiment::is_test() const { return _side == Side::Test; }
bool Experiment::is_base() const { return _side == Side::Base; }

int Experiment::flag_value(int flag_id) const
{
  return _flag_values.at(flag_id);
}

map<string, int> Experiment::flags_to_values() const
{
  map<string, int> out;

  for (const auto& flag : FlagRegister::singleton().flags()) {
    out.emplace(flag->name, _flag_values.at(flag->flag_id));
  }
  return out;
}

Experiment::Experiment(Side side, vector<int> flag_values)
    : _side(side), _flag_values(std::move(flag_values))
{}

void Experiment::override_flag_for_testing(const string& name, int value)
{
  for (const auto& flag : FlagRegister::singleton().flags()) {
    if (flag->name == name) {
      _flag_values.at(flag->flag_id) = value;
      return;
    }
  }
  assert(false && "Not such flag");
}

} // namespace blackbit
