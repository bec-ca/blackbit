#pragma once

#include "place.hpp"
#include "specialized_array.hpp"

namespace blackbit {

template <class T> using BoardArray = SpecializedArray<Place, T, 64>;

}
