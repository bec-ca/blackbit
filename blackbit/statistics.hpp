#pragma once

#include <vector>

namespace blackbit {

struct Statistics {
  static double bernoulli_confidence_95(double p, double n);
  static double normal_confidence_95(double stddev, double n);
  static double clamp(double value, double min, double max);

  static double mean(const std::vector<double>& values);
  static double stddev(const std::vector<double>& values);
};

} // namespace blackbit
