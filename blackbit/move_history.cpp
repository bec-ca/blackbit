#include "move_history.hpp"

namespace blackbit {

MoveHistory::MoveHistory() {}
MoveHistory::~MoveHistory() {}

void MoveHistory::clear() { memset(&_table, 0, sizeof(_table)); }

} // namespace blackbit
