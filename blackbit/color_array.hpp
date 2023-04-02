#pragma once

#include "color.hpp"
#include "specialized_array.hpp"

namespace blackbit {

template <class T> using ColorArray = SpecializedArray<Color, T, 2>;

}
