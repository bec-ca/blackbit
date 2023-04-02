#include "pgn_parser.hpp"

#include "bee/file_reader.hpp"

#include <cassert>

using std::map;
using std::nullopt;
using std::optional;
using std::string;
using std::unique_ptr;
using std::vector;

namespace blackbit {
namespace {

struct ReaderDriver {
 public:
  using ptr = unique_ptr<ReaderDriver>;
  virtual ~ReaderDriver() {}

  virtual bee::OrError<size_t> read(char* buffer, size_t size) = 0;
};

} // namespace

struct PGNReader {
 public:
  using ptr = unique_ptr<PGNReader>;

  PGNReader(ReaderDriver::ptr driver) : _driver(std::move(driver)) {}

  PGNReader(const PGNReader&) = delete;
  PGNReader(PGNReader&&) = delete;
  PGNReader& operator=(const PGNReader&) = delete;
  PGNReader& operator=(PGNReader&&) = delete;

  bool has_data()
  {
    auto ret = _maybe_read_more();
    if (ret.is_error()) return false;
    return ret.value();
  }

  char peek()
  {
    assert(has_data());
    return _buffer[_pos];
  }

  char pop()
  {
    char c = peek();
    _pos++;
    return c;
  }

  bool empty() { return !has_data(); }

  bool skip_blanks()
  {
    bool ret = false;
    while (has_data() && isblank(peek())) {
      pop();
      ret = true;
    }
    return ret;
  }

  void skip_spaces()
  {
    while (has_data() && isspace(peek())) { pop(); }
  }

  void read_until_space(string& out)
  {
    while (has_data() && !isspace(peek())) { out += pop(); }
  }

  bool skip_comments()
  {
    if (!has_data()) return false;
    char c = peek();
    if (c == '{') {
      while (has_data() && pop() != '}') {}
      return true;
    } else if (c == '(') {
      while (has_data() && pop() != ')') {}
      return true;
    }
    return false;
  }

  void skip_comments_and_blanks()
  {
    while (skip_blanks() || skip_comments()) {}
  }

 private:
  bee::OrError<bool> _maybe_read_more()
  {
    if (_pos < _buffer_size) return true;
    bail(ret, _driver->read((char*)_buffer, sizeof(_buffer)));
    if (ret <= 0) return false;
    _pos = 0;
    _buffer_size = ret;
    return true;
  }

  ReaderDriver::ptr _driver;
  size_t _pos = 0;
  char _buffer[1 << 12];
  size_t _buffer_size = 0;
};

namespace {

struct StringDriver : public ReaderDriver {
 public:
  StringDriver(string document) : _document(document) {}

  StringDriver(const StringDriver&) = delete;
  StringDriver(StringDriver&&) = delete;
  StringDriver& operator=(const StringDriver&) = delete;
  StringDriver& operator=(StringDriver&&) = delete;

  virtual bee::OrError<size_t> read(char* buffer, size_t size)
  {
    size = std::min(size, _document.size() - _pos);
    for (size_t i = 0; i < size; i++) { buffer[i] = _document[_pos + i]; }
    _pos += size;
    return size;
  }

 private:
  size_t _pos = 0;
  string _document;
};

struct FileDriver : public ReaderDriver {
 public:
  static bee::OrError<ReaderDriver::ptr> create(const string& filename)
  {
    bail(reader, bee::FileReader::open(bee::FilePath::of_string(filename)));
    return ReaderDriver::ptr(new FileDriver(std::move(reader)));
  }

  FileDriver(const FileDriver&) = delete;
  FileDriver(FileDriver&& other) = delete;
  FileDriver& operator=(const FileDriver&) = delete;
  FileDriver& operator=(FileDriver&&) = delete;

  virtual bee::OrError<size_t> read(char* buffer, size_t size)
  {
    return _reader->read(reinterpret_cast<std::byte*>(buffer), size);
  }

 private:
  FileDriver(bee::FileReader::ptr reader) : _reader(std::move(reader)) {}

  bee::FileReader::ptr _reader;
};

template <class K, class V>
optional<V> find_opt(const map<K, V>& container, const K& key)
{
  auto it = container.find(key);
  if (it == container.end()) { return nullopt; }
  return it->second;
}

} // namespace

bool is_ply_indicator(const string& str)
{
  bool saw_digit = false;
  bool on_dots = false;
  for (char c : str) {
    if (!on_dots) {
      if (c == '.') {
        if (!saw_digit) return false;
        on_dots = true;
      } else if (!isdigit(c))
        return false;
      else
        saw_digit = true;
    } else {
      if (c != '.') { return false; }
    }
  }
  if (!saw_digit || !on_dots) return false;
  return true;
}

bool is_result(const string& str)
{
  return str == "0-1" || str == "1-0" || str == "1/2-1/2" || str == "*";
}

bee::OrError<PGN> parse_one(PGNReader& reader)
{
  // Read tags
  map<string, string> tags;

  while (reader.has_data()) {
    reader.skip_comments_and_blanks();

    if (reader.empty()) {
      return bee::Error::format("Expected tag, got EOF instead");
    }
    char c = reader.pop();
    if (c != '[') {
      return bee::Error::format("malformed: expected '[', got '$'", int(c));
    }
    reader.skip_blanks();

    string tag_name;
    reader.read_until_space(tag_name);

    reader.skip_blanks();

    if (reader.empty()) { return bee::Error("malformed: expected tag value"); }
    c = reader.pop();
    if (c != '"') { return bee::Error("malformed: expected '\"'"); }

    string tag_value;

    while (true) {
      if (reader.empty()) { return bee::Error("malformed: expected any char"); }

      char c = reader.pop();
      if (c == '"')
        break;
      else if (c == '\\') {
        if (reader.empty()) {
          return bee::Error("malformed: expected '\\' or '\"', but got EOF");
        }
        c = reader.pop();
        if (c == '"') {
          tag_value += '"';
        } else if (c == '\\') {
          tag_value += '\\';
        } else {
          return bee::Error(
            "malformed: expected '\\' or '\"', but got other char");
        }
      } else {
        tag_value += c;
      }
    }

    if (reader.empty()) {
      return bee::Error("malformed: expected ']', got EOF");
    }
    c = reader.pop();
    if (c != ']') {
      return bee::Error::format("malformed: expected ']', got '$'", c);
    }

    tags.emplace(tag_name, tag_value);

    if (reader.empty()) {
      return bee::Error("malformed: expected end of line, got EOF");
    }
    c = reader.pop();
    if (c != '\n') {
      return bee::Error::format("malformed: expected end of line, got '$'", c);
    }

    if (reader.has_data() && reader.peek() == '\n') {
      reader.pop();
      break;
    }
  }

  // Read moves
  vector<string> moves;

  while (reader.has_data()) {
    reader.skip_comments_and_blanks();
    if (reader.empty()) { break; }
    char c = reader.peek();

    if (c == '\n') {
      reader.pop();
      if (reader.has_data() && reader.peek() == '\n') {
        reader.pop();
        break;
      } else {
        continue;
      }
    }

    string m;
    reader.read_until_space(m);
    if (is_ply_indicator(m) || is_result(m)) { continue; }

    moves.push_back(m);
  }

  return PGN{.tags = std::move(tags), .moves = std::move(moves)};
}

bee::OrError<PGN> PGN::of_string(const string& content)
{
  PGNReader reader(make_unique<StringDriver>(content));
  return parse_one(reader);
}

bee::OrError<vector<PGN>> PGN::of_string_many(const string& content)
{
  // Read tags
  vector<PGN> output;
  PGNReader reader(make_unique<StringDriver>(content));
  while (true) {
    reader.skip_comments_and_blanks();
    if (reader.empty()) break;
    bail(pgn, parse_one(reader));
    output.push_back(pgn);

    if (reader.empty()) { break; }
  }
  return output;
}

bee::OrError<PGNFileReader::ptr> PGNFileReader::create(const string& filename)
{
  bail(driver, FileDriver::create(filename));
  return ptr(new PGNFileReader(make_unique<PGNReader>(std::move(driver))));
}

PGNFileReader::PGNFileReader(unique_ptr<PGNReader> reader)
    : _reader(std::move(reader))
{}

PGNFileReader::~PGNFileReader() {}

bee::OrError<optional<PGN>> PGNFileReader::next()
{
  _reader->skip_spaces();
  if (_reader->empty()) { return bee::OrError<optional<PGN>>(nullopt); }
  bail(pgn, parse_one(*_reader));
  return bee::OrError<optional<PGN>>(std::move(pgn));
}

optional<int> PGN::white_elo() const
{
  auto v = find_opt(tags, string("WhiteElo"));
  if (!v) return nullopt;
  return stoi(*v);
}

optional<int> PGN::black_elo() const
{
  auto v = find_opt(tags, string("BlackElo"));
  if (!v) return nullopt;
  return stoi(*v);
}

optional<string> PGN::tag(const string& name) const
{
  auto it = tags.find(name);
  if (it == tags.end()) { return nullopt; }
  return it->second;
}

} // namespace blackbit
