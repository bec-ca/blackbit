#include "termino_board.hpp"

#include "board.hpp"
#include "pieces.hpp"
#include "place.hpp"
#include "termino/cell.hpp"
#include "termino/element.hpp"
#include "termino/grid.hpp"
#include "termino/text_box.hpp"

using namespace termino;
using std::make_shared;
using std::nullopt;
using std::optional;
using std::string;
using std::vector;

namespace blackbit {

namespace {

vector<string> pawn_image = {
  "                ",
  "                ",
  "       XX       ",
  "      XXXX      ",
  "     XXXXXX     ",
  "    XXXXXXXX    ",
  "    XXXXXXXX    ",
  "                ",
};

vector<string> rook_image = {
  "                ",
  "   XX  XX  XX   ",
  "   XXXXXXXXXX   ",
  "    XXXXXXXX    ",
  "    XXXXXXXX    ",
  "  XXXXXXXXXXXX  ",
  "  XXXXXXXXXXXX  ",
  "                ",
};

vector<string> bishop_image = {
  "                ",
  "       XX       ",
  "      XX XX     ",
  "     XX XXXX    ",
  "    XX XXXXX    ",
  "    XXXXXXXX    ",
  "   XXXXXXXXXX   ",
  "                ",
};

vector<string> knight_image = {
  "                ",
  "       XXXXX    ",
  "   XXXXXX  XX   ",
  "   XXXXXXXXXX   ",
  "       XXXXXX   ",
  "      XXXXXXX   ",
  "     XXXXXXXXX  ",
  "                ",
};

vector<string> queen_image = {
  "                ",
  " XX XX XX XX XX ",
  " XX XX XX XX XX ",
  " XXXXXXXXXXXXXX ",
  " XXXXXXXXXXXXXX ",
  "  XXXXXXXXXXXX  ",
  "   XXXXXXXXXX   ",
  "                ",
};

vector<string> king_image = {
  "                ",
  "       XX       ",
  "     XXXXXX     ",
  "       XX       ",
  "  XXXXXXXXXXXX  ",
  "  XXXXXXXXXXXX  ",
  "  XXXXXXXXXXXX  ",
  "                ",
};

const optional<vector<string>> get_image(PieceType type)
{
  switch (type) {
  case PieceType::PAWN:
    return pawn_image;
  case PieceType::ROOK:
    return rook_image;
  case PieceType::BISHOP:
    return bishop_image;
  case PieceType::KNIGHT:
    return knight_image;
  case PieceType::QUEEN:
    return queen_image;
  case PieceType::KING:
    return king_image;
  default:
    return nullopt;
  }
}

constexpr int light_square_color = 247;
constexpr int highlighted_light_square_color = 148;

constexpr int dark_square_color = 238;
constexpr int highlighted_dark_square_color = 106;

constexpr int white_color = 255;
constexpr int black_color = 0;

constexpr int square_size = 8;

} // namespace

struct BoardSquare : public Element {
  BoardSquare(bool is_light_square) : _is_light_square(is_light_square) {}

  void set_piece(PieceType type, Color player_color, bool highlighted)
  {
    if (
      type == _type && player_color == _color && !_box.empty() &&
      _is_highlighted == highlighted) {
      return;
    }
    auto piece_image = get_image(type);
    _box.clear();
    auto get_square_color = [&]() {
      if (_is_light_square && highlighted) {
        return highlighted_light_square_color;
      } else if (_is_light_square && !highlighted) {
        return light_square_color;
      } else if (!_is_light_square && highlighted) {
        return highlighted_dark_square_color;
      } else { // if(!_is_light_square && !highlighted)
        return dark_square_color;
      }
    };
    int square_color = get_square_color();
    if (!piece_image.has_value()) {
      vector<Cell> row;
      auto cell = Cell::char_with_color_and_background(' ', -1, square_color);
      for (int i = 0; i < square_size * 2; i++) { row.push_back(cell); }
      for (int i = 0; i < square_size; i++) { _box.add_line(row); }
    } else {
      int piece_color =
        player_color == Color::White ? white_color : black_color;
      for (const auto& line : *piece_image) {
        vector<Cell> row;
        for (char c : line) {
          int background = c == 'X' ? piece_color : square_color;
          row.push_back(
            Cell::char_with_color_and_background(' ', piece_color, background));
        }
        _box.add_line(row);
      }
    }
    _type = type;
    _color = player_color;
    _is_highlighted = highlighted;
  }

  virtual void draw(Termino& termino, int parent_row, int parent_col) const
  {
    _box.draw(termino, parent_row, parent_col);
  }

  virtual Size reflow(const Size&)
  {
    // Should consider that the available space could be smaller
    return Size{.height = square_size, .width = square_size * 2};
  }

 private:
  TextBox _box;
  bool _is_light_square;
  PieceType _type = PieceType::CLEAR;
  Color _color = Color::None;
  bool _is_highlighted = false;
};

TerminoBoard::TerminoBoard()
{
  for (Place place : PlaceIterator()) {
    bool light_square = (place.line() + place.col()) % 2 == 1;
    auto square = make_shared<BoardSquare>(light_square);
    _squares[place] = square;
    _grid.append_to_row(7 - place.line(), square);
  }
}

TerminoBoard::~TerminoBoard() {}

void TerminoBoard::update_board(
  const Board& board, std::optional<Move> last_move)
{
  for (Place place : PlaceIterator()) {
    auto& square = *_squares[place];
    const auto& pos = board.at(place);
    bool highlighted =
      last_move.has_value() && (last_move->d == place || last_move->o == place);
    square.set_piece(pos.type, pos.owner, highlighted);
  }
}

Size TerminoBoard::reflow(const Size& available_space)
{
  return _grid.reflow(available_space);
}

void TerminoBoard::draw(Termino& termino, int parent_row, int parent_col) const
{
  _grid.draw(termino, parent_row, parent_col);
}

} // namespace blackbit
