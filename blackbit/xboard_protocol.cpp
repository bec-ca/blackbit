#include "xboard_protocol.hpp"

#include "bot_state.hpp"
#include "communication.hpp"
#include "pcp.hpp"
#include "pieces.hpp"

#include "bee/format_filesystem.hpp"
#include "bee/string_util.hpp"
#include "command/command_builder.hpp"

#include <cstring>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>

namespace fs = std::filesystem;

using bee::Span;
using std::optional;
using std::string;

namespace blackbit {
namespace {

bee::OrError<XboardWriter::ptr> get_writer()
{
  char* home = getenv("HOME");
  fs::path log_dir;
  if (home == nullptr) {
    log_dir = ".";
  } else {
    log_dir = home;
  }
  log_dir /= ".blackbit";
  char cdate[256];
  auto t = time(NULL);
  struct tm* tmp = localtime(&t);
  strftime(cdate, 256, "%F/%H-%M-%S", tmp);
  log_dir /= cdate;
  fs::create_directories(log_dir);
  for (int i = 1;; i++) {
    auto tmp = log_dir / std::to_string(i);
    if (mkdir(tmp.string().data(), 0700) == -1) {
      if (errno == EEXIST) { continue; }
      return bee::Error::format(
        "Failed to create log dir to '$': $", tmp, strerror(errno));
    } else {
      log_dir = tmp;
      break;
    }
  }

  return XboardWriter::create(log_dir);
}

bee::OrError<Span> parse_base(const string& repr)
{
  auto parts = bee::split(repr, ":");
  if (parts.size() > 2) {
    return bee::Error("Ill formed base time representation, expected MM[:SS]");
  }
  double mins = stod(parts[0]);
  double secs = 0;
  if (parts.size() > 1) { secs = stod(parts[1]); }
  return Span::of_seconds(secs + mins * 60);
}

bee::OrError<bee::Unit> xboard_main(
  bool enable_test,
  bool use_mpv,
  int cache_size,
  const optional<string>& pcp_path)
{
  char command[1024];
  bool go = false;
  int op_time = 0;
  Color my_color = Color::Black;

  must(writer, get_writer());

  auto logger = writer->logger();

  PCP::ptr pcp;
  if (pcp_path.has_value()) {
    bail_assign(pcp, PCP::open_on_disk(bee::FilePath::of_string(*pcp_path)));
  }

  auto state = BotState::create(
    writer,
    enable_test ? Experiment::test_with_seed(0) : Experiment::base(),
    EvalParameters::default_params(),
    use_mpv,
    cache_size,
    std::move(pcp));

  while (1) {
    if (fgets(command, sizeof(command), stdin) == NULL) break;
    logger->log_line("<- $", command);
    command[strlen(command) - 1] = 0;
    if (strlen(command) == 0) { continue; }
    int i;
    for (i = 0; command[i] != 0 && !isspace(command[i]); ++i) {}
    if (command[i] != 0) {
      command[i] = 0;
      ++i;
      for (; isspace(command[i]); ++i) {}
    }
    char* args = command + i;

    if (strcmp(command, "xboard") == 0) {
      // nothing to do here
    } else if (strcmp(command, "protover") == 0) {
      writer->send(
        "feature myname=\"blackbit\" ping=1 usermove=1 draw=0 "
        "variants=\"normal\" sigint=0 sigterm=0 setboard=1 playother=1 "
        "analyze=1 colors=0 done=1");
    } else if (strcmp(command, "new") == 0) {
      state->reset();
      my_color = Color::Black;
      go = true;
    } else if (strcmp(command, "variant") == 0) {
    } else if (strcmp(command, "quit") == 0) {
      break;
    } else if (strcmp(command, "random") == 0) {
    } else if (strcmp(command, "force") == 0) {
      go = false;
    } else if (strcmp(command, "go") == 0) {
      go = true;
      my_color = state->get_turn();
    } else if (strcmp(command, "playother") == 0) {
      my_color = oponent(state->get_turn());
      go = true;
    } else if (strcmp(command, "white") == 0) {
      // Obsolete
    } else if (strcmp(command, "black") == 0) {
      // Obsolete
    } else if (strcmp(command, "level") == 0) {
      char base_str[128];
      int mps;
      double inc;
      sscanf(args, "%d %s %lf", &mps, base_str, &inc);
      auto base = parse_base(base_str);
      if (base.is_error()) {
        logger->log_line(
          "Illegal level configuration: $ $ $: $",
          mps,
          base_str,
          inc,
          base.error().msg().data());
      } else {
        state->set_time_control(mps, base.value(), Span::of_seconds(inc));
      }
    } else if (strcmp(command, "st") == 0) {
      double max_time_secs;
      sscanf(args, "%lf", &max_time_secs);
      state->set_max_time(Span::of_seconds(max_time_secs));
    } else if (strcmp(command, "sd") == 0) {
      state->set_max_depth(atoi(args));
    } else if (strcmp(command, "time") == 0) {
      /* my time */
      Span my_time = Span::of_millis(atoi(args) * 10);
      state->set_time_remaining(my_time);
      logger->log_line("my_time: $", my_time.to_string());
    } else if (strcmp(command, "otim") == 0) {
      /* tempo do opoente */
      op_time = atoi(args);
      logger->log_line("op_time: $", op_time);
    } else if (strcmp(command, "board") == 0) {
      /* extension */
      state->print_board();
    } else if (strcmp(command, "ping") == 0) {
      writer->send("pong $", args);
    } else if (strcmp(command, "draw") == 0) {
    } else if (strcmp(command, "result") == 0) {
      go = false;
    } else if (strcmp(command, "setboard") == 0) {
      state->set_fen(args);
    } else if (
      strcmp(command, "getboard") == 0 || strcmp(command, "fen") == 0) {
      writer->send("$", state->get_fen());
    } else if (strcmp(command, "edit") == 0) {
    } else if (strcmp(command, "hint") == 0) {
    } else if (strcmp(command, "bk") == 0) {
    } else if (strcmp(command, "undo") == 0) {
      state->undo();
    } else if (strcmp(command, "remove") == 0) {
    } else if (strcmp(command, "hard") == 0) {
      state->set_ponder(true);
    } else if (strcmp(command, "easy") == 0) {
      state->set_ponder(false);
    } else if (strcmp(command, "nopost") == 0) {
      state->set_post(false);
    } else if (strcmp(command, "post") == 0) {
      state->set_post(true);
    } else if (strcmp(command, "accepted") == 0) {
    } else if (strcmp(command, "rejected") == 0) {
    } else if (strcmp(command, "?") == 0) {
    } else if (strcmp(command, "analyze") == 0) {
      state->set_ponder(true);
    } else if (strcmp(command, "exit") == 0) {
      state->set_ponder(false);
    } else if (strcmp(command, "name") == 0) {
    } else if (strcmp(command, "rating") == 0) {
    } else if (strcmp(command, "ics") == 0) {
    } else if (strcmp(command, "computer") == 0) {
    } else if (strcmp(command, "pause") == 0) {
    } else if (strcmp(command, "resume") == 0) {
    } else if (strcmp(command, "status") == 0) {
      writer->send(
        "ponder:$ my_time:$ op_time:$",
        state->get_ponder() ? "true" : "false",
        state->get_time_remaining().to_millis() / 10,
        op_time);
    } else {
      // Got move from op
      string move_str =
        strcmp(command, "usermove") == 0 || strcmp(command, "move") == 0
          ? args
          : command;
      auto ret = state->user_move(move_str);
      if (ret.is_error()) { writer->send("$", ret.error()); }
    }
    fflush(stdout);
    /* joga se for o caso */
    if (my_color == state->get_turn() && go) {
      must(m, state->move());
      writer->send("move $", m);
    }
    if (state->is_over()) { go = false; }
  }
  logger->log_line("Exiting...\n");
  state->tear_down();

  return bee::unit;
}

} // namespace

command::Cmd XboardProtocol::command()
{
  using namespace command::flags;
  auto builder =
    command::CommandBuilder("Run the bot with the xboard protocol");
  auto enable_test = builder.no_arg("--enable-test");
  auto enable_mpv = builder.no_arg("--enable-mpv");
  auto cache_size = builder.optional_with_default("--cache-size", int_flag, 30);
  auto pcp = builder.optional("--pcp-file", string_flag);
  return builder.run([=]() {
    return xboard_main(*enable_test, *enable_mpv, *cache_size, *pcp);
  });
}

} // namespace blackbit
