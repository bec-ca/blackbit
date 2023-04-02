#pragma once

#include "pieces.hpp"
#include "specialized_array.hpp"

namespace blackbit {

template <class T> using PieceTypeArray = SpecializedArray<PieceType, T, 8>;

}
