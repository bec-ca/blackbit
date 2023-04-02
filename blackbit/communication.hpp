#pragma once

#include "bee/error.hpp"
#include "bee/file_descriptor.hpp"

#include <filesystem>
#include <memory>
#include <mutex>

namespace blackbit {

struct Logger {
 public:
  using ptr = std::shared_ptr<Logger>;

  ~Logger();

  static bee::OrError<ptr> create(const std::filesystem::path& log_dir);

  static ptr standard();

  static ptr null();

  template <class... Ts> void log_line(const char* format, Ts... args)
  {
    _log_line(bee::format(format, args...));
  }

  void log_line(std::string msg);

 private:
  Logger(const std::shared_ptr<bee::FileDescriptor>& main_log_fd);

  void _log_line(std::string msg);

  std::shared_ptr<bee::FileDescriptor> _main_log_fd;
};

struct XboardWriter {
 public:
  using ptr = std::shared_ptr<XboardWriter>;

  ~XboardWriter();

  static bee::OrError<ptr> create(const std::string& log_dir);

  static ptr standard();

  static ptr null();

  static bee::OrError<std::pair<ptr, bee::FileDescriptor::shared_ptr>> pipe();

  template <class... Ts> void send(const char* format, Ts... args)
  {
    _send(bee::format(format, args...));
  }

  const Logger::ptr& logger() const;

 private:
  XboardWriter(
    const bee::FileDescriptor::shared_ptr& fd, const Logger::ptr& logger);

  void _send(std::string msg);

  bee::FileDescriptor::shared_ptr _fd;
  Logger::ptr _logger;

  std::mutex _send_mutex;
};

} // namespace blackbit
