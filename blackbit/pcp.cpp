#include "pcp.hpp"

#include "generated_game_record.hpp"
#include "search_result_info.hpp"

#include "bee/copy.hpp"
#include "stone/stone_reader.hpp"
#include "yasf/cof.hpp"

#include <memory>
#include <unordered_map>

using bee::Span;
using std::make_shared;
using std::nullopt;
using std::optional;
using std::string;
using std::unordered_map;
using stone::StoneReader;

namespace blackbit {

namespace {

bee::OrError<PCP::entry> parse(const string& value)
{
  return yasf::Cof::deserialize<PCP::entry>(value);
}

optional<SearchResultInfo::ptr> to_sri(const PCP::entry& entry)
{
  if (entry.best_moves.empty()) { return nullopt; }
  auto& best = entry.best_moves[0];
  return SearchResultInfo::create(
    best.move,
    bee::copy(best.pv),
    best.evaluation.value_or(Score::zero()),
    best.nodes.value_or(0),
    best.depth.value_or(0),
    best.think_time.value_or(Span::zero()));
}

struct DiskPCP : public PCP {
 public:
  explicit DiskPCP(stone::StoneReader::ptr&& reader)
      : _reader(std::move(reader))
  {}

  virtual ~DiskPCP() {}

  virtual bee::OrError<optional<entry>> lookup_raw(const string& fen) override
  {
    bail(value, _reader->lookup(fen));

    if (!value.has_value()) { return nullopt; }

    bail(entry, parse(*value));
    return std::move(entry);
  }

  virtual bee::OrError<unordered_map<string, entry>> read_all() override
  {
    bail(values, _reader->read_all());

    unordered_map<string, entry> output;
    for (auto& [fen, str] : values) {
      bail(e, parse(str));
      output.emplace(std::move(fen), std::move(e));
    }

    return output;
  }

  static bee::OrError<ptr> open(const bee::FilePath& filename)
  {
    bail(reader, StoneReader::open(filename));
    return make_shared<DiskPCP>(std::move(reader));
  }

 private:
  stone::StoneReader::ptr _reader;
};

struct MemoryPCP final : public PCP {
 public:
  explicit MemoryPCP(unordered_map<string, entry>&& entries)
      : _entries(std::move(entries))
  {}

  virtual ~MemoryPCP() {}

  virtual bee::OrError<optional<entry>> lookup_raw(const string& fen) override
  {
    auto it = _entries.find(fen);
    if (it == _entries.end()) { return nullopt; }

    return it->second;
  }

  virtual bee::OrError<unordered_map<string, entry>> read_all() override
  {
    return _entries;
  }

  static bee::OrError<ptr> open(const bee::FilePath& filename)
  {
    bail(on_disk, DiskPCP::open(filename));
    bail(entries, on_disk->read_all());
    return make_shared<MemoryPCP>(std::move(entries));
  }

 private:
  unordered_map<string, entry> _entries;
};

} // namespace

PCP::~PCP() {}

bee::OrError<PCP::ptr> PCP::open_on_disk(const bee::FilePath& filename)
{
  return DiskPCP::open(filename);
}

bee::OrError<PCP::ptr> PCP::open_in_memory(const bee::FilePath& filename)
{
  return MemoryPCP::open(filename);
}

bee::OrError<optional<SearchResultInfo::ptr>> PCP::lookup(const string& fen)
{
  bail(entry, lookup_raw(fen));
  if (!entry.has_value()) { return nullopt; }
  return to_sri(*entry);
}

} // namespace blackbit
