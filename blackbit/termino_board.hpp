#pragma once

#include "board.hpp"
#include "termino/element.hpp"
#include "termino/grid.hpp"

namespace blackbit {

struct BoardSquare;

struct TerminoBoard : public termino::Element {
 public:
  TerminoBoard();

  ~TerminoBoard();

  void update_board(const Board& board, std::optional<Move> last_move);

  virtual termino::Size reflow(const termino::Size& available_space);

  virtual void draw(
    termino::Termino& termino, int parent_row, int parent_col) const;

 private:
  BoardArray<std::shared_ptr<BoardSquare>> _squares;
  termino::Grid _grid;
};

} // namespace blackbit
