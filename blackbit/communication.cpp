#include "communication.hpp"

#include "bee/file_descriptor.hpp"
#include "debug.hpp"

using bee::FileDescriptor;
using bee::FilePath;
using std::make_shared;
using std::pair;
using std::string;
using std::unique_lock;

namespace fs = std::filesystem;

namespace blackbit {

////////////////////////////////////////////////////////////////////////////////
// Logger
//

Logger::~Logger() {}

bee::OrError<Logger::ptr> Logger::create(const fs::path& log_dir)
{
  auto stdout_path = FilePath::of_std_path(log_dir / "stdout.log");
  auto stderr_path = FilePath::of_std_path(log_dir / "stderr.log");
  auto main_log_path = FilePath::of_std_path(log_dir / "main.log");

  bail(stdout_fd, FileDescriptor::create_file(stdout_path));
  bail(stderr_fd, FileDescriptor::create_file(stderr_path));
  bail(main_log_fd, FileDescriptor::create_file(main_log_path));

  bail_unit(stdout_fd.dup_onto(*FileDescriptor::stdout_filedesc()));
  bail_unit(stderr_fd.dup_onto(*FileDescriptor::stderr_filedesc()));

  return ptr(new Logger(make_shared<FileDescriptor>(std::move(main_log_fd))));
}

Logger::ptr Logger::standard()
{
  return ptr(new Logger(FileDescriptor::stderr_filedesc()));
}

Logger::ptr Logger::null() { return ptr(new Logger(nullptr)); }

void Logger::log_line(std::string msg) { _log_line(std::move(msg)); }

Logger::Logger(const FileDescriptor::shared_ptr& main_log_fd)
    : _main_log_fd(main_log_fd)
{}

void Logger::_log_line(std::string msg)
{
  if (_main_log_fd != nullptr) {
    msg += '\n';
    must_unit(_main_log_fd->write(msg));
  }
}

////////////////////////////////////////////////////////////////////////////////
// XboardWriter
//

XboardWriter::~XboardWriter() {}

bee::OrError<XboardWriter::ptr> XboardWriter::create(const string& log_dir)
{
  bail(stdout_fd, FileDescriptor::stdout_filedesc()->dup());
  bail(logger, Logger::create(log_dir));
  return ptr(new XboardWriter(std::move(stdout_fd).to_shared(), logger));
}

XboardWriter::ptr XboardWriter::standard()
{
  return ptr(
    new XboardWriter(FileDescriptor::stdout_filedesc(), Logger::standard()));
}

XboardWriter::ptr XboardWriter::null()
{
  return ptr(
    new XboardWriter(FileDescriptor::stdout_filedesc(), Logger::null()));
}

bee::OrError<pair<XboardWriter::ptr, FileDescriptor::shared_ptr>> XboardWriter::
  pipe()
{
  bail(pipe, bee::Pipe::create());
  auto xboard_writer = ptr(new XboardWriter(pipe.write_fd, Logger::null()));

  return make_pair(std::move(xboard_writer), std::move(pipe.read_fd));
}

const Logger::ptr& XboardWriter::logger() const { return _logger; }

XboardWriter::XboardWriter(
  const FileDescriptor::shared_ptr& fd, const Logger::ptr& logger)
    : _fd(fd), _logger(logger)
{}

void XboardWriter::_send(string msg)
{
  auto lock = unique_lock(_send_mutex);
  _logger->log_line("-> $", msg);
  msg += '\n';
  must_unit(_fd->write(msg));
  must_unit(_fd->flush());
}

} // namespace blackbit
