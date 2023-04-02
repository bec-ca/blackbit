#pragma once

#include "eval.hpp"

#include <vector>

namespace blackbit {

struct FeatureProvider {
 public:
  using vector_type = StaticVector<float, 512>;

  static int num_features();

  static std::vector<std::string> feature_names();

  static void make_features(
    const Features& features, const Board& board, vector_type& out);
};

}; // namespace blackbit
