#include "training_features.hpp"

#include "blackbit/static_vector.hpp"
#include "eval.hpp"

#include <array>
#include <concepts>
#include <string>
#include <vector>

using std::array;
using std::string;
using std::vector;

namespace blackbit {

namespace {

enum class Player {
  Current,
  Oponent,
};

string to_string(Player player)
{
  switch (player) {
  case Player::Current:
    return "cp";
  case Player::Oponent:
    return "op";
  }
}

inline constexpr Color to_color(Player player, Color turn)
{
  switch (player) {
  case Player::Current:
    return turn;
  case Player::Oponent:
    return oponent(turn);
  }
}

using vector_type = FeatureProvider::vector_type;

template <class T>
concept FeatureProviderConcept =
  requires(T, vector_type& out, const Features& features, const Board& board) {
    {
      T::num_features()
      } -> std::same_as<int>;

    {
      T::add_features(features, board, out)
    };

    {
      T::feature_names()
      } -> std::same_as<vector<string>>;
  };

template <class F> struct SingleFeatureProvider {
 public:
  static constexpr int num_features() { return 1; }

  template <class T>
  static inline void add_features(
    const T& features, const Board& board, vector_type& out)
  {
    out.push_back(F::get(features, board));
  }

  static vector<string> feature_names() { return {F::feature_name()}; }
};

template <FeatureProviderConcept... Ts> struct CombinedFeatureProvider {
 public:
  static constexpr int num_features() { return (Ts::num_features() + ...); }

  static inline void add_features(
    const Features& features, const Board& board, vector_type& out)
  {
    (Ts::add_features(features, board, out), ...);
  }

  static vector<string> feature_names()
  {
    vector<string> output;
    (
      [&]() {
        for (const auto& n : Ts::feature_names()) { output.push_back(n); }
      }(),
      ...);
    assert(output.size() == num_features());
    return output;
  }
};

template <class T, Player player> struct PlayerBasedFeatureOnePlayer {
 public:
  static constexpr int num_features() { return T::num_features(); }

  static vector<string> feature_names()
  {
    vector<string> out;
    for (const auto& n : T::feature_names()) {
      out.push_back(bee::format("$_$", to_string(player), n));
    }
    return out;
  }

  static inline void add_features(
    const Features& features, const Board& board, vector_type& out)
  {
    T::add_features(player, features, board, out);
  }
};

template <class T>
using PlayerBasedFeature = CombinedFeatureProvider<
  PlayerBasedFeatureOnePlayer<T, Player::Current>,
  PlayerBasedFeatureOnePlayer<T, Player::Oponent>>;

template <FeatureProviderConcept... Ts> struct GenericFeatureMaker {
  static void make_features(
    const Features& features, const Board& board, vector_type& out)
  {
    ([&] { Ts::add_features(features, board, out); }(), ...);
    assert(out.size() == num_features());
  }

  static constexpr int num_features() { return (Ts::num_features() + ...); }

  static vector<string> feature_names()
  {
    vector<string> output;
    (
      [&]() {
        for (const auto& n : Ts::feature_names()) { output.push_back(n); }
      }(),
      ...);
    assert(output.size() == num_features());
    return output;
  }
};

template <class... Ts> struct CopyFeatureProviderOnePlayer {
  static constexpr int num_features() { return sizeof...(Ts); }

  static vector<string> feature_names() { return {Ts::feature_name()...}; }

  static inline void add_features(
    Player player, const Features& f, const Board& b, vector_type& out)
  {
    (
      [&]() {
        out.push_back(Ts::get(f.get(to_color(player, b.turn))).to_pawns());
      }(),
      ...);
  }
};

template <class... Ts>
using CopyFeatureProvider =
  PlayerBasedFeature<CopyFeatureProviderOnePlayer<Ts...>>;

struct KingPosition {
  static constexpr int num_features() { return 2; }

  static vector<string> feature_names()
  {
    return {bee::format("king_rank"), bee::format("king_file")};
  }

  static inline void add_features(
    Player player, const Features&, const Board& board, vector_type& out)
  {
    auto color = to_color(player, board.turn);
    auto kp = board.pieces(color, PieceType::KING)[0].player_view(board.turn);
    out.push_back(kp.line());
    out.push_back(kp.col());
  }
};
using AllKingPositions = PlayerBasedFeature<KingPosition>;

struct QueenPosition {
  static constexpr int num_features() { return 2; }

  static vector<string> feature_names()
  {
    return {bee::format("queen_rank"), bee::format("queen_file")};
  }

  static inline void add_features(
    Player player, const Features&, const Board& board, vector_type& out)
  {
    auto color = to_color(player, board.turn);
    const auto& qs = board.pieces(color, PieceType::QUEEN);
    if (qs.empty()) {
      out.push_back(-1);
      out.push_back(-1);
    } else {
      auto qp = qs[0].player_view(board.turn);
      out.push_back(qp.line());
      out.push_back(qp.col());
    }
  }
};
using AllQueenPositions = PlayerBasedFeature<QueenPosition>;

template <Player player, PieceType piece> struct PiecePosition {
  constexpr static int num_features() { return 16; }

  static vector<string> feature_names()
  {
    vector<string> output;
    for (int i = 0; i < 8; i++) {
      output.push_back(
        bee::format("$_$_on_rank_$", to_string(player), piece, i + 1));
      output.push_back(
        bee::format("$_$_on_file_$", to_string(player), piece, char('a' + i)));
    }
    return output;
  }

  static inline void add_features(
    const Features&, const Board& board, vector_type& out)
  {
    array<int, 8> ranks{0, 0, 0, 0, 0, 0, 0, 0};
    array<int, 8> files{0, 0, 0, 0, 0, 0, 0, 0};
    auto color = to_color(player, board.turn);
    const auto& pieces = board.pieces(color, piece);
    for (auto p : pieces) {
      p = p.player_view(board.turn);
      ranks[p.line()]++;
      files[p.col()]++;
    }
    for (int i = 0; i < 8; i++) {
      out.push_back(ranks[i]);
      out.push_back(files[i]);
    }
  }
};

template <Player player>
using PiecePositionOnePlayer = CombinedFeatureProvider<
  PiecePosition<player, PieceType::PAWN>,
  PiecePosition<player, PieceType::ROOK>,
  PiecePosition<player, PieceType::KNIGHT>,
  PiecePosition<player, PieceType::BISHOP>,
  PiecePosition<player, PieceType::QUEEN>,
  PiecePosition<player, PieceType::KING>>;

using AllPiecePositions = CombinedFeatureProvider<
  PiecePositionOnePlayer<Player::Current>,
  PiecePositionOnePlayer<Player::Oponent>>;

template <Player player, PieceType piece>
struct PieceCount : public SingleFeatureProvider<PieceCount<player, piece>> {
  static string feature_name()
  {
    return bee::format("$_$_count", to_string(player), piece);
  }
  static inline float get(const Features&, const Board& b)
  {
    return b.pieces(to_color(player, b.turn), piece).size();
  }
};

template <Player player>
using PieceCountOnePlayer = CombinedFeatureProvider<
  PieceCount<player, PieceType::PAWN>,
  PieceCount<player, PieceType::ROOK>,
  PieceCount<player, PieceType::KNIGHT>,
  PieceCount<player, PieceType::BISHOP>,
  PieceCount<player, PieceType::QUEEN>>;

using AllPieceCounts = CombinedFeatureProvider<
  PieceCountOnePlayer<Player::Current>,
  PieceCountOnePlayer<Player::Oponent>>;

struct CurrentEvalProvider : public SingleFeatureProvider<CurrentEvalProvider> {
  static string feature_name() { return "current_eval"; }
  static inline float get(const Features& features, const Board& b)
  {
    return (features.get(b.turn).current_eval -
            features.get(oponent(b.turn)).current_eval)
      .to_pawns();
  }
};

struct PlyNum : public SingleFeatureProvider<PlyNum> {
  static string feature_name() { return "ply_num"; }
  static float get(const Features&, const Board& b) { return b.ply(); }
};

struct IsWhite : public SingleFeatureProvider<IsWhite> {
  static string feature_name() { return "is_white"; }
  static float get(const Features&, const Board& b)
  {
    return b.turn == Color::White ? 1.0 : 0.0;
  }
};

struct CurrentSideEval {
  static string feature_name() { return "current_side_eval"; }
  static Score get(const PlayerFeatures& f) { return f.current_eval; }
};

struct Material {
  static string feature_name() { return "material_points"; }
  static Score get(const PlayerFeatures& f) { return f.material_points; }
};

struct Attack {
  static string feature_name() { return "attack_points"; }
  static Score get(const PlayerFeatures& f) { return f.attack_points; }
};

struct Mobility {
  static string feature_name() { return "mobility_points"; }
  static Score get(const PlayerFeatures& f) { return f.mobility_points; }
};

struct Pawn {
  static string feature_name() { return "pawn_points"; }
  static Score get(const PlayerFeatures& f) { return f.pawn_points; }
};

struct KingSafeFromQueen {
  static string feature_name() { return "king_safe_from_queen"; }
  static Score get(const PlayerFeatures& f)
  {
    return f.king_safe_from_queen_points;
  }
};

struct KingRoughSafeFromQueen {
  static string feature_name() { return "king_rough_safe_from_queen"; }
  static Score get(const PlayerFeatures& f)
  {
    return f.king_rough_safe_from_queen_points;
  }
};

struct KingRoughSafeFromQueenWithPawns {
  static string feature_name()
  {
    return "king_rough_safe_from_queen_with_pawns";
  }
  static Score get(const PlayerFeatures& f)
  {
    return f.king_rough_safe_from_queen_with_pawns_points;
  }
};

struct KingBeingAttacked {
  static string feature_name() { return "king_being_attacked"; }
  static Score get(const PlayerFeatures& f)
  {
    return f.king_is_being_attacked_points;
  }
};

using FeatureProviderImpl = GenericFeatureMaker<
  CurrentEvalProvider,
  PlyNum,
  IsWhite,
  AllPieceCounts,
  AllKingPositions,
  AllQueenPositions,
  AllPiecePositions,
  CopyFeatureProvider<
    CurrentSideEval,
    Material,
    Attack,
    Mobility,
    Pawn,
    KingSafeFromQueen,
    KingRoughSafeFromQueen,
    KingRoughSafeFromQueenWithPawns,
    KingBeingAttacked>>;

} // namespace

int FeatureProvider::num_features()
{
  return FeatureProviderImpl::num_features();
}

vector<string> FeatureProvider::feature_names()
{
  return FeatureProviderImpl::feature_names();
}

void FeatureProvider::make_features(
  const Features& features, const Board& board, vector_type& out)
{
  out.clear();
  FeatureProviderImpl::make_features(features, board, out);
}

} // namespace blackbit
