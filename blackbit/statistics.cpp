#include "statistics.hpp"

#include <cmath>

using std::vector;

namespace blackbit {
namespace {

double squared(double v) { return v * v; }

}; // namespace

double Statistics::clamp(double value, double min, double max)
{
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

double Statistics::bernoulli_confidence_95(double p, double n)
{
  return 1.96 * sqrt(p * (1.0 - p) / n);
}

double Statistics::normal_confidence_95(double stddev, double n)
{
  return 1.96 * stddev / sqrt(n);
}

double Statistics::mean(const vector<double>& values)
{
  double sum = 0;
  for (double v : values) { sum += v; }
  return sum / values.size();
}

double Statistics::stddev(const vector<double>& values)
{
  double m = mean(values);
  double sum = 0;
  for (double v : values) { sum += squared(m - v); }
  return sqrt(sum / (values.size() - 1));
}

} // namespace blackbit
