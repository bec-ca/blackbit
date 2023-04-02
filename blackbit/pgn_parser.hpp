#pragma once

#include "bee/error.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace blackbit {

struct PGN {
 public:
  std::map<std::string, std::string> tags;
  std::vector<std::string> moves;

  std::optional<int> white_elo() const;
  std::optional<int> black_elo() const;

  std::optional<std::string> tag(const std::string& name) const;

  static bee::OrError<PGN> of_string(const std::string& content);
  static bee::OrError<std::vector<PGN>> of_string_many(
    const std::string& content);
};

struct PGNReader;

struct PGNFileReader {
 public:
  ~PGNFileReader();

  using ptr = std::unique_ptr<PGNFileReader>;

  static bee::OrError<ptr> create(const std::string& filename);

  bee::OrError<std::optional<PGN>> next();

 private:
  PGNFileReader(std::unique_ptr<PGNReader> reader);

  std::unique_ptr<PGNReader> _reader;
};

} // namespace blackbit
