#pragma once

#include "random.hpp"

#include <map>
#include <string>
#include <vector>

namespace blackbit {

struct Experiment {
 public:
  enum class Side {
    Base,
    Test,
  };

  Experiment(const Experiment&) = default;
  Experiment& operator=(const Experiment&) = default;

  static Experiment base();

  static Experiment test(Random& random);

  static Experiment test_with_seed(uint64_t seed);

  bool is_test() const;
  bool is_base() const;

  int flag_value(int flag_id) const;

  std::map<std::string, int> flags_to_values() const;

  void override_flag_for_testing(const std::string& name, int value);

 private:
  Experiment(Side side, std::vector<int> flag_values);

  Side _side;

  std::vector<int> _flag_values;
};

struct ExperimentFlag {
 public:
  int flag_id() const;

  int value(const Experiment& experiment) const;

  static ExperimentFlag register_flag(
    const std::string& name, int min_value, int max_value, int base_value);

  static std::vector<std::string> all_flags();

 private:
  ExperimentFlag(int flag_id);
  int _flag_id;
};

} // namespace blackbit
